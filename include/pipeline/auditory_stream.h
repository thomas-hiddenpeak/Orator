#pragma once

// AuditoryStream: the controller that runs speaker diarization and ASR as TWO
// INDEPENDENT pipelines over one streamed audio source, on independent threads.
//
// Per the architecture (Spec 001): audio arriving from the stream is the only
// thing the two businesses share. PushAudio() appends samples to a shared
// buffer; two worker threads (diarization, ASR) consume that buffer at their
// own pace, never reading each other's results, and deposit their output onto
// one mutex-guarded timeline keyed by the shared absolute clock. The controller
// owns the buffer, the workers, and the thread lifecycle (start, drain on
// flush, stop+join on end). "Whoever finishes first is fine" -- the only
// contract is the shared clock.
//
// This class is transport-agnostic: results are delivered through the `Emit`
// callback as JSON strings (incremental {"type":"asr",...} events from the ASR
// worker, and the unified {"type":"timeline",...} document on flush/end). It
// owns no sockets; the WebSocket handler and the offline streaming test drive
// it identically.

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "core/types.h"
#include "model/qwen3_asr.h"
#include "model/streaming_sortformer.h"
#include "pipeline/asr_worker.h"
#include "pipeline/diarization_worker.h"
#include "pipeline/shared_audio_buffer.h"
#include "pipeline/stream_timeline.h"

namespace orator {
namespace pipeline {

class AuditoryStream {
 public:
  struct Config {
    std::string diarizer_weights = "models/sortformer_4spk_v2.safetensors";
    std::string asr_model_dir = "";  // "" => ASR pipeline disabled
    int sample_rate = 16000;
    int max_speakers = 4;
    float diar_threshold = 0.5f;
    double diar_merge_gap_sec = 0.5;

    // ASR streaming endpointing (independent of diarization):
    double asr_endpoint_silence_sec = 0.8;  // trailing silence that ends an utterance
    double asr_max_utterance_sec = 28.0;     // force-flush cap (bounded decode context)
    double asr_min_utterance_sec = 0.20;     // ignore shorter speech blips
    float asr_vad_rel_threshold = 0.08f;     // speech if frame RMS > thr * session peak
    std::string asr_language = "Chinese";
  };

  // Delivers a result event as a JSON string. Invoked from the ASR worker thread
  // (incremental events) and from the controller (timeline); the controller
  // serializes these calls so the transport sees one at a time.
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

  // Emit the unified timeline. When finalize is true: signal end-of-stream,
  // drain + join both workers (flushing tails), then serialize. When false
  // (flush): wait until both workers have consumed all audio pushed so far,
  // then serialize; streaming continues with state preserved.
  void EmitTimeline(bool finalize);

  // Stop the workers (if running) and clear all state for a fresh session, then
  // re-arm the workers so streaming can resume.
  void Reset();

  bool asr_enabled() const { return asr_ != nullptr; }
  const std::vector<core::DiarSegment>& diar_segments() const { return last_segments_; }
  const core::Transcript& transcript() const { return last_transcript_; }
  double audio_sec() const;
  double diar_compute_sec() const;
  double asr_compute_sec() const;

 private:
  void StartWorkers();           // spawn diar + asr threads
  void StopWorkers();            // close buffer, join threads
  void WaitForBarrier(long target_samples);  // block until both workers caught up
  std::string Serialize();       // build the unified timeline JSON (snapshots)
  void EmitLocked(const std::string& json);  // serialized transport emit

  Config config_;
  Emit emit_;

  std::unique_ptr<model::SortformerDiarizer> diarizer_;
  std::unique_ptr<model::Qwen3Asr> asr_;  // null when ASR disabled

  SharedAudioBuffer buffer_;
  StreamTimeline timeline_;
  std::unique_ptr<DiarizationWorker> diar_worker_;
  std::unique_ptr<AsrWorker> asr_worker_;
  int diar_cursor_ = -1;
  int asr_cursor_ = -1;

  std::thread diar_thread_;
  std::thread asr_thread_;
  bool running_ = false;

  // Wakes WaitForBarrier whenever a worker advances or finishes.
  std::mutex progress_mutex_;
  std::condition_variable progress_cv_;
  // Serializes Emit calls across the worker threads and the controller.
  std::mutex emit_mutex_;

  // Last serialized snapshots, exposed for inspection by the test tool.
  std::vector<core::DiarSegment> last_segments_;
  core::Transcript last_transcript_;
};

}  // namespace pipeline
}  // namespace orator
