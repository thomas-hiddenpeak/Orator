#pragma once

#include <cuda_runtime.h>

#include <atomic>
#include <functional>
#include <vector>

#include "core/time_base.h"
#include "core/types.h"
#include "core/stages.h"

// DiarizationWorker: the speaker-separation pipeline as an independent unit.
//
// It owns a streaming diarizer and consumes audio spans handed to it (by the
// controller's worker thread, pulled from the SharedAudioBuffer). It keeps the
// diarizer's persistent streaming state and accumulates frame probabilities
// internally for segment derivation. It never reads ASR state.

namespace orator {
namespace pipeline {

class DiarizationWorker {
 public:
  // Tuning for frame->segment derivation and live delivery cadence.
  struct Params {
    float threshold = 0.5f;        // per-speaker activity threshold (FramesToSegments)
    double merge_gap_sec = 0.5;    // coalesce same-speaker gaps up to this
    double deliver_interval_sec = 1.0;  // min audio between live deliveries
    int sample_rate = 16000;
    // Onset/offset double-threshold post-processing (used by OnsetOffsetSegments)
    double onset = 0.45;            // segment start probability threshold
    double offset = 0.25;           // segment end probability threshold
    double pad_onset = 0.0;         // extra time before segment start
    double pad_offset = 0.0;        // extra time after segment end
    double min_dur_on = 0.5;        // minimum segment duration (seconds)
    double min_dur_off = 1.0;       // minimum gap for merging (seconds)
  };

  // Spec 004: delivers the pipeline's current speaker view (who/when) to the
  // comprehensive timeline. The diarization segments are a GLOBAL derivation
  // from frames (boundaries shift as frames arrive), so the whole current view
  // is delivered (the controller calls ReplaceSpeakers). Called live as audio
  // is processed (throttled) and once on Finalize.
  using SpeakerSink = std::function<void(const std::vector<core::DiarSegment>&)>;

  // `diarizer` is owned by the controller and must outlive the worker.
  // The diarizer must already be initialized + weight-loaded.
  // `stream` is the CUDA stream for all GPU work (kernels, copies, sync).
  // `tb` is the common time base inherited from SharedAudioBuffer::time_base().
  // The worker holds it as a member and derives all time codes from it.
  // Frames are accumulated internally (no external StreamTimeline needed).
  DiarizationWorker(core::IDiarizer* diarizer,
                     Params params, core::TimeBase tb, cudaStream_t stream);

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
  double compute_sec() const { return compute_sec_.load(); }

 private:
  // Derive the current speaker view from all accumulated frames and deliver it
  // through speaker_sink_. `force` bypasses the delivery-interval throttle.
  void DeliverSpeakers(bool force);

  // Accumulated frame probabilities (internal, replaces StreamTimeline storage).
  std::vector<float> diar_probs_;
  int diar_speakers_ = 0;
  double diar_frame_period_sec_ = 0.0;

  core::IDiarizer* diarizer_;
  Params params_;
  core::TimeBase tb_;
  cudaStream_t stream_;
  SpeakerSink speaker_sink_;
  long last_deliver_sample_ = 0;
  std::atomic<long> processed_samples_{0};
  std::atomic<double> compute_sec_{0.0};
};

}  // namespace pipeline
}  // namespace orator
