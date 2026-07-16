#pragma once

#include <utility>
#include <vector>

#include "pipeline/speaker_evidence_stage.h"

namespace orator {
namespace pipeline {

class TestSpeakerEvidenceStage {
 public:
  static std::vector<std::pair<int, int>> SplitPartialPhraseEdges(
      int source_start, int source_end,
      const std::vector<std::pair<int, int>>& phrase_ranges) {
    return SpeakerEvidenceStage::SplitPartialPhraseEdges(
        source_start, source_end, phrase_ranges);
  }

  static std::vector<ComprehensiveTimeline::SpeakerVoiceprintEvidence>
  BuildAdjacentBusinessPairs(
      const std::vector<ComprehensiveTimeline::SpeakerVoiceprintEvidence>&
          intervals,
      double min_embed_sec, double short_max_sec) {
    return SpeakerEvidenceStage::BuildAdjacentBusinessPairs(
        intervals, min_embed_sec, short_max_sec);
  }
};

}  // namespace pipeline
}  // namespace orator
