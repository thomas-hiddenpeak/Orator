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
#include <deque>
#include <functional>
#include <string>
#include <vector>

#include "model/qwen3_asr.h"
#include "pipeline/asr_preprocessor.h"
#include "pipeline/asr_vad.h"
#include "pipeline/stream_timeline.h"
#include "core/time_base.h"
#include <cuda_runtime.h>

namespace orator {
namespace pipeline {

class AsrWorker {
 public:
  struct Params {
    AsrSileroVad::Params vad;
    AsrPreprocessor::Params preproc;
    // Incremental KV-cache streaming (Spec 003): when true, the worker drives
    // the engine's StreamReset/StreamChunk/StreamFinalize session instead of
    // decoding each VAD span independently. VAD speech spans are fed into the
    // session and accumulated into a segment carrying KV context across windows;
    // the segment is committed at a natural endpoint or the segment cap.
    bool incremental = false;
    double segment_sec = 24.0;  // commit + reset cadence (cap), aligned to VAD
    // T050: use Silero-VAD speech endpoints to choose segment reset points. The
    // engine is still fed CONTINUOUS audio (the VAD only picks reset timing, it
    // does not trim). A segment closes at the first endpoint past
    // endpoint_min_segment_sec, or at segment_sec (hard cap) if no endpoint.
    bool endpoint_reset = false;
    double endpoint_min_segment_sec = 10.0;
  };

  // Emits an incremental result event (JSON) as each utterance completes.
  using Emit = std::function<void(const std::string&)>;

  // Spec 004 Step 2: delivers a committed text segment (what/when on the common
  // time base) to the comprehensive timeline. Called at each commit, alongside
  // the JSON Emit. The controller wires this to ComprehensiveTimeline::UpsertText
  // and pushes any returned revisions. Keeps the worker decoupled from the
  // timeline: the worker reports its segment; the controller delivers it.
  using TextSegmentSink =
      std::function<void(double start, double end, const std::string& text)>;

  // `asr` and `timeline` are owned by the controller and must outlive the
  // worker. The engine must already be initialized + weight-loaded.
  AsrWorker(model::Qwen3Asr* asr, StreamTimeline* timeline, const Params& params,
              Emit emit, cudaStream_t stream = 0,
              int rollback_tokens = 3);

  // Set the comprehensive-timeline delivery sink (Spec 004 Step 2). Optional;
  // when unset the worker only does the JSON Emit + timeline token (legacy).
  void set_text_sink(TextSegmentSink sink) { text_sink_ = std::move(sink); }

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
  void EmitUtterance(int begin, int end, bool finalize);

  // Incremental-session path (Params::incremental): feed continuous audio into
  // the engine's streaming session and commit + reset the segment at the cap or
  // on finalize. The session consumes continuous (untrimmed) audio so the
  // acoustic flow is preserved (VAD trimming degrades the streamed context).
  // ProcessIncremental slices large input chunks at the segment cap; each slice
  // is handed to EmitIncrementalChunk.
  void ProcessIncremental(const float* samples, int n, bool finalize);
  void EmitIncrementalChunk(const float* samples, int n, bool finalize);

  model::Qwen3Asr* asr_;
  StreamTimeline* timeline_;
  Params params_;
  Emit emit_;
  TextSegmentSink text_sink_;   // Spec 004 Step 2: live delivery to comp timeline
  // Spec 005: common time base (origin 0). The worker's sample positions are
  // absolute on the common clock; time codes go through this base.
  core::TimeBase tb_{params_.vad.sample_rate, 0};

  AsrSileroVad vad_;            // ASR-only independent front VAD (Silero)
  AsrPreprocessor preproc_;     // ASR-only enhancement after VAD
  std::atomic<long> processed_samples_{0};
  double compute_sec_ = 0.0;
  cudaStream_t stream_ = 0;    // GPU stream for ASR kernels (default = 0)
  int rollback_tokens_ = 3;
  std::string pending_prefix_;

  // Incremental-session segment state (Params::incremental).
  bool inc_in_segment_ = false;      // a segment is currently open
  long inc_abs_pos_ = 0;             // absolute samples fed to the session
  long inc_seg_start_sample_ = 0;    // absolute sample of segment start
  long inc_seg_end_sample_ = 0;      // absolute sample of last fed audio
  long inc_seg_samples_ = 0;         // samples accumulated this segment
  std::string inc_live_text_;        // current live transcript of the segment
  std::deque<long> inc_endpoints_;   // pending VAD endpoints (absolute samples)
};

}  // namespace pipeline
}  // namespace orator
