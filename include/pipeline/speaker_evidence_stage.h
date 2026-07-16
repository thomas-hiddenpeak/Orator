#pragma once

// Final/revisable speaker evidence producer. It reads immutable producer tracks
// from ComprehensiveTimeline snapshots and reuses the diarization pipeline's
// retained audio and TitaNet gallery. It emits acoustic evidence only; speaker
// selection remains the responsibility of BusinessSpeakerPipeline.

#include <string>
#include <utility>
#include <vector>

#include "pipeline/comprehensive_timeline.h"

namespace orator {
namespace pipeline {

class SpeakerIdentityStage;

class SpeakerEvidenceStage {
 public:
  struct Config {
    bool enabled = false;
    double min_embed_sec = 0.4;
    double edge_margin_sec = 0.0;
    double max_embed_window_sec = 3.0;
    double phrase_min_sec = 0.5;
    double phrase_max_sec = 3.0;
    double short_max_sec = 1.5;
    std::string punctuation = "，。？！；：、,.?!;:";
    float frame_activity_threshold = 0.5f;
    int minimum_gallery_size = 2;
  };

  SpeakerEvidenceStage(SpeakerIdentityStage* identity, Config config);

  std::vector<ComprehensiveTimeline::SpeakerInput> BuildPrimarySpeaker(
      const ComprehensiveTimeline::TrackSnapshot& snapshot);
  std::vector<ComprehensiveTimeline::SpeakerVoiceprintEvidence> BuildVoiceprint(
      const ComprehensiveTimeline::TrackSnapshot& snapshot);

 private:
  friend class TestSpeakerEvidenceStage;

  static std::vector<std::pair<int, int>> SplitPartialPhraseEdges(
      int source_start, int source_end,
      const std::vector<std::pair<int, int>>& phrase_ranges);
  static std::vector<ComprehensiveTimeline::SpeakerVoiceprintEvidence>
  BuildAdjacentBusinessPairs(
      const std::vector<ComprehensiveTimeline::SpeakerVoiceprintEvidence>&
          intervals,
      double min_embed_sec, double short_max_sec);

  SpeakerIdentityStage* identity_ = nullptr;
  Config config_;
};

}  // namespace pipeline
}  // namespace orator
