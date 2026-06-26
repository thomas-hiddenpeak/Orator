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
  class VadCache {
   public:
    void AddSegment(double start, double end) {
      std::lock_guard<std::mutex> lk(mutex_);
      segments_.emplace_back(start, end);
    }
    std::vector<std::pair<double, double>> GetAll() const {
      std::lock_guard<std::mutex> lk(mutex_);
      return segments_;
    }
    // The absolute time (common clock, sec) up to which VAD has processed and
    // confirmed its speech/silence decision. ASR treats a silence sub-span as
    // skippable only when its end is within this horizon, so at real-time pacing
    // (VAD ~= ASR at the leading edge) ASR can still skip confirmed silence
    // instead of spending GPU on it. Published by the VAD pipeline via
    // `vad/progress` and consumed here -- ASR never blocks on VAD.
    void set_horizon(double sec) {
      std::lock_guard<std::mutex> lk(mutex_);
      horizon_ = sec;
    }
    double horizon() const {
      std::lock_guard<std::mutex> lk(mutex_);
      return horizon_;
    }
   private:
    mutable std::mutex mutex_;
    std::vector<std::pair<double, double>> segments_;
    double horizon_ = -1e9;
  };

  AsrWorker(core::IAsr* asr, const Params& params,
            Emit emit, core::TimeBase tb, cudaStream_t stream = 0,
            VadCache* vad_cache = nullptr);

  void set_text_sink(TextSegmentSink sink) { text_sink_ = std::move(sink); }

  void ProcessSpan(const float* samples, int n);
  void Finalize();
  void Reset();

  long processed_samples() const { return processed_samples_.load(); }
  double compute_sec() const { return compute_sec_.load(); }

 private:
  void ProcessIncremental(const float* samples, int n, bool finalize);
  void EmitIncrementalChunk(const float* samples, int n, bool finalize);

  // Event-driven VAD gate. ProcessSpan cuts the incoming audio span at the VAD
  // segment boundaries that fall inside it (VadCutSamples) so each sub-span lies
  // wholly within one VAD region, then ProcessGateSubSpan acts on it: process
  // speech and UNCONFIRMED audio (never drop speech), skip only CONFIRMED
  // silence (the GPU saving), committing the open utterance after its trailing
  // window. Cut points are VAD audio times, so coverage does not depend on how
  // large the span is (chunk-invariant): a flooded multi-minute span is no
  // longer skipped wholesale because its end happens to land in a silence gap.
  // This never blocks on VAD -- it only consults what VAD has already published.
  void ProcessGateSubSpan(const float* sub, int sub_n);
  std::vector<long> VadCutSamples(long span_start, long span_end) const;

  core::IAsr* asr_;
  Params params_;
  Emit emit_;
  TextSegmentSink text_sink_;
  core::TimeBase tb_;

  // Local VAD segment cache — updated by subscribing to VAD events via
  // ProtocolTimeline. Eliminates O(N^2) Replay calls on hot path.
  VadCache* vad_cache_ = nullptr;

  std::atomic<long> processed_samples_{0};
  std::atomic<int> debug_segments_started_{0};
  std::atomic<int> debug_segments_finalized_{0};
  std::atomic<double> compute_sec_{0.0};
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

  // VAD gate state: IDLE = skipping silence / between utterances; PROCESSING =
  // an engine segment is being fed. (TRAILING is retained for ABI but unused by
  // the event-driven gate, which commits inline at confirmed silence.)
  enum class VadState { IDLE, PROCESSING, TRAILING };
  VadState vad_state_ = VadState::IDLE;
  double vad_trail_start_sec_ = 0.0;  // retained; unused by event-driven gate
  // End (common clock, sec) of the most recently fed speech, for the trailing
  // window measurement at a confirmed silence gap.
  double last_speech_end_sec_ = -1e9;
};

}  // namespace pipeline
}  // namespace orator
