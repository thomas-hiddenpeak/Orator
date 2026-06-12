#pragma once

// AuditoryStream: the real-time orchestrator that runs speaker diarization and
// ASR as TWO INDEPENDENT pipelines over a single streamed audio source.
//
// Design (per the project directive): audio arriving from the stream is the
// only thing the two businesses share. After PushAudio() hands a frame in:
//   * the DIARIZATION pipeline consumes it incrementally (Sortformer keeps
//     persistent streaming state; O(n), bounded memory), and
//   * the ASR pipeline consumes the SAME samples independently -- it buffers
//     them, runs its own energy endpointing to find a complete utterance, and
//     transcribes that utterance.
// Neither pipeline reads the other's results. Both write their results back
// onto one shared time base (absolute stream seconds). EmitTimeline() fuses the
// two result streams into a single timeline. "Whoever finishes first is fine"
// -- the only contract is the shared clock.
//
// This class is transport-agnostic: results are delivered through the `Emit`
// callback as JSON strings, so it is driven identically by the WebSocket
// handler (sends to the client) and by the offline streaming test (captures
// strings). It owns no sockets.

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "core/types.h"
#include "model/qwen3_asr.h"
#include "model/streaming_sortformer.h"

namespace orator {
namespace pipeline {

class AuditoryStream {
 public:
  struct Config {
    std::string diarizer_weights = "models/sortformer_4spk_v2.safetensors";
    std::string asr_model_dir = "";  // "" => ASR pipeline disabled
    int sample_rate = 16000;
    int max_speakers = 4;
    float diar_threshold = 0.5f;
    double diar_merge_gap_sec = 0.5;

    // ASR streaming endpointing (independent of diarization):
    double asr_endpoint_silence_sec = 0.8;  // trailing silence that ends an utterance
    double asr_max_utterance_sec = 28.0;     // force-flush cap (bounded decode context)
    double asr_min_utterance_sec = 0.20;     // ignore shorter speech blips
    float asr_vad_rel_threshold = 0.08f;     // speech if frame RMS > thr * session peak
    std::string asr_language = "Chinese";
  };

  // Delivers a result event as a JSON string (see the .cc for the schemas:
  // {"type":"asr",...} incremental utterances, {"type":"timeline",...} fusion).
  using Emit = std::function<void(const std::string&)>;

  AuditoryStream(const Config& config, Emit emit);

  // Load weights and initialize both pipelines. Throws on failure.
  void Start();

  // Feed one block of mono float PCM (already decoded). Runs diarization
  // incrementally and advances ASR endpointing; emits incremental "asr" events
  // for any utterances that complete within this call.
  void PushAudio(const float* samples, int n);

  // Fuse the diarization + transcript results so far into one timeline and emit
  // it. When finalize is true, drains the diarizer's tail frames and transcribes
  // any pending (un-endpointed) ASR audio first.
  void EmitTimeline(bool finalize);

  // Clear all state for a fresh session (keeps loaded weights).
  void Reset();

  bool asr_enabled() const { return asr_ != nullptr; }
  const std::vector<core::DiarSegment>& diar_segments() const { return diar_segments_; }
  const core::Transcript& transcript() const { return transcript_; }
  double audio_sec() const;
  double diar_compute_sec() const { return diar_compute_sec_; }
  double asr_compute_sec() const { return asr_compute_sec_; }

 private:
  // Pull complete utterances out of the ASR buffer and transcribe them. When
  // finalize is true, also transcribes a trailing utterance that has no closing
  // silence yet. Consumes (erases) buffered audio it is done with.
  void DrainAsr(bool finalize);
  // Transcribe asr_pcm_[begin,end) (relative) as one utterance and emit it.
  void EmitUtterance(int begin, int end);

  Config config_;
  Emit emit_;

  std::unique_ptr<model::SortformerDiarizer> diarizer_;
  std::unique_ptr<model::Qwen3Asr> asr_;  // null when ASR disabled

  // Diarization pipeline accumulation (full-session frames on the shared clock).
  std::vector<float> diar_probs_;
  long diar_total_frames_ = 0;
  std::vector<core::DiarSegment> diar_segments_;  // built at EmitTimeline
  long total_samples_ = 0;  // shared clock: total audio ingested this session
  double diar_compute_sec_ = 0.0;

  // ASR pipeline state. asr_pcm_ holds only the UNCONSUMED tail; asr_base_sample_
  // is the absolute sample index of asr_pcm_[0] so timestamps stay on the shared
  // clock as the prefix is erased.
  std::vector<float> asr_pcm_;
  long asr_base_sample_ = 0;
  float asr_peak_rms_ = 0.0f;  // monotonic session peak for the relative VAD
  core::Transcript transcript_;
  double asr_compute_sec_ = 0.0;
};

}  // namespace pipeline
}  // namespace orator
