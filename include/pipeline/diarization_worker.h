#pragma once

#include <cuda_runtime.h>

#include <atomic>
#include <functional>
#include <vector>

#include "core/types.h"
#include "model/streaming_sortformer.h"
#include "pipeline/stream_timeline.h"

// DiarizationWorker: the speaker-separation pipeline as an independent unit.
//
// It owns a streaming diarizer and consumes audio spans handed to it (by the
// controller's worker thread, pulled from the SharedAudioBuffer). It keeps the
// diarizer's persistent streaming state, deposits newly produced frames into
// the shared StreamTimeline, and tracks how much audio it has committed (so the
// controller can implement flush barriers) plus its own compute time (for
// honest per-pipeline real-time factors). It never reads ASR state.

namespace orator {
namespace pipeline {

class DiarizationWorker {
 public:
  // Tuning for frame->segment derivation and live delivery cadence.
  struct Params {
    float threshold = 0.5f;        // per-speaker activity threshold
    double merge_gap_sec = 0.5;    // coalesce same-speaker gaps up to this
    double deliver_interval_sec = 1.0;  // min audio between live deliveries
    int sample_rate = 16000;
  };

  // Spec 004: delivers the pipeline's current speaker view (who/when) to the
  // comprehensive timeline. The diarization segments are a GLOBAL derivation
  // from frames (boundaries shift as frames arrive), so the whole current view
  // is delivered (the controller calls ReplaceSpeakers). Called live as audio
  // is processed (throttled) and once on Finalize.
  using SpeakerSink = std::function<void(const std::vector<core::DiarSegment>&)>;

  // `diarizer` and `timeline` are owned by the controller and must outlive the
  // worker. The diarizer must already be initialized + weight-loaded.
  // `stream` is the CUDA stream for all GPU work (kernels, copies, sync).
  DiarizationWorker(model::SortformerDiarizer* diarizer, StreamTimeline* timeline,
                    Params params, cudaStream_t stream);

  // Set the comprehensive-timeline speaker-view sink (Spec 004). Optional.
  void set_speaker_sink(SpeakerSink sink) { speaker_sink_ = std::move(sink); }

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
  // Derive the current speaker view from all accumulated frames and deliver it
  // through speaker_sink_. `force` bypasses the delivery-interval throttle.
  void DeliverSpeakers(bool force);

  model::SortformerDiarizer* diarizer_;
  StreamTimeline* timeline_;
  Params params_;
  cudaStream_t stream_;
  SpeakerSink speaker_sink_;
  long last_deliver_sample_ = 0;
  std::atomic<long> processed_samples_{0};
  double compute_sec_ = 0.0;
};

}  // namespace pipeline
}  // namespace orator
