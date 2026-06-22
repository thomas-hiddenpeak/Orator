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

#include "core/stages.h"
#include "core/time_base.h"
#include <cuda_runtime.h>

namespace orator {
namespace pipeline {

class AsrWorker {
 public:
  struct Params {
    int sample_rate = 16000;
    double segment_sec = 24.0;  // commit + reset cadence (cap, unused with VAD gate)
    int max_audio_tokens = 1500;       // KV-cache safety cap (prevents crash above ~1768 tokens)
    bool asr_vad_gate = true;           // enable VAD-gated processing
    int asr_vad_lead_ms = 200;          // lead buffer (ms) before VAD speech onset
    double asr_vad_trail_sec = 1.5;     // trailing window (sec) after VAD silence before commit
  };

  using Emit = std::function<void(const std::string&)>;

  using TextSegmentSink =
      std::function<void(long id, double start, double end,
                         const std::string& text)>;

  // `tb` is the common time base inherited from SharedAudioBuffer::time_base().
  // The worker holds it as a member and derives all time codes from it.
  // Text segments are delivered via text_sink_ (→ ProtocolTimeline → comp_);
  // raw tokens are no longer written to an external StreamTimeline.
  // `vad_reader` is a callable that returns VAD speech segments from the
  // ComprehensiveTimeline (Constitution Art. III §8 — pipelines communicate
  // only through the timeline, never via direct callbacks).
  using VadSegmentReader = std::function<std::vector<std::pair<double, double>>()>;

  AsrWorker(core::IAsr* asr, const Params& params,
              Emit emit, core::TimeBase tb, cudaStream_t stream = 0,
              VadSegmentReader vad_reader = nullptr);

  void set_text_sink(TextSegmentSink sink) { text_sink_ = std::move(sink); }

  void ProcessSpan(const float* samples, int n);
  void Finalize();
  void Reset();

  long processed_samples() const { return processed_samples_.load(); }
  double compute_sec() const { return compute_sec_; }

 private:
  void ProcessIncremental(const float* samples, int n, bool finalize);
  void EmitIncrementalChunk(const float* samples, int n, bool finalize);

  core::IAsr* asr_;
  Params params_;
  Emit emit_;
  TextSegmentSink text_sink_;
  core::TimeBase tb_;

  // VAD segments reader — fetches speech segments from ComprehensiveTimeline.
  // Called from ProcessSpan() on the ASR worker thread; the reader handles its
  // own synchronization (Constitution Art. III §8).
  VadSegmentReader vad_reader_;

  std::atomic<long> processed_samples_{0};
  std::atomic<int> debug_segments_started_{0};
  std::atomic<int> debug_segments_finalized_{0};
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

  // Push samples into the ring buffer (thread-safe within worker thread).
  void RingPush(const float* samples, int n, long abs_pos);
  // Pop the last `n` samples from the ring buffer into *out.
  void RingPop(int n, std::vector<float>* out);
  static constexpr int kRingBufferSamples = 8000;
  std::vector<float> ring_buffer_;
  int ring_write_pos_ = 0;
  int ring_count_ = 0;
  long ring_base_abs_pos_ = 0;  // absolute sample of ring_buffer_[0]

  // VAD state machine
  enum class VadState { IDLE, PROCESSING, TRAILING };
  VadState vad_state_ = VadState::IDLE;
  double vad_trail_start_sec_ = 0.0;  // when trailing started (common clock)
};

}  // namespace pipeline
}  // namespace orator
