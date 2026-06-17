#include "pipeline/auditory_stream.h"

#include <chrono>
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
  // asr_stream_ is owned by scheduler_ (destroyed with it); nothing to free here.
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
      std::fprintf(stderr,
                   "[gpu-sched] priority range [greatest=%d, least=%d]; "
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
        std::make_unique<DiarizationWorker>(diarizer_.get(), &timeline_, dp,
                                            diar_stream_);
    // Spec 004 Step 2: diarization delivers its whole current speaker view
    // (who/when) to the comprehensive timeline live. The view is the raw diar
    // track too, so we store it under comp_mutex_ for Serialize and re-project
    // all text via ReplaceSpeakers, pushing any attribution-change revisions.
    diar_worker_->set_speaker_sink(
        [this](const std::vector<core::DiarSegment>& segs) {
          std::vector<ComprehensiveTimeline::Revision> revs;
          std::string diar_msg = "{\"type\":\"diar\",\"source\":\"sortformer\",\"segments\":[";
          {
            std::lock_guard<std::mutex> lk(comp_mutex_);
            last_segments_ = segs;
            std::vector<ComprehensiveTimeline::SpeakerInput> spk;
            spk.reserve(segs.size());
            for (size_t i = 0; i < segs.size(); ++i) {
              const auto& s = segs[i];
              const std::string label =
                  s.speaker_id.empty()
                      ? ("speaker_" + std::to_string(s.local_speaker))
                      : s.speaker_id;
              spk.push_back({s.start_sec, s.end_sec, label, s.confidence});
              char b[160];
              std::snprintf(b, sizeof(b),
                            "{\"start\":%.3f,\"end\":%.3f,\"speaker\":\"%s\","
                            "\"confidence\":%.3f}",
                            s.start_sec, s.end_sec, label.c_str(), s.confidence);
              diar_msg += b;
              if (i + 1 < segs.size()) diar_msg += ",";
            }
            revs = comp_.ReplaceSpeakers(spk);
          }
          diar_msg += "]}";
          // The diarization pipeline's own live output (meta + data + time codes).
          EmitLocked(diar_msg);
          for (const auto& r : revs) EmitLocked(SerializeRevision(r, "sortformer"));
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
    p.sample_rate = config_.sample_rate;
    p.segment_sec = config_.asr_segment_sec;
    asr_worker_ = std::make_unique<AsrWorker>(
        asr_.get(), &timeline_, p,
        [this](const std::string& json) { EmitLocked(json); },
      asr_stream_);
    // Spec 004 Step 2: deliver each committed text segment to the comprehensive
    // timeline in real time. The worker reports its segment (what/when on the
    // common base); the controller upserts it (id-keyed) and pushes any
    // revisions. Text gets a provisional attribution against the current
    // speaker set and is revised when diarization refreshes at the next emit.
    // The worker owns the text id: it delivers the SAME id repeatedly while a
    // segment's text is revised in place (ASR self-revision, Spec 004 G3), so
    // UpsertText revises the existing entry instead of inserting a new one.
    asr_worker_->set_text_sink(
        [this](long id, double start, double end, const std::string& text) {
          std::vector<ComprehensiveTimeline::Revision> revs;
          {
            std::lock_guard<std::mutex> lk(comp_mutex_);
            revs = comp_.UpsertText(id, start, end, text);
          }
          for (const auto& r : revs) EmitLocked(SerializeRevision(r, "qwen3_asr"));
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
          // sp = absolute [start,end) samples on the common clock.
          const double s = tb.SecondsAt(sp.first);
          const double e = tb.SecondsAt(sp.second);
          {
            std::lock_guard<std::mutex> lk(comp_mutex_);
            comp_.AddVad(s, e);
          }
          char buf[128];
          std::snprintf(buf, sizeof(buf),
                        "{\"type\":\"vad\",\"source\":\"silero_gpu\","
                        "\"start\":%.3f,\"end\":%.3f}",
                        s, e);
          EmitLocked(buf);
        }
      };
      while (buffer_.WaitAndRead(vad_cursor_, &chunk)) {
        vad_detector_->Push(chunk.data(), static_cast<int>(chunk.size()));
        drain(/*finalize=*/false);
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
    std::snprintf(buf, sizeof(buf),
                  "{\"start\":%.3f,\"end\":%.3f,\"text_id\":%ld,\"speaker\":\"",
                  e.start, e.end, e.text_id);
    out += std::string(buf) + JsonEscape(e.speaker) + "\",\"text\":\"" +
           JsonEscape(e.text) + "\"}";
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
  core::Transcript transcript = timeline_.SnapshotTranscript();

  // Spec 004 Step 2: the diarization worker is the sole producer of the speaker
  // view (it delivers live via ReplaceSpeakers + keeps last_segments_ fresh);
  // the ASR worker delivers text live via UpsertText. So Serialize is a pure
  // reader: snapshot last_segments_ (diar track) and the comprehensive view
  // under comp_mutex_. No derivation, no upserts here.
  std::vector<core::DiarSegment> diar_view;
  std::vector<ComprehensiveTimeline::Entry> comp_view;
  std::vector<ComprehensiveTimeline::VadSeg> vad_view;
  {
    std::lock_guard<std::mutex> lk(comp_mutex_);
    diar_view = last_segments_;
    comp_view = comp_.Snapshot();
    vad_view = comp_.SnapshotVad();
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
  }
  if (diarizer_) diarizer_->Reset();
  if (asr_) asr_->Reset();
  StartWorkers();        // start new workers so streaming can resume
}

}  // namespace pipeline
}  // namespace orator
