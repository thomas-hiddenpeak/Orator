#include "pipeline/auditory_stream.h"

#include <cstdio>
#include <utility>

#include "pipeline/diar_postprocess.h"
#include "pipeline/json_util.h"

namespace orator {
namespace pipeline {

AuditoryStream::AuditoryStream(const Config& config, Emit emit)
    : config_(config), emit_(std::move(emit)), buffer_(config.sample_rate) {}

AuditoryStream::~AuditoryStream() { StopWorkers(); }

void AuditoryStream::Start() {
  diarizer_ = std::make_unique<model::SortformerDiarizer>();
  core::DiarizationConfig dc;
  dc.sample_rate = config_.sample_rate;
  dc.max_speakers = config_.max_speakers;
  dc.activity_threshold = config_.diar_threshold;
  diarizer_->Initialize(dc);
  diarizer_->LoadWeights(config_.diarizer_weights);

  if (!config_.asr_model_dir.empty()) {
    asr_ = std::make_unique<model::Qwen3Asr>();
    core::AsrConfig ac;
    ac.sample_rate = config_.sample_rate;
    ac.language = config_.asr_language;
    asr_->Initialize(ac);
    asr_->set_language(config_.asr_language);
    asr_->LoadWeights(config_.asr_model_dir);
  }
  StartWorkers();
}

void AuditoryStream::StartWorkers() {
  diar_worker_ = std::make_unique<DiarizationWorker>(diarizer_.get(), &timeline_);
  diar_cursor_ = buffer_.AddConsumer();
  diar_thread_ = std::thread([this] {
    std::vector<float> chunk;
    while (buffer_.WaitAndRead(diar_cursor_, &chunk)) {
      diar_worker_->ProcessSpan(chunk.data(), static_cast<int>(chunk.size()));
      progress_cv_.notify_all();
    }
    diar_worker_->Finalize();
    progress_cv_.notify_all();
  });

  if (asr_) {
    AsrWorker::Params p;
    p.sample_rate = config_.sample_rate;
    p.endpoint_silence_sec = config_.asr_endpoint_silence_sec;
    p.max_utterance_sec = config_.asr_max_utterance_sec;
    p.min_utterance_sec = config_.asr_min_utterance_sec;
    p.vad_rel_threshold = config_.asr_vad_rel_threshold;
    // Wrap the transport emit so worker-thread events are serialized with the
    // controller's timeline emit.
    asr_worker_ = std::make_unique<AsrWorker>(
        asr_.get(), &timeline_, p,
        [this](const std::string& json) { EmitLocked(json); });
    asr_cursor_ = buffer_.AddConsumer();
    asr_thread_ = std::thread([this] {
      std::vector<float> chunk;
      while (buffer_.WaitAndRead(asr_cursor_, &chunk)) {
        asr_worker_->ProcessSpan(chunk.data(), static_cast<int>(chunk.size()));
        progress_cv_.notify_all();
      }
      asr_worker_->Finalize();
      progress_cv_.notify_all();
    });
  }
  running_ = true;
}

void AuditoryStream::StopWorkers() {
  if (!running_) return;
  buffer_.Close();
  if (diar_thread_.joinable()) diar_thread_.join();
  if (asr_thread_.joinable()) asr_thread_.join();
  running_ = false;
}

double AuditoryStream::audio_sec() const {
  return config_.sample_rate > 0
             ? static_cast<double>(buffer_.total_samples()) / config_.sample_rate
             : 0.0;
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
  // Drain the pipelines before serializing. On finalize: signal end-of-stream
  // and join the workers (each flushes its tail inside its loop). On flush:
  // wait until both workers have consumed all audio pushed so far, without
  // stopping -- streaming continues with state preserved.
  if (finalize) {
    StopWorkers();
  } else {
    WaitForBarrier(buffer_.total_samples());
  }
  EmitLocked(Serialize());
}

std::string AuditoryStream::Serialize() {
  // Snapshot both result streams under the timeline lock, then build the
  // unified document: independent diarization + transcript arrays on one clock
  // (fusion/attribution is a downstream concern).
  core::DiarizationFrames frames = timeline_.SnapshotDiarFrames();
  core::Transcript transcript = timeline_.SnapshotTranscript();

  last_segments_.clear();
  if (frames.num_frames > 0 && frames.num_speakers > 0) {
    auto segs = FramesToSegments(frames, config_.diar_threshold,
                                 config_.diar_merge_gap_sec);
    last_segments_ = CoalesceSegments(std::move(segs), config_.diar_merge_gap_sec);
  }
  last_transcript_ = transcript;

  std::string out = "{\"type\":\"timeline\",";
  char buf[256];
  std::snprintf(buf, sizeof(buf),
                "\"audio_sec\":%.3f,\"diar_compute_sec\":%.3f,"
                "\"asr_compute_sec\":%.3f,",
                audio_sec(), diar_compute_sec(), asr_compute_sec());
  out += buf;

  out += "\"diarization\":[";
  for (size_t i = 0; i < last_segments_.size(); ++i) {
    const auto& s = last_segments_[i];
    std::snprintf(buf, sizeof(buf),
                  "{\"start\":%.3f,\"end\":%.3f,\"speaker\":%d,\"confidence\":%.3f}",
                  s.start_sec, s.end_sec, s.local_speaker, s.confidence);
    out += buf;
    if (i + 1 < last_segments_.size()) out += ",";
  }
  out += "],\"transcript\":[";
  for (size_t i = 0; i < last_transcript_.tokens.size(); ++i) {
    const auto& t = last_transcript_.tokens[i];
    std::snprintf(buf, sizeof(buf), "{\"start\":%.3f,\"end\":%.3f,\"text\":\"",
                  t.start_sec, t.end_sec);
    out += std::string(buf) + JsonEscape(t.text) + "\"}";
    if (i + 1 < last_transcript_.tokens.size()) out += ",";
  }
  out += "]}";
  return out;
}

void AuditoryStream::Reset() {
  StopWorkers();         // stop any running threads first
  buffer_.Reset();
  timeline_.Clear();
  last_segments_.clear();
  last_transcript_.tokens.clear();
  if (diarizer_) diarizer_->Reset();
  if (asr_) asr_->Reset();
  StartWorkers();        // re-arm so streaming can resume
}

}  // namespace pipeline
}  // namespace orator
