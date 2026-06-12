#pragma once

// Stage interfaces: the decoupling boundary of the system.
//
// The pipeline depends ONLY on these abstract interfaces, never on concrete
// model implementations. Concrete models (Sortformer, stubs, future encoders)
// are constructed through the registry (core/registry.h) and injected into the
// pipeline. This is what makes "模型解耦" (model decoupling) real: swapping a
// model is a config/registration change, not a code change in consumers.

#include <string>
#include <vector>

#include "core/types.h"

namespace orator {
namespace core {

// Configuration handed to a diarizer at initialization. Fields are intentionally
// model-agnostic; model-specific knobs live inside each implementation.
struct DiarizationConfig {
  int sample_rate = 16000;
  int max_speakers = 4;
  float activity_threshold = 0.5f;  // prob >= threshold => speaker active
};

// Streaming speaker diarization model.
// Generalizes Streaming Sortformer; any frame-level diarizer fits this contract.
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

  virtual int max_speakers() const = 0;
  virtual double frame_period_sec() const = 0;
  virtual std::string name() const = 0;
};

// Extracts a fixed-dimension speaker embedding from an audio span.
class ISpeakerEmbedder {
 public:
  virtual ~ISpeakerEmbedder() = default;
  virtual void LoadWeights(const std::string& path) = 0;
  virtual int dim() const = 0;
  virtual std::vector<float> Embed(const AudioChunk& chunk) = 0;
  virtual std::string name() const = 0;
};

// Persistent registry of enrolled speakers, supporting 1:N matching.
class ISpeakerRegistry {
 public:
  virtual ~ISpeakerRegistry() = default;
  virtual bool Enroll(const std::string& speaker_id, const float* embedding) = 0;
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
// Transcript it returns is the input the ITimelineMerger fuses with
// diarization, so ASR and diarization meet on a single timeline.
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

  virtual std::string name() const = 0;
};

// Fuses diarization segments with the ASR transcript into a labeled timeline.
class ITimelineMerger {
 public:
  virtual ~ITimelineMerger() = default;
  virtual Timeline Merge(const std::vector<DiarSegment>& diarization,
                         const Transcript& transcript) const = 0;
};

// Terminal consumer of a timeline (e.g. JSON for an LLM, a socket, a file).
class ISink {
 public:
  virtual ~ISink() = default;
  virtual void Consume(const Timeline& timeline) = 0;
};

}  // namespace core
}  // namespace orator
