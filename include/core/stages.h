#pragma once

// Stage interfaces: the decoupling boundary of the system.
//
// The pipeline depends ONLY on these abstract interfaces, never on concrete
// model implementations. Concrete models (Sortformer, stubs, future encoders)
// are constructed through the registry (core/registry.h) and injected into the
// pipeline. This is what makes "模型解耦" (model decoupling) real: swapping a
// model is a config/registration change, not a code change in consumers.

#include <cuda_runtime.h>
#include <string>
#include <vector>

#include "core/types.h"

namespace orator {
namespace core {

// Configuration handed to a diarizer at initialization. Fields are
// intentionally model-agnostic; model-specific knobs live inside each
// implementation.
struct DiarizationConfig {
  int sample_rate = 16000;
  int max_speakers = 4;
  float activity_threshold = 0.5f;  // prob >= threshold => speaker active
};

// Streaming speaker diarization model.
// Generalizes Streaming Sortformer; any frame-level diarizer fits this
// contract.
class IDiarizer {
 public:
  virtual ~IDiarizer() = default;

  virtual void Initialize(const DiarizationConfig& config) = 0;
  // Load weights from a safetensors path. Stubs may ignore the path.
  virtual void LoadWeights(const std::string& path) = 0;
  // Clear streaming state (speaker cache, FIFO, etc.) between sessions.
  virtual void Reset() = 0;
  // Consume one audio chunk and return frame-level activity for that chunk.
  virtual DiarizationFrames ProcessChunk(const AudioChunk& chunk) = 0;

  // ── Streaming incremental diarization ────────────────────────────
  // Feed mono-16k samples and return frame-level speaker probabilities.
  // `final` flushes any residual frames. `stream` is the CUDA stream.
  virtual DiarizationFrames StreamAudio(const float* samples, int num_samples,
                                        bool final, cudaStream_t stream) = 0;

  virtual int max_speakers() const = 0;
  virtual double frame_period_sec() const = 0;
  virtual std::string name() const = 0;
};

// Extracts a fixed-dimension speaker embedding from an audio span.
// RETAINED BUT INACTIVE — no concrete implementation is wired into any
// runtime pipeline. Retained for future speaker-identification features.
class ISpeakerEmbedder {
 public:
  virtual ~ISpeakerEmbedder() = default;
  virtual void LoadWeights(const std::string& path) = 0;
  virtual int dim() const = 0;
  virtual std::vector<float> Embed(const AudioChunk& chunk) = 0;
  virtual std::string name() const = 0;
};

// Persistent registry of enrolled speakers, supporting 1:N matching.
// RETAINED BUT INACTIVE — the concrete implementation (speaker_database.h)
// compiles but is not wired into any runtime pipeline. Retained for
// future speaker-identification features.
class ISpeakerRegistry {
 public:
  virtual ~ISpeakerRegistry() = default;
  virtual bool Enroll(const std::string& speaker_id,
                      const float* embedding) = 0;
  // Returns matched index or -1; writes the score when out_score != nullptr.
  virtual int Match(const float* embedding, float threshold,
                    float* out_score) const = 0;
  virtual std::string SpeakerIdAt(int index) const = 0;
  virtual int EmbeddingDim() const = 0;
  virtual int Size() const = 0;
};

// Configuration handed to an ASR engine at initialization. Fields are
// model-agnostic; model-specific knobs (beam size, encoder chunking, etc.)
// live inside each implementation.
struct AsrConfig {
  int sample_rate = 16000;
  std::string language = "";  // BCP-47-ish hint; "" => auto-detect
};

// Automatic speech recognition: turns audio into a timed token Transcript.
// Generalizes any encoder-decoder ASR (Whisper-family, Qwen3-ASR, etc.); the
// pipeline depends only on this contract, never on a concrete engine. The
// Transcript it returns is deposited, with diarization, into the
// ComprehensiveTimeline, so ASR and diarization meet on a single time base.
class IAsr {
 public:
  virtual ~IAsr() = default;

  virtual void Initialize(const AsrConfig& config) = 0;
  // Load weights from a model path. Stubs may ignore the path.
  virtual void LoadWeights(const std::string& path) = 0;
  // Clear any streaming/decoding state between sessions.
  virtual void Reset() = 0;
  // Transcribe a span of audio into timed tokens. Token times are absolute
  // within the stream (offset by audio.t_start_sec).
  virtual Transcript Transcribe(const AudioChunk& audio) = 0;

  virtual void set_max_new_tokens(int /*max_tokens*/) {}

  // ── Streaming incremental decode ─────────────────────────────────
  // Begin a new segment at absolute sample position `base_sample`.
  virtual void StreamReset(long base_sample) = 0;
  // Feed mono-16k PCM samples; returns the current live transcript.
  // `stream` is the CUDA stream for GPU work in this call.
  virtual std::string StreamChunk(const float* pcm, int n,
                                  cudaStream_t stream) = 0;
  // Flush residual tail; returns the final transcript for the segment.
  virtual std::string StreamFinalize(cudaStream_t stream) = 0;
  // Total accumulated audio tokens in the current segment.
  virtual int stream_audio_tokens() const = 0;

  virtual std::string name() const = 0;
};

// Terminal consumer of a timeline (e.g. JSON for an LLM, a socket, a file).
// RETAINED BUT INACTIVE — the runtime pipeline uses Emit callbacks
// (std::function) rather than this interface. Retained as a contract
// option for future non-callback consumers.
class ISink {
 public:
  virtual ~ISink() = default;
  virtual void Consume(const Timeline& timeline) = 0;
};

// Configuration handed to a VAD engine at initialization. Fields are
// model-agnostic; model-specific knobs live inside each implementation.
struct VadConfig {
  int sample_rate = 16000;
  float threshold = 0.5f;
  int min_speech_ms = 250;
  int min_silence_ms = 300;
};

// A single VAD speech segment identified by absolute sample indices.
struct VadSegmentResult {
  long start_sample = 0;
  long end_sample = 0;
};

// Voice Activity Detection: segments audio into speech/non-speech regions.
// Generalizes any VAD model (Silero, energy-based, etc.); the pipeline
// depends only on this contract, never on a concrete detector.
class IVad {
 public:
  virtual ~IVad() = default;

  virtual void Initialize(const VadConfig& config) = 0;
  // Load weights from a model path. Stubs may ignore the path.
  virtual void LoadWeights(const std::string& path) = 0;
  // Clear streaming state (LSTM state, endpoint counters) between sessions.
  virtual void Reset() = 0;

  virtual std::string name() const = 0;

  // Push audio samples for VAD processing.
  virtual void Push(const float* samples, int n) = 0;

  // Drain completed VAD segments. When finalize is true, flush any open
  // speech segment at end of stream.
  virtual void DrainSegments(bool finalize,
                             std::vector<VadSegmentResult>* segments) = 0;

  // Whether the latest processed samples are classified as speech.
  virtual bool is_in_speech() const = 0;

  // Cumulative GPU compute time for this VAD instance.
  virtual double compute_sec() const = 0;
};

// A forced-alignment unit: a word/character and its time span (seconds, on the
// local clock of the aligned audio span).
struct AlignUnit {
  std::string text;
  double start_sec = 0.0;
  double end_sec = 0.0;
};

// Forced alignment: aligns a known transcript to its audio, producing precise
// per-unit timestamps in a single non-autoregressive pass. Generalizes any
// forced aligner; the pipeline depends only on this contract.
class IForcedAligner {
 public:
  virtual ~IForcedAligner() = default;
  // Load weights from a model directory.
  virtual void LoadWeights(const std::string& path) = 0;
  // Align `transcript` to `pcm` (mono 16 kHz, `n` samples). `language` is a
  // full name (e.g. "Chinese") or empty. Times are seconds relative to pcm
  // start.
  virtual std::vector<AlignUnit> Align(const float* pcm, int n,
                                       const std::string& transcript,
                                       const std::string& language) const = 0;
  virtual std::string name() const = 0;
};

}  // namespace core
}  // namespace orator
