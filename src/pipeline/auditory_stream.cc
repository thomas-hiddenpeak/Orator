#include "pipeline/auditory_stream.h"
#include "pipeline/auditory_stream_subscriptions.h"

#include "core/log.h"
#include "core/registry.h"
#include "model/builtin_registration.h"
#include "protocol/protocol_timeline.h"
#include "protocol/session_store.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>
#include <utility>

#include <cuda_runtime.h>

namespace orator {
namespace pipeline {

AuditoryStream::AuditoryStream(const Config& config, Emit emit)
    : config_(config), emit_(std::move(emit)), buffer_(config.sample_rate) {}

AuditoryStream::~AuditoryStream() { StopWorkers(); }

void AuditoryStream::Start() {
  model::EnsureBuiltinsRegistered();
  {
    std::string session_dir = config_.session_dir;
    if (session_dir.empty() && !config_.storage_disk_path.empty()) {
      session_dir = config_.storage_disk_path;
      if (session_dir.back() != '/') session_dir.push_back('/');
      session_dir += "sessions";
    }
    session_store_ = std::make_unique<protocol::SessionStore>(session_dir);
  }
  protocol_timeline_ = std::make_unique<protocol::ProtocolTimeline>(
      128 * 1024 * 1024,
      config_.storage_disk_path,
      "session_" + std::to_string(session_start_wall_sec_.load()));

  protocol::PipelineDescriptor ws_desc;
  ws_desc.name = "ws_input";
  ws_desc.version = "1.0.0";
  ws_desc.produces = {protocol::kAudioRaw};
  ws_input_handle_ = protocol_timeline_->RegisterPipeline(std::move(ws_desc));

  protocol::PipelineDescriptor vad_desc;
  vad_desc.name = "vad";
  vad_desc.version = "1.0.0";
  vad_desc.produces = {protocol::kVadSpeechSegment};
  vad_desc.consumes = {protocol::TopicPattern{"audio/+"}};
  vad_handle_ = protocol_timeline_->RegisterPipeline(std::move(vad_desc));

  protocol::PipelineDescriptor asr_desc;
  asr_desc.name = "asr";
  asr_desc.version = "1.0.0";
  asr_desc.produces = {protocol::kAsrTranscript, protocol::kAsrTranscriptPartial};
  asr_desc.consumes = {protocol::TopicPattern{"vad/+"}};
  asr_handle_ = protocol_timeline_->RegisterPipeline(std::move(asr_desc));

  protocol::PipelineDescriptor diar_desc;
  diar_desc.name = "diar";
  diar_desc.version = "1.0.0";
  diar_desc.produces = {protocol::kDiarSpeakerSegment};
  diar_desc.consumes = {protocol::TopicPattern{"audio/+"}};
  diar_handle_ = protocol_timeline_->RegisterPipeline(std::move(diar_desc));

  vad_sub_id_ = protocol_timeline_->SubscribeInternal(
      protocol::TopicPattern{"vad/+"},
      [this](const protocol::Message& msg) {
        HandleVadSubscription(comp_, comp_mutex_, msg);
      });
  diar_sub_id_ = protocol_timeline_->SubscribeInternal(
      protocol::TopicPattern{"diar/+"},
      [this](const protocol::Message& msg) {
        HandleDiarSubscription(comp_, comp_mutex_, msg,
            [this](const std::string& rev_json) { EmitLocked(rev_json); });
      });
  asr_sub_id_ = protocol_timeline_->SubscribeInternal(
      protocol::TopicPattern{"asr/+"},
      [this](const protocol::Message& msg) {
        HandleAsrSubscription(comp_, comp_mutex_, msg,
            [this](const std::string& rev_json) { EmitLocked(rev_json); });
      });

  if (!config_.diarizer_weights.empty()) {
    diarizer_ = core::Registry<core::IDiarizer>::Instance().Create("sortformer");
    core::DiarizationConfig dc;
    dc.sample_rate = config_.sample_rate;
    dc.max_speakers = config_.max_speakers;
    dc.activity_threshold = config_.diar_threshold;
    diarizer_->Initialize(dc);
    diarizer_->LoadWeights(config_.diarizer_weights);
    diar_stream_ = scheduler_.Register("diarization", /*priority_index=*/0,
                                       /*background=*/false,
                                       /*create_stream=*/true);
  }
  if (!config_.asr_model_dir.empty()) {
    asr_ = core::Registry<core::IAsr>::Instance().Create("qwen3_asr");
    core::AsrConfig ac;
    ac.sample_rate = config_.sample_rate;
    ac.language = config_.asr_language;
    asr_->Initialize(ac);
    asr_->set_max_new_tokens(config_.asr_max_new_tokens);
    asr_->LoadWeights(config_.asr_model_dir);
    asr_stream_ = scheduler_.Register("asr", /*priority_index=*/1,
                                      /*background=*/false,
                                      /*create_stream=*/true);
    int greatest = 0, least = 0;
    scheduler_.PriorityRange(&greatest, &least);
    LOG_INFO("[gpu-sched] priority range [greatest=%d, least=%d]; "
             "asr at index 1 (foreground)\n",
             greatest, least);
  }
  StartWorkers();
}

void AuditoryStream::StartWorkers() {
  if (diarizer_) {
    DiarizationWorker::Params dp;
    dp.threshold = config_.diar_threshold;
    dp.merge_gap_sec = config_.diar_merge_gap_sec;
    dp.deliver_interval_sec = config_.diar_deliver_interval_sec;
    dp.sample_rate = config_.sample_rate;
    diar_worker_ =
        std::make_unique<DiarizationWorker>(diarizer_.get(), dp,
            buffer_.time_base(), diar_stream_);
    diar_worker_->set_speaker_sink(
        [this](const std::vector<core::DiarSegment>& segs) {
          HandleSpeakerSink(comp_mutex_, last_segments_,
                            protocol_timeline_.get(), diar_handle_.get(),
                            segs);
        });
    diar_cursor_ = buffer_.AddConsumer();
    diar_thread_ = std::thread([this] {
      std::vector<float> chunk;
      long span_start_abs = 0;
      while (buffer_.WaitAndRead(diar_cursor_, &chunk, &span_start_abs)) {
        diar_worker_->ProcessSpan(chunk.data(), static_cast<int>(chunk.size()));
        progress_cv_.notify_all();
      }
      diar_worker_->Finalize();
      progress_cv_.notify_all();
    });
  }
  if (asr_) {
    AsrWorker::Params p;
    p.sample_rate = config_.sample_rate;
    p.segment_sec = config_.asr_segment_sec;
    p.asr_vad_gate = config_.asr_vad_gate;
    p.asr_vad_lead_ms = config_.asr_vad_lead_ms;
    p.asr_vad_trail_sec = config_.asr_vad_trail_sec;
    p.max_audio_tokens = config_.asr_max_audio_tokens;
    AsrWorker::VadSegmentReader vad_reader =
        [this]() -> std::vector<std::pair<double, double>> {
      std::lock_guard<std::mutex> lk(comp_mutex_);
      auto vad_segs = comp_.SnapshotVad();
      std::vector<std::pair<double, double>> result;
      result.reserve(vad_segs.size());
      for (const auto& s : vad_segs) {
        result.emplace_back(s.start, s.end);
      }
      return result;
    };
    asr_worker_ = std::make_unique<AsrWorker>(asr_.get(), p,
        [this](const std::string& json) { EmitLocked(json); },
        buffer_.time_base(), asr_stream_, vad_reader);
    asr_worker_->set_text_sink(
        [this](long id, double start, double end, const std::string& text) {
          HandleTextSink(protocol_timeline_.get(), asr_handle_.get(),
                         id, start, end, text);
        });
    asr_cursor_ = buffer_.AddConsumer();
    asr_thread_ = std::thread([this] {
      std::vector<float> chunk;
      long span_start_abs = 0;
      while (buffer_.WaitAndRead(asr_cursor_, &chunk, &span_start_abs)) {
        asr_worker_->ProcessSpan(chunk.data(), static_cast<int>(chunk.size()));
        progress_cv_.notify_all();
      }
      asr_worker_->Finalize();
      progress_cv_.notify_all();
    });
  }
  if (config_.vad_stream) {
    vad_stream_ = scheduler_.Register("vad", /*priority_index=*/2,
                                      /*background=*/true,
                                      /*create_stream=*/true);
    GpuVad::Params vp;
    vp.sample_rate = config_.sample_rate;
    vp.silero_model_path = config_.vad_model;
    vp.silero_threshold = config_.vad_threshold;
    vp.silero_min_speech_ms = config_.vad_min_speech_ms;
    vp.silero_min_silence_ms = config_.vad_min_silence_ms;
    vp.silero_speech_pad_ms = config_.vad_speech_pad_ms;
    vp.stream = vad_stream_;
    auto vad_params_ptr = std::make_shared<GpuVad::Params>(vp);
    core::Registry<core::IVad>::Instance().Register(
        "silero_vad",
        [vad_params_ptr] { return std::make_unique<GpuVad>(*vad_params_ptr); });
    vad_detector_ =
        core::Registry<core::IVad>::Instance().Create("silero_vad");
    vad_cursor_ = buffer_.AddConsumer();
    vad_thread_ = std::thread([this] {
      const core::TimeBase tb = buffer_.time_base();
      std::vector<float> chunk;
      std::vector<core::VadSegmentResult> segs;
      auto drain = [this, &tb, &segs](bool finalize) {
        HandleVadDrain(vad_detector_.get(), protocol_timeline_.get(),
                       vad_handle_.get(), tb, &segs, finalize);
      };
      long span_start_abs = 0;
      while (buffer_.WaitAndRead(vad_cursor_, &chunk, &span_start_abs)) {
        vad_detector_->Push(chunk.data(), static_cast<int>(chunk.size()));
        drain(/*finalize=*/false);
        {
          char buf[64];
          std::snprintf(buf, sizeof(buf),
                        "{\"type\":\"vad_state\",\"speech\":%s}",
                        vad_detector_->is_in_speech() ? "true" : "false");
          EmitLocked(buf);
        }
        progress_cv_.notify_all();
      }
      drain(/*finalize=*/true);
      progress_cv_.notify_all();
    });
  }
  if (config_.gpu_telemetry_interval_sec > 0.0) {
    telemetry_stop_ = false;
    telemetry_thread_ = std::thread([this] {
      const auto interval = std::chrono::duration<double>(
          config_.gpu_telemetry_interval_sec);
      for (;;) {
        {
          std::unique_lock<std::mutex> lk(telemetry_mutex_);
          telemetry_cv_.wait_for(lk, interval,
                                 [this] { return telemetry_stop_; });
          if (telemetry_stop_) break;
        }
        const std::string msg = SerializeGpuTelemetry();
        if (!msg.empty()) EmitLocked(msg);
      }
    });
  }
  running_ = true;
}

void AuditoryStream::StopWorkers() {
  if (!running_) return;
  {
    std::lock_guard<std::mutex> lk(telemetry_mutex_);
    telemetry_stop_ = true;
  }
  telemetry_cv_.notify_all();
  if (telemetry_thread_.joinable()) telemetry_thread_.join();
  buffer_.Close();
  if (diar_thread_.joinable()) diar_thread_.join();
  if (asr_thread_.joinable()) asr_thread_.join();
  if (vad_thread_.joinable()) vad_thread_.join();
  running_ = false;
}

double AuditoryStream::audio_sec() const {
  return buffer_.time_base().Duration(buffer_.total_samples());
}

double AuditoryStream::diar_compute_sec() const {
  return diar_worker_ ? diar_worker_->compute_sec() : 0.0;
}

double AuditoryStream::asr_compute_sec() const {
  return asr_worker_ ? asr_worker_->compute_sec() : 0.0;
}

void AuditoryStream::PushAudio(const float* samples, int n) {
  buffer_.Append(samples, n);
}

void AuditoryStream::WaitForBarrier(long target) {
  std::unique_lock<std::mutex> lock(progress_mutex_);
  progress_cv_.wait(lock, [&] {
    if (!running_) return true;
    const bool diar_ok =
        !diar_worker_ || diar_worker_->processed_samples() >= target;
    const bool asr_ok =
        !asr_worker_ || asr_worker_->processed_samples() >= target;
    return diar_ok && asr_ok;
  });
}

void AuditoryStream::EmitLocked(const std::string& json) {
  std::lock_guard<std::mutex> lock(emit_mutex_);
  if (emit_) emit_(json);
}

void AuditoryStream::EmitTimeline(bool finalize) {
  if (finalize) {
    StopWorkers();
    const auto exit_wall = std::chrono::system_clock::now();
    const double exit_sec =
        std::chrono::duration<double>(exit_wall.time_since_epoch()).count();
    const double entry_sec = session_start_wall_sec_.load();
    const double drift = std::fabs(exit_sec - entry_sec) - audio_sec();
    if (drift > 1.0) wall_clock_ok_.store(false);
  } else {
    WaitForBarrier(buffer_.total_samples());
  }
  if (const char* c = std::getenv("ORATOR_TIMEBASE_CHECK"); c && c[0] == '1') {
    const long total = buffer_.total_samples();
    auto report = [total](const char* who, long processed) {
      const long gap = core::TimeBase::ReconcileExtent(processed, total);
      if (gap != 0)
        LOG_INFO("[timebase] %s extent %ld vs common total %ld -> gap %ld\n",
                 who, processed, total, gap);
    };
    if (diar_worker_) report("diarization", diar_worker_->processed_samples());
    if (asr_worker_) report("asr", asr_worker_->processed_samples());
  }
  EmitLocked(Serialize());
}

// Serialize/SerializeRevision/SerializeGpuTelemetry in serialize.cc.

void AuditoryStream::Reset() {
  StopWorkers();
  if (session_store_ && session_store_->enabled()) {
    std::string timeline_json = Serialize();
    const auto now = std::chrono::system_clock::now();
    double wall_sec = std::chrono::duration<double>(now.time_since_epoch()).count();
    char session_id_buf[64];
    std::snprintf(session_id_buf, sizeof(session_id_buf), "%08x%08x",
                  static_cast<unsigned>(static_cast<long long>(wall_sec)),
                  static_cast<unsigned>(::getpid()));
    session_store_->Save(session_id_buf, timeline_json);
  }
  buffer_.Reset();
  last_segments_.clear();
  last_transcript_.tokens.clear();
  {
    std::lock_guard<std::mutex> lk(comp_mutex_);
    comp_.Clear();
  }
  if (diarizer_) diarizer_->Reset();
  if (asr_) asr_->Reset();
  const auto now = std::chrono::system_clock::now();
  session_start_wall_sec_.store(
      std::chrono::duration<double>(now.time_since_epoch()).count());
  wall_clock_ok_.store(true);
  StartWorkers();
}

}  // namespace pipeline
}  // namespace orator
