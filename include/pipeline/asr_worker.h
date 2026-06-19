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
#include <mutex>
#include <string>
#include <vector>

#include "model/qwen3_asr.h"
#include "pipeline/stream_timeline.h"
#include "core/time_base.h"
#include "protocol/protocol_timeline.h"
#include <cuda_runtime.h>

namespace orator {
namespace pipeline {

class AsrWorker {
 public:
  struct Params {
    int sample_rate = 16000;
    double segment_sec = 24.0;  // commit + reset cadence (cap)
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
  AsrWorker(model::Qwen3Asr* asr, StreamTimeline* timeline, const Params& params,
              Emit emit, core::TimeBase tb, cudaStream_t stream = 0);

  void set_text_sink(TextSegmentSink sink) { text_sink_ = std::move(sink); }

  void set_protocol_timeline(protocol::ProtocolTimeline* pt) {
    protocol_timeline_ = pt;
  }

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

  // VAD gating via ProtocolTimeline (Constitution Art. III §8).
  // The worker reads VAD segments from the protocol layer, not via direct push.
  protocol::ProtocolTimeline* protocol_timeline_ = nullptr;

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
