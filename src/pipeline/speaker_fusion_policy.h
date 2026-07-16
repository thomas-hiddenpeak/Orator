#pragma once

// Internal owner of accepted speaker-fusion policy. It reads an immutable view
// of BusinessSpeakerPipeline state and returns projected entries without
// subscribing to or publishing timeline tracks.

#include <vector>

#include "pipeline/business_speaker_pipeline.h"

namespace orator {
namespace pipeline {

class SpeakerFusionPolicy {
 public:
  static std::vector<BusinessSpeakerPipeline::Entry> Apply(
      const BusinessSpeakerPipeline& pipeline,
      const BusinessSpeakerPipeline::TextSeg& text,
      std::vector<BusinessSpeakerPipeline::Entry> entries);
};

}  // namespace pipeline
}  // namespace orator
