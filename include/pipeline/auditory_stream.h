#pragma once

// AuditoryStream: the controller that runs speaker diarization and ASR as two
// independent pipelines over one streamed audio source, on independent threads.
//
// Per the architecture (Spec 001): the input audio is the only data the two
// pipelines share. PushAudio() appends samples to a shared buffer; two worker
// threads (diarization, ASR) read that buffer at their own rate, do not read
// each other's results, and append their output to one mutex-guarded timeline
// indexed by the shared absolute time base. The controller owns the buffer, the
// workers, and the thread lifecycle: start on session open; on flush, wait for
// both workers to reach the current input position; on end, close the buffer,
// process all remaining audio, and join the threads. The pipelines may complete
// in any order; their only shared reference is the time base.
//
// This class is independent of the transport: results are delivered through the
// `Emit` callback as JSON strings (incremental {"type":"asr",...} messages from
// the ASR worker, and the {"type":"timeline",...} document on flush/end). It
// contains no socket code; the WebSocket handler and the streaming test drive
// it through the same interface.

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <cuda_runtime.h>

#include "core/stages.h"
#include "core/types.h"
#include "gpu/scheduler.h"
#include "pipeline/align_worker.h"
#include "pipeline/asr_worker.h"
#include "pipeline/business_speaker_pipeline.h"
#include "pipeline/comprehensive_timeline.h"
#include "pipeline/diarization_worker.h"
#include "pipeline/gpu_vad.h"
#include "pipeline/pipeline_audio_cache.h"
#include "pipeline/retained_audio_buffer.h"
// Forward declarations for protocol types (layering: pipeline depends on
// protocol interfaces only, not includes).
namespace orator {
namespace protocol {
class ProtocolTimeline;
class SessionStore;
class PipelineHandle;
}  // namespace protocol
}  // namespace orator

namespace orator {
namespace model {
class TitaNetEmbedder;
class SpeakerDatabase;
}  // namespace model
}  // namespace orator

namespace orator {
namespace pipeline {

class SpeakerIdentityStage;
class SpeakerEvidenceStage;

class AuditoryStream {
 public:
  struct Config {
    // ── Model paths ──────────────────────────────────────────────────
    // Spec 013 closing baseline. The deprecated v2 checkpoint was removed.
    std::string diarizer_weights = "models/sortformer_4spk_v2.1.safetensors";
    std::string asr_model_dir = "models/asr/Qwen/Qwen3-ASR-1.7B";
    std::string vad_model = "models/vad/silero_vad.safetensors";

    // ── Hardware ─────────────────────────────────────────────────────
    int sample_rate = 16000;
    int gpu_scheduling_mode = 0;  // 0=auto, 1=serial, 2=full_concurrent

    // ── Server ───────────────────────────────────────────────────────
    int port = 8765;
    int ui_port = 0;  // 0 = auto (port+1)
    std::string ui_root = "web";
    std::string config_source_path = "orator.toml";

    // ── ASR pipeline ─────────────────────────────────────────────────
    bool asr_vad_gate = true;
    int asr_vad_lead_ms = 200;
    int asr_vad_gate_chunk_ms = 100;
    double asr_vad_trail_sec = 1.0;
    double asr_vad_min_overlap_sec = 0.12;
    int asr_max_audio_tokens = 1500;
    int asr_max_new_tokens = 32;
    double asr_segment_sec = 24.0;
    std::string asr_language = "Chinese";
    std::string asr_system_prompt =
        "你是一个专业的中文普通话语音识别系统，请准确识别并转录所有语音内容。";
    int asr_ban_steps = 3;
    int asr_decode_batch = 4;
    bool asr_profile = false;
    bool asr_windowed_encoder = false;
    bool asr_cuda_graph_enabled = true;

    // ── Forced alignment pipeline ────────────────────────────────────
    std::string align_model_dir = "";  // empty = forced aligner disabled
    bool align_enable = false;         // master switch (also needs model dir)
    std::string align_language = "Chinese";
    double align_max_segment_sec = 300.0;  // skip absurdly long spans
    double align_retain_sec = 180.0;       // retained audio window for readback
    bool align_profile = false;

    // ── Comprehensive timeline view ──────────────────────────────────
    double timeline_align_snap_pause_sec = 0.25;
    double timeline_align_boundary_split_tolerance_sec = 0.08;
    double timeline_speaker_support_min_coverage_ratio = 0.50;
    double timeline_speaker_support_max_gap_sec = 1.00;
    int timeline_speaker_support_max_islands = 1;
    bool timeline_gap_fill_enabled = true;
    std::string timeline_speaker_overlap_tie_policy = "shorter_span";

    // ── VAD pipeline ─────────────────────────────────────────────────
    bool vad_stream = true;
    float vad_threshold = 0.5f;
    int vad_min_speech_ms = 250;
    int vad_min_silence_ms = 300;
    int vad_speech_pad_ms = 60;

    // ── Diarizer pipeline ────────────────────────────────────────────
    int max_speakers = 4;
    float diar_threshold = 0.4f;
    double diar_merge_gap_sec = 0.8;
    double diar_deliver_interval_sec = 1.0;
    // Sortformer streaming tuning (affects speaker segmentation quality)
    int diar_spkcache_len = 188;            // speaker cache length (frames)
    int diar_chunk_len = 340;               // processing chunk size (frames)
    int diar_spkcache_update_period = 188;  // cache update interval (frames)
    int diar_chunk_left_context = 1;        // left context chunks
    int diar_chunk_right_context = 1;       // right context chunks
    int diar_spkcache_sil_frames = 3;  // mean-silence cache slots per speaker
    int diar_fifo_len = 188;  // async FIFO length; 0 selects sync mode
    // Onset/offset post-processing (NeMo-style double threshold)
    double diar_onset = 0.45;  // probability to START a segment
    double diar_offset =
        0.35;  // probability to END a segment (lower = stickier;
               // 0.35 keeps hysteresis but tracks the NeMo
               // reference's segment granularity, vs 0.25 which
               // over-merged across brief probability dips)
    double diar_pad_onset = 0.0;   // extra time added before each segment start
    double diar_pad_offset = 0.0;  // extra time added after each segment end
    double diar_min_dur_on = 0.5;  // minimum segment duration (seconds)
    double diar_min_dur_off = 1.0;  // minimum gap to merge segments (seconds)
    // Periodically reset the diarizer streaming state (0 = never). This was a
    // workaround for the SYNC streaming path (fifo_len=0), whose fixed spkcache
    // saturates over a long continuous session (late-window ~66%), recovered by
    // a fresh session (~83%) plus per-session voiceprint re-stitching. The
    // ASYNC path (fifo_len>0, the default) refreshes the spkcache continuously
    // and does NOT saturate, so the reset is unnecessary and OFF by default:
    // the diarizer keeps stable speaker slots for the whole session and the
    // voiceprint stage resolves each slot to one global identity (measured: 4
    // speakers -> 4 stable global ids, vs the reset path's per-session slot
    // churn -> id over-creation).
    double diar_reset_period_sec = 0.0;

    // ── Speaker identity (Spec 010, post-diarization stage) ──────────
    bool speaker_enable = false;  // master switch (also needs model dir)
    std::string speaker_model_dir =
        "";  // dir holding titanet_large.safetensors
    std::string speaker_registry_path = "";  // persistence file ("" = none)
    float speaker_match_threshold = 0.55f;   // cosine tau for re-identification
    double speaker_min_embed_sec = 3.0;      // shortest clean span to embed
    float speaker_min_confidence = 0.5f;     // diar mean-activity gate
    double speaker_retain_sec = 180.0;       // audio retention window
    double speaker_overlap_eps_sec = 0.1;  // overlap tolerance for clean spans
    int speaker_max_ref_segs = 6;          // best clean refs kept per speaker
    double speaker_edge_margin_sec = 0.3;  // edge trim before embedding
    double speaker_max_embed_window_sec =
        10.0;                               // cap embedded voiceprint audio
    int speaker_enroll_min_refs = 1;        // refs required for new global id
    int speaker_speakers_per_session = 4;   // Sortformer local slots/session
    float speaker_merge_threshold = 0.70f;  // global-id duplicate threshold
    float speaker_cosession_merge_threshold = 0.85f;  // same-session merge gate
    int speaker_cross_session_match_min_refs = 1;     // refs before reset re-id
    bool speaker_defer_unmatched_cross_session = false;
    float speaker_local_drift_threshold =
        0.0f;  // <=0 disables local epoch split
    double speaker_local_drift_min_span_sec = 5.0;
    double speaker_local_drift_min_epoch_sec = 60.0;
    bool speaker_local_drift_allow_same_session_match = true;
    float speaker_local_drift_competing_threshold = 0.0f;
    float speaker_local_drift_competing_margin = 0.05f;
    double speaker_local_drift_competing_min_span_sec = 5.0;
    float speaker_local_drift_competing_candidate_threshold = 0.0f;
    float speaker_local_drift_competing_candidate_margin = 0.05f;
    int speaker_local_drift_competing_candidate_min_confirmations = 0;
    double speaker_local_drift_competing_backfill_sec = 0.0;
    double speaker_local_drift_competing_backfill_gap_sec = 3.0;

    // Final/revisable multi-resolution speaker evidence. These fields govern
    // model queries and evidence construction only; business selection gates
    // remain in the timeline configuration below.
    bool speaker_fusion_enable = false;
    double speaker_fusion_min_embed_sec = 0.4;
    double speaker_fusion_edge_margin_sec = 0.0;
    double speaker_fusion_max_embed_window_sec = 3.0;
    double speaker_fusion_phrase_min_sec = 0.5;
    double speaker_fusion_phrase_max_sec = 3.0;
    std::string speaker_fusion_punctuation = "，。？！；：、,.?!;:";
    float speaker_fusion_frame_activity_threshold = 0.5f;
    int speaker_fusion_minimum_gallery_size = 2;
    double speaker_fusion_short_max_sec = 1.5;
    float speaker_fusion_short_min_score = 0.0f;
    float speaker_fusion_short_min_margin = 0.04f;
    float speaker_fusion_regular_min_score = 0.55f;
    float speaker_fusion_regular_min_margin = 0.04f;
    int speaker_fusion_four_view_min_aligned_units = 2;
    double speaker_fusion_future_epoch_lookahead_sec = 0.0;
    bool speaker_fusion_posterior_future_epoch_enable = false;
    bool speaker_fusion_source_leading_primary_prefix_enable = false;
    bool speaker_fusion_right_bounded_short_primary_unit_enable = false;
    double speaker_fusion_precompute_interval_sec = 0.0;
    int speaker_fusion_precompute_max_spans_per_cycle = 1;

    // ── Storage ──────────────────────────────────────────────────────
    std::string storage_disk_path = "/tmp/orator/storage/";
    std::string session_dir;

    // ── Buffer ───────────────────────────────────────────────────────
    size_t buffer_max_samples = 0;              // 0 = no limit
    size_t buffer_shrink_threshold = 10000000;  // 10M samples ~ 40MB

    // ── Telemetry ────────────────────────────────────────────────────
    double gpu_telemetry_interval_sec = 0.0;

    // ── Cursor Telemetry ─────────────────────────────────────────────
    double cursor_telemetry_interval_sec = 0.0;
    size_t cursor_lag_warn_samples = 0;
    size_t cursor_lag_critical_samples = 0;

    // ── Debug ────────────────────────────────────────────────────────
    int log_level = 2;  // 0=DEBUG, 1=INFO, 2=WARN, 3=ERROR
    bool timebase_check = false;
    bool stream_progress = false;
    std::string ws_text_log_path;
  };

  // Delivers a result event as a JSON string. Invoked from the ASR worker
  // thread (incremental events) and from the controller (timeline); the
  // controller serializes these calls so the transport sees one at a time.
  using Emit = std::function<void(const std::string&)>;

  struct TrackExtent {
    std::string pipeline;
    long processed_samples = 0;
    long common_total_samples = 0;
    long gap_samples = 0;
  };

  AuditoryStream(const Config& config, Emit emit);
  ~AuditoryStream();

  AuditoryStream(const AuditoryStream&) = delete;
  AuditoryStream& operator=(const AuditoryStream&) = delete;

  // Load weights, initialize both pipelines, and spawn the worker threads.
  // Throws on failure.
  void Start();

  // Feed one block of mono float PCM into the shared buffer. Returns
  // immediately; the worker threads consume it independently.
  void PushAudio(const float* samples, int n);

  // Send the comprehensive timeline. When finalize is true: signal
  // end-of-stream, process all remaining audio, join every worker, reconcile
  // every active pipeline extent, then serialize. When false (flush): wait for
  // the direct audio consumers to process all audio appended so far, then
  // serialize; streaming continues and worker state is retained.
  void EmitTimeline(bool finalize);

  // Stop the workers (if running), clear all state for a fresh session, then
  // start new workers so streaming can resume.
  void Reset();

  bool asr_enabled() const { return asr_ != nullptr; }
  const std::vector<core::DiarSegment>& diar_segments() const {
    return last_segments_;
  }
  const core::Transcript& transcript() const { return last_transcript_; }
  double audio_sec() const;
  double diar_compute_sec() const;
  double asr_compute_sec() const;

  double session_start_wall_sec() const {
    return session_start_wall_sec_.load();
  }
  bool wall_clock_ok() const { return wall_clock_ok_.load(); }
  bool timebase_reconciled() const { return timebase_reconciled_.load(); }
  bool timebase_ok() const { return timebase_ok_.load(); }
  std::vector<TrackExtent> track_extents() const;
  const core::TimeBase& time_base() const { return time_base_; }

  // Spec 004 Phase 12: access to protocol timeline for describe command.
  const protocol::ProtocolTimeline* protocol_timeline() const {
    return protocol_timeline_.get();
  }

  // Spec 004 Phase 13: access to session store for session list/load.
  protocol::SessionStore* session_store() const { return session_store_.get(); }

  // Spec 006: speaker registry for the Web UI naming/management panel.
  // SerializeSpeakers lists the global identities resolved in this session
  // (id + display name) as {"type":"speakers","speakers":[{"id","name"}...]}.
  // RenameSpeaker sets a display name and persists the registry; both are
  // safe to call from the WS thread (comp_mutex_ + the database's name mutex).
  std::string SerializeSpeakers() const;
  bool RenameSpeaker(const std::string& speaker_id, const std::string& name);

 private:
  void StartWorkers();  // start diar + asr threads
  void StopWorkers();   // close buffer, join threads
  // Block until direct audio consumers have processed `target_samples`.
  void WaitForBarrier(long target_samples);
  void ReconcileFinalExtents();
  std::string Serialize();  // build the comprehensive timeline JSON
  void EmitLocked(const std::string& json);  // serialize transport sends
  // Spec 002 FR7: build the periodic GPU-scheduling telemetry message from the
  // priority registry + each pipeline's compute/occupancy summary.
  std::string SerializeGpuTelemetry() const;
  // Build the periodic cursor progress telemetry message.
  std::string SerializeCursorTelemetry() const;

  // The session's common time base (origin = stream start, sample 0). Every
  // pipeline cache and worker derives its time codes from this, so all
  // pipelines align by construction rather than each counting from 0.
  const core::TimeBase& common_time_base() const { return time_base_; }

  // Build a fresh private audio cache for one pipeline. All caches share the
  // common clock and the same backlog cap; the cap is an interface placeholder
  // (enforced once SSD spill-over lands -- see PipelineAudioCache::Config).
  std::unique_ptr<PipelineAudioCache> MakeAudioCache() const {
    PipelineAudioCache::Config cc;
    cc.max_memory_samples = config_.buffer_max_samples;
    return std::make_unique<PipelineAudioCache>(time_base_, cc);
  }

  Config config_;
  const core::TimeBase time_base_;
  Emit emit_;

  std::unique_ptr<core::IDiarizer> diarizer_;
  std::unique_ptr<core::IAsr> asr_;  // null when ASR disabled
  std::unique_ptr<core::IForcedAligner>
      aligner_;  // null when alignment disabled

  // Per-pipeline private audio caches (Constitution Art. III). PushAudio fans
  // each frame out to every active cache; each worker drains only its own.
  // A cache exists iff its pipeline is active. `total_samples_` is the common
  // clock head (samples appended this session), shared by all caches by
  // construction so the absolute time base stays valid across pipelines.
  std::unique_ptr<PipelineAudioCache> diar_audio_;
  std::unique_ptr<PipelineAudioCache> asr_audio_;
  std::unique_ptr<PipelineAudioCache> vad_audio_;
  // Retained-window audio for forced alignment: read back by absolute sample
  // span when a transcript segment arrives (the transcript lags its audio), so
  // this is a sliding window rather than a read-then-free cache.
  std::unique_ptr<RetainedAudioBuffer> align_audio_;
  std::atomic<long> total_samples_{0};
  std::unique_ptr<DiarizationWorker> diar_worker_;
  std::unique_ptr<AsrWorker> asr_worker_;
  std::unique_ptr<AlignWorker> align_worker_;
  std::thread diar_thread_;
  std::thread asr_thread_;
  bool running_ = false;

  // Spec 010: speaker identity as a post-diarization stage inside the diar
  // pipeline (null when disabled). The stage owns the voiceprint embedder +
  // registry and resolves a global identity for each diarizer-local speaker.
  std::unique_ptr<model::TitaNetEmbedder> speaker_embedder_;
  std::unique_ptr<model::SpeakerDatabase> speaker_db_;
  std::unique_ptr<SpeakerIdentityStage> speaker_id_stage_;
  std::unique_ptr<SpeakerEvidenceStage> speaker_evidence_stage_;

  // Spec 004: independent VAD detector (third pipeline consumer).
  std::unique_ptr<core::IVad> vad_detector_;
  std::thread vad_thread_;
  std::atomic<long> vad_processed_samples_{0};

  // Spec 002: per-pipeline GPU stream priority registry. Each pipeline declares
  // a priority index + class at registration and (when stream-routed) receives
  // a prioritized CUDA stream owned by the scheduler. It is also the single
  // source of truth for the GPU-scheduling telemetry snapshot.
  gpu::GpuScheduler scheduler_;
  // Per-pipeline GPU streams sourced from the scheduler (owned by it).
  cudaStream_t diar_stream_ = nullptr;
  cudaStream_t asr_stream_ = nullptr;
  cudaStream_t speaker_stream_ = nullptr;
  // VAD pipeline GPU stream, sourced from the scheduler (owned by it, not
  // here).
  cudaStream_t vad_stream_ = nullptr;

  // Wakes WaitForBarrier whenever a worker advances or finishes.
  std::mutex progress_mutex_;
  std::condition_variable progress_cv_;
  // Serializes Emit calls across the worker threads and the controller.
  std::mutex emit_mutex_;

  // Spec 002 FR7: periodic GPU-scheduling telemetry timer thread.
  std::thread telemetry_thread_;
  std::mutex telemetry_mutex_;
  std::condition_variable telemetry_cv_;
  bool telemetry_stop_ = false;

  // Cursor progress telemetry thread.
  std::thread cursor_telemetry_thread_;
  std::mutex cursor_telemetry_mutex_;
  std::condition_variable cursor_telemetry_cv_;
  bool cursor_telemetry_stop_ = false;

  // Last serialized snapshots, exposed for inspection by the test tool.
  std::vector<core::DiarSegment> last_segments_;
  core::Transcript last_transcript_;

  // Authoritative typed evidence store. It owns its track synchronization;
  // comp_mutex_ protects only controller-side compatibility snapshots such as
  // last_segments_ and last_transcript_.
  ComprehensiveTimeline comp_;
  // Declared after comp_ so automatic destruction unsubscribes before the
  // evidence store is destroyed.
  std::unique_ptr<BusinessSpeakerPipeline> business_speaker_pipeline_;
  mutable std::mutex comp_mutex_;

  // Spec 004 Phase 12: protocol timeline for registration and external mirrors.
  // Runtime evidence exchange remains in ComprehensiveTimeline.
  std::unique_ptr<protocol::ProtocolTimeline> protocol_timeline_;
  std::unique_ptr<protocol::PipelineHandle> ws_input_handle_;
  std::unique_ptr<protocol::PipelineHandle> vad_handle_;
  std::unique_ptr<protocol::PipelineHandle> asr_handle_;
  std::unique_ptr<protocol::PipelineHandle> diar_handle_;
  // Forced-alignment pipeline handle. The worker consumes finalized ASR records
  // through the typed ComprehensiveTimeline subscription below.
  std::unique_ptr<protocol::PipelineHandle> align_handle_;
  std::unique_ptr<protocol::PipelineHandle> business_speaker_handle_;
  long comp_asr_subscription_id_ = 0;

  // Spec 004 Phase 13: session persistence store. Saves timeline JSON on
  // Reset(). Null when persistence is disabled (empty storage_disk_path).
  std::unique_ptr<protocol::SessionStore> session_store_;

  // Wall-clock anchor is set by the first audio sample, so connection setup,
  // optional commands, and idle time before ingest are excluded.
  std::atomic<double> session_start_wall_sec_{0.0};
  std::atomic<bool> wall_clock_ok_{true};
  std::atomic<bool> timebase_reconciled_{false};
  std::atomic<bool> timebase_ok_{true};
};

}  // namespace pipeline
}  // namespace orator
