#include "pipeline/auditory_stream.h"

#include "core/log.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>
#include <utility>

#include <cuda_runtime.h>

#include "pipeline/diar_postprocess.h"
#include "pipeline/json_util.h"

namespace orator {
namespace pipeline {

AuditoryStream::AuditoryStream(const Config& config, Emit emit)
    : config_(config), emit_(std::move(emit)), buffer_(config.sample_rate) {}

AuditoryStream::~AuditoryStream() {
  StopWorkers();
  // asr_stream_ is owned by scheduler_ (destroyed with it); nothing to free here.
}

void AuditoryStream::Start() {
  // Spec 004 Phase 13: initialize session store from config_.session_dir.
  // When empty, derive from storage_disk_path (backward compat).
  {
    std::string session_dir = config_.session_dir;
    if (session_dir.empty() && !config_.storage_disk_path.empty()) {
      session_dir = config_.storage_disk_path;
      if (session_dir.back() != '/') session_dir.push_back('/');
      session_dir += "sessions";
    }
    session_store_ = std::make_unique<protocol::SessionStore>(session_dir);
  }

  // Spec 004 Phase 12: initialize protocol timeline and register pipelines.
  protocol_timeline_ = std::make_unique<protocol::ProtocolTimeline>(
      128 * 1024 * 1024,  // 128 MB memory backend
      config_.storage_disk_path,
      "session_" + std::to_string(session_start_wall_sec_.load()));

  // Register ws_input pipeline (produces audio/raw).
  protocol::PipelineDescriptor ws_desc;
  ws_desc.name = "ws_input";
  ws_desc.version = "1.0.0";
  ws_desc.produces = {protocol::kAudioRaw};
  ws_input_handle_ = protocol_timeline_->RegisterPipeline(std::move(ws_desc));

  // Register vad pipeline (produces vad/speech_segment).
  protocol::PipelineDescriptor vad_desc;
  vad_desc.name = "vad";
  vad_desc.version = "1.0.0";
  vad_desc.produces = {protocol::kVadSpeechSegment};
  vad_desc.consumes = {protocol::TopicPattern{"audio/+"}};
  vad_handle_ = protocol_timeline_->RegisterPipeline(std::move(vad_desc));

  // Register asr pipeline (produces asr/transcript, asr/transcript_partial).
  protocol::PipelineDescriptor asr_desc;
  asr_desc.name = "asr";
  asr_desc.version = "1.0.0";
  asr_desc.produces = {protocol::kAsrTranscript, protocol::kAsrTranscriptPartial};
  asr_desc.consumes = {protocol::TopicPattern{"vad/+"}};
  asr_handle_ = protocol_timeline_->RegisterPipeline(std::move(asr_desc));

  // Register diar pipeline (produces diar/speaker_segment).
  protocol::PipelineDescriptor diar_desc;
  diar_desc.name = "diar";
  diar_desc.version = "1.0.0";
  diar_desc.produces = {protocol::kDiarSpeakerSegment};
  diar_desc.consumes = {protocol::TopicPattern{"audio/+"}};
  diar_handle_ = protocol_timeline_->RegisterPipeline(std::move(diar_desc));

  // -----------------------------------------------------------------------
  // Spec 004 Phase 12: bridge ProtocolTimeline messages to ComprehensiveTimeline.
  // Each pipeline publishes to protocol_timeline_; these internal subscriptions
  // forward the parsed data into comp_ so Serialize() can snapshot it.
  // -----------------------------------------------------------------------

  // VAD subscription: parse {"start":..., "end":...} → comp_.AddVad()
  vad_sub_id_ = protocol_timeline_->SubscribeInternal(
      protocol::TopicPattern{"vad/+"},
      [this](const protocol::Message& msg) {
        const std::string& data = msg.data;
        auto start_pos = data.find("\"start\":");
        auto end_pos = data.find("\"end\":");
        if (start_pos != std::string::npos && end_pos != std::string::npos) {
          auto start_val_start = start_pos + 8; // len of "\"start\":"
          auto start_val_end = data.find_first_of(",}", start_val_start);
          auto end_val_start = end_pos + 6;     // len of "\"end\":"
          auto end_val_end = data.find_first_of(",}", end_val_start);
          if (start_val_end != std::string::npos && end_val_end != std::string::npos) {
            try {
              double start = std::stod(data.substr(start_val_start, start_val_end - start_val_start));
              double end = std::stod(data.substr(end_val_start, end_val_end - end_val_start));
              std::lock_guard<std::mutex> lk(comp_mutex_);
              comp_.AddVad(start, end);
            } catch (const std::exception&) {
              // malformed VAD message — skip gracefully
            }
          }
        }
      });

  // Diar subscription: parse {"segments":[{...},...]} → comp_.ReplaceSpeakers()
  diar_sub_id_ = protocol_timeline_->SubscribeInternal(
      protocol::TopicPattern{"diar/+"},
      [this](const protocol::Message& msg) {
        const std::string& data = msg.data;
        auto seg_start = data.find("\"segments\":[");
        if (seg_start == std::string::npos) return;
        seg_start += 12; // len of "\"segments\":["
        // Find matching closing bracket for the segments array
        int bracket_depth = 1;
        size_t pos = seg_start;
        while (pos < data.size() && bracket_depth > 0) {
          if (data[pos] == '[') bracket_depth++;
          else if (data[pos] == ']') bracket_depth--;
          pos++;
        }
        if (bracket_depth != 0) return;
        std::string seg_json = data.substr(seg_start, pos - seg_start - 1);

        std::vector<ComprehensiveTimeline::SpeakerInput> speakers;
        // Parse each {"start":...,"end":...,"speaker":"...","confidence":...} object
        size_t obj_pos = 0;
        while (obj_pos < seg_json.size()) {
          size_t brace = seg_json.find('{', obj_pos);
          if (brace == std::string::npos) break;
          size_t brace_end = seg_json.find('}', brace);
          if (brace_end == std::string::npos) break;
          std::string obj = seg_json.substr(brace, brace_end - brace + 1);

          auto parse_num = [&obj](const char* key) -> double {
            auto kp = obj.find("\"" + std::string(key) + "\":");
            if (kp == std::string::npos) return 0.0;
            kp += std::string("\"" + std::string(key) + ":").size();
            auto ve = obj.find_first_of(",}", kp);
            if (ve == std::string::npos) return 0.0;
            try {
              return std::stod(obj.substr(kp, ve - kp));
            } catch (const std::exception&) {
              return 0.0;
            }
          };
          auto parse_str = [&obj](const char* key) -> std::string {
            std::string search = "\"" + std::string(key) + "\":";
            auto kp = obj.find(search);
            if (kp == std::string::npos) return "";
            kp += search.size();
            if (kp >= obj.size() || obj[kp] != '"') return "";
            kp++; // skip opening quote
            auto ve = obj.find('"', kp);
            if (ve == std::string::npos) return "";
            return obj.substr(kp, ve - kp);
          };

          ComprehensiveTimeline::SpeakerInput si;
          si.start = parse_num("start");
          si.end = parse_num("end");
          si.speaker = parse_str("speaker");
          si.conf = static_cast<float>(parse_num("confidence"));
          speakers.push_back(si);

          obj_pos = brace_end + 1;
        }

        std::lock_guard<std::mutex> lk(comp_mutex_);
        comp_.ReplaceSpeakers(speakers);
      });

  // ASR subscription: parse {"id":..., "start":..., "end":..., "text":"..."} → comp_.UpsertText()
  asr_sub_id_ = protocol_timeline_->SubscribeInternal(
      protocol::TopicPattern{"asr/+"},
      [this](const protocol::Message& msg) {
        const std::string& data = msg.data;

        auto parse_num = [&data](const char* key) -> double {
          std::string search = "\"" + std::string(key) + "\":";
          auto kp = data.find(search);
          if (kp == std::string::npos) return 0.0;
          kp += search.size();
          auto ve = data.find_first_of(",}", kp);
          if (ve == std::string::npos) return 0.0;
          try {
            return std::stod(data.substr(kp, ve - kp));
          } catch (const std::exception&) {
            return 0.0;
          }
        };
        auto parse_long = [&data](const char* key) -> long {
          std::string search = "\"" + std::string(key) + "\":";
          auto kp = data.find(search);
          if (kp == std::string::npos) return -1;
          kp += search.size();
          auto ve = data.find_first_of(",}", kp);
          if (ve == std::string::npos) return -1;
          try {
            return static_cast<long>(std::stol(data.substr(kp, ve - kp)));
          } catch (const std::exception&) {
            return -1;
          }
        };
        auto parse_str = [&data](const char* key) -> std::string {
          std::string search = "\"" + std::string(key) + "\":";
          auto kp = data.find(search);
          if (kp == std::string::npos) return "";
          kp += search.size();
          if (kp >= data.size() || data[kp] != '"') return "";
          kp++; // skip opening quote
          // Handle escaped quotes within the string value
          std::string result;
          while (kp < data.size()) {
            if (data[kp] == '\\' && kp + 1 < data.size()) {
              result += data[kp + 1];
              kp += 2;
            } else if (data[kp] == '"') {
              break;
            } else {
              result += data[kp];
              kp++;
            }
          }
          return result;
        };

        long id = parse_long("id");
        double start = parse_num("start");
        double end = parse_num("end");
        std::string text = parse_str("text");

        if (id < 0) return;

        std::lock_guard<std::mutex> lk(comp_mutex_);
        comp_.UpsertText(id, start, end, text);
      });

  // The diarizer is optional: an empty weights path disables the diarization
  // pipeline (symmetric with ASR). This supports measuring each pipeline in
  // isolation. In normal operation both weights are provided.
  if (!config_.diarizer_weights.empty()) {
    diarizer_ = std::make_unique<model::SortformerDiarizer>();
    core::DiarizationConfig dc;
    dc.sample_rate = config_.sample_rate;
    dc.max_speakers = config_.max_speakers;
    dc.activity_threshold = config_.diar_threshold;
    diarizer_->Initialize(dc);
    diarizer_->LoadWeights(config_.diarizer_weights);
    // Spec 002: register the diarization pipeline as foreground priority index 0
    // (the latency-critical pipeline). Its kernels are now fully stream-routed
    // (mel, pre-encode, conformer, decoder all use the dedicated stream), so
    // create_stream=true — the worker no longer holds the global GPU lock.
    diar_stream_ = scheduler_.Register("diarization", /*priority_index=*/0,
                                       /*background=*/false,
                                       /*create_stream=*/true);
  }

  if (!config_.asr_model_dir.empty()) {
    asr_ = std::make_unique<model::Qwen3Asr>();
    core::AsrConfig ac;
    ac.sample_rate = config_.sample_rate;
    ac.language = config_.asr_language;
    asr_->Initialize(ac);
    asr_->set_language(config_.asr_language);
    asr_->set_max_new_tokens(config_.asr_max_new_tokens);
    asr_->LoadWeights(config_.asr_model_dir);

      // Spec 002: source the ASR pipeline's GPU stream from the priority
      // registry. ASR registers as a foreground pipeline at priority index 1
      // (below diarization at index 0); the scheduler derives the concrete CUDA
      // priority from the index and creates the stream. On Tegra the supported
      // priority range may be a single value, in which case priorities collapse
      // to plain stream concurrency.
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
    dp.sample_rate = config_.sample_rate;
    diar_worker_ =
        std::make_unique<DiarizationWorker>(diarizer_.get(), dp,
                                             buffer_.time_base(), diar_stream_);
    // Spec 004 Step 2: diarization publishes speaker segments to the protocol
    // timeline on the diar/speaker_segment topic. last_segments_ is stored
    // under comp_mutex_ for Serialize(). ComprehensiveTimeline will be updated
    // via ProtocolTimeline subscriptions (handled by the subscription bridge).
    diar_worker_->set_speaker_sink(
        [this](const std::vector<core::DiarSegment>& segs) {
          std::string segments_json = "[";
          {
            std::lock_guard<std::mutex> lk(comp_mutex_);
            last_segments_ = segs;
            for (size_t i = 0; i < segs.size(); ++i) {
              const auto& s = segs[i];
              const std::string label =
                  s.speaker_id.empty()
                      ? ("speaker_" + std::to_string(s.local_speaker))
                      : s.speaker_id;
              char b[160];
              std::snprintf(b, sizeof(b),
                            "{\"start\":%.3f,\"end\":%.3f,\"speaker\":\"%s\","
                            "\"confidence\":%.3f}",
                            s.start_sec, s.end_sec, label.c_str(), s.confidence);
              segments_json += b;
              if (i + 1 < segs.size()) segments_json += ",";
            }
          }
          segments_json += "]";

          protocol::Message msg;
          msg.topic = protocol::kDiarSpeakerSegment.to_string();
          msg.pipeline = "diar";
          msg.pipeline_version = "1.0.0";
          msg.timestamp_sec = segs.empty() ? 0.0 : segs[0].start_sec;
          msg.qos = static_cast<uint8_t>(protocol::QoS::AT_LEAST_ONCE);
          msg.schema_version = 1;
          msg.data = "{\"source\":\"sortformer\",\"segments\":" + segments_json + "}";

          protocol_timeline_->Publish(*diar_handle_, protocol::kDiarSpeakerSegment,
                                      msg, protocol::QoS::AT_LEAST_ONCE);
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
    asr_worker_ = std::make_unique<AsrWorker>(
        asr_.get(), p,
        [this](const std::string& json) { EmitLocked(json); },
        buffer_.time_base(), asr_stream_);
    asr_worker_->set_protocol_timeline(protocol_timeline_.get());
    asr_worker_->set_text_sink(
        [this](long id, double start, double end, const std::string& text) {
          protocol::Message msg;
          msg.topic = protocol::kAsrTranscript.to_string();
          msg.pipeline = "asr";
          msg.pipeline_version = "1.0.0";
          msg.timestamp_sec = start;
          msg.qos = static_cast<uint8_t>(protocol::QoS::AT_LEAST_ONCE);
          msg.schema_version = 1;
          msg.data = "{\"id\":" + std::to_string(id)
                     + ",\"start\":" + std::to_string(start)
                     + ",\"end\":" + std::to_string(end)
                     + ",\"text\":\"" + JsonEscape(text) + "\"}";
          protocol_timeline_->Publish(*asr_handle_,
                                      protocol::kAsrTranscript,
                                      msg,
                                      protocol::QoS::AT_LEAST_ONCE);
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

  // Spec 004 Phase 5: independent GPU VAD as a THIRD buffer consumer. It runs
  // the GPU Silero detector (GpuVad) on the continuous audio purely to publish
  // speech segments (voice-activity regions) on the common time base; it is
  // independent of the ASR and diarization pipelines (it reads only the shared
  // audio, never their output). Its per-window compute is batched on the GPU, so
  // draining a large buffered span never stalls on a single CPU core.
  if (config_.vad_stream) {
    // Spec 002: register the VAD pipeline as BACKGROUND (priority index 2). It
    // only publishes speech segments and is not latency-critical, so it yields
    // to the foreground pipelines. Its kernels are now stream-routed to their
    // own dedicated CUDA stream (lock-free).
    vad_stream_ = scheduler_.Register("vad", /*priority_index=*/2,
                                      /*background=*/true,
                                      /*create_stream=*/true);
    GpuVad::Params vp;
    vp.sample_rate = config_.sample_rate;
    vp.silero_model_path = config_.vad_model;
    vp.silero_threshold = config_.vad_threshold;
    vp.silero_min_speech_ms = config_.vad_min_speech_ms;
    vp.silero_min_silence_ms = config_.vad_min_silence_ms;
    vp.stream = vad_stream_;
    vad_detector_ = std::make_unique<GpuVad>(vp);
    vad_cursor_ = buffer_.AddConsumer();
    vad_thread_ = std::thread([this] {
      const core::TimeBase tb = buffer_.time_base();
      std::vector<float> chunk;
      std::vector<std::pair<long, long>> segs;
      auto drain = [this, &tb, &segs](bool finalize) {
        segs.clear();
        vad_detector_->DrainSegments(finalize, &segs);
        for (const auto& sp : segs) {
          const double s = tb.SecondsAt(sp.first);
          const double e = tb.SecondsAt(sp.second);
          protocol::Message msg;
          msg.topic = protocol::kVadSpeechSegment.to_string();
          msg.pipeline = "vad";
          msg.pipeline_version = "1.0.0";
          msg.timestamp_sec = s;
          msg.qos = static_cast<uint8_t>(protocol::QoS::AT_LEAST_ONCE);
          msg.schema_version = 1;
          char buf[128];
          std::snprintf(buf, sizeof(buf),
                        "{\"start\":%.3f,\"end\":%.3f,\"source\":\"silero_gpu\"}",
                        s, e);
          msg.data = buf;
          protocol_timeline_->Publish(*vad_handle_,
                                      protocol::kVadSpeechSegment,
                                      msg,
                                      protocol::QoS::AT_LEAST_ONCE);
        }
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

  // Spec 002 FR7: periodic GPU-scheduling telemetry. A dedicated low-rate timer
  // thread builds a snapshot from the priority registry + each pipeline's
  // compute/occupancy summary and emits it through the serialized transport, so
  // no GPU worker thread emits on its hot path. Disabled when the interval is 0.
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
  // Stop the telemetry timer first so it does not emit during teardown.
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
  // The producer side: append to the shared buffer and return immediately. The
  // diarization and ASR worker threads consume it independently.
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
  // Ensure all buffered audio has been processed before serializing. On
  // finalize: close the buffer and join the workers (each transcribes its
  // remaining audio inside its loop before exiting). On flush: wait until both
  // workers have processed all audio appended so far, without stopping;
  // streaming continues and worker state is retained.
  if (finalize) {
    StopWorkers();
    // Validate wall clock at session exit against the entry anchor.
    // A drift > 1s indicates system clock tampering (NTP jump, manual change).
    const auto exit_wall = std::chrono::system_clock::now();
    const double exit_sec =
        std::chrono::duration<double>(exit_wall.time_since_epoch()).count();
    const double entry_sec = session_start_wall_sec_.load();
    const double drift = std::fabs(exit_sec - entry_sec) - audio_sec();
    if (drift > 1.0) {
      wall_clock_ok_.store(false);
    }
  } else {
    WaitForBarrier(buffer_.total_samples());
  }

  // Spec 005 T040: end-point time-base reconciliation (diagnostic, env-gated).
  // After all buffered audio is processed, each pipeline's processed sample
  // extent should equal the common clock total. A non-zero gap means a pipeline
  // drifted from the shared time base. Logged only, never alters behavior.
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

std::string AuditoryStream::SerializeRevision(
    const ComprehensiveTimeline::Revision& r, const char* source) {
  char buf[160];
  std::string out = "{\"type\":\"revision\",\"source\":\"";
  out += source;
  out += "\",";
  std::snprintf(buf, sizeof(buf),
                "\"dirty_start\":%.3f,\"dirty_end\":%.3f,\"entries\":[",
                r.dirty_start, r.dirty_end);
  out += buf;
    for (size_t i = 0; i < r.entries.size(); ++i) {
    const auto& e = r.entries[i];
    int spk_idx = -1;
    if (e.speaker.size() > 8 && e.speaker.substr(0, 8) == "speaker_") {
      try { spk_idx = std::stoi(e.speaker.substr(8)); }
      catch (...) { spk_idx = -1; }
    }
    std::snprintf(buf, sizeof(buf),
                  "{\"start\":%.3f,\"end\":%.3f,\"text_id\":%ld,\"speaker\":%d,\"text\":\"",
                  e.start, e.end, e.text_id, spk_idx);
    out += std::string(buf) + JsonEscape(e.text) + "\"}";
    if (i + 1 < r.entries.size()) out += ",";
  }
  out += "]}";
  return out;
}

std::string AuditoryStream::SerializeGpuTelemetry() const {
  // Spec 002 FR7: an additive snapshot of GPU scheduling state. For each
  // registered pipeline (from the priority registry) report its declared
  // priority index + class, whether it currently runs on a dedicated prioritized
  // stream, its assigned CUDA stream priority, and a compute/occupancy summary
  // (compute_sec + real-time factor already tracked per worker). The message
  // carries the common time base like every other message and is additive: no
  // existing message changes.
  const auto entries = scheduler_.Snapshot();
  const double audio = audio_sec();
  char buf[256];
  std::string out = "{\"type\":\"gpu_telemetry\",";
  std::snprintf(buf, sizeof(buf), "\"time_sec\":%.3f,\"sample_rate\":%d,",
                audio, config_.sample_rate);
  out += buf;
  out += "\"pipelines\":[";
  for (size_t i = 0; i < entries.size(); ++i) {
    const auto& e = entries[i];
    double compute = 0.0;
    if (e.name == "diarization")
      compute = diar_worker_ ? diar_worker_->compute_sec() : 0.0;
    else if (e.name == "asr")
      compute = asr_worker_ ? asr_worker_->compute_sec() : 0.0;
    else if (e.name == "vad")
      compute = vad_detector_ ? vad_detector_->compute_sec() : 0.0;
    const double rtf = compute > 0.0 ? audio / compute : 0.0;
    std::snprintf(buf, sizeof(buf),
                  "{\"name\":\"%s\",\"priority_index\":%d,\"class\":\"%s\","
                  "\"stream_active\":%s,\"cuda_priority\":%d,"
                  "\"compute_sec\":%.3f,\"real_time_factor\":%.3f}",
                  e.name.c_str(), e.priority_index,
                  e.background ? "background" : "foreground",
                  e.stream_active ? "true" : "false", e.cuda_priority, compute,
                  rtf);
    out += buf;
    if (i + 1 < entries.size()) out += ",";
  }
  out += "]}";
  return out;
}

std::string AuditoryStream::Serialize() {
  // Read both result sets from the timeline store under its lock, then build the
  // timeline document. The document has a shared time axis and three parts:
  //   - "tracks": one independent track per pipeline (diarization, asr), each a
  //     list of that pipeline's time-ordered entries.
  //   - "comprehensive": a derived view that attributes each ASR utterance to
  //     the diarization speaker with the greatest temporal overlap and groups
  //     consecutive same-speaker utterances. Its unit is the speaker turn: who
  //     spoke, from when to when, and the text spoken. Attribution is at
  //     utterance granularity (the ASR engine does not emit per-word times).
  // A consumer reads whichever part it needs. Adding a future pipeline adds a
  // track and, optionally, a contribution to the comprehensive view.
  // StreamTimeline removed — ASR track data now comes from comp_.SnapshotRawTexts().
  // Spec 004 Step 2: the diarization worker is the sole producer of the speaker
  // view (it delivers live via ReplaceSpeakers + keeps last_segments_ fresh);
  // the ASR worker delivers text live via UpsertText. So Serialize is a pure
  // reader: snapshot everything under comp_mutex_. No derivation, no upserts here.
  std::vector<core::DiarSegment> diar_view;
  std::vector<ComprehensiveTimeline::Entry> comp_view;
  std::vector<ComprehensiveTimeline::VadSeg> vad_view;
  std::vector<ComprehensiveTimeline::RawTextSeg> raw_texts;
  {
    std::lock_guard<std::mutex> lk(comp_mutex_);
    diar_view = last_segments_;
    comp_view = comp_.Snapshot();
    vad_view = comp_.SnapshotVad();
    raw_texts = comp_.SnapshotRawTexts();
  }
  // Populate last_transcript_ from raw_texts for the transcript() accessor.
  {
    core::Transcript transcript;
    for (const auto& r : raw_texts) {
      core::AsrToken tok;
      tok.start_sec = r.start;
      tok.end_sec = r.end;
      tok.text = r.text;
      transcript.tokens.push_back(std::move(tok));
    }
    last_transcript_ = std::move(transcript);
  }

  const double audio = audio_sec();
  const double diar_c = diar_compute_sec();
  const double asr_c = asr_compute_sec();
  const double wall_start = session_start_wall_sec_.load();
  const bool wclk_ok = wall_clock_ok_.load();

  char buf[256];
  std::string out = "{\"type\":\"timeline\",\"schema_version\":1,";
  std::snprintf(buf, sizeof(buf),
                "\"audio_sec\":%.3f,\"sample_rate\":%d,"
                "\"session_start_wall_sec\":%.3f,\"wall_clock_ok\":%s,",
                audio, config_.sample_rate, wall_start, wclk_ok ? "true" : "false");
  out += buf;
  out += "\"tracks\":[";

  // Track: speaker diarization.
  std::snprintf(buf, sizeof(buf),
                "{\"kind\":\"diarization\",\"source\":\"sortformer\","
                "\"compute_sec\":%.3f,\"real_time_factor\":%.3f,\"entries\":[",
                diar_c, diar_c > 0 ? audio / diar_c : 0.0);
  out += buf;
  for (size_t i = 0; i < diar_view.size(); ++i) {
    const auto& s = diar_view[i];
    std::snprintf(buf, sizeof(buf),
                  "{\"start\":%.3f,\"end\":%.3f,\"speaker\":%d,\"confidence\":%.3f}",
                  s.start_sec, s.end_sec, s.local_speaker, s.confidence);
    out += buf;
    if (i + 1 < diar_view.size()) out += ",";
  }
  out += "]}";

  // Track: automatic speech recognition (present only when ASR is enabled).
  if (asr_) {
    std::snprintf(buf, sizeof(buf),
                  ",{\"kind\":\"asr\",\"source\":\"qwen3_asr\","
                  "\"compute_sec\":%.3f,\"real_time_factor\":%.3f,\"entries\":[",
                  asr_c, asr_c > 0 ? audio / asr_c : 0.0);
    out += buf;
    for (size_t i = 0; i < last_transcript_.tokens.size(); ++i) {
      const auto& t = last_transcript_.tokens[i];
      std::snprintf(buf, sizeof(buf), "{\"start\":%.3f,\"end\":%.3f,\"text\":\"",
                    t.start_sec, t.end_sec);
      out += std::string(buf) + JsonEscape(t.text) + "\"}";
      if (i + 1 < last_transcript_.tokens.size()) out += ",";
    }
    out += "]}";
  }

  // Track: voice activity (VAD). The VAD pipeline's data track: speech segments
  // [start,end) on the common time base. It does not alter the diarization or
  // ASR tracks, nor drive the comprehensive view's boundaries. Present only when
  // the VAD pipeline is enabled.
  if (config_.vad_stream) {
    const double vad_c = vad_detector_ ? vad_detector_->compute_sec() : 0.0;
    std::snprintf(buf, sizeof(buf),
                  ",{\"kind\":\"vad\",\"source\":\"silero_gpu\","
                  "\"compute_sec\":%.3f,\"real_time_factor\":%.3f,\"entries\":[",
                  vad_c, vad_c > 0 ? audio / vad_c : 0.0);
    out += buf;
    for (size_t i = 0; i < vad_view.size(); ++i) {
      std::snprintf(buf, sizeof(buf), "{\"start\":%.3f,\"end\":%.3f}",
                    vad_view[i].start, vad_view[i].end);
      out += buf;
      if (i + 1 < vad_view.size()) out += ",";
    }
    out += "]}";
  }
  out += "]";  // close "tracks"

  // Comprehensive view: speaker turns with their spoken text, ordered by time.
  // Present only when ASR is enabled (it requires both modalities). Built from
  // the native stateful ComprehensiveTimeline (Spec 004): diarization provides
  // who/when, ASR provides what/when, attribution is by time alignment on the
  // common base. All three pipelines (diarization, ASR, VAD) deliver LIVE
  // to comp_ via their sinks; Serialize is a pure reader and just emits the
  // snapshot taken above (comp_view). No upserts here.
  out += ",\"comprehensive\":[";
  if (asr_) {
    for (size_t i = 0; i < comp_view.size(); ++i) {
      const auto& e = comp_view[i];
      // Extract numeric speaker index from "speaker_N" format.
      int spk_idx = -1;
      if (e.speaker.size() > 8 && e.speaker.substr(0, 8) == "speaker_") {
        try { spk_idx = std::stoi(e.speaker.substr(8)); }
        catch (...) { spk_idx = -1; }
      }
      std::snprintf(buf, sizeof(buf),
                    "{\"start\":%.3f,\"end\":%.3f,\"text_id\":%ld,\"speaker\":%d,\"text\":\"",
                    e.start, e.end, e.text_id, spk_idx);
      out += std::string(buf) + JsonEscape(e.text) + "\"}";
      if (i + 1 < comp_view.size()) out += ",";
    }
  }
  out += "]}";
  return out;
}

void AuditoryStream::Reset() {
  StopWorkers();         // stop any running threads first

  // Spec 004 Phase 13: save the timeline before clearing state.
  if (session_store_ && session_store_->enabled()) {
    // Capture the timeline of the session that just ended.
    std::string timeline_json = Serialize();
    // Generate a unique session ID from wall clock time.
    const auto now = std::chrono::system_clock::now();
    double wall_sec = std::chrono::duration<double>(now.time_since_epoch()).count();
    char session_id_buf[64];
    std::snprintf(session_id_buf, sizeof(session_id_buf), "%08x%08x",
                  static_cast<unsigned>(static_cast<long long>(wall_sec)),
                  static_cast<unsigned>(::getpid()));
    session_store_->Save(session_id_buf, timeline_json);
  }

  // NOTE: protocol timeline subscriptions and pipeline registrations are
  // session-invariant — they are set up once in Start() and MUST persist
  // across client resets. Do NOT unsubscribe or unregister here.
  buffer_.Reset();
  last_segments_.clear();
  last_transcript_.tokens.clear();
  {
    std::lock_guard<std::mutex> lk(comp_mutex_);
    comp_.Clear();
  }
  if (diarizer_) diarizer_->Reset();
  if (asr_) asr_->Reset();
  // Record wall clock at session entry for physical-time mapping.
  const auto now = std::chrono::system_clock::now();
  session_start_wall_sec_.store(
      std::chrono::duration<double>(now.time_since_epoch()).count());
  wall_clock_ok_.store(true);
  StartWorkers();        // start new workers so streaming can resume
}

}  // namespace pipeline
}  // namespace orator
