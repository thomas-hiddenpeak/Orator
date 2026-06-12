#pragma once

// AuditoryPipeline: the orchestrator that wires decoupled stages together.
//
// It owns interface pointers only (IDiarizer, ISpeakerEmbedder,
// ISpeakerRegistry, ITimelineMerger). Concrete implementations are injected,
// typically constructed from the registries by name. The pipeline implements
// the project's design goals:
//   1) speaker diarization over a streaming audio source
//   2) speaker-id resolution against a registry
//   3) fusion of diarization with ASR onto a timeline
//   4) a timeline output consumable by a downstream LLM

#include <memory>
#include <string>
#include <vector>

#include "core/stages.h"

namespace orator {
namespace pipeline {

struct PipelineConfig {
  std::string diarizer = "stub";        // registry key
  std::string embedder = "";            // optional registry key ("" = disabled)
  std::string asr = "";                 // optional registry key ("" = disabled)
  std::string diarizer_weights = "";    // optional safetensors path
  std::string embedder_weights = "";
  std::string asr_weights = "";         // optional ASR model path
  std::string asr_language = "";        // optional ASR language hint

  int sample_rate = 16000;
  int max_speakers = 4;
  float diar_threshold = 0.5f;          // frame activity threshold
  float speaker_match_threshold = 0.5f; // registry cosine threshold
  double segment_merge_gap_sec = 0.32;  // bridge gaps when coalescing segments
};

// Components injected into the pipeline. Any subset may be provided; the
// pipeline degrades gracefully (e.g. no embedder => local speaker labels only).
struct PipelineComponents {
  std::unique_ptr<core::IDiarizer> diarizer;
  std::unique_ptr<core::ISpeakerEmbedder> embedder;       // optional
  std::shared_ptr<core::ISpeakerRegistry> registry;       // optional
  std::unique_ptr<core::IAsr> asr;                        // optional
  std::unique_ptr<core::ITimelineMerger> timeline_merger; // required
};

class AuditoryPipeline {
 public:
  AuditoryPipeline(PipelineConfig config, PipelineComponents components);

  // Build a pipeline from configuration using the global registries.
  // Requires model::EnsureBuiltinsRegistered() to have been called.
  static std::unique_ptr<AuditoryPipeline> FromConfig(const PipelineConfig& config,
                                                      std::shared_ptr<core::ISpeakerRegistry> registry);

  // Begin a new streaming session.
  void Start();

  // Feed one chunk of streaming audio. Diarization runs immediately and the
  // resulting segments are accumulated with absolute stream timing. When an
  // ASR component is configured, the chunk's samples are also buffered so the
  // session can be transcribed in Finalize().
  void ProcessAudio(const core::AudioChunk& chunk);

  // Fuse accumulated diarization with an externally-supplied ASR transcript
  // into a timeline.
  core::Timeline Finalize(const core::Transcript& transcript);

  // Run the configured ASR over the buffered session audio (if any), then fuse
  // its transcript with diarization. This is the unified path: a single call
  // yields a speaker-attributed, transcribed timeline. With no ASR component,
  // the timeline carries diarization-only segments (empty text).
  core::Timeline Finalize();

  const std::vector<core::DiarSegment>& diar_segments() const {
    return diar_segments_;
  }
  // The transcript produced by the most recent no-arg Finalize() (empty until
  // then). Useful for inspection/eval alongside the timeline.
  const core::Transcript& transcript() const { return transcript_; }
  const PipelineConfig& config() const { return config_; }

 private:
  // Resolve local diarization speakers to registry ids using the embedder.
  void ResolveSpeakerIds(const core::AudioChunk& chunk,
                         std::vector<core::DiarSegment>* segments);

  // Transcribe the buffered session audio with the configured ASR component.
  core::Transcript RunAsr();

  PipelineConfig config_;
  PipelineComponents components_;
  std::vector<core::DiarSegment> diar_segments_;

  // Session audio buffer, populated only when an ASR component is present.
  std::vector<float> audio_buffer_;
  int session_sample_rate_ = 16000;
  double session_t_start_sec_ = 0.0;
  bool have_session_start_ = false;
  core::Transcript transcript_;
};

}  // namespace pipeline
}  // namespace orator
