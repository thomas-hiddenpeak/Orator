#include "pipeline/business_speaker_pipeline.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <set>
#include <stdexcept>
#include <utility>

#include "business_speaker_utils.h"
#include "speaker_fusion_policy.h"

namespace orator {
namespace pipeline {
namespace {

using business_speaker_internal::CoveredDuration;
using business_speaker_internal::MaxGap;
using business_speaker_internal::MergeIntervals;
using business_speaker_internal::NearEqual;
using business_speaker_internal::Overlap;
using business_speaker_internal::Utf8Offsets;

std::optional<std::vector<std::string>> SliceSourceTextByAlignedUnits(
    const std::string& source,
    const std::vector<ComprehensiveTimeline::AlignUnitSeg>& units,
    const std::vector<std::size_t>& unit_turns, std::size_t turn_count) {
  if (units.empty() || units.size() != unit_turns.size() || turn_count == 0) {
    return std::nullopt;
  }

  std::vector<std::pair<std::size_t, std::size_t>> source_matches;
  source_matches.reserve(units.size());
  std::size_t source_cursor = 0;
  for (const auto& unit : units) {
    const auto offsets = Utf8Offsets(unit.text);
    if (offsets.size() <= 1) return std::nullopt;
    std::size_t match_start = std::string::npos;
    std::size_t match_end = source_cursor;
    for (std::size_t i = 0; i + 1 < offsets.size(); ++i) {
      const std::string codepoint =
          unit.text.substr(offsets[i], offsets[i + 1] - offsets[i]);
      const std::size_t found = source.find(codepoint, source_cursor);
      if (found == std::string::npos) return std::nullopt;
      if (match_start == std::string::npos) match_start = found;
      source_cursor = found + codepoint.size();
      match_end = source_cursor;
    }
    source_matches.push_back({match_start, match_end});
  }

  std::vector<std::string> slices(turn_count);
  std::size_t current_turn = unit_turns.front();
  if (current_turn >= turn_count) return std::nullopt;
  std::size_t slice_start = 0;
  for (std::size_t i = 1; i < units.size(); ++i) {
    const std::size_t next_turn = unit_turns[i];
    if (next_turn >= turn_count || next_turn < current_turn) {
      return std::nullopt;
    }
    if (next_turn == current_turn) continue;
    const std::size_t boundary = source_matches[i].first;
    slices[current_turn] += source.substr(slice_start, boundary - slice_start);
    slice_start = boundary;
    current_turn = next_turn;
  }
  slices[current_turn] += source.substr(slice_start);
  return slices;
}

bool EntriesEqual(const std::vector<ComprehensiveTimeline::Entry>& a,
                  const std::vector<ComprehensiveTimeline::Entry>& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (a[i].text_id != b[i].text_id || a[i].speaker != b[i].speaker ||
        a[i].speaker_id != b[i].speaker_id || a[i].text != b[i].text ||
        !NearEqual(a[i].start, b[i].start) || !NearEqual(a[i].end, b[i].end) ||
        !NearEqual(a[i].diar_overlap_sec, b[i].diar_overlap_sec) ||
        !NearEqual(a[i].diar_total_overlap_sec, b[i].diar_total_overlap_sec) ||
        !NearEqual(a[i].diar_coverage_ratio, b[i].diar_coverage_ratio) ||
        !NearEqual(a[i].diar_total_coverage_ratio,
                   b[i].diar_total_coverage_ratio) ||
        !NearEqual(a[i].diar_max_gap_sec, b[i].diar_max_gap_sec) ||
        a[i].diar_island_count != b[i].diar_island_count ||
        a[i].speaker_support != b[i].speaker_support ||
        a[i].speaker_uncertain != b[i].speaker_uncertain ||
        a[i].speaker_decision.speaker_source !=
            b[i].speaker_decision.speaker_source ||
        a[i].speaker_decision.text_projection_source !=
            b[i].speaker_decision.text_projection_source ||
        a[i].speaker_decision.reason != b[i].speaker_decision.reason ||
        !NearEqual(a[i].speaker_decision.overlap_margin_sec,
                   b[i].speaker_decision.overlap_margin_sec) ||
        !NearEqual(a[i].speaker_decision.confidence_margin,
                   b[i].speaker_decision.confidence_margin) ||
        a[i].speaker_decision.candidates.size() !=
            b[i].speaker_decision.candidates.size()) {
      return false;
    }
    for (std::size_t candidate = 0;
         candidate < a[i].speaker_decision.candidates.size(); ++candidate) {
      const auto& left = a[i].speaker_decision.candidates[candidate];
      const auto& right = b[i].speaker_decision.candidates[candidate];
      if (left.speaker != right.speaker ||
          left.speaker_id != right.speaker_id ||
          !NearEqual(left.overlap_sec, right.overlap_sec) ||
          !NearEqual(left.coverage_ratio, right.coverage_ratio) ||
          !NearEqual(left.confidence, right.confidence) ||
          left.island_count != right.island_count ||
          left.selected != right.selected) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace

BusinessSpeakerPipeline::BusinessSpeakerPipeline(
    ComprehensiveTimeline* timeline, Config config, core::TimeBase time_base,
    RevisionSink revision_sink)
    : timeline_(timeline),
      config_(config),
      time_base_(time_base),
      revision_sink_(std::move(revision_sink)) {
  if (timeline_ == nullptr) {
    throw std::invalid_argument(
        "BusinessSpeakerPipeline requires a ComprehensiveTimeline");
  }
  config_.align_snap_pause_sec = std::max(0.0, config_.align_snap_pause_sec);
  config_.align_boundary_split_tolerance_sec =
      std::max(0.0, config_.align_boundary_split_tolerance_sec);
  config_.speaker_support_min_coverage_ratio =
      std::clamp(config_.speaker_support_min_coverage_ratio, 0.0, 1.0);
  config_.speaker_support_max_gap_sec =
      std::max(0.0, config_.speaker_support_max_gap_sec);
  config_.speaker_support_max_islands =
      std::max(1, config_.speaker_support_max_islands);
  config_.voiceprint_short_max_sec =
      std::max(0.0, config_.voiceprint_short_max_sec);
  config_.voiceprint_short_min_margin =
      std::max(0.0f, config_.voiceprint_short_min_margin);
  config_.voiceprint_regular_min_margin =
      std::max(0.0f, config_.voiceprint_regular_min_margin);
  config_.voiceprint_primary_consensus_min_sec =
      std::max(0.0, config_.voiceprint_primary_consensus_min_sec);
  config_.voiceprint_phrase_max_sec = std::max(
      config_.voiceprint_short_max_sec, config_.voiceprint_phrase_max_sec);
  config_.voiceprint_four_view_min_aligned_units =
      std::max(1, config_.voiceprint_four_view_min_aligned_units);
  config_.voiceprint_future_epoch_lookahead_sec =
      std::max(0.0, config_.voiceprint_future_epoch_lookahead_sec);
}

BusinessSpeakerPipeline::~BusinessSpeakerPipeline() { Stop(); }

void BusinessSpeakerPipeline::Start() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (started_) return;
    started_ = true;
    evidence_subscription_id_ = timeline_->SubscribeEvidence(
        [this](const ComprehensiveTimeline::EvidenceUpdate& update) {
          OnEvidence(update);
        });
  }
  SynchronizeAll();
}

void BusinessSpeakerPipeline::Stop() {
  long subscription_id = 0;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!started_) return;
    started_ = false;
    subscription_id = evidence_subscription_id_;
    evidence_subscription_id_ = 0;
  }
  timeline_->UnsubscribeEvidence(subscription_id);
}

void BusinessSpeakerPipeline::Finalize(long total_samples) {
  std::lock_guard<std::mutex> lock(mutex_);
  processed_samples_ = std::max(processed_samples_, total_samples);
}

long BusinessSpeakerPipeline::processed_samples() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return processed_samples_;
}

void BusinessSpeakerPipeline::set_align_snap_pause_sec(double sec) {
  std::lock_guard<std::mutex> lock(mutex_);
  config_.align_snap_pause_sec = std::max(0.0, sec);
}

void BusinessSpeakerPipeline::set_align_boundary_split_tolerance_sec(
    double sec) {
  std::lock_guard<std::mutex> lock(mutex_);
  config_.align_boundary_split_tolerance_sec = std::max(0.0, sec);
}

void BusinessSpeakerPipeline::set_speaker_support_min_coverage_ratio(
    double ratio) {
  std::lock_guard<std::mutex> lock(mutex_);
  config_.speaker_support_min_coverage_ratio = std::clamp(ratio, 0.0, 1.0);
}

void BusinessSpeakerPipeline::set_speaker_support_max_gap_sec(double sec) {
  std::lock_guard<std::mutex> lock(mutex_);
  config_.speaker_support_max_gap_sec = std::max(0.0, sec);
}

void BusinessSpeakerPipeline::set_speaker_support_max_islands(int count) {
  std::lock_guard<std::mutex> lock(mutex_);
  config_.speaker_support_max_islands = std::max(1, count);
}

void BusinessSpeakerPipeline::set_gap_fill_enabled(bool enabled) {
  std::lock_guard<std::mutex> lock(mutex_);
  config_.gap_fill_enabled = enabled;
}

void BusinessSpeakerPipeline::ResetState() {
  speakers_.clear();
  primary_speakers_.clear();
  texts_.clear();
  align_.clear();
  voiceprint_.clear();
  voiceprint_vad_.clear();
  voiceprint_aligned_units_.clear();
  voiceprint_business_intervals_.clear();
  voiceprint_by_text_.clear();
  pieces_.clear();
  processed_samples_ = 0;
}

void BusinessSpeakerPipeline::SynchronizeAll() {
  const auto snapshot = timeline_->SnapshotTracks();
  std::vector<Revision> revisions;
  std::lock_guard<std::mutex> lock(mutex_);
  if (!started_) return;

  ResetState();
  speakers_.reserve(snapshot.diarization.size());
  for (const auto& segment : snapshot.diarization) {
    speakers_.push_back({segment.start, segment.end, segment.speaker,
                         segment.conf, segment.speaker_id});
  }
  primary_speakers_.reserve(snapshot.primary_speaker.size());
  for (const auto& segment : snapshot.primary_speaker) {
    primary_speakers_.push_back({segment.start, segment.end, segment.speaker,
                                 segment.conf, segment.speaker_id});
  }
  texts_.reserve(snapshot.asr.size());
  for (const auto& segment : snapshot.asr) {
    texts_.push_back({segment.id, segment.start, segment.end, segment.text});
  }
  for (const auto& group : snapshot.align) {
    align_[group.text_id] = group;
  }
  voiceprint_ = snapshot.speaker_voiceprint;
  ReindexSpeakerVoiceprint();
  for (const auto& text : texts_) ReprojectText(text, &revisions);
  PublishRevisions(revisions);
}

void BusinessSpeakerPipeline::OnEvidence(
    const ComprehensiveTimeline::EvidenceUpdate& update) {
  std::vector<Revision> revisions;
  std::lock_guard<std::mutex> lock(mutex_);
  if (!started_) return;

  switch (update.track) {
    case ComprehensiveTimeline::EvidenceTrack::kDiarization:
      ApplyDiarization(timeline_->SnapshotDiarization(), &revisions);
      break;
    case ComprehensiveTimeline::EvidenceTrack::kPrimarySpeaker:
      ApplyPrimarySpeaker(timeline_->SnapshotPrimarySpeaker(), &revisions);
      break;
    case ComprehensiveTimeline::EvidenceTrack::kAsrFinal: {
      const auto segment = timeline_->FindAsrFinal(update.record_id);
      if (segment) ApplyAsrFinal(*segment, &revisions);
      break;
    }
    case ComprehensiveTimeline::EvidenceTrack::kAlignment: {
      const auto group = timeline_->FindAlignment(update.record_id);
      if (group) ApplyAlignment(*group, &revisions);
      break;
    }
    case ComprehensiveTimeline::EvidenceTrack::kDiarFrames:
      break;
    case ComprehensiveTimeline::EvidenceTrack::kSpeakerVoiceprint:
      ApplySpeakerVoiceprint(timeline_->SnapshotSpeakerVoiceprint(),
                             &revisions);
      break;
    case ComprehensiveTimeline::EvidenceTrack::kReset:
      ResetState();
      break;
    case ComprehensiveTimeline::EvidenceTrack::kVad:
      break;
  }
  PublishRevisions(revisions);
}

void BusinessSpeakerPipeline::ApplyDiarization(
    const std::vector<SpeakerInput>& segments,
    std::vector<Revision>* revisions) {
  speakers_.clear();
  speakers_.reserve(segments.size());
  for (const auto& segment : segments) {
    speakers_.push_back({segment.start, segment.end, segment.speaker,
                         segment.conf, segment.speaker_id});
  }
  for (const auto& text : texts_) ReprojectText(text, revisions);
}

void BusinessSpeakerPipeline::ApplyPrimarySpeaker(
    const std::vector<SpeakerInput>& segments,
    std::vector<Revision>* revisions) {
  primary_speakers_.clear();
  primary_speakers_.reserve(segments.size());
  for (const auto& segment : segments) {
    primary_speakers_.push_back({segment.start, segment.end, segment.speaker,
                                 segment.conf, segment.speaker_id});
  }
  for (const auto& text : texts_) ReprojectText(text, revisions);
}

void BusinessSpeakerPipeline::ApplyAsrFinal(const RawTextSeg& segment,
                                            std::vector<Revision>* revisions) {
  const auto existing =
      std::find_if(texts_.begin(), texts_.end(),
                   [&](const TextSeg& text) { return text.id == segment.id; });
  if (existing != texts_.end()) return;

  TextSeg text{segment.id, segment.start, segment.end, segment.text};
  const auto pos = std::lower_bound(texts_.begin(), texts_.end(), text.start,
                                    [](const TextSeg& current, double start) {
                                      return current.start < start;
                                    });
  const auto inserted = texts_.insert(pos, std::move(text));
  ReprojectText(*inserted, revisions);
}

void BusinessSpeakerPipeline::ApplyAlignment(const AlignGroup& group,
                                             std::vector<Revision>* revisions) {
  align_[group.text_id] = group;
  const auto text = std::find_if(
      texts_.begin(), texts_.end(),
      [&](const TextSeg& current) { return current.id == group.text_id; });
  if (text != texts_.end()) ReprojectText(*text, revisions);
}

void BusinessSpeakerPipeline::ApplySpeakerVoiceprint(
    const std::vector<SpeakerVoiceprintEvidence>& evidence,
    std::vector<Revision>* revisions) {
  voiceprint_ = evidence;
  ReindexSpeakerVoiceprint();
  for (const auto& text : texts_) ReprojectText(text, revisions);
}

void BusinessSpeakerPipeline::ReindexSpeakerVoiceprint() {
  voiceprint_vad_.clear();
  voiceprint_aligned_units_.clear();
  voiceprint_business_intervals_.clear();
  voiceprint_by_text_.clear();
  for (const auto& item : voiceprint_) {
    if (item.kind == "vad") voiceprint_vad_.push_back(item);
    if (item.kind == "aligned_unit") {
      voiceprint_aligned_units_.push_back(item);
    }
    if (item.kind == "business_interval") {
      voiceprint_business_intervals_.push_back(item);
    }
    if (item.text_id >= 0) voiceprint_by_text_[item.text_id].push_back(item);
  }
}

void BusinessSpeakerPipeline::PublishRevisions(
    const std::vector<Revision>& revisions) {
  for (const auto& revision : revisions) {
    timeline_->DepositBusinessSpeakerRevision(revision);
    processed_samples_ =
        std::max(processed_samples_, time_base_.SampleAt(revision.dirty_end));
    if (revision_sink_) revision_sink_(revision);
  }
}

BusinessSpeakerPipeline::SpeakerAttr BusinessSpeakerPipeline::AttributeInterval(
    double start, double end) const {
  double best_overlap = 0.0;
  std::vector<const SpeakerSeg*> tied;
  std::vector<const SpeakerSeg*> active;
  for (const auto& segment : speakers_) {
    const double overlap = Overlap(start, end, segment.start, segment.end);
    if (overlap <= 0.0) continue;
    active.push_back(&segment);
    if (overlap > best_overlap + 1e-9) {
      best_overlap = overlap;
      tied.clear();
      tied.push_back(&segment);
    } else if (NearEqual(overlap, best_overlap)) {
      tied.push_back(&segment);
    }
  }
  if (tied.empty()) return {"unknown", "", ""};

  const SpeakerSeg* best = nullptr;
  auto prefer = [&](const SpeakerSeg* segment, const SpeakerSeg* current) {
    if (current == nullptr) return true;
    const double span = segment->end - segment->start;
    const double current_span = current->end - current->start;
    if (config_.speaker_overlap_tie_policy ==
        SpeakerOverlapTiePolicy::kHigherConfidence) {
      return segment->conf > current->conf ||
             (segment->conf == current->conf &&
              span < current_span - 1e-9);
    }
    return span < current_span - 1e-9 ||
           (NearEqual(span, current_span) &&
            segment->conf > current->conf);
  };
  if (best == nullptr) {
    for (const SpeakerSeg* segment : tied) {
      if (prefer(segment, best)) best = segment;
    }
  }

  std::string primary_reason;
  if (config_.speaker_overlap_tie_policy ==
      SpeakerOverlapTiePolicy::kPrimarySpeaker) {
    std::set<std::pair<std::string, std::string>> active_identities;
    for (const SpeakerSeg* segment : active) {
      active_identities.insert({segment->speaker, segment->speaker_id});
    }
    if (active_identities.size() > 1) {
      const SpeakerAttr primary = PrimaryAttribute(start, end);
      const double duration = end - start;
      std::vector<std::pair<double, double>> primary_intervals;
      bool primary_conflict = false;
      for (const auto& segment : primary_speakers_) {
        const double overlap = Overlap(start, end, segment.start, segment.end);
        if (overlap <= 0.0) continue;
        const bool id_match = !primary.speaker_id.empty() &&
                              segment.speaker_id == primary.speaker_id;
        const bool label_match = primary.speaker_id.empty() &&
                                 primary.speaker != "unknown" &&
                                 segment.speaker == primary.speaker;
        if (id_match || label_match) {
          primary_intervals.push_back(
              {std::max(start, segment.start), std::min(end, segment.end)});
        } else {
          primary_conflict = true;
        }
      }
      const bool primary_covers =
          !primary_conflict && primary.speaker != "unknown" &&
          CoveredDuration(MergeIntervals(std::move(primary_intervals))) +
                  1e-9 >=
              duration;
      std::vector<const SpeakerSeg*> matches;
      if (primary_covers) {
        for (const SpeakerSeg* segment : active) {
          const bool id_match = !primary.speaker_id.empty() &&
                                segment->speaker_id == primary.speaker_id;
          const bool label_match = primary.speaker_id.empty() &&
                                   segment->speaker == primary.speaker;
          if (id_match || label_match) matches.push_back(segment);
        }
      }
      if (!matches.empty()) {
        const auto identity = [](const SpeakerSeg* segment) {
          return std::make_pair(segment->speaker, segment->speaker_id);
        };
        const auto selected_identity = identity(matches.front());
        const bool one_identity = std::all_of(
            matches.begin(), matches.end(), [&](const SpeakerSeg* segment) {
              return identity(segment) == selected_identity;
            });
        if (one_identity) {
          const SpeakerSeg* primary_best = nullptr;
          for (const SpeakerSeg* segment : matches) {
            if (prefer(segment, primary_best)) primary_best = segment;
          }
          std::set<std::pair<std::string, std::string>> tied_identities;
          for (const SpeakerSeg* segment : tied) {
            tied_identities.insert(identity(segment));
          }
          const bool exact_tie =
              tied_identities.size() > 1 &&
              tied_identities.count(selected_identity) != 0;
          if (exact_tie || identity(best) != selected_identity) {
            best = primary_best;
            auto is_primary_only_boundary = [&](double boundary) {
              bool primary_boundary = false;
              for (const auto& segment : primary_speakers_) {
                if (NearEqual(segment.start, boundary) ||
                    NearEqual(segment.end, boundary)) {
                  primary_boundary = true;
                  break;
                }
              }
              if (!primary_boundary) return false;
              for (const auto& segment : speakers_) {
                if (NearEqual(segment.start, boundary) ||
                    NearEqual(segment.end, boundary)) {
                  return false;
                }
              }
              return true;
            };
            primary_reason =
                is_primary_only_boundary(start) ||
                        is_primary_only_boundary(end)
                    ? "primary_speaker_overlap_refinement"
                    : "primary_speaker_tie_break";
          }
        }
      }
    }
  }
  return {best->speaker, best->speaker_id, primary_reason};
}

BusinessSpeakerPipeline::SpeakerAttr BusinessSpeakerPipeline::PrimaryAttribute(
    double start, double end) const {
  double best_overlap = 0.0;
  const SpeakerSeg* best = nullptr;
  bool ambiguous = false;
  for (const auto& segment : primary_speakers_) {
    const double overlap = Overlap(start, end, segment.start, segment.end);
    if (overlap > best_overlap + 1e-9) {
      best_overlap = overlap;
      best = &segment;
      ambiguous = false;
    } else if (overlap > 0.0 && NearEqual(overlap, best_overlap) &&
               best != nullptr &&
               (segment.speaker != best->speaker ||
                segment.speaker_id != best->speaker_id)) {
      ambiguous = true;
    }
  }
  if (best == nullptr || ambiguous) return {"unknown", "", ""};
  return {best->speaker, best->speaker_id, ""};
}

bool BusinessSpeakerPipeline::PrimaryTieSelected(
    double start, double end, const std::string& speaker,
    const std::string& speaker_id) const {
  if (config_.speaker_overlap_tie_policy !=
      SpeakerOverlapTiePolicy::kPrimarySpeaker) {
    return false;
  }
  double best_overlap = 0.0;
  std::vector<std::pair<std::string, std::string>> tied;
  for (const auto& segment : speakers_) {
    const double overlap = Overlap(start, end, segment.start, segment.end);
    if (overlap > best_overlap + 1e-9) {
      best_overlap = overlap;
      tied.clear();
      tied.push_back({segment.speaker, segment.speaker_id});
    } else if (overlap > 0.0 && NearEqual(overlap, best_overlap)) {
      tied.push_back({segment.speaker, segment.speaker_id});
    }
  }
  std::sort(tied.begin(), tied.end());
  tied.erase(std::unique(tied.begin(), tied.end()), tied.end());
  if (tied.size() < 2) return false;
  const SpeakerAttr primary = PrimaryAttribute(start, end);
  return primary.speaker == speaker && primary.speaker_id == speaker_id;
}

BusinessSpeakerPipeline::SpeakerSupport
BusinessSpeakerPipeline::ComputeSpeakerSupport(
    double start, double end, const std::string& speaker,
    const std::string& speaker_id) const {
  SpeakerSupport support;
  const double duration = std::max(0.0, end - start);
  if (duration <= 1e-9) return support;

  std::vector<std::pair<double, double>> selected;
  std::vector<std::pair<double, double>> any_speaker;
  selected.reserve(speakers_.size());
  any_speaker.reserve(speakers_.size());
  for (const auto& segment : speakers_) {
    const double overlap = Overlap(start, end, segment.start, segment.end);
    if (overlap <= 0.0) continue;
    any_speaker.push_back(
        {std::max(start, segment.start), std::min(end, segment.end)});
    const bool same_label = speaker != "unknown" && segment.speaker == speaker;
    const bool same_id = speaker_id.empty() || segment.speaker_id == speaker_id;
    if (same_label && same_id) {
      selected.push_back(
          {std::max(start, segment.start), std::min(end, segment.end)});
    }
  }

  const auto selected_merged = MergeIntervals(std::move(selected));
  const auto any_merged = MergeIntervals(std::move(any_speaker));
  support.overlap_sec = CoveredDuration(selected_merged);
  support.total_overlap_sec = CoveredDuration(any_merged);
  support.coverage_ratio = std::min(1.0, support.overlap_sec / duration);
  support.total_coverage_ratio =
      std::min(1.0, support.total_overlap_sec / duration);
  support.max_gap_sec = MaxGap(start, end, selected_merged);
  support.island_count = static_cast<int>(selected_merged.size());

  if (speaker == "unknown" || support.overlap_sec <= 1e-9) {
    support.level = "none";
  } else if (support.coverage_ratio + 1e-9 <
                 config_.speaker_support_min_coverage_ratio ||
             support.max_gap_sec > config_.speaker_support_max_gap_sec + 1e-9 ||
             support.island_count > config_.speaker_support_max_islands) {
    support.level = "weak";
  } else {
    support.level = "strong";
  }
  return support;
}

BusinessSpeakerPipeline::SpeakerDecisionAudit
BusinessSpeakerPipeline::ComputeSpeakerDecision(
    double start, double end, const std::string& speaker,
    const std::string& speaker_id,
    const std::string& text_projection_source) const {
  struct CandidateAccumulator {
    std::vector<std::pair<double, double>> intervals;
    double confidence_weighted_sum = 0.0;
    double confidence_weight = 0.0;
  };

  using CandidateKey = std::pair<std::string, std::string>;
  std::map<CandidateKey, CandidateAccumulator> accumulated;
  for (const auto& segment : speakers_) {
    const double overlap = Overlap(start, end, segment.start, segment.end);
    if (overlap <= 0.0) continue;
    auto& candidate = accumulated[{segment.speaker, segment.speaker_id}];
    candidate.intervals.push_back(
        {std::max(start, segment.start), std::min(end, segment.end)});
    candidate.confidence_weighted_sum += segment.conf * overlap;
    candidate.confidence_weight += overlap;
  }

  SpeakerDecisionAudit decision;
  decision.text_projection_source = text_projection_source;
  const double duration = std::max(0.0, end - start);
  decision.candidates.reserve(accumulated.size());
  for (auto& [key, accumulator] : accumulated) {
    auto intervals = MergeIntervals(std::move(accumulator.intervals));
    ComprehensiveTimeline::SpeakerCandidateEvidence candidate;
    candidate.speaker = key.first;
    candidate.speaker_id = key.second;
    candidate.overlap_sec = CoveredDuration(intervals);
    candidate.coverage_ratio =
        duration > 1e-9 ? std::min(1.0, candidate.overlap_sec / duration) : 0.0;
    candidate.confidence = accumulator.confidence_weight > 1e-9
                               ? accumulator.confidence_weighted_sum /
                                     accumulator.confidence_weight
                               : 0.0;
    candidate.island_count = static_cast<int>(intervals.size());
    candidate.selected =
        candidate.speaker == speaker && candidate.speaker_id == speaker_id;
    decision.candidates.push_back(std::move(candidate));
  }

  std::sort(decision.candidates.begin(), decision.candidates.end(),
            [](const auto& left, const auto& right) {
              if (left.selected != right.selected) return left.selected;
              if (!NearEqual(left.overlap_sec, right.overlap_sec)) {
                return left.overlap_sec > right.overlap_sec;
              }
              if (!NearEqual(left.confidence, right.confidence)) {
                return left.confidence > right.confidence;
              }
              if (left.speaker != right.speaker) {
                return left.speaker < right.speaker;
              }
              return left.speaker_id < right.speaker_id;
            });

  const ComprehensiveTimeline::SpeakerCandidateEvidence* selected = nullptr;
  const ComprehensiveTimeline::SpeakerCandidateEvidence* best_rejected =
      nullptr;
  for (const auto& candidate : decision.candidates) {
    if (candidate.selected) {
      selected = &candidate;
      continue;
    }
    if (best_rejected == nullptr ||
        candidate.overlap_sec > best_rejected->overlap_sec + 1e-9 ||
        (NearEqual(candidate.overlap_sec, best_rejected->overlap_sec) &&
         candidate.confidence > best_rejected->confidence)) {
      best_rejected = &candidate;
    }
  }

  if (selected == nullptr) {
    decision.reason = "no_diar_support";
  } else if (best_rejected != nullptr) {
    decision.reason = PrimaryTieSelected(start, end, speaker, speaker_id)
                          ? "primary_speaker_tie_break"
                          : "competing_diar_interval_policy";
    if (decision.reason == "primary_speaker_tie_break") {
      decision.speaker_source = "sortformer_activity+primary_top1";
    }
    decision.overlap_margin_sec =
        selected->overlap_sec - best_rejected->overlap_sec;
    decision.confidence_margin =
        selected->confidence - best_rejected->confidence;
  } else if (config_.gap_fill_enabled && selected->island_count > 1 &&
             selected->coverage_ratio < 1.0 - 1e-9) {
    decision.reason = "same_speaker_gap_fill";
  } else {
    decision.reason = "sole_diar_support";
  }
  return decision;
}

BusinessSpeakerPipeline::Entry BusinessSpeakerPipeline::MakeEntry(
    double start, double end, const std::string& speaker,
    const std::string& speaker_id, std::string text, long text_id,
    const std::string& text_projection_source,
    const std::string& primary_reason) const {
  Entry entry;
  entry.start = start;
  entry.end = end;
  entry.speaker = speaker;
  entry.speaker_id = speaker_id;
  entry.text = std::move(text);
  entry.text_id = text_id;
  const SpeakerSupport support =
      ComputeSpeakerSupport(start, end, speaker, speaker_id);
  entry.diar_overlap_sec = support.overlap_sec;
  entry.diar_total_overlap_sec = support.total_overlap_sec;
  entry.diar_coverage_ratio = support.coverage_ratio;
  entry.diar_total_coverage_ratio = support.total_coverage_ratio;
  entry.diar_max_gap_sec = support.max_gap_sec;
  entry.diar_island_count = support.island_count;
  entry.speaker_support = support.level;
  entry.speaker_uncertain = entry.speaker_support != "strong";
  entry.speaker_decision = ComputeSpeakerDecision(
      start, end, speaker, speaker_id, text_projection_source);
  if (!primary_reason.empty()) {
    entry.speaker_decision.reason = primary_reason;
    entry.speaker_decision.speaker_source =
        "sortformer_activity+primary_top1";
  }
  return entry;
}

void BusinessSpeakerPipeline::MergeEntrySupport(Entry* destination,
                                                const Entry& source) const {
  if (destination == nullptr) return;
  destination->diar_overlap_sec += source.diar_overlap_sec;
  destination->diar_total_overlap_sec += source.diar_total_overlap_sec;
  const double duration = std::max(0.0, destination->end - destination->start);
  if (duration > 1e-9) {
    destination->diar_coverage_ratio =
        std::min(1.0, destination->diar_overlap_sec / duration);
    destination->diar_total_coverage_ratio =
        std::min(1.0, destination->diar_total_overlap_sec / duration);
  } else {
    destination->diar_coverage_ratio = 0.0;
    destination->diar_total_coverage_ratio = 0.0;
  }
  destination->diar_max_gap_sec =
      std::max(destination->diar_max_gap_sec, source.diar_max_gap_sec);
  destination->diar_island_count += source.diar_island_count;
  if (destination->speaker == "unknown" ||
      destination->diar_overlap_sec <= 1e-9) {
    destination->speaker_support = "none";
  } else if (destination->diar_coverage_ratio + 1e-9 <
                 config_.speaker_support_min_coverage_ratio ||
             destination->diar_max_gap_sec >
                 config_.speaker_support_max_gap_sec + 1e-9 ||
             destination->diar_island_count >
                 config_.speaker_support_max_islands) {
    destination->speaker_support = "weak";
  } else {
    destination->speaker_support = "strong";
  }
  destination->speaker_uncertain = destination->speaker_support != "strong";
}

std::vector<BusinessSpeakerPipeline::Entry>
BusinessSpeakerPipeline::SplitTextByDiar(const TextSeg& text) const {
  std::vector<Entry> entries = SplitTextByDiarBase(text);
  if (!config_.voiceprint_fusion_enabled || voiceprint_.empty()) return entries;
  return ApplyVoiceprintEvidence(text, std::move(entries));
}

std::vector<BusinessSpeakerPipeline::Entry>
BusinessSpeakerPipeline::SplitTextByDiarBase(const TextSeg& text) const {
  std::vector<double> bounds;
  std::vector<double> primary_refinement_bounds;
  std::vector<double> corroborated_alignment_transition_bounds;
  bounds.reserve(speakers_.size() * 2 + 2);
  bounds.push_back(text.start);
  bounds.push_back(text.end);
  for (const auto& speaker : speakers_) {
    if (speaker.start > text.start + 1e-9 && speaker.start < text.end - 1e-9) {
      bounds.push_back(speaker.start);
    }
    if (speaker.end > text.start + 1e-9 && speaker.end < text.end - 1e-9) {
      bounds.push_back(speaker.end);
    }
  }
  if (config_.speaker_overlap_tie_policy ==
      SpeakerOverlapTiePolicy::kPrimarySpeaker) {
    auto has_activity_overlap = [&](double boundary) {
      std::set<std::pair<std::string, std::string>> identities;
      for (const auto& speaker : speakers_) {
        if (speaker.start < boundary - 1e-9 &&
            speaker.end > boundary + 1e-9) {
          identities.insert({speaker.speaker, speaker.speaker_id});
        }
      }
      return identities.size() > 1;
    };
    for (const auto& primary : primary_speakers_) {
      if (primary.start > text.start + 1e-9 &&
          primary.start < text.end - 1e-9 &&
          has_activity_overlap(primary.start)) {
        bounds.push_back(primary.start);
        primary_refinement_bounds.push_back(primary.start);
      }
      if (primary.end > text.start + 1e-9 &&
          primary.end < text.end - 1e-9 &&
          has_activity_overlap(primary.end)) {
        bounds.push_back(primary.end);
        primary_refinement_bounds.push_back(primary.end);
      }
    }

    const double tolerance = config_.align_boundary_split_tolerance_sec;
    const double native_gap = config_.align_snap_pause_sec;
    const double minimum_run = config_.voiceprint_primary_consensus_min_sec;
    if (tolerance > 0.0 && native_gap > 0.0 && minimum_run > 0.0) {
      auto is_isolated_transition = [&](const auto& track,
                                        const SpeakerSeg& candidate) {
        if (candidate.speaker_id.empty() ||
            candidate.end - candidate.start + 1e-9 < minimum_run) {
          return false;
        }

        const SpeakerSeg* previous = nullptr;
        for (const auto& segment : track) {
          if (&segment == &candidate) continue;
          if (segment.start < candidate.start + 1e-9 &&
              segment.end > candidate.start + 1e-9) {
            return false;
          }
          if (segment.end <= candidate.start + 1e-9 &&
              (previous == nullptr || segment.end > previous->end)) {
            previous = &segment;
          }
        }
        if (previous == nullptr || previous->speaker_id.empty() ||
            previous->speaker_id == candidate.speaker_id ||
            candidate.start - previous->end + 1e-9 < native_gap) {
          return false;
        }

        const double protected_end = candidate.start + minimum_run;
        return std::none_of(track.begin(), track.end(),
                            [&](const SpeakerSeg& segment) {
                              return segment.speaker_id !=
                                         candidate.speaker_id &&
                                     !segment.speaker_id.empty() &&
                                     Overlap(candidate.start, protected_end,
                                             segment.start, segment.end) > 1e-9;
                            });
      };

      for (const auto& activity : speakers_) {
        if (activity.speaker_id.empty() ||
            activity.start <= text.start + 1e-9 ||
            activity.start >= text.end - 1e-9 ||
            !is_isolated_transition(speakers_, activity)) {
          continue;
        }
        const bool corroborated = std::any_of(
            primary_speakers_.begin(), primary_speakers_.end(),
            [&](const SpeakerSeg& primary) {
              return primary.speaker_id == activity.speaker_id &&
                     std::abs(primary.start - activity.start) <=
                         tolerance + 1e-9 &&
                     is_isolated_transition(primary_speakers_, primary);
            });
        if (corroborated) {
          corroborated_alignment_transition_bounds.push_back(activity.start);
        }
      }
      std::sort(corroborated_alignment_transition_bounds.begin(),
                corroborated_alignment_transition_bounds.end());
      corroborated_alignment_transition_bounds.erase(
          std::unique(corroborated_alignment_transition_bounds.begin(),
                      corroborated_alignment_transition_bounds.end(),
                      [](double left, double right) {
                        return NearEqual(left, right);
                      }),
          corroborated_alignment_transition_bounds.end());
    }
  }
  std::sort(bounds.begin(), bounds.end());
  bounds.erase(std::unique(bounds.begin(), bounds.end(),
                           [](double a, double b) { return NearEqual(a, b); }),
               bounds.end());

  struct Turn {
    double start = 0.0;
    double end = 0.0;
    std::string speaker;
    std::string speaker_id;
    std::string primary_reason;
  };
  std::vector<Turn> turns;
  turns.reserve(bounds.size());
  for (std::size_t i = 0; i + 1 < bounds.size(); ++i) {
    const double start = bounds[i];
    const double end = bounds[i + 1];
    if (end - start <= 1e-9) continue;
    const SpeakerAttr attribution = AttributeInterval(start, end);
    if (!turns.empty() && turns.back().speaker == attribution.speaker &&
        turns.back().speaker_id == attribution.speaker_id &&
        turns.back().primary_reason == attribution.primary_reason) {
      turns.back().end = end;
    } else {
      turns.push_back({start, end, attribution.speaker,
                       attribution.speaker_id,
                       attribution.primary_reason});
    }
  }

  if (config_.gap_fill_enabled) {
    for (auto& turn : turns) {
      if (turn.speaker != "unknown") continue;
      const SpeakerSeg* before = nullptr;
      const SpeakerSeg* after = nullptr;
      for (const auto& speaker : speakers_) {
        if (speaker.end <= turn.start + 1e-9 &&
            (before == nullptr || speaker.end > before->end)) {
          before = &speaker;
        }
        if (speaker.start >= turn.end - 1e-9 &&
            (after == nullptr || speaker.start < after->start)) {
          after = &speaker;
        }
      }
      if (before != nullptr && after != nullptr &&
          before->speaker == after->speaker &&
          before->speaker_id == after->speaker_id) {
        turn.speaker = before->speaker;
        turn.speaker_id = before->speaker_id;
      }
    }

    std::vector<Turn> merged;
    merged.reserve(turns.size());
    for (const auto& turn : turns) {
      if (!merged.empty() && merged.back().speaker == turn.speaker &&
          merged.back().speaker_id == turn.speaker_id &&
          merged.back().primary_reason == turn.primary_reason) {
        merged.back().end = turn.end;
      } else {
        merged.push_back(turn);
      }
    }
    turns.swap(merged);
  }

  if (turns.empty()) {
    return {MakeEntry(text.start, text.end, "unknown", "", text.text, text.id,
                      "asr_exact")};
  }
  if (turns.size() == 1) {
    return {MakeEntry(turns[0].start, turns[0].end, turns[0].speaker,
                      turns[0].speaker_id, text.text, text.id, "asr_exact",
                      turns[0].primary_reason)};
  }

  const auto alignment = align_.find(text.id);
  if (alignment != align_.end() && !alignment->second.units.empty()) {
    const auto& units = alignment->second.units;
    std::vector<std::size_t> unit_turns(units.size(), 0);
    auto boundary_near_gap = [&](double gap_start, double gap_end) {
      if (config_.align_boundary_split_tolerance_sec <= 0.0) return false;
      for (std::size_t i = 1; i < turns.size(); ++i) {
        const Turn& left = turns[i - 1];
        const Turn& right = turns[i];
        if (left.speaker == right.speaker &&
            left.speaker_id == right.speaker_id) {
          continue;
        }
        const double boundary = right.start;
        if (boundary >=
                gap_start - config_.align_boundary_split_tolerance_sec &&
            boundary <= gap_end + config_.align_boundary_split_tolerance_sec) {
          return true;
        }
      }
      return false;
    };
    auto next_unit_crosses_primary_boundary = [&](double current_end,
                                                   double next_end) {
      return std::any_of(
          primary_refinement_bounds.begin(), primary_refinement_bounds.end(),
          [&](double boundary) {
            return boundary > current_end + 1e-9 &&
                   boundary < next_end - 1e-9;
          });
    };
    auto unit_straddles_corroborated_transition = [&](double unit_start,
                                                       double unit_end) {
      return std::any_of(
          corroborated_alignment_transition_bounds.begin(),
          corroborated_alignment_transition_bounds.end(),
          [&](double boundary) {
            return boundary > unit_start + 1e-9 &&
                   boundary < unit_end - 1e-9;
          });
    };

    std::size_t i = 0;
    while (i < units.size()) {
      std::size_t end = i;
      if (config_.align_snap_pause_sec > 0.0) {
        while (end + 1 < units.size()) {
          const double gap = units[end + 1].start - units[end].end;
          if (gap > config_.align_snap_pause_sec ||
              unit_straddles_corroborated_transition(units[end].start,
                                                      units[end].end) ||
              boundary_near_gap(units[end].end, units[end + 1].start) ||
              next_unit_crosses_primary_boundary(units[end].end,
                                                  units[end + 1].end)) {
            break;
          }
          ++end;
        }
      }
      const double midpoint = 0.5 * (units[i].start + units[end].end);
      std::size_t turn_index = 0;
      while (turn_index + 1 < turns.size() &&
             midpoint >= turns[turn_index].end) {
        ++turn_index;
      }
      for (std::size_t unit = i; unit <= end; ++unit)
        unit_turns[unit] = turn_index;
      i = end + 1;
    }

    const auto slices = SliceSourceTextByAlignedUnits(text.text, units,
                                                      unit_turns, turns.size());
    if (slices) {
      std::vector<Entry> entries;
      entries.reserve(turns.size());
      for (std::size_t turn = 0; turn < turns.size(); ++turn) {
        if ((*slices)[turn].empty()) continue;
        entries.push_back(MakeEntry(turns[turn].start, turns[turn].end,
                                    turns[turn].speaker, turns[turn].speaker_id,
                                    (*slices)[turn], text.id,
                                    "forced_alignment",
                                    turns[turn].primary_reason));
      }
      if (!entries.empty()) return entries;
    }
  }

  const std::vector<std::size_t> offsets = Utf8Offsets(text.text);
  const int codepoints = static_cast<int>(offsets.size()) - 1;
  const double duration = text.end - text.start;
  std::vector<Entry> entries;
  entries.reserve(turns.size());
  int used = 0;
  for (std::size_t turn = 0; turn < turns.size(); ++turn) {
    int end;
    if (turn + 1 == turns.size() || duration <= 0.0) {
      end = codepoints;
    } else {
      const double fraction = (turns[turn].end - text.start) / duration;
      end = static_cast<int>(std::llround(fraction * codepoints));
      end = std::max(used, std::min(codepoints, end));
    }
    std::string slice =
        text.text.substr(offsets[used], offsets[end] - offsets[used]);
    entries.push_back(MakeEntry(turns[turn].start, turns[turn].end,
                                turns[turn].speaker, turns[turn].speaker_id,
                                std::move(slice), text.id, "asr_proportional",
                                turns[turn].primary_reason));
    used = end;
  }
  return entries;
}

std::vector<BusinessSpeakerPipeline::Entry>
BusinessSpeakerPipeline::ApplyVoiceprintEvidence(
    const TextSeg& text, std::vector<Entry> entries) const {
  return SpeakerFusionPolicy::Apply(*this, text, std::move(entries));
}

void BusinessSpeakerPipeline::ReprojectText(const TextSeg& text,
                                            std::vector<Revision>* revisions) {
  std::vector<Entry> entries = SplitTextByDiar(text);
  const auto current = pieces_.find(text.id);
  const bool changed =
      current == pieces_.end() || !EntriesEqual(current->second, entries);
  pieces_[text.id] = entries;
  if (changed && revisions != nullptr) {
    revisions->push_back({text.start, text.end, std::move(entries)});
  }
}

}  // namespace pipeline
}  // namespace orator
