#pragma once

// AsrWorker: the speech-recognition pipeline as an independent unit.
//
// It owns the ASR engine and drives it over audio spans handed to it (by the
// controller's worker thread, pulled from its private PipelineAudioCache). It
// reads finalized VAD evidence from ComprehensiveTimeline and runs the
// Spec 003 incremental KV-cache session: continuous audio is fed into a
// persistent decode session and committed in fixed-cadence segments, each
// deposited as a typed ASR record and emitted as an incremental event. It keeps
// its own state and never reads diarization output.

#include <atomic>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "core/stages.h"
#include "core/time_base.h"
#include <cuda_runtime.h>

namespace orator {
namespace pipeline {

class ComprehensiveTimeline;

class AsrWorker {
 public:
  struct Params {
    int sample_rate = 16000;
    double segment_sec =
        24.0;  // commit + reset cadence (cap, unused with VAD gate)
    int max_audio_tokens =
        1500;  // KV-cache safety cap (prevents crash above ~1768 tokens)
    bool asr_vad_gate = true;   // enable VAD-gated processing
    int asr_vad_lead_ms = 200;  // lead buffer (ms) before VAD speech onset
    int asr_vad_gate_chunk_ms =
        100;  // deterministic decoder feed quantum for decided speech
    double asr_vad_trail_sec =
        1.5;  // trailing window (sec) after VAD silence before commit
    double asr_vad_min_overlap_sec =
        0.12;  // minimum confirmed VAD speech overlap to keep an ASR final
  };

  using Emit = std::function<void(const std::string&)>;

  using TextSegmentSink =
      std::function<void(long id, double start, double end,
                         const std::string& text, bool is_final)>;

  // `tb` comes from the session audio-ingest owner's canonical time base. The
  // worker holds that value and derives all time codes from it.
  // Text segments are deposited by the controller through text_sink_. VAD
  // evidence is read only from the typed ComprehensiveTimeline track.

  AsrWorker(core::IAsr* asr, const Params& params, Emit emit, core::TimeBase tb,
            cudaStream_t stream = 0,
            const ComprehensiveTimeline* timeline = nullptr);

  void set_text_sink(TextSegmentSink sink) { text_sink_ = std::move(sink); }

  void ProcessSpan(const float* samples, int n);
  void Finalize();
  void Reset();

  long processed_samples() const { return processed_samples_.load(); }
  double compute_sec() const { return compute_sec_.load(); }

 private:
  void ProcessIncremental(const float* samples, int n, bool finalize);
  void EmitIncrementalChunk(const float* samples, int n, bool finalize);

  // The VAD gate buffers audio ahead of the typed decision frontier, then
  // consumes stable speech in deterministic TOML-sized quanta. It never waits
  // for VAD and never reads detector state directly.
  void DrainVadGate(bool final);
  void FeedPendingUntil(long end_sample, bool exact_end);
  void SkipPendingUntil(long end_sample);
  void ConsumePending(long n);
  long PendingSamples() const;
  const float* PendingData() const;

  core::IAsr* asr_;
  Params params_;
  Emit emit_;
  TextSegmentSink text_sink_;
  core::TimeBase tb_;

  // Authoritative typed evidence store. Snapshot reads are non-blocking with
  // respect to VAD progress and never access VAD worker state directly.
  const ComprehensiveTimeline* timeline_ = nullptr;

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
  bool finalizing_ = false;

  // Pending audio covers [inc_abs_pos_, received_samples_) on the common clock.
  // `pending_offset_` avoids front-erasing on every fixed feed quantum.
  std::vector<float> pending_audio_;
  size_t pending_offset_ = 0;
  long received_samples_ = 0;
  long last_speech_end_sample_ = -1;
};

}  // namespace pipeline
}  // namespace orator
