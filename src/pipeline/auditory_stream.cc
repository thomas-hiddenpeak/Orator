#include "pipeline/auditory_stream.h"

#include <cstdio>
#include <utility>

#include "pipeline/diar_postprocess.h"
#include "pipeline/json_util.h"
#include "pipeline/timeline_merger.h"

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
  // Ensure all buffered audio has been processed before serializing. On
  // finalize: close the buffer and join the workers (each transcribes its
  // remaining audio inside its loop before exiting). On flush: wait until both
  // workers have processed all audio appended so far, without stopping;
  // streaming continues and worker state is retained.
  if (finalize) {
    StopWorkers();
  } else {
    WaitForBarrier(buffer_.total_samples());
  }
  EmitLocked(Serialize());
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
  core::DiarizationFrames frames = timeline_.SnapshotDiarFrames();
  core::Transcript transcript = timeline_.SnapshotTranscript();

  last_segments_.clear();
  if (frames.num_frames > 0 && frames.num_speakers > 0) {
    auto segs = FramesToSegments(frames, config_.diar_threshold,
                                 config_.diar_merge_gap_sec);
    last_segments_ = CoalesceSegments(std::move(segs), config_.diar_merge_gap_sec);
  }
  last_transcript_ = transcript;

  const double audio = audio_sec();
  const double diar_c = diar_compute_sec();
  const double asr_c = asr_compute_sec();

  char buf[256];
  std::string out = "{\"type\":\"timeline\",\"schema_version\":1,";
  std::snprintf(buf, sizeof(buf), "\"audio_sec\":%.3f,\"sample_rate\":%d,",
                audio, config_.sample_rate);
  out += buf;
  out += "\"tracks\":[";

  // Track: speaker diarization.
  std::snprintf(buf, sizeof(buf),
                "{\"kind\":\"diarization\",\"source\":\"sortformer\","
                "\"compute_sec\":%.3f,\"real_time_factor\":%.3f,\"entries\":[",
                diar_c, diar_c > 0 ? audio / diar_c : 0.0);
  out += buf;
  for (size_t i = 0; i < last_segments_.size(); ++i) {
    const auto& s = last_segments_[i];
    std::snprintf(buf, sizeof(buf),
                  "{\"start\":%.3f,\"end\":%.3f,\"speaker\":%d,\"confidence\":%.3f}",
                  s.start_sec, s.end_sec, s.local_speaker, s.confidence);
    out += buf;
    if (i + 1 < last_segments_.size()) out += ",";
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
  out += "]";  // close "tracks"

  // Comprehensive view: speaker turns with their spoken text, ordered by time.
  // Present only when ASR is enabled (it requires both modalities).
  out += ",\"comprehensive\":[";
  if (asr_) {
    OverlapTimelineMerger merger;
    core::Timeline merged = merger.Merge(last_segments_, last_transcript_);
    for (size_t i = 0; i < merged.segments.size(); ++i) {
      const auto& seg = merged.segments[i];
      std::snprintf(buf, sizeof(buf),
                    "{\"start\":%.3f,\"end\":%.3f,\"speaker\":\"",
                    seg.start_sec, seg.end_sec);
      out += std::string(buf) + JsonEscape(seg.speaker_id) + "\",\"text\":\"" +
             JsonEscape(seg.text) + "\"}";
      if (i + 1 < merged.segments.size()) out += ",";
    }
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
  StartWorkers();        // start new workers so streaming can resume
}

}  // namespace pipeline
}  // namespace orator
