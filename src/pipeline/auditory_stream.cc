#include "pipeline/auditory_stream.h"

#include <cstdio>
#include <utility>

#include <cuda_runtime.h>

#include "pipeline/diar_postprocess.h"
#include "pipeline/json_util.h"
#include "pipeline/timeline_merger.h"

namespace orator {
namespace pipeline {

AuditoryStream::AuditoryStream(const Config& config, Emit emit)
    : config_(config), emit_(std::move(emit)), buffer_(config.sample_rate) {}

AuditoryStream::~AuditoryStream() {
  StopWorkers();
  if (asr_stream_) cudaStreamDestroy(asr_stream_);
}

void AuditoryStream::Start() {
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

      // Create a CUDA stream for the ASR pipeline. Give it a lower priority than
      // the default (diarization) stream so diarization latency is protected.
      // On Tegra the priority range may be [0, 0]; cudaStreamCreateWithPriority
      // still succeeds, it just creates a normal stream.
      int leastPri = 0, greatestPri = 0;
      cudaDeviceGetStreamPriorityRange(&leastPri, &greatestPri);
      cudaStreamCreateWithPriority(&asr_stream_, cudaStreamNonBlocking, leastPri);
      std::fprintf(stderr, "[gpu-sched] stream priority range [%d,%d]; "
                   "asr_stream at %d (lower priority)\n",
                   greatestPri, leastPri, leastPri);
  }
  StartWorkers();
}

void AuditoryStream::StartWorkers() {
  if (diarizer_) {
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
  }

  if (asr_) {
    AsrWorker::Params p;
    p.vad.sample_rate = config_.sample_rate;
    p.vad.max_utterance_sec = config_.asr_max_utterance_sec;
    p.vad.min_utterance_sec = config_.asr_min_utterance_sec;
    p.vad.silero_model_path = config_.asr_vad_model;
    p.vad.silero_threshold = config_.asr_vad_threshold;
    p.vad.silero_min_speech_ms = config_.asr_vad_min_speech_ms;
    p.vad.silero_min_silence_ms = config_.asr_vad_min_silence_ms;
    p.vad.silero_speech_pad_ms = config_.asr_vad_speech_pad_ms;
    p.preproc.sample_rate = config_.sample_rate;
    p.preproc.mode = config_.asr_preproc_mode;
    p.preproc.frcrn_model_path = config_.asr_frcrn_model;
    p.preproc.tfgridnet_model_path = config_.asr_tfgridnet_model;
    p.incremental = config_.asr_incremental;
    p.segment_sec = config_.asr_incremental_segment_sec;
    p.endpoint_reset = config_.asr_incremental_endpoint_reset;
    p.endpoint_min_segment_sec = config_.asr_incremental_min_segment_sec;
    // Wrap the transport emit so worker-thread events are serialized with the
    // controller's timeline emit.
    asr_worker_ = std::make_unique<AsrWorker>(
        asr_.get(), &timeline_, p,
        [this](const std::string& json) { EmitLocked(json); },
      asr_stream_, config_.asr_rollback_tokens);
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

  // Spec 004 T031: independent speech-endpoint detector as a THIRD buffer
  // consumer. It runs Silero on the continuous audio purely to publish endpoint
  // markers on the common time base; it is independent of the ASR and
  // diarization pipelines (it reads only the shared audio, never their output).
  if (config_.endpoint_stream) {
    AsrSileroVad::Params vp;
    vp.sample_rate = config_.sample_rate;
    vp.silero_model_path = config_.asr_vad_model;
    vp.silero_threshold = config_.asr_vad_threshold;
    vp.silero_min_speech_ms = config_.asr_vad_min_speech_ms;
    vp.silero_min_silence_ms = config_.asr_vad_min_silence_ms;
    vp.silero_speech_pad_ms = config_.asr_vad_speech_pad_ms;
    endpoint_vad_ = std::make_unique<AsrSileroVad>(vp);
    endpoint_cursor_ = buffer_.AddConsumer();
    endpoint_thread_ = std::thread([this] {
      const int sr = config_.sample_rate;
      std::vector<float> chunk;
      auto drain = [this, sr](bool finalize) {
        long ep = 0;
        while (endpoint_vad_->NextEndpoint(finalize, &ep)) {
          const double t = static_cast<double>(ep) / sr;
          {
            std::lock_guard<std::mutex> lk(comp_mutex_);
            comp_.MarkEndpoint(t);
          }
          char buf[96];
          std::snprintf(buf, sizeof(buf),
                        "{\"type\":\"endpoint\",\"time\":%.3f}", t);
          EmitLocked(buf);
        }
      };
      while (buffer_.WaitAndRead(endpoint_cursor_, &chunk)) {
        endpoint_vad_->Push(chunk.data(), static_cast<int>(chunk.size()));
        drain(/*finalize=*/false);
        progress_cv_.notify_all();
      }
      drain(/*finalize=*/true);
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
  if (endpoint_thread_.joinable()) endpoint_thread_.join();
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
  // Present only when ASR is enabled (it requires both modalities). Built from
  // the native stateful ComprehensiveTimeline (Spec 004): diarization provides
  // who/when, ASR provides what/when, and the attribution is by time alignment
  // on the common base. Diar segments are refreshed here (frame->segment is
  // global); ASR text is upserted live by the worker. A tie on overlap prefers
  // the tighter (more specific) speaker segment, fixing the multi-speaker
  // mis-attribution of the old one-shot merger.
  out += ",\"comprehensive\":[";
  if (asr_) {
    std::lock_guard<std::mutex> lk(comp_mutex_);
    comp_.Clear();
    comp_next_text_id_ = 0;
    for (const auto& s : last_segments_) {
      const std::string label =
          s.speaker_id.empty() ? ("speaker_" + std::to_string(s.local_speaker))
                               : s.speaker_id;
      comp_.UpsertSpeaker(s.start_sec, s.end_sec, label, s.confidence);
    }
    for (const auto& t : last_transcript_.tokens)
      comp_.UpsertText(comp_next_text_id_++, t.start_sec, t.end_sec, t.text);

    auto view = comp_.Snapshot();
    for (size_t i = 0; i < view.size(); ++i) {
      const auto& e = view[i];
      std::snprintf(buf, sizeof(buf),
                    "{\"start\":%.3f,\"end\":%.3f,\"speaker\":\"",
                    e.start, e.end);
      out += std::string(buf) + JsonEscape(e.speaker) + "\",\"text\":\"" +
             JsonEscape(e.text) + "\"}";
      if (i + 1 < view.size()) out += ",";
    }

    // Spec 004 T021: revision push. Compare the new comprehensive view against
    // the one last emitted; if an already-reported region changed (e.g. the
    // head's "unknown" is now attributed after diarization warmup covered it),
    // emit a revision so the consumer overwrites those entries. We find the
    // first index where the view diverges from prev_view_ and send everything
    // from there as the revised region. No revision on the first emission.
    if (!prev_view_.empty()) {
      size_t idx = 0;
      const size_t common = std::min(prev_view_.size(), view.size());
      while (idx < common && prev_view_[idx].start == view[idx].start &&
             prev_view_[idx].end == view[idx].end &&
             prev_view_[idx].speaker == view[idx].speaker &&
             prev_view_[idx].text == view[idx].text) {
        ++idx;
      }
      if (idx < view.size()) {
        const double dirty_start = view[idx].start;
        const double dirty_end = view.back().end;
        std::string rev = "{\"type\":\"revision\",";
        std::snprintf(buf, sizeof(buf),
                      "\"dirty_start\":%.3f,\"dirty_end\":%.3f,\"entries\":[",
                      dirty_start, dirty_end);
        rev += buf;
        for (size_t i = idx; i < view.size(); ++i) {
          const auto& e = view[i];
          std::snprintf(buf, sizeof(buf),
                        "{\"start\":%.3f,\"end\":%.3f,\"speaker\":\"",
                        e.start, e.end);
          rev += std::string(buf) + JsonEscape(e.speaker) + "\",\"text\":\"" +
                 JsonEscape(e.text) + "\"}";
          if (i + 1 < view.size()) rev += ",";
        }
        rev += "]}";
        EmitLocked(rev);
      }
    }
    prev_view_ = std::move(view);
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
  {
    std::lock_guard<std::mutex> lk(comp_mutex_);
    comp_.Clear();
    prev_view_.clear();
    comp_next_text_id_ = 0;
  }
  if (diarizer_) diarizer_->Reset();
  if (asr_) asr_->Reset();
  StartWorkers();        // start new workers so streaming can resume
}

}  // namespace pipeline
}  // namespace orator
