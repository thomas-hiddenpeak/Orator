#pragma once

#include <utility>

#include "pipeline/business_speaker_pipeline.h"
#include "pipeline/comprehensive_timeline.h"

namespace orator {
namespace pipeline {

class TestBusinessSpeakerPipeline {
 public:
  using AlignUnitSeg = ComprehensiveTimeline::AlignUnitSeg;
  using Entry = ComprehensiveTimeline::Entry;
  using Revision = ComprehensiveTimeline::Revision;
  using SpeakerInput = ComprehensiveTimeline::SpeakerInput;
  using SpeakerVoiceprintEvidence =
      ComprehensiveTimeline::SpeakerVoiceprintEvidence;

  explicit TestBusinessSpeakerPipeline(
      BusinessSpeakerPipeline::Config config = {})
      : pipeline_(&timeline_, config,
                  core::TimeBase(16000), [this](const Revision& revision) {
                    revisions_.push_back(revision);
                  }) {
    pipeline_.Start();
  }

  std::vector<Revision> UpsertSpeaker(double start, double end,
                                      const std::string& speaker, float conf) {
    revisions_.clear();
    timeline_.DepositDiarizationSegment({start, end, speaker, conf, ""});
    return TakeRevisions();
  }

  std::vector<Revision> ReplaceSpeakers(
      const std::vector<SpeakerInput>& segments) {
    revisions_.clear();
    timeline_.DepositDiarization(segments);
    return TakeRevisions();
  }

  std::vector<Revision> ReplacePrimarySpeakers(
      const std::vector<SpeakerInput>& segments) {
    revisions_.clear();
    timeline_.DepositPrimarySpeaker(segments);
    return TakeRevisions();
  }

  std::vector<Revision> UpsertText(long id, double start, double end,
                                   const std::string& text) {
    revisions_.clear();
    timeline_.DepositAsrFinal({id, start, end, text});
    return TakeRevisions();
  }

  void AddVad(double start, double end) { timeline_.DepositVad({start, end}); }

  std::vector<Revision> UpsertAlign(long text_id, double start, double end,
                                    const std::vector<AlignUnitSeg>& units) {
    revisions_.clear();
    timeline_.DepositAlignment({text_id, start, end, units});
    return TakeRevisions();
  }

  std::vector<Revision> ReplaceVoiceprint(
      const std::vector<SpeakerVoiceprintEvidence>& evidence) {
    revisions_.clear();
    timeline_.DepositSpeakerVoiceprint(evidence);
    return TakeRevisions();
  }

  std::vector<Entry> Snapshot() const { return timeline_.Snapshot(); }

  void set_align_snap_pause_sec(double sec) {
    pipeline_.set_align_snap_pause_sec(sec);
  }
  void set_align_boundary_split_tolerance_sec(double sec) {
    pipeline_.set_align_boundary_split_tolerance_sec(sec);
  }
  void set_speaker_support_min_coverage_ratio(double ratio) {
    pipeline_.set_speaker_support_min_coverage_ratio(ratio);
  }
  void set_speaker_support_max_gap_sec(double sec) {
    pipeline_.set_speaker_support_max_gap_sec(sec);
  }
  void set_speaker_support_max_islands(int count) {
    pipeline_.set_speaker_support_max_islands(count);
  }
  void set_gap_fill_enabled(bool enabled) {
    pipeline_.set_gap_fill_enabled(enabled);
  }

  ComprehensiveTimeline& timeline() { return timeline_; }
  const ComprehensiveTimeline& timeline() const { return timeline_; }

 private:
  std::vector<Revision> TakeRevisions() {
    std::vector<Revision> result;
    result.swap(revisions_);
    return result;
  }

  ComprehensiveTimeline timeline_;
  BusinessSpeakerPipeline pipeline_;
  std::vector<Revision> revisions_;
};

}  // namespace pipeline
}  // namespace orator
