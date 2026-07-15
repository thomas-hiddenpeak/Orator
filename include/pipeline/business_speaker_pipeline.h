#pragma once

// BusinessSpeakerPipeline derives the user-facing "who said what when" track
// from immutable diarization, finalized ASR, and forced-alignment evidence in
// ComprehensiveTimeline. It owns all speaker choice, text projection, gap-fill,
// and support-diagnostic policy. Raw producer tracks remain unchanged.

#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "core/time_base.h"
#include "pipeline/comprehensive_timeline.h"

namespace orator {
namespace pipeline {

class BusinessSpeakerPipeline {
 public:
  struct Config {
    double align_snap_pause_sec = 0.25;
    double align_boundary_split_tolerance_sec = 0.08;
    double speaker_support_min_coverage_ratio = 0.50;
    double speaker_support_max_gap_sec = 1.00;
    int speaker_support_max_islands = 1;
    bool gap_fill_enabled = true;
  };

  using Revision = ComprehensiveTimeline::Revision;
  using RevisionSink = std::function<void(const Revision&)>;

  BusinessSpeakerPipeline(ComprehensiveTimeline* timeline, Config config,
                          core::TimeBase time_base, RevisionSink revision_sink);
  ~BusinessSpeakerPipeline();

  BusinessSpeakerPipeline(const BusinessSpeakerPipeline&) = delete;
  BusinessSpeakerPipeline& operator=(const BusinessSpeakerPipeline&) = delete;

  void Start();
  void Stop();
  void Finalize(long total_samples);
  long processed_samples() const;

  // Test/configuration hooks. Runtime configuration is supplied from TOML at
  // construction; these setters preserve focused policy tests.
  void set_align_snap_pause_sec(double sec);
  void set_align_boundary_split_tolerance_sec(double sec);
  void set_speaker_support_min_coverage_ratio(double ratio);
  void set_speaker_support_max_gap_sec(double sec);
  void set_speaker_support_max_islands(int count);
  void set_gap_fill_enabled(bool enabled);

 private:
  using AlignGroup = ComprehensiveTimeline::AlignGroup;
  using Entry = ComprehensiveTimeline::Entry;
  using RawTextSeg = ComprehensiveTimeline::RawTextSeg;
  using SpeakerDecisionAudit = ComprehensiveTimeline::SpeakerDecisionAudit;
  using SpeakerInput = ComprehensiveTimeline::SpeakerInput;

  struct SpeakerSeg {
    double start = 0.0;
    double end = 0.0;
    std::string speaker;
    float conf = 0.0f;
    std::string speaker_id;
  };

  struct SpeakerAttr {
    std::string speaker;
    std::string speaker_id;
  };

  struct SpeakerSupport {
    double overlap_sec = 0.0;
    double total_overlap_sec = 0.0;
    double coverage_ratio = 0.0;
    double total_coverage_ratio = 0.0;
    double max_gap_sec = 0.0;
    int island_count = 0;
    std::string level = "none";
  };

  struct TextSeg {
    long id = -1;
    double start = 0.0;
    double end = 0.0;
    std::string text;
  };

  void OnEvidence(const ComprehensiveTimeline::EvidenceUpdate& update);
  void SynchronizeAll();
  void ApplyDiarization(const std::vector<SpeakerInput>& segments,
                        std::vector<Revision>* revisions);
  void ApplyAsrFinal(const RawTextSeg& segment,
                     std::vector<Revision>* revisions);
  void ApplyAlignment(const AlignGroup& group,
                      std::vector<Revision>* revisions);
  void PublishRevisions(const std::vector<Revision>& revisions);
  void ResetState();

  SpeakerAttr AttributeInterval(double start, double end) const;
  SpeakerSupport ComputeSpeakerSupport(double start, double end,
                                       const std::string& speaker,
                                       const std::string& speaker_id) const;
  SpeakerDecisionAudit ComputeSpeakerDecision(
      double start, double end, const std::string& speaker,
      const std::string& speaker_id,
      const std::string& text_projection_source) const;
  Entry MakeEntry(double start, double end, const std::string& speaker,
                  const std::string& speaker_id, std::string text, long text_id,
                  const std::string& text_projection_source) const;
  void MergeEntrySupport(Entry* dst, const Entry& src) const;
  std::vector<Entry> SplitTextByDiar(const TextSeg& text) const;
  void ReprojectText(const TextSeg& text, std::vector<Revision>* revisions);

  ComprehensiveTimeline* timeline_ = nullptr;
  Config config_;
  core::TimeBase time_base_;
  RevisionSink revision_sink_;

  std::vector<SpeakerSeg> speakers_;
  std::vector<TextSeg> texts_;
  std::map<long, AlignGroup> align_;
  std::map<long, std::vector<Entry>> pieces_;

  mutable std::mutex mutex_;
  long evidence_subscription_id_ = 0;
  long processed_samples_ = 0;
  bool started_ = false;
};

}  // namespace pipeline
}  // namespace orator
