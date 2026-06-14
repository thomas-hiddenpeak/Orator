#pragma once

// AsrWorker: the speech-recognition pipeline as an independent unit.
//
// It owns the ASR engine and its own endpointing buffer. Audio spans handed to
// it (by the controller's worker thread, pulled from the SharedAudioBuffer) are
// appended to a local PCM buffer and segmented by an energy VAD into complete
// utterances. Each completed utterance is transcribed, deposited into the shared
// StreamTimeline as a timed token, and emitted as an incremental event. It keeps
// its own state and never reads diarization output.
//
// Endpointing (Silero VAD): an ASR-only front gate detects speech segments
// from the same PCM stream and emits bounded utterances for decode. This keeps
// ASR segmentation independent from diarization and each decode context bounded.

#include <atomic>
#include <functional>
#include <string>
#include <vector>

#include "model/qwen3_asr.h"
#include "pipeline/asr_vad.h"
#include "pipeline/stream_timeline.h"
#include <cuda_runtime.h>

namespace orator {
namespace pipeline {

class AsrWorker {
 public:
  using Params = AsrSileroVad::Params;

  // Emits an incremental result event (JSON) as each utterance completes.
  using Emit = std::function<void(const std::string&)>;

  // `asr` and `timeline` are owned by the controller and must outlive the
  // worker. The engine must already be initialized + weight-loaded.
  AsrWorker(model::Qwen3Asr* asr, StreamTimeline* timeline, const Params& params,
              Emit emit, cudaStream_t stream = 0);

  // Consume `n` contiguous samples at the current stream position: append to the
  // endpoint buffer and transcribe any utterances that complete.
  void ProcessSpan(const float* samples, int n);

  // Transcribe the trailing (un-endpointed) utterance, if any (call once at end
  // of stream).
  void Finalize();

  // Reset endpointing + engine state for a fresh session.
  void Reset();

  // Absolute sample count consumed and committed so far (monotonic).
  long processed_samples() const { return processed_samples_.load(); }
  double compute_sec() const { return compute_sec_; }

 private:
  // Pull complete utterances from the front VAD; when finalize, also flush the
  // trailing open utterance. Consumes audio that has already been processed.
  void DrainUtterances(bool finalize);
  // Transcribe pcm_[begin,end) (relative) as one utterance, commit + emit it.
  void EmitUtterance(int begin, int end);

  model::Qwen3Asr* asr_;
  StreamTimeline* timeline_;
  Params params_;
  Emit emit_;

  AsrSileroVad vad_;            // ASR-only independent front VAD (Silero)
  std::atomic<long> processed_samples_{0};
  double compute_sec_ = 0.0;
  cudaStream_t stream_ = 0;    // GPU stream for ASR kernels (default = 0)
};

}  // namespace pipeline
}  // namespace orator
