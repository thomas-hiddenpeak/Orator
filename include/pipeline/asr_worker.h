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
// Endpointing (energy VAD): 10 ms RMS frames, a relative threshold against the
// monotonic session peak; an utterance ends after `endpoint_silence_sec` of
// trailing silence or at the `max_utterance_sec` cap, and utterances shorter
// than `min_utterance_sec` are ignored. This keeps each decode bounded so the
// pipeline can run at its own maximum rate.

#include <atomic>
#include <functional>
#include <string>
#include <vector>

#include "model/qwen3_asr.h"
#include "pipeline/stream_timeline.h"

namespace orator {
namespace pipeline {

class AsrWorker {
 public:
  struct Params {
    int sample_rate = 16000;
    double endpoint_silence_sec = 0.8;
    double max_utterance_sec = 28.0;
    double min_utterance_sec = 0.20;
    float vad_rel_threshold = 0.08f;
  };

  // Emits an incremental result event (JSON) as each utterance completes.
  using Emit = std::function<void(const std::string&)>;

  // `asr` and `timeline` are owned by the controller and must outlive the
  // worker. The engine must already be initialized + weight-loaded.
  AsrWorker(model::Qwen3Asr* asr, StreamTimeline* timeline, const Params& params,
            Emit emit);

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
  // Pull complete utterances from the front of `pcm_`; when `finalize`, also
  // flush the trailing open utterance. Consumes (erases) audio it is done with.
  void DrainUtterances(bool finalize);
  // Transcribe pcm_[begin,end) (relative) as one utterance, commit + emit it.
  void EmitUtterance(int begin, int end);

  model::Qwen3Asr* asr_;
  StreamTimeline* timeline_;
  Params params_;
  Emit emit_;

  std::vector<float> pcm_;     // unconsumed tail of the stream
  long base_sample_ = 0;       // absolute index of pcm_.front()
  float peak_rms_ = 0.0f;      // monotonic session peak for the relative VAD
  std::atomic<long> processed_samples_{0};
  double compute_sec_ = 0.0;
};

}  // namespace pipeline
}  // namespace orator
