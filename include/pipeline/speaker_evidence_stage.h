#pragma once

// Final/revisable speaker evidence producer. It reads upstream producer tracks
// and the current base business projection from ComprehensiveTimeline, then
// reuses the diarization pipeline's retained audio and TitaNet gallery. Business
// intervals follow the projection's source partition. The stage emits acoustic
// evidence only; speaker selection remains BusinessSpeakerPipeline's
// responsibility.

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <set>
#include <string>
#include <thread>
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
    double boundary_tolerance_sec = 0.0;
    std::string punctuation = "，。？！；：、,.?!;:";
    float frame_activity_threshold = 0.5f;
    int minimum_gallery_size = 2;
    bool source_leading_primary_prefix_enabled = false;
    double precompute_interval_sec = 0.0;
    int precompute_max_spans_per_cycle = 1;
  };

  SpeakerEvidenceStage(SpeakerIdentityStage* identity, Config config);
  ~SpeakerEvidenceStage();

  SpeakerEvidenceStage(const SpeakerEvidenceStage&) = delete;
  SpeakerEvidenceStage& operator=(const SpeakerEvidenceStage&) = delete;

  void StartPrecompute(ComprehensiveTimeline* timeline,
                       std::function<bool()> ready);
  void StopPrecompute(bool drain);
  std::size_t precomputed_span_count() const;

  std::vector<ComprehensiveTimeline::SpeakerInput> BuildPrimarySpeaker(
      const ComprehensiveTimeline::TrackSnapshot& snapshot);
  std::vector<ComprehensiveTimeline::SpeakerVoiceprintEvidence> BuildVoiceprint(
      const ComprehensiveTimeline::SpeakerEvidenceSnapshot& snapshot);

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

  std::vector<ComprehensiveTimeline::SpeakerVoiceprintEvidence>
  BuildVoiceprintQueries(
      const ComprehensiveTimeline::SpeakerEvidenceSnapshot& snapshot) const;
  void Precompute(
      const ComprehensiveTimeline::SpeakerEvidenceSnapshot& snapshot,
      std::size_t max_spans);
  void PrecomputeLoop();

  SpeakerIdentityStage* identity_ = nullptr;
  Config config_;
  ComprehensiveTimeline* timeline_ = nullptr;
  std::function<bool()> precompute_ready_;
  std::thread precompute_thread_;
  mutable std::mutex precompute_mutex_;
  std::condition_variable precompute_cv_;
  bool precompute_stop_ = false;
  bool precompute_drain_ = false;
  std::set<std::pair<long, long>> precomputed_spans_;
};

}  // namespace pipeline
}  // namespace orator
