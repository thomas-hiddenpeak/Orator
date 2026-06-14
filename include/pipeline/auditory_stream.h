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
// contains no socket code; the WebSocket handler and the streaming test drive it
// through the same interface.

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <cuda_runtime.h>

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
    double asr_max_utterance_sec = 42.0;     // force-flush cap (bounded decode context)
    double asr_min_utterance_sec = 0.18;     // ignore shorter speech blips
    std::string asr_vad_model = "models/asr/silero_vad.safetensors";
    float asr_vad_threshold = 0.46f;
    int asr_vad_min_speech_ms = 200;
    int asr_vad_min_silence_ms = 360;
    int asr_vad_speech_pad_ms = 100;
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
  const std::vector<core::DiarSegment>& diar_segments() const { return last_segments_; }
  const core::Transcript& transcript() const { return last_transcript_; }
  double audio_sec() const;
  double diar_compute_sec() const;
  double asr_compute_sec() const;

 private:
  void StartWorkers();           // start diar + asr threads
  void StopWorkers();            // close buffer, join threads
  // Block until both workers have processed up to `target_samples`.
  void WaitForBarrier(long target_samples);
  std::string Serialize();       // build the comprehensive timeline JSON
  void EmitLocked(const std::string& json);  // serialize transport sends

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

    // Per-pipeline GPU stream. diar runs on the default stream (0); asr runs
    // on asr_stream_ so its kernels can overlap with diar's idle intervals.
    cudaStream_t asr_stream_ = nullptr;

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
