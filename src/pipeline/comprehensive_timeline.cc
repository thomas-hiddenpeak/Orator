#include "pipeline/comprehensive_timeline.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace orator {
namespace pipeline {
namespace {

bool NearEqual(double a, double b) { return std::abs(a - b) <= 1e-9; }

}  // namespace

bool ComprehensiveTimeline::ValidSpan(double start, double end) {
  return std::isfinite(start) && std::isfinite(end) && start >= 0.0 &&
         end > start;
}

bool ComprehensiveTimeline::SameText(const RawTextSeg& a, const RawTextSeg& b) {
  return a.id == b.id && NearEqual(a.start, b.start) &&
         NearEqual(a.end, b.end) && a.text == b.text;
}

bool ComprehensiveTimeline::SameAlignment(const AlignGroup& a,
                                          const AlignGroup& b) {
  if (a.text_id != b.text_id || !NearEqual(a.start, b.start) ||
      !NearEqual(a.end, b.end) || a.units.size() != b.units.size()) {
    return false;
  }
  for (std::size_t i = 0; i < a.units.size(); ++i) {
    if (!NearEqual(a.units[i].start, b.units[i].start) ||
        !NearEqual(a.units[i].end, b.units[i].end) ||
        a.units[i].text != b.units[i].text) {
      return false;
    }
  }
  return true;
}

std::vector<ComprehensiveTimeline::EvidenceSubscriber>
ComprehensiveTimeline::CopyEvidenceSubscribersLocked() const {
  std::vector<EvidenceSubscriber> subscribers;
  subscribers.reserve(evidence_subscribers_.size());
  for (const auto& [id, subscriber] : evidence_subscribers_) {
    (void)id;
    subscribers.push_back(subscriber);
  }
  return subscribers;
}

void ComprehensiveTimeline::DispatchEvidence(
    const EvidenceUpdate& update,
    const std::vector<EvidenceSubscriber>& subscribers) const {
  for (const auto& subscriber : subscribers) subscriber(update);
}

void ComprehensiveTimeline::DepositDiarization(
    const std::vector<SpeakerInput>& segments) {
  std::vector<EvidenceSubscriber> subscribers;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    diarization_ = segments;
    std::stable_sort(diarization_.begin(), diarization_.end(),
                     [](const SpeakerInput& a, const SpeakerInput& b) {
                       if (!NearEqual(a.start, b.start))
                         return a.start < b.start;
                       return a.end < b.end;
                     });
    for (const auto& segment : diarization_) {
      if (!segment.speaker_id.empty()) {
        seen_speaker_ids_.insert(segment.speaker_id);
      }
    }
    subscribers = CopyEvidenceSubscribersLocked();
  }
  DispatchEvidence({EvidenceTrack::kDiarization, -1}, subscribers);
}

void ComprehensiveTimeline::DepositDiarizationSegment(
    const SpeakerInput& segment) {
  if (!ValidSpan(segment.start, segment.end)) return;
  std::vector<EvidenceSubscriber> subscribers;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto pos = std::lower_bound(diarization_.begin(), diarization_.end(),
                                segment.start,
                                [](const SpeakerInput& current, double start) {
                                  return current.start < start;
                                });
    diarization_.insert(pos, segment);
    if (!segment.speaker_id.empty()) {
      seen_speaker_ids_.insert(segment.speaker_id);
    }
    subscribers = CopyEvidenceSubscribersLocked();
  }
  DispatchEvidence({EvidenceTrack::kDiarization, -1}, subscribers);
}

void ComprehensiveTimeline::DepositPrimarySpeaker(
    const std::vector<SpeakerInput>& segments) {
  std::vector<EvidenceSubscriber> subscribers;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    primary_speaker_.clear();
    primary_speaker_.reserve(segments.size());
    for (const auto& segment : segments) {
      if (ValidSpan(segment.start, segment.end)) {
        primary_speaker_.push_back(segment);
      }
    }
    std::stable_sort(primary_speaker_.begin(), primary_speaker_.end(),
                     [](const SpeakerInput& a, const SpeakerInput& b) {
                       if (!NearEqual(a.start, b.start)) {
                         return a.start < b.start;
                       }
                       return a.end < b.end;
                     });
    subscribers = CopyEvidenceSubscribersLocked();
  }
  DispatchEvidence({EvidenceTrack::kPrimarySpeaker, -1}, subscribers);
}

ComprehensiveTimeline::DepositResult ComprehensiveTimeline::DepositAsrFinal(
    const RawTextSeg& segment) {
  if (segment.id < 0 || !ValidSpan(segment.start, segment.end)) {
    return DepositResult::kInvalid;
  }

  std::vector<EvidenceSubscriber> evidence_subscribers;
  std::vector<AsrFinalSubscriber> asr_subscribers;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto existing = std::find_if(
        asr_.begin(), asr_.end(),
        [&](const RawTextSeg& current) { return current.id == segment.id; });
    if (existing != asr_.end()) {
      return SameText(*existing, segment) ? DepositResult::kUnchanged
                                          : DepositResult::kConflict;
    }

    const auto pos =
        std::lower_bound(asr_.begin(), asr_.end(), segment.start,
                         [](const RawTextSeg& current, double start) {
                           return current.start < start;
                         });
    asr_.insert(pos, segment);
    evidence_subscribers = CopyEvidenceSubscribersLocked();
    asr_subscribers.reserve(asr_final_subscribers_.size());
    for (const auto& [id, subscriber] : asr_final_subscribers_) {
      (void)id;
      asr_subscribers.push_back(subscriber);
    }
  }

  DispatchEvidence({EvidenceTrack::kAsrFinal, segment.id},
                   evidence_subscribers);
  for (const auto& subscriber : asr_subscribers) subscriber(segment);
  return DepositResult::kInserted;
}

void ComprehensiveTimeline::DepositVad(const VadSeg& segment) {
  if (!ValidSpan(segment.start, segment.end)) return;
  std::vector<EvidenceSubscriber> subscribers;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto duplicate =
        std::find_if(vad_.begin(), vad_.end(), [&](const VadSeg& current) {
          return NearEqual(current.start, segment.start) &&
                 NearEqual(current.end, segment.end);
        });
    if (duplicate != vad_.end()) return;
    const auto pos = std::lower_bound(vad_.begin(), vad_.end(), segment.start,
                                      [](const VadSeg& current, double start) {
                                        return current.start < start;
                                      });
    vad_.insert(pos, segment);
    vad_snapshot_ = std::make_shared<const std::vector<VadSeg>>(vad_);
    subscribers = CopyEvidenceSubscribersLocked();
  }
  DispatchEvidence({EvidenceTrack::kVad, -1}, subscribers);
}

void ComprehensiveTimeline::AdvanceVadHorizon(double horizon_sec) {
  if (!std::isfinite(horizon_sec)) return;
  std::lock_guard<std::mutex> lock(mutex_);
  vad_horizon_sec_ = std::max(vad_horizon_sec_, horizon_sec);
}

void ComprehensiveTimeline::UpdateVadState(bool in_speech,
                                           double observed_at_sec) {
  if (!std::isfinite(observed_at_sec)) return;
  std::lock_guard<std::mutex> lock(mutex_);
  if (observed_at_sec + 1e-9 < vad_state_observed_at_sec_) return;
  vad_in_speech_ = in_speech;
  vad_state_observed_at_sec_ = observed_at_sec;
}

ComprehensiveTimeline::DepositResult ComprehensiveTimeline::DepositAlignment(
    const AlignGroup& group) {
  if (group.text_id < 0 || !ValidSpan(group.start, group.end)) {
    return DepositResult::kInvalid;
  }
  for (const auto& unit : group.units) {
    if (!std::isfinite(unit.start) || !std::isfinite(unit.end) ||
        unit.start < 0.0 || unit.end + 1e-9 < unit.start ||
        unit.start + 1e-9 < group.start || unit.end > group.end + 1e-9) {
      return DepositResult::kInvalid;
    }
  }

  std::vector<EvidenceSubscriber> subscribers;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto existing = align_.find(group.text_id);
    if (existing != align_.end()) {
      return SameAlignment(existing->second, group) ? DepositResult::kUnchanged
                                                    : DepositResult::kConflict;
    }
    align_.emplace(group.text_id, group);
    subscribers = CopyEvidenceSubscribersLocked();
  }
  DispatchEvidence({EvidenceTrack::kAlignment, group.text_id}, subscribers);
  return DepositResult::kInserted;
}

ComprehensiveTimeline::DepositResult
ComprehensiveTimeline::DepositDiarFrameBlock(const DiarFrameBlock& block) {
  if (!std::isfinite(block.start) || block.start < 0.0 ||
      !std::isfinite(block.frame_period_sec) ||
      block.frame_period_sec <= 0.0 || block.num_frames <= 0 ||
      block.num_speakers <= 0 || block.local_speaker_offset < 0 ||
      block.probabilities.size() !=
          static_cast<std::size_t>(block.num_frames) * block.num_speakers) {
    return DepositResult::kInvalid;
  }

  std::vector<EvidenceSubscriber> subscribers;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!diar_frames_.empty()) {
      const DiarFrameBlock& previous = diar_frames_.back();
      const double previous_end =
          previous.start + previous.num_frames * previous.frame_period_sec;
      if (block.start + 1e-6 < previous_end) {
        return DepositResult::kConflict;
      }
    }
    diar_frames_.push_back(block);
    subscribers = CopyEvidenceSubscribersLocked();
  }
  DispatchEvidence({EvidenceTrack::kDiarFrames, -1}, subscribers);
  return DepositResult::kInserted;
}

void ComprehensiveTimeline::DepositSpeakerVoiceprint(
    const std::vector<SpeakerVoiceprintEvidence>& evidence) {
  std::vector<EvidenceSubscriber> subscribers;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    speaker_voiceprint_ = evidence;
    std::stable_sort(
        speaker_voiceprint_.begin(), speaker_voiceprint_.end(),
        [](const SpeakerVoiceprintEvidence& left,
           const SpeakerVoiceprintEvidence& right) {
          if (left.text_id != right.text_id) return left.text_id < right.text_id;
          if (left.source_start != right.source_start) {
            return left.source_start < right.source_start;
          }
          if (left.source_end != right.source_end) {
            return left.source_end < right.source_end;
          }
          return left.evidence_id < right.evidence_id;
        });
    subscribers = CopyEvidenceSubscribersLocked();
  }
  DispatchEvidence({EvidenceTrack::kSpeakerVoiceprint, -1}, subscribers);
}

void ComprehensiveTimeline::DepositBusinessSpeakerRevision(
    const Revision& revision) {
  if (revision.entries.empty()) return;
  const long text_id = revision.entries.front().text_id;
  if (text_id < 0) return;
  for (const auto& entry : revision.entries) {
    if (entry.text_id != text_id || !ValidSpan(entry.start, entry.end)) return;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  business_speaker_[text_id] = revision.entries;
}

long ComprehensiveTimeline::SubscribeEvidence(EvidenceSubscriber subscriber) {
  if (!subscriber) return 0;
  std::lock_guard<std::mutex> lock(mutex_);
  const long id = next_evidence_subscription_id_++;
  evidence_subscribers_.emplace(id, std::move(subscriber));
  return id;
}

void ComprehensiveTimeline::UnsubscribeEvidence(long subscription_id) {
  if (subscription_id <= 0) return;
  std::lock_guard<std::mutex> lock(mutex_);
  evidence_subscribers_.erase(subscription_id);
}

long ComprehensiveTimeline::SubscribeAsrFinals(AsrFinalSubscriber subscriber) {
  if (!subscriber) return 0;
  std::lock_guard<std::mutex> lock(mutex_);
  const long id = next_asr_subscription_id_++;
  asr_final_subscribers_.emplace(id, std::move(subscriber));
  return id;
}

void ComprehensiveTimeline::UnsubscribeAsrFinals(long subscription_id) {
  if (subscription_id <= 0) return;
  std::lock_guard<std::mutex> lock(mutex_);
  asr_final_subscribers_.erase(subscription_id);
}

std::vector<ComprehensiveTimeline::Entry>
ComprehensiveTimeline::BuildBusinessSnapshotLocked() const {
  std::vector<Entry> entries;
  for (const auto& [text_id, pieces] : business_speaker_) {
    (void)text_id;
    entries.insert(entries.end(), pieces.begin(), pieces.end());
  }
  std::stable_sort(entries.begin(), entries.end(),
                   [](const Entry& a, const Entry& b) {
                     if (!NearEqual(a.start, b.start)) return a.start < b.start;
                     return a.text_id < b.text_id;
                   });
  return entries;
}

std::map<std::string, std::string>
ComprehensiveTimeline::BuildSpeakerLabelIdsLocked() const {
  std::map<std::string, std::string> ids;
  for (const auto& segment : diarization_) {
    if (!segment.speaker_id.empty()) {
      ids[segment.speaker] = segment.speaker_id;
    }
  }
  return ids;
}

ComprehensiveTimeline::TrackSnapshot ComprehensiveTimeline::SnapshotTracks()
    const {
  std::lock_guard<std::mutex> lock(mutex_);
  TrackSnapshot snapshot;
  snapshot.diarization = diarization_;
  snapshot.primary_speaker = primary_speaker_;
  snapshot.asr = asr_;
  snapshot.vad = vad_;
  snapshot.align.reserve(align_.size());
  for (const auto& [text_id, group] : align_) {
    (void)text_id;
    snapshot.align.push_back(group);
  }
  std::stable_sort(snapshot.align.begin(), snapshot.align.end(),
                   [](const AlignGroup& a, const AlignGroup& b) {
                     return a.start < b.start;
                   });
  snapshot.diar_frames = diar_frames_;
  snapshot.speaker_voiceprint = speaker_voiceprint_;
  snapshot.business_speaker = BuildBusinessSnapshotLocked();
  snapshot.speaker_label_ids = BuildSpeakerLabelIdsLocked();
  snapshot.speaker_ids.assign(seen_speaker_ids_.begin(),
                              seen_speaker_ids_.end());
  return snapshot;
}

std::vector<ComprehensiveTimeline::SpeakerInput>
ComprehensiveTimeline::SnapshotDiarization() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return diarization_;
}

std::vector<ComprehensiveTimeline::SpeakerInput>
ComprehensiveTimeline::SnapshotPrimarySpeaker() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return primary_speaker_;
}

std::vector<ComprehensiveTimeline::RawTextSeg>
ComprehensiveTimeline::SnapshotRawTexts() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return asr_;
}

std::vector<ComprehensiveTimeline::VadSeg> ComprehensiveTimeline::SnapshotVad()
    const {
  std::lock_guard<std::mutex> lock(mutex_);
  return vad_;
}

ComprehensiveTimeline::VadEvidence ComprehensiveTimeline::SnapshotVadEvidence()
    const {
  std::lock_guard<std::mutex> lock(mutex_);
  return {vad_snapshot_, vad_horizon_sec_, vad_in_speech_,
          vad_state_observed_at_sec_};
}

std::vector<ComprehensiveTimeline::AlignGroup>
ComprehensiveTimeline::SnapshotAlign() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<AlignGroup> groups;
  groups.reserve(align_.size());
  for (const auto& [text_id, group] : align_) {
    (void)text_id;
    groups.push_back(group);
  }
  std::stable_sort(groups.begin(), groups.end(),
                   [](const AlignGroup& a, const AlignGroup& b) {
                     return a.start < b.start;
                   });
  return groups;
}

std::vector<ComprehensiveTimeline::DiarFrameBlock>
ComprehensiveTimeline::SnapshotDiarFrames() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return diar_frames_;
}

std::vector<ComprehensiveTimeline::SpeakerVoiceprintEvidence>
ComprehensiveTimeline::SnapshotSpeakerVoiceprint() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return speaker_voiceprint_;
}

std::optional<ComprehensiveTimeline::RawTextSeg>
ComprehensiveTimeline::FindAsrFinal(long text_id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto found = std::find_if(
      asr_.begin(), asr_.end(),
      [&](const RawTextSeg& segment) { return segment.id == text_id; });
  if (found == asr_.end()) return std::nullopt;
  return *found;
}

std::optional<ComprehensiveTimeline::AlignGroup>
ComprehensiveTimeline::FindAlignment(long text_id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto found = align_.find(text_id);
  if (found == align_.end()) return std::nullopt;
  return found->second;
}

std::vector<ComprehensiveTimeline::Entry> ComprehensiveTimeline::Snapshot()
    const {
  std::lock_guard<std::mutex> lock(mutex_);
  return BuildBusinessSnapshotLocked();
}

std::map<std::string, std::string> ComprehensiveTimeline::SpeakerLabelIds()
    const {
  std::lock_guard<std::mutex> lock(mutex_);
  return BuildSpeakerLabelIdsLocked();
}

std::vector<std::string> ComprehensiveTimeline::AllSpeakerIds() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return {seen_speaker_ids_.begin(), seen_speaker_ids_.end()};
}

void ComprehensiveTimeline::Clear() {
  std::vector<EvidenceSubscriber> subscribers;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    diarization_.clear();
    primary_speaker_.clear();
    asr_.clear();
    vad_.clear();
    vad_snapshot_ = std::make_shared<const std::vector<VadSeg>>();
    vad_horizon_sec_ = -1e9;
    vad_in_speech_ = false;
    vad_state_observed_at_sec_ = -1e9;
    align_.clear();
    diar_frames_.clear();
    speaker_voiceprint_.clear();
    business_speaker_.clear();
    seen_speaker_ids_.clear();
    subscribers = CopyEvidenceSubscribersLocked();
  }
  DispatchEvidence({EvidenceTrack::kReset, -1}, subscribers);
}

}  // namespace pipeline
}  // namespace orator
