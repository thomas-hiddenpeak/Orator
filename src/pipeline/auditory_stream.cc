#include "pipeline/auditory_stream.h"

#include <cstdio>
#include <cstdlib>
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
    DiarizationWorker::Params dp;
    dp.threshold = config_.diar_threshold;
    dp.merge_gap_sec = config_.diar_merge_gap_sec;
    dp.sample_rate = config_.sample_rate;
    diar_worker_ =
        std::make_unique<DiarizationWorker>(diarizer_.get(), &timeline_, dp);
    // Spec 004 Step 2: diarization delivers its whole current speaker view
    // (who/when) to the comprehensive timeline live. The view is the raw diar
    // track too, so we store it under comp_mutex_ for Serialize and re-project
    // all text via ReplaceSpeakers, pushing any attribution-change revisions.
    diar_worker_->set_speaker_sink(
        [this](const std::vector<core::DiarSegment>& segs) {
          std::vector<ComprehensiveTimeline::Revision> revs;
          {
            std::lock_guard<std::mutex> lk(comp_mutex_);
            last_segments_ = segs;
            std::vector<ComprehensiveTimeline::SpeakerInput> spk;
            spk.reserve(segs.size());
            for (const auto& s : segs) {
              const std::string label =
                  s.speaker_id.empty()
                      ? ("speaker_" + std::to_string(s.local_speaker))
                      : s.speaker_id;
              spk.push_back({s.start_sec, s.end_sec, label, s.confidence});
            }
            revs = comp_.ReplaceSpeakers(spk);
          }
          for (const auto& r : revs) EmitLocked(SerializeRevision(r));
        });
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
    // Spec 004 Step 2: deliver each committed text segment to the comprehensive
    // timeline in real time. The worker reports its segment (what/when on the
    // common base); the controller upserts it (id-keyed) and pushes any
    // revisions. Text gets a provisional attribution against the current
    // speaker set and is revised when diarization refreshes at the next emit.
    asr_worker_->set_text_sink(
        [this](double start, double end, const std::string& text) {
          std::vector<ComprehensiveTimeline::Revision> revs;
          {
            std::lock_guard<std::mutex> lk(comp_mutex_);
            revs = comp_.UpsertText(comp_next_text_id_++, start, end, text);
          }
          for (const auto& r : revs) EmitLocked(SerializeRevision(r));
        });
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

  // Spec 004 T031/Phase 5: independent speech-endpoint detector as a THIRD
  // buffer consumer. It runs the GPU Silero detector (GpuVad) on the continuous
  // audio purely to publish endpoint markers on the common time base; it is
  // independent of the ASR and diarization pipelines (it reads only the shared
  // audio, never their output). Its per-window compute is batched on the GPU, so
  // draining a large buffered span never stalls on a single CPU core.
  if (config_.endpoint_stream) {
    GpuVad::Params vp;
    vp.sample_rate = config_.sample_rate;
    vp.silero_model_path = config_.asr_vad_model;
    vp.silero_threshold = config_.asr_vad_threshold;
    vp.silero_min_speech_ms = config_.asr_vad_min_speech_ms;
    vp.silero_min_silence_ms = config_.asr_vad_min_silence_ms;
    vp.silero_speech_pad_ms = config_.asr_vad_speech_pad_ms;
    endpoint_vad_ = std::make_unique<GpuVad>(vp);
    endpoint_cursor_ = buffer_.AddConsumer();
    endpoint_thread_ = std::thread([this] {
      const core::TimeBase tb = buffer_.time_base();
      std::vector<float> chunk;
      std::vector<long> eps;
      auto drain = [this, &tb, &eps](bool finalize) {
        eps.clear();
        endpoint_vad_->DrainEndpoints(finalize, &eps);
        for (long ep : eps) {
          // `ep` is an absolute sample on the common clock; convert via the base.
          const double t = tb.SecondsAt(ep);
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

  // Spec 005 T040: end-point time-base reconciliation (diagnostic, env-gated).
  // After all buffered audio is processed, each pipeline's processed sample
  // extent should equal the common clock total. A non-zero gap means a pipeline
  // drifted from the shared time base. Logged only, never alters behavior.
  if (const char* c = std::getenv("ORATOR_TIMEBASE_CHECK"); c && c[0] == '1') {
    const long total = buffer_.total_samples();
    auto report = [total](const char* who, long processed) {
      const long gap = core::TimeBase::ReconcileExtent(processed, total);
      if (gap != 0)
        std::fprintf(stderr,
                     "[timebase] %s extent %ld vs common total %ld -> gap %ld\n",
                     who, processed, total, gap);
    };
    if (diar_worker_) report("diarization", diar_worker_->processed_samples());
    if (asr_worker_) report("asr", asr_worker_->processed_samples());
  }

  EmitLocked(Serialize());
}

std::string AuditoryStream::SerializeRevision(
    const ComprehensiveTimeline::Revision& r) {
  char buf[128];
  std::string out = "{\"type\":\"revision\",";
  std::snprintf(buf, sizeof(buf),
                "\"dirty_start\":%.3f,\"dirty_end\":%.3f,\"entries\":[",
                r.dirty_start, r.dirty_end);
  out += buf;
  for (size_t i = 0; i < r.entries.size(); ++i) {
    const auto& e = r.entries[i];
    std::snprintf(buf, sizeof(buf), "{\"start\":%.3f,\"end\":%.3f,\"speaker\":\"",
                  e.start, e.end);
    out += std::string(buf) + JsonEscape(e.speaker) + "\",\"text\":\"" +
           JsonEscape(e.text) + "\"}";
    if (i + 1 < r.entries.size()) out += ",";
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
  core::Transcript transcript = timeline_.SnapshotTranscript();

  // Spec 004 Step 2: the diarization worker is the sole producer of the speaker
  // view (it delivers live via ReplaceSpeakers + keeps last_segments_ fresh);
  // the ASR worker delivers text live via UpsertText. So Serialize is a pure
  // reader: snapshot last_segments_ (diar track) and the comprehensive view
  // under comp_mutex_. No derivation, no upserts here.
  std::vector<core::DiarSegment> diar_view;
  std::vector<ComprehensiveTimeline::Entry> comp_view;
  std::vector<double> endpoint_view;
  {
    std::lock_guard<std::mutex> lk(comp_mutex_);
    diar_view = last_segments_;
    comp_view = comp_.Snapshot();
    endpoint_view = comp_.SnapshotEndpoints();
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

  // Track: speech endpoints (Spec 004 FR7). A PURE MARKER track on the common
  // time base: it carries the endpoint times the detector published, and does
  // not alter the diarization or ASR tracks. Present only when the endpoint
  // pipeline is enabled.
  if (config_.endpoint_stream) {
    out += ",{\"kind\":\"endpoint\",\"source\":\"silero_gpu\",\"entries\":[";
    for (size_t i = 0; i < endpoint_view.size(); ++i) {
      std::snprintf(buf, sizeof(buf), "{\"time\":%.3f}", endpoint_view[i]);
      out += buf;
      if (i + 1 < endpoint_view.size()) out += ",";
    }
    out += "]}";
  }
  out += "]";  // close "tracks"

  // Comprehensive view: speaker turns with their spoken text, ordered by time.
  // Present only when ASR is enabled (it requires both modalities). Built from
  // the native stateful ComprehensiveTimeline (Spec 004): diarization provides
  // who/when, ASR provides what/when, attribution is by time alignment on the
  // common base. All three pipelines (diarization, ASR, endpoint) deliver LIVE
  // to comp_ via their sinks; Serialize is a pure reader and just emits the
  // snapshot taken above (comp_view). No upserts here.
  out += ",\"comprehensive\":[";
  if (asr_) {
    for (size_t i = 0; i < comp_view.size(); ++i) {
      const auto& e = comp_view[i];
      std::snprintf(buf, sizeof(buf),
                    "{\"start\":%.3f,\"end\":%.3f,\"speaker\":\"",
                    e.start, e.end);
      out += std::string(buf) + JsonEscape(e.speaker) + "\",\"text\":\"" +
             JsonEscape(e.text) + "\"}";
      if (i + 1 < comp_view.size()) out += ",";
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
  {
    std::lock_guard<std::mutex> lk(comp_mutex_);
    comp_.Clear();
    comp_next_text_id_ = 0;
  }
  if (diarizer_) diarizer_->Reset();
  if (asr_) asr_->Reset();
  StartWorkers();        // start new workers so streaming can resume
}

}  // namespace pipeline
}  // namespace orator
