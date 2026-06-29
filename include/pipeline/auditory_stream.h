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
#include "pipeline/gpu_vad.h"
#include "pipeline/comprehensive_timeline.h"
#include "pipeline/diarization_worker.h"
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

class AuditoryStream {
 public:
  struct Config {
    // ── Model paths ──────────────────────────────────────────────────
    std::string diarizer_weights = "models/sortformer_4spk_v2.safetensors";
    std::string asr_model_dir = "models/asr/Qwen/Qwen3-ASR-1.7B";
    std::string vad_model = "models/vad/silero_vad.safetensors";

    // ── Hardware ─────────────────────────────────────────────────────
    int sample_rate = 16000;
    int gpu_scheduling_mode = 0;  // 0=auto, 1=serial, 2=full_concurrent

    // ── Server ───────────────────────────────────────────────────────
    int port = 8765;
    int ui_port = 0;  // 0 = auto (port+1)
    std::string ui_root = "web";

    // ── ASR pipeline ─────────────────────────────────────────────────
    bool asr_vad_gate = true;
    int asr_vad_lead_ms = 200;
    double asr_vad_trail_sec = 1.0;
    int asr_max_audio_tokens = 1500;
    int asr_max_new_tokens = 32;
    double asr_segment_sec = 24.0;
    std::string asr_language = "Chinese";
    std::string asr_system_prompt = "";
    int asr_ban_steps = 3;
    int asr_decode_batch = 4;
    bool asr_profile = false;

    // ── Forced alignment pipeline ────────────────────────────────────
    std::string align_model_dir = "";  // empty = forced aligner disabled
    bool align_enable = false;         // master switch (also needs model dir)
    std::string align_language = "Chinese";
    double align_max_segment_sec = 300.0;  // skip absurdly long spans
    double align_retain_sec = 180.0;       // retained audio window for readback

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
    int diar_spkcache_update_period = 300;  // cache update interval (frames)
    int diar_chunk_left_context = 1;        // left context chunks
    int diar_chunk_right_context = 40;      // right context chunks
    int diar_spkcache_sil_frames = 5;       // silent frames before cache reset
    int diar_fifo_len = 0;  // FIFO length for async streaming (0=off)
    int diar_spkcache_refresh_rate = 0;  // cache refresh cadence (0=drain all)
    bool diar_use_silence_profile =
        false;  // v2.1: use silence profile in cache
    // Onset/offset post-processing (NeMo-style double threshold)
    double diar_onset = 0.45;   // probability to START a segment
    double diar_offset = 0.35;  // probability to END a segment (lower = stickier;
                                // 0.35 keeps hysteresis but tracks the NeMo
                                // reference's segment granularity, vs 0.25 which
                                // over-merged across brief probability dips)
    double diar_pad_onset = 0.0;   // extra time added before each segment start
    double diar_pad_offset = 0.0;  // extra time added after each segment end
    double diar_min_dur_on = 0.5;  // minimum segment duration (seconds)
    double diar_min_dur_off = 1.0;  // minimum gap to merge segments (seconds)

    // ── Speaker identity (Spec 010, post-diarization stage) ──────────
    bool speaker_enable = false;  // master switch (also needs model dir)
    std::string speaker_model_dir = "";  // dir holding titanet_large.safetensors
    std::string speaker_registry_path = "";  // persistence file ("" = none)
  float speaker_match_threshold = 0.55f;   // cosine tau for re-identification
  double speaker_min_embed_sec = 3.0;      // shortest clean span to embed
    float speaker_min_confidence = 0.5f;     // diar mean-activity gate
    double speaker_retain_sec = 180.0;       // audio retention window

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
  };

  // Delivers a result event as a JSON string. Invoked from the ASR worker
  // thread (incremental events) and from the controller (timeline); the
  // controller serializes these calls so the transport sees one at a time.
  using Emit = std::function<void(const std::string&)>;

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
  // end-of-stream, process all remaining audio, join both workers, then
  // serialize. When false (flush): wait until both workers have processed all
  // audio appended so far, then serialize; streaming continues and worker state
  // is retained.
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

  // Spec 004 Phase 12: access to protocol timeline for describe command.
  const protocol::ProtocolTimeline* protocol_timeline() const {
    return protocol_timeline_.get();
  }

  // Spec 004 Phase 13: access to session store for session list/load.
  protocol::SessionStore* session_store() const { return session_store_.get(); }

 private:
  void StartWorkers();  // start diar + asr threads
  void StopWorkers();   // close buffer, join threads
  // Block until both workers have processed up to `target_samples`.
  void WaitForBarrier(long target_samples);
  std::string Serialize();  // build the comprehensive timeline JSON
  // Serialize one revision (Spec 004) to a {"type":"revision",...} message.
  static std::string SerializeRevision(const ComprehensiveTimeline::Revision& r,
                                       const char* source);
  void EmitLocked(const std::string& json);  // serialize transport sends
  // Spec 002 FR7: build the periodic GPU-scheduling telemetry message from the
  // priority registry + each pipeline's compute/occupancy summary.
  std::string SerializeGpuTelemetry() const;
  // Build the periodic cursor progress telemetry message.
  std::string SerializeCursorTelemetry() const;

  // The session's common time base (origin = stream start, sample 0). Every
  // pipeline cache and worker derives its time codes from this, so all
  // pipelines align by construction rather than each counting from 0.
  core::TimeBase common_time_base() const {
    return core::TimeBase(config_.sample_rate, 0);
  }

  // Build a fresh private audio cache for one pipeline. All caches share the
  // common clock and the same backlog cap; the cap is an interface placeholder
  // (enforced once SSD spill-over lands -- see PipelineAudioCache::Config).
  std::unique_ptr<PipelineAudioCache> MakeAudioCache() const {
    PipelineAudioCache::Config cc;
    cc.max_memory_samples = config_.buffer_max_samples;
    return std::make_unique<PipelineAudioCache>(config_.sample_rate, cc);
  }

  Config config_;
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
  long speaker_vad_sub_id_ = 0;

  // Spec 004: independent VAD detector (third pipeline consumer).
  std::unique_ptr<core::IVad> vad_detector_;
  std::thread vad_thread_;

  // Spec 002: per-pipeline GPU stream priority registry. Each pipeline declares
  // a priority index + class at registration and (when stream-routed) receives
  // a prioritized CUDA stream owned by the scheduler. It is also the single
  // source of truth for the GPU-scheduling telemetry snapshot.
  gpu::GpuScheduler scheduler_;
  // Per-pipeline GPU streams sourced from the scheduler (owned by it).
  cudaStream_t diar_stream_ = nullptr;
  cudaStream_t asr_stream_ = nullptr;
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

  // Spec 004: native stateful comprehensive view. ASR commits upsert text
  // incrementally (with live revision push); diar segments are upserted at
  // serialize (frame->segment is global). Guarded by comp_mutex_ because the
  // ASR worker thread and the controller both touch it.
  ComprehensiveTimeline comp_;
  std::mutex comp_mutex_;

  // Spec 004 Phase 12: protocol timeline for pipeline registration and routing.
  // Bridges to ComprehensiveTimeline for the comprehensive view.
  std::unique_ptr<protocol::ProtocolTimeline> protocol_timeline_;
  std::unique_ptr<protocol::PipelineHandle> ws_input_handle_;
  std::unique_ptr<protocol::PipelineHandle> vad_handle_;
  std::unique_ptr<protocol::PipelineHandle> asr_handle_;
  std::unique_ptr<protocol::PipelineHandle> diar_handle_;
  // Spec 004 Phase 12: internal subscription IDs bridging ProtocolTimeline
  // messages back to ComprehensiveTimeline. Unsubscribed in Reset().
  long vad_sub_id_ = 0;
  long vad_progress_sub_id_ = 0;
  long diar_sub_id_ = 0;
  long asr_sub_id_ = 0;
  // Forced-alignment pipeline handle + asr/transcript subscription. The aligner
  // consumes published transcripts only (never ASR internal state, Art. III).
  std::unique_ptr<protocol::PipelineHandle> align_handle_;
  long align_sub_id_ = 0;
  long align_units_sub_id_ = 0;

  // VAD gate: local cache populated by ProtocolTimeline subscription.
  // Avoids O(N^2) Replay calls on the ASR worker hot path.
  std::unique_ptr<orator::pipeline::AsrWorker::VadCache> vad_cache_;

  // Spec 004 Phase 13: session persistence store. Saves timeline JSON on
  // Reset(). Null when persistence is disabled (empty storage_disk_path).
  std::unique_ptr<protocol::SessionStore> session_store_;

  // Wall clock anchor for session-level physical-time mapping.
  // Set at Reset() (entry), validated at EmitTimeline(finalize=true) (exit).
  std::atomic<double> session_start_wall_sec_{0.0};
  std::atomic<bool> wall_clock_ok_{true};
};

}  // namespace pipeline
}  // namespace orator
