#pragma once

// AsrWorker: the speech-recognition pipeline as an independent unit.
//
// It owns the ASR engine and drives it over audio spans handed to it (by the
// controller's worker thread, pulled from the SharedAudioBuffer). It runs the
// Spec 003 incremental KV-cache session: continuous audio is fed into a
// persistent decode session and committed in fixed-cadence segments, each
// deposited into the shared StreamTimeline as a timed token and emitted as an
// incremental event. It keeps its own state and never reads diarization output.

#include <atomic>
#include <functional>
#include <string>
#include <vector>

#include "model/qwen3_asr.h"
#include "pipeline/stream_timeline.h"
#include "core/time_base.h"
#include <cuda_runtime.h>

namespace orator {
namespace pipeline {

class AsrWorker {
 public:
  struct Params {
    int sample_rate = 16000;
    double segment_sec = 24.0;  // commit + reset cadence (cap)
  };

  using Emit = std::function<void(const std::string&)>;

  using TextSegmentSink =
      std::function<void(long id, double start, double end,
                         const std::string& text)>;

  AsrWorker(model::Qwen3Asr* asr, StreamTimeline* timeline, const Params& params,
              Emit emit, cudaStream_t stream = 0);

  void set_text_sink(TextSegmentSink sink) { text_sink_ = std::move(sink); }

  void ProcessSpan(const float* samples, int n);
  void Finalize();
  void Reset();

  long processed_samples() const { return processed_samples_.load(); }
  double compute_sec() const { return compute_sec_; }

 private:
  void ProcessIncremental(const float* samples, int n, bool finalize);
  void EmitIncrementalChunk(const float* samples, int n, bool finalize);

  model::Qwen3Asr* asr_;
  StreamTimeline* timeline_;
  Params params_;
  Emit emit_;
  TextSegmentSink text_sink_;
  core::TimeBase tb_;

  std::atomic<long> processed_samples_{0};
  double compute_sec_ = 0.0;
  cudaStream_t stream_ = 0;

  bool inc_in_segment_ = false;
  long inc_abs_pos_ = 0;
  long inc_seg_start_sample_ = 0;
  long inc_seg_end_sample_ = 0;
  long inc_seg_samples_ = 0;
  std::string inc_live_text_;
  long inc_text_id_ = 0;
  std::string inc_delivered_text_;
};

}  // namespace pipeline
}  // namespace orator
