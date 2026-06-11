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
  std::string diarizer_weights = "";    // optional safetensors path
  std::string embedder_weights = "";

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
  // resulting segments are accumulated with absolute stream timing.
  void ProcessAudio(const core::AudioChunk& chunk);

  // Fuse accumulated diarization with the ASR transcript into a timeline.
  core::Timeline Finalize(const core::Transcript& transcript);

  const std::vector<core::DiarSegment>& diar_segments() const {
    return diar_segments_;
  }
  const PipelineConfig& config() const { return config_; }

 private:
  // Resolve local diarization speakers to registry ids using the embedder.
  void ResolveSpeakerIds(const core::AudioChunk& chunk,
                         std::vector<core::DiarSegment>* segments);

  PipelineConfig config_;
  PipelineComponents components_;
  std::vector<core::DiarSegment> diar_segments_;
};

}  // namespace pipeline
}  // namespace orator
