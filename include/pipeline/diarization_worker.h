#pragma once

// DiarizationWorker: the speaker-separation pipeline as an independent unit.
//
// It owns a streaming diarizer and consumes audio spans handed to it (by the
// controller's worker thread, pulled from the SharedAudioBuffer). It keeps the
// diarizer's persistent streaming state, deposits newly produced frames into
// the shared StreamTimeline, and tracks how much audio it has committed (so the
// controller can implement flush barriers) plus its own compute time (for
// honest per-pipeline real-time factors). It never reads ASR state.

#include <atomic>

#include "model/streaming_sortformer.h"
#include "pipeline/stream_timeline.h"

namespace orator {
namespace pipeline {

class DiarizationWorker {
 public:
  // `diarizer` and `timeline` are owned by the controller and must outlive the
  // worker. The diarizer must already be initialized + weight-loaded.
  DiarizationWorker(model::SortformerDiarizer* diarizer, StreamTimeline* timeline);

  // Consume `n` contiguous samples at the current stream position. Runs the
  // diarizer incrementally and appends any stabilized frames to the timeline.
  void ProcessSpan(const float* samples, int n);

  // Flush the diarizer's buffered tail frames (call once at end of stream).
  void Finalize();

  // Reset streaming state for a fresh session.
  void Reset();

  // Absolute sample count consumed and committed so far (monotonic).
  long processed_samples() const { return processed_samples_.load(); }
  double compute_sec() const { return compute_sec_; }

 private:
  model::SortformerDiarizer* diarizer_;
  StreamTimeline* timeline_;
  std::atomic<long> processed_samples_{0};
  double compute_sec_ = 0.0;
};

}  // namespace pipeline
}  // namespace orator
