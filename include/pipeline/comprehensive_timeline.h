#pragma once

// ComprehensiveTimeline is the session-scoped, thread-safe typed evidence
// store. Producer pipelines deposit their own records here on the common time
// base; downstream pipelines read or subscribe here. This class stores and
// indexes records only. It never chooses a speaker, splits text, fills evidence
// gaps, or changes producer content.

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace orator {
namespace pipeline {

class ComprehensiveTimeline {
 public:
  struct SpeakerCandidateEvidence {
    std::string speaker;
    std::string speaker_id;
    double overlap_sec = 0.0;
    double coverage_ratio = 0.0;
    double confidence = 0.0;
    int island_count = 0;
    bool selected = false;
  };

  struct SpeakerDecisionAudit {
    std::string speaker_source = "sortformer_diarization";
    std::string text_projection_source = "asr_exact";
    std::string reason = "no_diar_support";
    double overlap_margin_sec = 0.0;
    double confidence_margin = 0.0;
    std::vector<SpeakerCandidateEvidence> candidates;
  };

  struct Entry {
    double start = 0.0;
    double end = 0.0;
    std::string speaker;
    std::string speaker_id;
    std::string text;
    long text_id = -1;
    double diar_overlap_sec = 0.0;
    double diar_total_overlap_sec = 0.0;
    double diar_coverage_ratio = 0.0;
    double diar_total_coverage_ratio = 0.0;
    double diar_max_gap_sec = 0.0;
    int diar_island_count = 0;
    std::string speaker_support = "none";
    bool speaker_uncertain = true;
    SpeakerDecisionAudit speaker_decision;
  };

  // A business-speaker pipeline revision replaces all entries with the same
  // text_id. dirty_start/end bound the affected source text span.
  struct Revision {
    double dirty_start = 0.0;
    double dirty_end = 0.0;
    std::vector<Entry> entries;
  };

  struct RawTextSeg {
    long id = -1;
    double start = 0.0;
    double end = 0.0;
    std::string text;
  };

  struct SpeakerInput {
    double start = 0.0;
    double end = 0.0;
    std::string speaker;
    float conf = 0.0f;
    std::string speaker_id;
  };

  struct VadSeg {
    double start = 0.0;
    double end = 0.0;
  };

  struct VadEvidence {
    std::shared_ptr<const std::vector<VadSeg>> segments;
    double horizon = -1e9;
    bool in_speech = false;
    double state_observed_at = -1e9;
    double active_start = -1e9;
    double active_horizon = -1e9;
  };

  struct AlignUnitSeg {
    double start = 0.0;
    double end = 0.0;
    std::string text;
  };

  struct AlignGroup {
    long text_id = -1;
    double start = 0.0;
    double end = 0.0;
    std::vector<AlignUnitSeg> units;
  };

  // One incremental Sortformer output block on the session common clock.
  // local_speaker_offset disambiguates slots after an optional model reset.
  struct DiarFrameBlock {
    double start = 0.0;
    double frame_period_sec = 0.0;
    int num_frames = 0;
    int num_speakers = 0;
    int local_speaker_offset = 0;
    std::vector<float> probabilities;
  };

  struct VoiceprintScore {
    std::string speaker_id;
    float score = 0.0f;
  };

  // Immutable acoustic evidence for a source-character range. The producer
  // supplies two independent registry views; this store does not rank them or
  // choose a speaker.
  struct SpeakerVoiceprintEvidence {
    std::string evidence_id;
    std::string kind;
    long text_id = -1;
    int source_start = 0;
    int source_end = 0;
    double start = 0.0;
    double end = 0.0;
    bool embedding_available = false;
    bool robust_gallery_complete = false;
    std::vector<VoiceprintScore> session_scores;
    std::vector<VoiceprintScore> robust_scores;
  };

  enum class EvidenceTrack {
    kDiarization,
    kPrimarySpeaker,
    kAsrFinal,
    kVad,
    kAlignment,
    kDiarFrames,
    kSpeakerVoiceprint,
    kReset,
  };

  struct EvidenceUpdate {
    EvidenceTrack track = EvidenceTrack::kReset;
    long record_id = -1;
  };

  enum class DepositResult {
    kInserted,
    kUnchanged,
    kConflict,
    kInvalid,
  };

  using EvidenceSubscriber = std::function<void(const EvidenceUpdate&)>;
  using AsrFinalSubscriber = std::function<void(const RawTextSeg&)>;

  struct TrackSnapshot {
    std::vector<SpeakerInput> diarization;
    std::vector<SpeakerInput> primary_speaker;
    std::vector<RawTextSeg> asr;
    std::vector<VadSeg> vad;
    std::vector<AlignGroup> align;
    std::vector<DiarFrameBlock> diar_frames;
    std::vector<SpeakerVoiceprintEvidence> speaker_voiceprint;
    std::vector<Entry> business_speaker;
    std::map<std::string, std::string> speaker_label_ids;
    std::vector<std::string> speaker_ids;
  };

  // Reduced typed input for multi-resolution speaker evidence. The current
  // business projection defines business-interval query ranges. Raw diar frames
  // and previously derived voiceprint evidence are excluded so a long session
  // does not repeatedly copy unrelated data or feed evidence into itself.
  struct SpeakerEvidenceSnapshot {
    std::vector<SpeakerInput> diarization;
    std::vector<RawTextSeg> asr;
    std::vector<VadSeg> vad;
    std::vector<AlignGroup> align;
    std::vector<Entry> business_speaker;
  };

  // Producer API. ASR finals and alignment groups are append-once by text_id:
  // an identical repeat is idempotent and a conflicting repeat is rejected.
  void DepositDiarization(const std::vector<SpeakerInput>& segments);
  void DepositDiarizationSegment(const SpeakerInput& segment);
  void DepositPrimarySpeaker(const std::vector<SpeakerInput>& segments);
  DepositResult DepositAsrFinal(const RawTextSeg& segment);
  void DepositVad(const VadSeg& segment);
  void AdvanceVadHorizon(double horizon_sec);
  void UpdateVadState(bool in_speech, double observed_at_sec,
                      double active_start_sec = -1e9,
                      double active_horizon_sec = -1e9);
  DepositResult DepositAlignment(const AlignGroup& group);
  DepositResult DepositDiarFrameBlock(const DiarFrameBlock& block);
  void DepositSpeakerVoiceprint(
      const std::vector<SpeakerVoiceprintEvidence>& evidence);

  // Registered derived pipelines write only their own track. This operation
  // cannot mutate any producer track.
  void DepositBusinessSpeakerRevision(const Revision& revision);

  // Typed downstream API. Subscribers run after commit and outside the store
  // lock. EvidenceUpdate identifies the changed track/key; consumers read the
  // committed typed value back from this store.
  long SubscribeEvidence(EvidenceSubscriber subscriber);
  void UnsubscribeEvidence(long subscription_id);
  long SubscribeAsrFinals(AsrFinalSubscriber subscriber);
  void UnsubscribeAsrFinals(long subscription_id);

  TrackSnapshot SnapshotTracks() const;
  SpeakerEvidenceSnapshot SnapshotSpeakerEvidenceInputs() const;
  std::vector<SpeakerInput> SnapshotDiarization() const;
  std::vector<SpeakerInput> SnapshotPrimarySpeaker() const;
  std::vector<RawTextSeg> SnapshotRawTexts() const;
  std::vector<VadSeg> SnapshotVad() const;
  VadEvidence SnapshotVadEvidence() const;
  std::vector<AlignGroup> SnapshotAlign() const;
  std::vector<DiarFrameBlock> SnapshotDiarFrames() const;
  std::vector<SpeakerVoiceprintEvidence> SnapshotSpeakerVoiceprint() const;
  std::optional<RawTextSeg> FindAsrFinal(long text_id) const;
  std::optional<AlignGroup> FindAlignment(long text_id) const;

  // Compatibility accessor for the final business view. It returns stored
  // business_speaker records and performs no derivation.
  std::vector<Entry> Snapshot() const;
  std::map<std::string, std::string> SpeakerLabelIds() const;
  std::vector<std::string> AllSpeakerIds() const;

  void Clear();

 private:
  static bool SameText(const RawTextSeg& a, const RawTextSeg& b);
  static bool SameAlignment(const AlignGroup& a, const AlignGroup& b);
  static bool ValidSpan(double start, double end);

  std::vector<EvidenceSubscriber> CopyEvidenceSubscribersLocked() const;
  void DispatchEvidence(
      const EvidenceUpdate& update,
      const std::vector<EvidenceSubscriber>& subscribers) const;
  std::vector<Entry> BuildBusinessSnapshotLocked() const;
  std::map<std::string, std::string> BuildSpeakerLabelIdsLocked() const;

  std::vector<SpeakerInput> diarization_;
  std::vector<SpeakerInput> primary_speaker_;
  std::vector<RawTextSeg> asr_;
  std::vector<VadSeg> vad_;
  std::shared_ptr<const std::vector<VadSeg>> vad_snapshot_ =
      std::make_shared<const std::vector<VadSeg>>();
  double vad_horizon_sec_ = -1e9;
  bool vad_in_speech_ = false;
  double vad_state_observed_at_sec_ = -1e9;
  double vad_active_start_sec_ = -1e9;
  double vad_active_horizon_sec_ = -1e9;
  std::map<long, AlignGroup> align_;
  std::vector<DiarFrameBlock> diar_frames_;
  std::vector<SpeakerVoiceprintEvidence> speaker_voiceprint_;
  std::map<long, std::vector<Entry>> business_speaker_;
  std::set<std::string> seen_speaker_ids_;

  mutable std::mutex mutex_;
  long next_evidence_subscription_id_ = 1;
  std::map<long, EvidenceSubscriber> evidence_subscribers_;
  long next_asr_subscription_id_ = 1;
  std::map<long, AsrFinalSubscriber> asr_final_subscribers_;
};

}  // namespace pipeline
}  // namespace orator
