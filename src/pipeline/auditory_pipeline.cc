#include "pipeline/auditory_pipeline.h"

#include <algorithm>
#include <stdexcept>
#include <unordered_map>

#include "core/registry.h"
#include "pipeline/diar_postprocess.h"
#include "pipeline/timeline_merger.h"

namespace orator {
namespace pipeline {

AuditoryPipeline::AuditoryPipeline(PipelineConfig config,
                                   PipelineComponents components)
    : config_(std::move(config)), components_(std::move(components)) {
  if (!components_.diarizer) {
    throw std::invalid_argument("pipeline requires a diarizer");
  }
  if (!components_.timeline_merger) {
    components_.timeline_merger = std::make_unique<OverlapTimelineMerger>();
  }
}

std::unique_ptr<AuditoryPipeline> AuditoryPipeline::FromConfig(
    const PipelineConfig& config,
    std::shared_ptr<core::ISpeakerRegistry> registry) {
  PipelineComponents components;

  components.diarizer =
      core::Registry<core::IDiarizer>::Instance().Create(config.diarizer);

  core::DiarizationConfig diar_cfg;
  diar_cfg.sample_rate = config.sample_rate;
  diar_cfg.max_speakers = config.max_speakers;
  diar_cfg.activity_threshold = config.diar_threshold;
  components.diarizer->Initialize(diar_cfg);
  if (!config.diarizer_weights.empty()) {
    components.diarizer->LoadWeights(config.diarizer_weights);
  }

  if (!config.embedder.empty()) {
    components.embedder =
        core::Registry<core::ISpeakerEmbedder>::Instance().Create(config.embedder);
    if (!config.embedder_weights.empty()) {
      components.embedder->LoadWeights(config.embedder_weights);
    }
  }

  components.registry = std::move(registry);
  components.timeline_merger = std::make_unique<OverlapTimelineMerger>();

  return std::make_unique<AuditoryPipeline>(config, std::move(components));
}

void AuditoryPipeline::Start() {
  diar_segments_.clear();
  components_.diarizer->Reset();
}

void AuditoryPipeline::ProcessAudio(const core::AudioChunk& chunk) {
  core::DiarizationFrames frames = components_.diarizer->ProcessChunk(chunk);

  std::vector<core::DiarSegment> segments =
      FramesToSegments(frames, config_.diar_threshold,
                       config_.segment_merge_gap_sec);

  if (components_.embedder && components_.registry) {
    ResolveSpeakerIds(chunk, &segments);
  }

  for (auto& seg : segments) {
    diar_segments_.push_back(std::move(seg));
  }
}

void AuditoryPipeline::ResolveSpeakerIds(
    const core::AudioChunk& chunk, std::vector<core::DiarSegment>* segments) {
  if (segments->empty()) return;

  // For MVP, embed the whole chunk once and resolve all of its local speakers
  // against the registry with the same descriptor. (A finer implementation
  // would slice the chunk per segment.)
  std::vector<float> emb = components_.embedder->Embed(chunk);
  if (static_cast<int>(emb.size()) != components_.registry->EmbeddingDim()) {
    return;  // dimension mismatch; leave local labels
  }

  float score = 0.0f;
  const int idx =
      components_.registry->Match(emb.data(), config_.speaker_match_threshold,
                                  &score);
  if (idx < 0) return;
  const std::string id = components_.registry->SpeakerIdAt(idx);
  for (auto& seg : *segments) {
    seg.speaker_id = id;
  }
}

core::Timeline AuditoryPipeline::Finalize(const core::Transcript& transcript) {
  std::vector<core::DiarSegment> coalesced =
      CoalesceSegments(diar_segments_, config_.segment_merge_gap_sec);
  return components_.timeline_merger->Merge(coalesced, transcript);
}

}  // namespace pipeline
}  // namespace orator
