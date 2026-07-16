#include "pipeline/business_speaker_pipeline.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <set>
#include <stdexcept>
#include <utility>

namespace orator {
namespace pipeline {
namespace {

double Overlap(double a0, double a1, double b0, double b1) {
  return std::max(0.0, std::min(a1, b1) - std::max(a0, b0));
}

bool NearEqual(double a, double b) { return std::abs(a - b) <= 1e-9; }

std::vector<std::pair<double, double>> MergeIntervals(
    std::vector<std::pair<double, double>> intervals) {
  if (intervals.empty()) return {};
  std::sort(intervals.begin(), intervals.end());
  std::vector<std::pair<double, double>> merged;
  merged.reserve(intervals.size());
  for (const auto& interval : intervals) {
    if (interval.second <= interval.first + 1e-9) continue;
    if (!merged.empty() && interval.first <= merged.back().second + 1e-9) {
      merged.back().second = std::max(merged.back().second, interval.second);
    } else {
      merged.push_back(interval);
    }
  }
  return merged;
}

double CoveredDuration(
    const std::vector<std::pair<double, double>>& intervals) {
  double total = 0.0;
  for (const auto& interval : intervals) {
    total += interval.second - interval.first;
  }
  return total;
}

double MaxGap(double start, double end,
              const std::vector<std::pair<double, double>>& intervals) {
  if (end <= start + 1e-9) return 0.0;
  if (intervals.empty()) return end - start;
  double max_gap = std::max(0.0, intervals.front().first - start);
  for (std::size_t i = 1; i < intervals.size(); ++i) {
    max_gap = std::max(max_gap, intervals[i].first - intervals[i - 1].second);
  }
  return std::max(max_gap, end - intervals.back().second);
}

std::vector<std::size_t> Utf8Offsets(const std::string& text) {
  std::vector<std::size_t> offsets;
  offsets.reserve(text.size() + 1);
  for (std::size_t i = 0; i < text.size();) {
    offsets.push_back(i);
    const unsigned char byte = static_cast<unsigned char>(text[i]);
    int advance = 1;
    if (byte >= 0xF0) {
      advance = 4;
    } else if (byte >= 0xE0) {
      advance = 3;
    } else if (byte >= 0xC0) {
      advance = 2;
    }
    i += advance;
  }
  offsets.push_back(text.size());
  return offsets;
}

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
  for (const auto& text : texts_) ReprojectText(text, revisions);
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
  if (entries.empty() || text.text.empty()) return entries;

  std::string reconstructed;
  for (const auto& entry : entries) reconstructed += entry.text;
  if (reconstructed != text.text) return entries;

  const std::vector<std::size_t> offsets = Utf8Offsets(text.text);
  const int character_count = static_cast<int>(offsets.size()) - 1;
  struct CharacterLabel {
    std::string speaker;
    std::string speaker_id;
    std::string reason = "diarization_baseline";
    std::size_t owner = 0;
    bool voiceprint = false;
  };
  std::vector<CharacterLabel> labels;
  labels.reserve(character_count);
  for (std::size_t owner = 0; owner < entries.size(); ++owner) {
    const int count =
        static_cast<int>(Utf8Offsets(entries[owner].text).size()) - 1;
    for (int index = 0; index < count; ++index) {
      labels.push_back({entries[owner].speaker, entries[owner].speaker_id,
                        entries[owner].speaker_decision.reason, owner, false});
    }
  }
  if (static_cast<int>(labels.size()) != character_count) return entries;

  struct Selection {
    std::string speaker_id;
    float score = 0.0f;
    float margin = 0.0f;
    bool score_pass = false;
    bool margin_pass = false;
  };
  auto rank = [&](const std::vector<ComprehensiveTimeline::VoiceprintScore>&
                      scores,
                  double duration) -> std::optional<Selection> {
    if (scores.size() < 2) return std::nullopt;
    std::vector<ComprehensiveTimeline::VoiceprintScore> ranked = scores;
    std::sort(ranked.begin(), ranked.end(),
              [](const auto& left, const auto& right) {
                if (!NearEqual(left.score, right.score)) {
                  return left.score > right.score;
                }
                return left.speaker_id < right.speaker_id;
              });
    const bool short_span = duration < config_.voiceprint_short_max_sec;
    const float score_gate = short_span
                                 ? config_.voiceprint_short_min_score
                                 : config_.voiceprint_regular_min_score;
    const float margin_gate = short_span
                                  ? config_.voiceprint_short_min_margin
                                  : config_.voiceprint_regular_min_margin;
    const float margin = ranked[0].score - ranked[1].score;
    return Selection{ranked[0].speaker_id, ranked[0].score, margin,
                     ranked[0].score + 1e-9f >= score_gate,
                     margin + 1e-9f >= margin_gate};
  };
  auto select = [&](const std::vector<ComprehensiveTimeline::VoiceprintScore>&
                        scores,
                    double duration,
                    bool require_score) -> std::optional<Selection> {
    const auto selected = rank(scores, duration);
    if (!selected || (require_score && !selected->score_pass) ||
        !selected->margin_pass) {
      return std::nullopt;
    }
    return selected;
  };

  auto speaker_label = [&](const std::string& speaker_id) {
    for (const auto& segment : speakers_) {
      if (segment.speaker_id == speaker_id) return segment.speaker;
    }
    for (const auto& segment : primary_speakers_) {
      if (segment.speaker_id == speaker_id) return segment.speaker;
    }
    return std::string("speaker_voiceprint");
  };

  std::map<std::string, std::pair<double, std::string>> initial_identities;
  for (const auto& segment : speakers_) {
    if (segment.speaker.empty() || segment.speaker_id.empty()) continue;
    const auto found = initial_identities.find(segment.speaker);
    if (found == initial_identities.end() ||
        segment.start < found->second.first) {
      initial_identities[segment.speaker] =
          {segment.start, segment.speaker_id};
    }
  }

  std::vector<const SpeakerVoiceprintEvidence*> source_evidence;
  std::vector<const SpeakerVoiceprintEvidence*> evidence;
  for (const auto& item : voiceprint_) {
    if (item.text_id != text.id || item.source_start < 0 ||
        item.source_end > character_count ||
        item.source_end <= item.source_start) {
      continue;
    }
    source_evidence.push_back(&item);
    if (item.embedding_available) evidence.push_back(&item);
  }
  auto priority = [](const std::string& kind) {
    if (kind == "business_interval") return 0;
    if (kind == "aligned_unit") return 1;
    if (kind == "punctuation_phrase") return 2;
    if (kind == "complete_source") return 3;
    return 4;
  };
  std::stable_sort(evidence.begin(), evidence.end(),
                   [&](const auto* left, const auto* right) {
                     const int left_priority = priority(left->kind);
                     const int right_priority = priority(right->kind);
                     if (left_priority != right_priority) {
                       return left_priority < right_priority;
                     }
                     if (left->source_start != right->source_start) {
                       return left->source_start < right->source_start;
                     }
                     return left->source_end < right->source_end;
                   });

  auto has_conflicting_direct_anchor = [&](const auto& item,
                                           const std::string& selected) {
    for (int index = item.source_start; index < item.source_end; ++index) {
      if (labels[index].reason.find("voiceprint_direct_") == 0 &&
          !labels[index].speaker_id.empty() &&
          labels[index].speaker_id != selected) {
        return true;
      }
    }
    return false;
  };
  auto has_conflicting_primary_label = [&](const auto& item,
                                            const std::string& selected) {
    for (int index = item.source_start; index < item.source_end; ++index) {
      const bool primary_selected =
          labels[index].reason == "primary_speaker_tie_break" ||
          labels[index].reason == "primary_speaker_overlap_refinement";
      if (primary_selected && !labels[index].speaker_id.empty() &&
          labels[index].speaker_id != selected) {
        return true;
      }
    }
    return false;
  };
  auto identity_coverage = [&](const std::vector<SpeakerSeg>& segments,
                               double start, double end,
                               const std::string& speaker_id) {
    std::vector<std::pair<double, double>> intervals;
    for (const auto& segment : segments) {
      if (segment.speaker_id != speaker_id) continue;
      const double overlap = Overlap(start, end, segment.start, segment.end);
      if (overlap <= 0.0) continue;
      intervals.push_back(
          {std::max(start, segment.start), std::min(end, segment.end)});
    }
    return CoveredDuration(MergeIntervals(std::move(intervals)));
  };
  auto local_coverage = [&](double start, double end,
                            const std::string& local_speaker) {
    std::vector<std::pair<double, double>> intervals;
    for (const auto& segment : speakers_) {
      if (segment.speaker != local_speaker) continue;
      const double overlap = Overlap(start, end, segment.start, segment.end);
      if (overlap <= 0.0) continue;
      intervals.push_back(
          {std::max(start, segment.start), std::min(end, segment.end)});
    }
    return CoveredDuration(MergeIntervals(std::move(intervals)));
  };
  auto primary_activity_phrase_consensus = [&](const auto& item,
                                                const std::string& selected) {
    const double duration = item.end - item.start;
    if (duration + 1e-9 < config_.voiceprint_primary_consensus_min_sec) {
      return false;
    }
    const double primary_coverage =
        identity_coverage(primary_speakers_, item.start, item.end, selected);
    if (primary_coverage + 1e-9 < duration) return false;
    for (const auto& segment : primary_speakers_) {
      if (segment.speaker_id != selected &&
          Overlap(item.start, item.end, segment.start, segment.end) > 1e-9) {
        return false;
      }
    }
    const double activity_coverage =
        identity_coverage(speakers_, item.start, item.end, selected);
    return activity_coverage + 1e-9 >=
           config_.voiceprint_primary_consensus_min_sec;
  };
  auto native_views_preserve_current = [&](const auto& item,
                                            const std::string& selected) {
    const double duration = item.end - item.start;
    if ((item.kind != "business_interval" &&
         item.kind != "punctuation_phrase") ||
        duration + 1e-9 < config_.voiceprint_primary_consensus_min_sec ||
        duration + 1e-9 >= config_.voiceprint_short_max_sec) {
      return false;
    }

    int overlapping_vad_count = 0;
    bool has_containing_vad = false;
    const double tolerance = config_.align_boundary_split_tolerance_sec;
    for (const auto& evidence_item : voiceprint_) {
      if (evidence_item.kind != "vad" ||
          Overlap(item.start, item.end, evidence_item.start,
                  evidence_item.end) <= 1e-9) {
        continue;
      }
      ++overlapping_vad_count;
      if (evidence_item.start <= item.start + tolerance &&
          evidence_item.end + tolerance >= item.end) {
        has_containing_vad = true;
      }
    }
    if (overlapping_vad_count < 2 || has_containing_vad) return false;

    std::string current_identity;
    for (int index = item.source_start; index < item.source_end; ++index) {
      if (labels[index].speaker_id.empty()) return false;
      if (current_identity.empty()) {
        current_identity = labels[index].speaker_id;
      } else if (labels[index].speaker_id != current_identity) {
        return false;
      }
    }
    if (current_identity.empty() || current_identity == selected) return false;

    auto complete_uncontested_coverage = [&](const auto& segments) {
      if (identity_coverage(segments, item.start, item.end,
                            current_identity) +
              1e-9 <
          duration) {
        return false;
      }
      for (const auto& segment : segments) {
        if (segment.speaker_id != current_identity &&
            Overlap(item.start, item.end, segment.start, segment.end) > 1e-9) {
          return false;
        }
      }
      return true;
    };
    return complete_uncontested_coverage(speakers_) &&
           complete_uncontested_coverage(primary_speakers_);
  };
  auto apply = [&](const auto& item, const std::string& selected,
                   const std::string& reason) {
    const std::string label = speaker_label(selected);
    for (int index = item.source_start; index < item.source_end; ++index) {
      labels[index].speaker = label;
      labels[index].speaker_id = selected;
      labels[index].reason = reason;
      labels[index].voiceprint = true;
    }
  };
  auto initial_slot_phrase_challenge = [&](const auto& item)
      -> std::optional<std::string> {
    const double duration = item.end - item.start;
    if (item.kind != "punctuation_phrase" ||
        duration + 1e-9 < config_.voiceprint_short_max_sec ||
        duration > config_.voiceprint_phrase_max_sec + 1e-9 ||
        !item.robust_gallery_complete) {
      return std::nullopt;
    }

    std::string current_identity;
    for (int index = item.source_start; index < item.source_end; ++index) {
      if (labels[index].speaker_id.empty() ||
          labels[index].reason.rfind("primary_speaker_", 0) == 0) {
        return std::nullopt;
      }
      if (current_identity.empty()) {
        current_identity = labels[index].speaker_id;
      } else if (labels[index].speaker_id != current_identity) {
        return std::nullopt;
      }
    }

    const auto session = select(item.session_scores, duration, false);
    const auto robust = select(item.robust_scores, duration, false);
    if (!session || !robust ||
        session->speaker_id != robust->speaker_id ||
        session->speaker_id == current_identity) {
      return std::nullopt;
    }

    std::string covered_local;
    for (const auto& [local_speaker, initial] : initial_identities) {
      if (initial.second != session->speaker_id) continue;
      if (local_coverage(item.start, item.end, local_speaker) + 1e-9 <
          duration) {
        continue;
      }
      if (!covered_local.empty()) return std::nullopt;
      covered_local = local_speaker;
    }
    if (covered_local.empty()) return std::nullopt;

    for (const auto& segment : speakers_) {
      if (segment.speaker == covered_local) continue;
      if (Overlap(item.start, item.end, segment.start, segment.end) > 1e-9) {
        return std::nullopt;
      }
    }
    return session->speaker_id;
  };
  auto four_view_margin_challenge = [&](const auto& item)
      -> std::optional<std::string> {
    if (item.kind != "punctuation_phrase" ||
        !item.robust_gallery_complete) {
      return std::nullopt;
    }

    std::string current_identity;
    for (int index = item.source_start; index < item.source_end; ++index) {
      if (labels[index].speaker_id.empty() ||
          labels[index].reason.rfind("primary_speaker_", 0) == 0) {
        return std::nullopt;
      }
      if (current_identity.empty()) {
        current_identity = labels[index].speaker_id;
      } else if (labels[index].speaker_id != current_identity) {
        return std::nullopt;
      }
    }

    const auto alignment = align_.find(text.id);
    if (alignment == align_.end()) return std::nullopt;
    int aligned_unit_count = 0;
    for (const auto& unit : alignment->second.units) {
      if (unit.end <= unit.start || unit.start + 1e-9 < item.start ||
          unit.end > item.end + 1e-9) {
        continue;
      }
      ++aligned_unit_count;
    }
    if (aligned_unit_count < config_.voiceprint_four_view_min_aligned_units) {
      return std::nullopt;
    }

    const double tolerance = config_.align_boundary_split_tolerance_sec;
    const SpeakerSeg* covering_primary = nullptr;
    for (const auto& primary : primary_speakers_) {
      if (primary.start > item.start + tolerance ||
          primary.end + tolerance < item.end) {
        continue;
      }
      if (covering_primary != nullptr) return std::nullopt;
      covering_primary = &primary;
    }
    if (covering_primary == nullptr ||
        covering_primary->speaker_id != current_identity) {
      return std::nullopt;
    }

    const SpeakerVoiceprintEvidence* containing_vad = nullptr;
    for (const auto& candidate : voiceprint_) {
      if (candidate.kind != "vad" || !candidate.embedding_available ||
          !candidate.robust_gallery_complete ||
          candidate.start > item.start + tolerance ||
          candidate.end + tolerance < item.end) {
        continue;
      }
      if (containing_vad != nullptr) return std::nullopt;
      containing_vad = &candidate;
    }
    if (containing_vad == nullptr) return std::nullopt;

    const double phrase_duration = item.end - item.start;
    const double vad_duration = containing_vad->end - containing_vad->start;
    const std::array<std::optional<Selection>, 4> ranked = {
        rank(item.session_scores, phrase_duration),
        rank(item.robust_scores, phrase_duration),
        rank(containing_vad->session_scores, vad_duration),
        rank(containing_vad->robust_scores, vad_duration)};
    if (!ranked[0] || !ranked[1] || !ranked[2] || !ranked[3]) {
      return std::nullopt;
    }
    const std::string selected = ranked[0]->speaker_id;
    if (selected == current_identity || ranked[1]->speaker_id != selected ||
        ranked[2]->speaker_id != selected ||
        ranked[3]->speaker_id != selected) {
      return std::nullopt;
    }

    int eligible_count = 0;
    int margin_only_count = 0;
    for (const auto& view : ranked) {
      if (view->score_pass && view->margin_pass) {
        ++eligible_count;
      } else if (view->score_pass && !view->margin_pass) {
        ++margin_only_count;
      }
    }
    if (eligible_count != 3 || margin_only_count != 1) {
      return std::nullopt;
    }
    return selected;
  };
  auto short_initial_slot_vad_challenge = [&](const auto& item)
      -> std::optional<std::string> {
    const double duration = item.end - item.start;
    if (item.kind != "punctuation_phrase" ||
        duration + 1e-9 < config_.voiceprint_primary_consensus_min_sec ||
        duration + 1e-9 >= config_.voiceprint_short_max_sec ||
        !item.robust_gallery_complete) {
      return std::nullopt;
    }

    std::string current_identity;
    for (int index = item.source_start; index < item.source_end; ++index) {
      if (labels[index].speaker_id.empty() ||
          labels[index].reason.rfind("primary_speaker_", 0) != 0) {
        return std::nullopt;
      }
      if (current_identity.empty()) {
        current_identity = labels[index].speaker_id;
      } else if (labels[index].speaker_id != current_identity) {
        return std::nullopt;
      }
    }
    if (current_identity.empty()) return std::nullopt;

    const double tolerance = config_.align_boundary_split_tolerance_sec;
    const SpeakerSeg* covering_primary = nullptr;
    for (const auto& primary : primary_speakers_) {
      if (primary.start > item.start + tolerance ||
          primary.end + tolerance < item.end) {
        continue;
      }
      if (covering_primary != nullptr) return std::nullopt;
      covering_primary = &primary;
    }
    if (covering_primary == nullptr ||
        covering_primary->speaker_id != current_identity) {
      return std::nullopt;
    }

    std::set<std::string> current_slots;
    for (const auto& segment : speakers_) {
      if (segment.speaker_id == current_identity) {
        current_slots.insert(segment.speaker);
      }
    }
    std::string current_slot;
    for (const auto& local_slot : current_slots) {
      std::vector<std::pair<double, double>> intervals;
      for (const auto& segment : speakers_) {
        if (segment.speaker != local_slot ||
            segment.speaker_id != current_identity) {
          continue;
        }
        const double overlap =
            Overlap(item.start, item.end, segment.start, segment.end);
        if (overlap > 0.0) {
          intervals.push_back({std::max(item.start, segment.start),
                               std::min(item.end, segment.end)});
        }
      }
      if (CoveredDuration(MergeIntervals(std::move(intervals))) + 1e-9 <
          duration) {
        continue;
      }
      if (!current_slot.empty()) return std::nullopt;
      current_slot = local_slot;
    }
    const auto initial = initial_identities.find(current_slot);
    if (current_slot.empty() || initial == initial_identities.end() ||
        initial->second.second.empty() ||
        initial->second.second == current_identity) {
      return std::nullopt;
    }
    const std::string candidate_identity = initial->second.second;

    const auto alignment = align_.find(text.id);
    if (alignment == align_.end()) return std::nullopt;
    int aligned_unit_count = 0;
    for (const auto& unit : alignment->second.units) {
      if (unit.end > unit.start && unit.start + 1e-9 >= item.start &&
          unit.end <= item.end + 1e-9) {
        ++aligned_unit_count;
      }
    }
    if (aligned_unit_count < config_.voiceprint_four_view_min_aligned_units) {
      return std::nullopt;
    }

    const SpeakerVoiceprintEvidence* containing_vad = nullptr;
    for (const auto& candidate : voiceprint_) {
      if (candidate.kind != "vad" || !candidate.embedding_available ||
          !candidate.robust_gallery_complete ||
          candidate.start > item.start + tolerance ||
          candidate.end + tolerance < item.end) {
        continue;
      }
      if (containing_vad != nullptr) return std::nullopt;
      containing_vad = &candidate;
    }
    if (containing_vad == nullptr) return std::nullopt;

    const double vad_duration = containing_vad->end - containing_vad->start;
    const auto phrase_session = rank(item.session_scores, duration);
    const auto phrase_robust = rank(item.robust_scores, duration);
    const auto vad_session = rank(containing_vad->session_scores, vad_duration);
    const auto vad_robust = rank(containing_vad->robust_scores, vad_duration);
    if (!phrase_session || !phrase_robust || !vad_session || !vad_robust) {
      return std::nullopt;
    }
    const bool phrase_session_margin_only =
        phrase_session->speaker_id == current_identity &&
        phrase_session->score_pass && !phrase_session->margin_pass;
    const bool phrase_robust_margin_only =
        phrase_robust->speaker_id == candidate_identity &&
        phrase_robust->score_pass && !phrase_robust->margin_pass;
    const bool vad_agreement =
        vad_session->speaker_id == candidate_identity &&
        vad_session->score_pass && vad_session->margin_pass &&
        vad_robust->speaker_id == candidate_identity &&
        vad_robust->score_pass && vad_robust->margin_pass;
    if (!phrase_session_margin_only || !phrase_robust_margin_only ||
        !vad_agreement) {
      return std::nullopt;
    }
    return candidate_identity;
  };
  auto initial_slot_four_view_near_tie_challenge = [&](const auto& item)
      -> std::optional<std::string> {
    const double phrase_duration = item.end - item.start;
    if (item.kind != "punctuation_phrase" ||
        phrase_duration + 1e-9 <
            config_.voiceprint_primary_consensus_min_sec ||
        phrase_duration + 1e-9 >= config_.voiceprint_short_max_sec ||
        !item.embedding_available || !item.robust_gallery_complete) {
      return std::nullopt;
    }

    std::string current_identity;
    for (int index = item.source_start; index < item.source_end; ++index) {
      if (labels[index].speaker_id.empty() || labels[index].voiceprint) {
        return std::nullopt;
      }
      if (current_identity.empty()) {
        current_identity = labels[index].speaker_id;
      } else if (labels[index].speaker_id != current_identity) {
        return std::nullopt;
      }
    }

    std::set<std::string> covering_slots;
    for (const auto& segment : speakers_) {
      if (segment.speaker_id == current_identity &&
          local_coverage(item.start, item.end, segment.speaker) + 1e-9 >=
              phrase_duration) {
        covering_slots.insert(segment.speaker);
      }
    }
    if (covering_slots.size() != 1) return std::nullopt;
    const std::string current_slot = *covering_slots.begin();
    for (const auto& segment : speakers_) {
      if (Overlap(item.start, item.end, segment.start, segment.end) <= 1e-9) {
        continue;
      }
      if (segment.speaker != current_slot ||
          segment.speaker_id != current_identity) {
        return std::nullopt;
      }
    }
    const auto initial = initial_identities.find(current_slot);
    if (initial == initial_identities.end() || initial->second.second.empty() ||
        initial->second.second == current_identity) {
      return std::nullopt;
    }
    const std::string candidate_identity = initial->second.second;

    const SpeakerSeg* covering_primary = nullptr;
    int overlapping_primary_count = 0;
    for (const auto& primary : primary_speakers_) {
      if (Overlap(item.start, item.end, primary.start, primary.end) <= 1e-9) {
        continue;
      }
      ++overlapping_primary_count;
      if (primary.start <= item.start + 1e-9 &&
          primary.end + 1e-9 >= item.end) {
        covering_primary = &primary;
      }
    }
    if (overlapping_primary_count != 1 || covering_primary == nullptr ||
        covering_primary->speaker != current_slot ||
        covering_primary->speaker_id != current_identity) {
      return std::nullopt;
    }

    const double tolerance = config_.align_boundary_split_tolerance_sec;
    const SpeakerVoiceprintEvidence* containing_vad = nullptr;
    for (const auto& candidate : voiceprint_) {
      if (candidate.kind != "vad" || !candidate.embedding_available ||
          !candidate.robust_gallery_complete ||
          candidate.start > item.start + tolerance ||
          candidate.end + tolerance < item.end) {
        continue;
      }
      if (containing_vad != nullptr) return std::nullopt;
      containing_vad = &candidate;
    }
    if (containing_vad == nullptr) return std::nullopt;

    const auto alignment = align_.find(text.id);
    if (alignment == align_.end()) return std::nullopt;
    int aligned_unit_count = 0;
    for (const auto& unit : alignment->second.units) {
      if (unit.end > unit.start && unit.start + 1e-9 >= item.start &&
          unit.end <= item.end + 1e-9) {
        ++aligned_unit_count;
      }
    }
    if (aligned_unit_count < config_.voiceprint_four_view_min_aligned_units) {
      return std::nullopt;
    }

    struct RankedPair {
      Selection first;
      std::string second_id;
    };
    auto ranked_pair = [&](const auto& scores,
                           double duration) -> std::optional<RankedPair> {
      const auto selected = rank(scores, duration);
      if (!selected || scores.size() < 2) return std::nullopt;
      auto ordered = scores;
      std::sort(ordered.begin(), ordered.end(), [](const auto& left,
                                                   const auto& right) {
        if (!NearEqual(left.score, right.score)) {
          return left.score > right.score;
        }
        return left.speaker_id < right.speaker_id;
      });
      return RankedPair{*selected, ordered[1].speaker_id};
    };
    const double vad_duration = containing_vad->end - containing_vad->start;
    const std::array<std::optional<RankedPair>, 4> views = {
        ranked_pair(item.session_scores, phrase_duration),
        ranked_pair(item.robust_scores, phrase_duration),
        ranked_pair(containing_vad->session_scores, vad_duration),
        ranked_pair(containing_vad->robust_scores, vad_duration)};
    for (const auto& view : views) {
      if (!view || !view->first.score_pass || view->first.margin_pass) {
        return std::nullopt;
      }
    }

    std::string competitor_identity;
    int candidate_top_count = 0;
    for (const auto& view : views) {
      const bool candidate_first =
          view->first.speaker_id == candidate_identity;
      const bool candidate_second = view->second_id == candidate_identity;
      if (candidate_first == candidate_second) return std::nullopt;
      const std::string competitor =
          candidate_first ? view->second_id : view->first.speaker_id;
      if (competitor.empty() || competitor == candidate_identity ||
          competitor == current_identity) {
        return std::nullopt;
      }
      if (competitor_identity.empty()) {
        competitor_identity = competitor;
      } else if (competitor_identity != competitor) {
        return std::nullopt;
      }
      if (candidate_first) ++candidate_top_count;
    }
    if (candidate_top_count != 1) return std::nullopt;
    return candidate_identity;
  };
  auto cross_scale_symmetric_near_tie_challenge = [&](const auto& item)
      -> std::optional<std::string> {
    const double phrase_duration = item.end - item.start;
    if (item.kind != "punctuation_phrase" ||
        phrase_duration + 1e-9 <
            config_.voiceprint_primary_consensus_min_sec ||
        phrase_duration + 1e-9 >= config_.voiceprint_short_max_sec ||
        !item.embedding_available || !item.robust_gallery_complete) {
      return std::nullopt;
    }

    std::string current_identity;
    for (int index = item.source_start; index < item.source_end; ++index) {
      const bool direct_label =
          labels[index].reason == "voiceprint_direct_short" ||
          labels[index].reason == "voiceprint_direct_regular";
      if (labels[index].speaker_id.empty() || !labels[index].voiceprint ||
          !direct_label) {
        return std::nullopt;
      }
      if (current_identity.empty()) {
        current_identity = labels[index].speaker_id;
      } else if (labels[index].speaker_id != current_identity) {
        return std::nullopt;
      }
    }

    std::set<std::string> covering_slots;
    for (const auto& segment : speakers_) {
      if (segment.speaker_id == current_identity &&
          local_coverage(item.start, item.end, segment.speaker) + 1e-9 >=
              phrase_duration) {
        covering_slots.insert(segment.speaker);
      }
    }
    if (covering_slots.size() != 1) return std::nullopt;
    const std::string current_slot = *covering_slots.begin();
    for (const auto& segment : speakers_) {
      if (Overlap(item.start, item.end, segment.start, segment.end) <= 1e-9) {
        continue;
      }
      if (segment.speaker != current_slot ||
          segment.speaker_id != current_identity) {
        return std::nullopt;
      }
    }

    const SpeakerSeg* covering_primary = nullptr;
    int overlapping_primary_count = 0;
    for (const auto& primary : primary_speakers_) {
      if (Overlap(item.start, item.end, primary.start, primary.end) <= 1e-9) {
        continue;
      }
      ++overlapping_primary_count;
      if (primary.start <= item.start + 1e-9 &&
          primary.end + 1e-9 >= item.end) {
        covering_primary = &primary;
      }
    }
    if (overlapping_primary_count != 1 || covering_primary == nullptr ||
        covering_primary->speaker != current_slot ||
        covering_primary->speaker_id != current_identity) {
      return std::nullopt;
    }

    const double tolerance = config_.align_boundary_split_tolerance_sec;
    const SpeakerVoiceprintEvidence* containing_vad = nullptr;
    for (const auto& candidate : voiceprint_) {
      if (candidate.kind != "vad" || !candidate.embedding_available ||
          !candidate.robust_gallery_complete ||
          candidate.start > item.start + tolerance ||
          candidate.end + tolerance < item.end) {
        continue;
      }
      if (containing_vad != nullptr) return std::nullopt;
      containing_vad = &candidate;
    }
    if (containing_vad == nullptr) return std::nullopt;

    const auto alignment = align_.find(text.id);
    if (alignment == align_.end()) return std::nullopt;
    int aligned_unit_count = 0;
    for (const auto& unit : alignment->second.units) {
      if (unit.end > unit.start && unit.start + 1e-9 >= item.start &&
          unit.end <= item.end + 1e-9) {
        ++aligned_unit_count;
      }
    }
    if (aligned_unit_count < config_.voiceprint_four_view_min_aligned_units) {
      return std::nullopt;
    }

    struct RankedPair {
      Selection first;
      std::string second_id;
    };
    auto ranked_pair = [&](const auto& scores,
                           double duration) -> std::optional<RankedPair> {
      const auto selected = rank(scores, duration);
      if (!selected || scores.size() < 2) return std::nullopt;
      auto ordered = scores;
      std::sort(ordered.begin(), ordered.end(), [](const auto& left,
                                                   const auto& right) {
        if (!NearEqual(left.score, right.score)) {
          return left.score > right.score;
        }
        return left.speaker_id < right.speaker_id;
      });
      return RankedPair{*selected, ordered[1].speaker_id};
    };
    const double vad_duration = containing_vad->end - containing_vad->start;
    const auto phrase_session =
        ranked_pair(item.session_scores, phrase_duration);
    const auto phrase_robust =
        ranked_pair(item.robust_scores, phrase_duration);
    const auto vad_session =
        ranked_pair(containing_vad->session_scores, vad_duration);
    const auto vad_robust =
        ranked_pair(containing_vad->robust_scores, vad_duration);
    if (!phrase_session || !phrase_robust || !vad_session || !vad_robust) {
      return std::nullopt;
    }

    const std::string candidate_identity = phrase_session->first.speaker_id;
    if (candidate_identity.empty() || candidate_identity == current_identity ||
        phrase_robust->first.speaker_id != candidate_identity ||
        phrase_session->second_id != current_identity ||
        phrase_robust->second_id != current_identity ||
        vad_session->first.speaker_id != current_identity ||
        vad_robust->first.speaker_id != current_identity ||
        vad_session->second_id != candidate_identity ||
        vad_robust->second_id != candidate_identity) {
      return std::nullopt;
    }
    const bool phrase_abstains_on_margin =
        phrase_session->first.score_pass &&
        !phrase_session->first.margin_pass &&
        phrase_robust->first.score_pass &&
        !phrase_robust->first.margin_pass;
    const bool vad_abstains_on_score_and_margin =
        !vad_session->first.score_pass &&
        !vad_session->first.margin_pass &&
        !vad_robust->first.score_pass &&
        !vad_robust->first.margin_pass;
    if (!phrase_abstains_on_margin || !vad_abstains_on_score_and_margin) {
      return std::nullopt;
    }
    return candidate_identity;
  };
  auto exact_interval_primary_conflict_challenge = [&](const auto& item)
      -> std::optional<std::string> {
    const double interval_duration = item.end - item.start;
    if (item.kind != "business_interval" || !item.embedding_available ||
        !item.robust_gallery_complete ||
        interval_duration + 1e-9 <
            config_.voiceprint_primary_consensus_min_sec ||
        interval_duration + 1e-9 >= config_.voiceprint_short_max_sec) {
      return std::nullopt;
    }

    std::string current_identity;
    for (int index = item.source_start; index < item.source_end; ++index) {
      const bool primary_label =
          labels[index].reason == "primary_speaker_tie_break" ||
          labels[index].reason == "primary_speaker_overlap_refinement";
      if (labels[index].speaker_id.empty() || labels[index].voiceprint ||
          !primary_label) {
        return std::nullopt;
      }
      if (current_identity.empty()) {
        current_identity = labels[index].speaker_id;
      } else if (labels[index].speaker_id != current_identity) {
        return std::nullopt;
      }
    }

    const auto interval_session = rank(item.session_scores, interval_duration);
    const auto interval_robust = rank(item.robust_scores, interval_duration);
    if (!interval_session || !interval_robust ||
        interval_session->speaker_id.empty() ||
        interval_session->speaker_id == current_identity ||
        interval_robust->speaker_id != interval_session->speaker_id ||
        !interval_session->score_pass || !interval_session->margin_pass ||
        !interval_robust->score_pass || !interval_robust->margin_pass) {
      return std::nullopt;
    }
    const std::string candidate_identity = interval_session->speaker_id;

    using ActivityKey = std::pair<std::string, std::string>;
    std::map<ActivityKey, std::vector<std::pair<double, double>>>
        activity_intervals;
    for (const auto& segment : speakers_) {
      const double overlap =
          Overlap(item.start, item.end, segment.start, segment.end);
      if (overlap <= 1e-9) continue;
      activity_intervals[{segment.speaker, segment.speaker_id}].push_back(
          {std::max(item.start, segment.start),
           std::min(item.end, segment.end)});
    }
    if (activity_intervals.size() != 2) return std::nullopt;

    std::string current_slot;
    std::string competitor_identity;
    for (auto& [activity, intervals] : activity_intervals) {
      if (activity.second.empty() || activity.second == candidate_identity ||
          CoveredDuration(MergeIntervals(std::move(intervals))) + 1e-9 <
              interval_duration) {
        return std::nullopt;
      }
      if (activity.second == current_identity) {
        if (!current_slot.empty()) return std::nullopt;
        current_slot = activity.first;
      } else {
        if (!competitor_identity.empty()) return std::nullopt;
        competitor_identity = activity.second;
      }
    }
    if (current_slot.empty() || competitor_identity.empty() ||
        competitor_identity == current_identity) {
      return std::nullopt;
    }

    const SpeakerSeg* covering_primary = nullptr;
    for (const auto& primary : primary_speakers_) {
      if (Overlap(item.start, item.end, primary.start, primary.end) <= 1e-9) {
        continue;
      }
      if (covering_primary != nullptr || primary.speaker != current_slot ||
          primary.speaker_id != current_identity ||
          primary.start > item.start + 1e-9 ||
          primary.end + 1e-9 < item.end) {
        return std::nullopt;
      }
      covering_primary = &primary;
    }
    if (covering_primary == nullptr) return std::nullopt;

    const double tolerance = config_.align_boundary_split_tolerance_sec;
    const SpeakerVoiceprintEvidence* containing_vad = nullptr;
    for (const auto& evidence_item : voiceprint_) {
      if (evidence_item.kind != "vad" ||
          evidence_item.start > item.start + tolerance ||
          evidence_item.end + tolerance < item.end) {
        continue;
      }
      if (containing_vad != nullptr ||
          !evidence_item.embedding_available ||
          !evidence_item.robust_gallery_complete) {
        return std::nullopt;
      }
      containing_vad = &evidence_item;
    }
    if (containing_vad == nullptr) return std::nullopt;

    struct RankedPair {
      Selection first;
      std::string second_id;
    };
    auto ranked_pair = [&](const auto& scores,
                           double duration) -> std::optional<RankedPair> {
      const auto selected = rank(scores, duration);
      if (!selected || scores.size() < 2) return std::nullopt;
      auto ordered = scores;
      std::sort(ordered.begin(), ordered.end(), [](const auto& left,
                                                   const auto& right) {
        if (!NearEqual(left.score, right.score)) {
          return left.score > right.score;
        }
        return left.speaker_id < right.speaker_id;
      });
      return RankedPair{*selected, ordered[1].speaker_id};
    };
    const double vad_duration = containing_vad->end - containing_vad->start;
    const auto vad_session =
        ranked_pair(containing_vad->session_scores, vad_duration);
    const auto vad_robust =
        ranked_pair(containing_vad->robust_scores, vad_duration);
    if (!vad_session || !vad_robust || vad_session->first.score_pass ||
        vad_session->first.margin_pass || vad_robust->first.score_pass ||
        vad_robust->first.margin_pass) {
      return std::nullopt;
    }
    const bool session_candidate_first =
        vad_session->first.speaker_id == candidate_identity &&
        vad_session->second_id == competitor_identity;
    const bool session_competitor_first =
        vad_session->first.speaker_id == competitor_identity &&
        vad_session->second_id == candidate_identity;
    const bool robust_candidate_first =
        vad_robust->first.speaker_id == candidate_identity &&
        vad_robust->second_id == competitor_identity;
    const bool robust_competitor_first =
        vad_robust->first.speaker_id == competitor_identity &&
        vad_robust->second_id == candidate_identity;
    if (!((session_candidate_first && robust_competitor_first) ||
          (session_competitor_first && robust_candidate_first))) {
      return std::nullopt;
    }

    const auto alignment = align_.find(text.id);
    if (alignment == align_.end()) return std::nullopt;
    int aligned_unit_count = 0;
    for (const auto& unit : alignment->second.units) {
      if (unit.end > unit.start && unit.start + 1e-9 >= item.start &&
          unit.end <= item.end + 1e-9) {
        ++aligned_unit_count;
      }
    }
    if (aligned_unit_count < config_.voiceprint_four_view_min_aligned_units) {
      return std::nullopt;
    }
    return candidate_identity;
  };
  auto adjacent_phrase_continuation_challenge = [&](const auto& item)
      -> std::optional<std::string> {
    const double duration = item.end - item.start;
    if (item.kind != "business_interval" ||
        duration + 1e-9 < config_.voiceprint_primary_consensus_min_sec ||
        duration + 1e-9 >= config_.voiceprint_short_max_sec ||
        !item.robust_gallery_complete) {
      return std::nullopt;
    }

    std::string current_identity;
    for (int index = item.source_start; index < item.source_end; ++index) {
      if (labels[index].speaker_id.empty()) return std::nullopt;
      if (current_identity.empty()) {
        current_identity = labels[index].speaker_id;
      } else if (labels[index].speaker_id != current_identity) {
        return std::nullopt;
      }
    }
    if (current_identity.empty()) return std::nullopt;

    const auto target_session = rank(item.session_scores, duration);
    const auto target_robust = rank(item.robust_scores, duration);
    if (!target_session || !target_robust ||
        target_session->speaker_id != current_identity ||
        target_robust->speaker_id != current_identity ||
        !target_session->score_pass || target_session->margin_pass ||
        !target_robust->score_pass || target_robust->margin_pass) {
      return std::nullopt;
    }
    auto runner_up = [](const auto& scores) -> std::optional<std::string> {
      if (scores.size() < 2) return std::nullopt;
      auto ranked = scores;
      std::sort(ranked.begin(), ranked.end(), [](const auto& left,
                                                 const auto& right) {
        if (!NearEqual(left.score, right.score)) {
          return left.score > right.score;
        }
        return left.speaker_id < right.speaker_id;
      });
      if (NearEqual(ranked[0].score, ranked[1].score)) return std::nullopt;
      return ranked[1].speaker_id;
    };
    const auto session_runner_up = runner_up(item.session_scores);
    const auto robust_runner_up = runner_up(item.robust_scores);
    if (!session_runner_up || !robust_runner_up ||
        *session_runner_up != *robust_runner_up ||
        *session_runner_up == current_identity) {
      return std::nullopt;
    }
    const std::string anchor_identity = *session_runner_up;

    const double tolerance = config_.align_boundary_split_tolerance_sec;
    const SpeakerVoiceprintEvidence* anchor = nullptr;
    for (const auto& candidate : voiceprint_) {
      if (candidate.kind != "punctuation_phrase" ||
          candidate.text_id != item.text_id || !candidate.embedding_available ||
          !candidate.robust_gallery_complete ||
          candidate.source_end != item.source_start ||
          std::abs(candidate.end - item.start) > tolerance + 1e-9) {
        continue;
      }
      const double anchor_duration = candidate.end - candidate.start;
      const auto anchor_session =
          select(candidate.session_scores, anchor_duration, true);
      const auto anchor_robust =
          select(candidate.robust_scores, anchor_duration, true);
      if (!anchor_session || !anchor_robust ||
          anchor_session->speaker_id != anchor_identity ||
          anchor_robust->speaker_id != anchor_identity) {
        continue;
      }
      if (anchor != nullptr) return std::nullopt;
      anchor = &candidate;
    }
    if (anchor == nullptr) return std::nullopt;

    const double continuous_end =
        item.start + config_.voiceprint_primary_consensus_min_sec;
    const double continuous_duration = continuous_end - anchor->start;
    if (identity_coverage(speakers_, anchor->start, continuous_end,
                          anchor_identity) +
            1e-9 <
        continuous_duration) {
      return std::nullopt;
    }

    const SpeakerSeg* covering_primary = nullptr;
    for (const auto& primary : primary_speakers_) {
      if (primary.start > item.start + tolerance ||
          primary.end + tolerance < item.end) {
        continue;
      }
      if (covering_primary != nullptr) return std::nullopt;
      covering_primary = &primary;
    }
    if (covering_primary == nullptr ||
        covering_primary->speaker_id != current_identity) {
      return std::nullopt;
    }

    const auto alignment = align_.find(text.id);
    if (alignment == align_.end()) return std::nullopt;
    int aligned_unit_count = 0;
    for (const auto& unit : alignment->second.units) {
      if (unit.end > unit.start && unit.start + 1e-9 >= item.start &&
          unit.end <= item.end + 1e-9) {
        ++aligned_unit_count;
      }
    }
    if (aligned_unit_count < config_.voiceprint_four_view_min_aligned_units) {
      return std::nullopt;
    }
    return anchor_identity;
  };
  auto short_initial_slot_direct_challenge = [&](const auto& item)
      -> std::optional<std::string> {
    const double duration = item.end - item.start;
    if (item.kind != "punctuation_phrase" ||
        duration + 1e-9 < config_.voiceprint_primary_consensus_min_sec ||
        duration + 1e-9 >= config_.voiceprint_short_max_sec ||
        !item.robust_gallery_complete) {
      return std::nullopt;
    }

    std::string current_identity;
    for (int index = item.source_start; index < item.source_end; ++index) {
      if (labels[index].speaker_id.empty() ||
          labels[index].reason.rfind("voiceprint_direct_", 0) != 0) {
        return std::nullopt;
      }
      if (current_identity.empty()) {
        current_identity = labels[index].speaker_id;
      } else if (labels[index].speaker_id != current_identity) {
        return std::nullopt;
      }
    }
    if (current_identity.empty()) return std::nullopt;

    std::set<std::string> covering_slots;
    for (const auto& segment : speakers_) {
      if (segment.speaker_id == current_identity &&
          local_coverage(item.start, item.end, segment.speaker) + 1e-9 >=
              duration) {
        covering_slots.insert(segment.speaker);
      }
    }
    if (covering_slots.size() != 1) return std::nullopt;
    const std::string current_slot = *covering_slots.begin();
    for (const auto& segment : speakers_) {
      if (segment.speaker != current_slot &&
          Overlap(item.start, item.end, segment.start, segment.end) > 1e-9) {
        return std::nullopt;
      }
    }
    const auto initial = initial_identities.find(current_slot);
    if (initial == initial_identities.end() || initial->second.second.empty() ||
        initial->second.second == current_identity) {
      return std::nullopt;
    }
    const std::string candidate_identity = initial->second.second;

    const auto phrase_session = rank(item.session_scores, duration);
    const auto phrase_robust = rank(item.robust_scores, duration);
    if (!phrase_session || !phrase_robust ||
        phrase_session->speaker_id != candidate_identity ||
        phrase_robust->speaker_id != candidate_identity ||
        !phrase_session->score_pass || phrase_session->margin_pass ||
        !phrase_robust->score_pass || phrase_robust->margin_pass) {
      return std::nullopt;
    }

    const double tolerance = config_.align_boundary_split_tolerance_sec;
    const SpeakerSeg* covering_primary = nullptr;
    for (const auto& primary : primary_speakers_) {
      if (primary.start > item.start + tolerance ||
          primary.end + tolerance < item.end) {
        continue;
      }
      if (covering_primary != nullptr) return std::nullopt;
      covering_primary = &primary;
    }
    if (covering_primary == nullptr ||
        covering_primary->speaker_id != current_identity) {
      return std::nullopt;
    }

    const auto alignment = align_.find(text.id);
    if (alignment == align_.end()) return std::nullopt;
    int aligned_unit_count = 0;
    for (const auto& unit : alignment->second.units) {
      if (unit.end > unit.start && unit.start + 1e-9 >= item.start &&
          unit.end <= item.end + 1e-9) {
        ++aligned_unit_count;
      }
    }
    if (aligned_unit_count < config_.voiceprint_four_view_min_aligned_units) {
      return std::nullopt;
    }
    return candidate_identity;
  };
  auto isolated_subminimum_unit_vad_challenge = [&](const auto& item)
      -> std::optional<std::string> {
    const double duration = item.end - item.start;
    if (item.kind != "aligned_unit" || item.embedding_available ||
        item.text_id != text.id || item.source_start < 0 ||
        item.source_end > character_count ||
        item.source_end <= item.source_start || duration <= 0.0 ||
        duration + 1e-9 >= config_.voiceprint_primary_consensus_min_sec) {
      return std::nullopt;
    }

    std::string current_identity;
    for (int index = item.source_start; index < item.source_end; ++index) {
      if (labels[index].speaker_id.empty() || labels[index].voiceprint) {
        return std::nullopt;
      }
      if (current_identity.empty()) {
        current_identity = labels[index].speaker_id;
      } else if (labels[index].speaker_id != current_identity) {
        return std::nullopt;
      }
    }

    const auto alignment = align_.find(text.id);
    if (alignment == align_.end()) return std::nullopt;
    std::optional<std::size_t> unit_index;
    for (std::size_t index = 0; index < alignment->second.units.size();
         ++index) {
      const auto& unit = alignment->second.units[index];
      if (!NearEqual(unit.start, item.start) || !NearEqual(unit.end, item.end)) {
        continue;
      }
      if (unit_index) return std::nullopt;
      unit_index = index;
    }
    if (!unit_index) return std::nullopt;

    const ComprehensiveTimeline::AlignUnitSeg* previous_unit = nullptr;
    for (std::size_t index = *unit_index; index > 0; --index) {
      const auto& unit = alignment->second.units[index - 1];
      if (unit.end > unit.start) {
        previous_unit = &unit;
        break;
      }
    }
    const ComprehensiveTimeline::AlignUnitSeg* next_unit = nullptr;
    for (std::size_t index = *unit_index + 1;
         index < alignment->second.units.size(); ++index) {
      const auto& unit = alignment->second.units[index];
      if (unit.end > unit.start) {
        next_unit = &unit;
        break;
      }
    }
    if (previous_unit == nullptr || next_unit == nullptr ||
        item.start - previous_unit->end + 1e-9 <
            config_.align_snap_pause_sec ||
        next_unit->start - item.end + 1e-9 < config_.align_snap_pause_sec) {
      return std::nullopt;
    }

    std::set<std::string> covering_slots;
    for (const auto& segment : speakers_) {
      if (segment.speaker_id != current_identity) continue;
      std::vector<std::pair<double, double>> intervals;
      for (const auto& same_slot : speakers_) {
        if (same_slot.speaker != segment.speaker ||
            same_slot.speaker_id != current_identity) {
          continue;
        }
        const double overlap =
            Overlap(item.start, item.end, same_slot.start, same_slot.end);
        if (overlap > 0.0) {
          intervals.push_back({std::max(item.start, same_slot.start),
                               std::min(item.end, same_slot.end)});
        }
      }
      if (CoveredDuration(MergeIntervals(std::move(intervals))) + 1e-9 >=
          duration) {
        covering_slots.insert(segment.speaker);
      }
    }
    if (covering_slots.size() != 1) return std::nullopt;
    const std::string current_slot = *covering_slots.begin();
    for (const auto& segment : speakers_) {
      if (Overlap(item.start, item.end, segment.start, segment.end) <= 1e-9) {
        continue;
      }
      if (segment.speaker != current_slot ||
          segment.speaker_id != current_identity) {
        return std::nullopt;
      }
    }

    const auto initial = initial_identities.find(current_slot);
    if (initial == initial_identities.end() || initial->second.second.empty() ||
        initial->second.second == current_identity) {
      return std::nullopt;
    }
    const std::string candidate_identity = initial->second.second;

    const double tolerance = config_.align_boundary_split_tolerance_sec;
    const SpeakerSeg* covering_primary = nullptr;
    int overlapping_primary_count = 0;
    for (const auto& primary : primary_speakers_) {
      if (Overlap(item.start, item.end, primary.start, primary.end) <= 1e-9) {
        continue;
      }
      ++overlapping_primary_count;
      if (primary.start <= item.start + tolerance &&
          primary.end + tolerance >= item.end) {
        covering_primary = &primary;
      }
    }
    if (overlapping_primary_count != 1 || covering_primary == nullptr ||
        covering_primary->speaker_id != current_identity) {
      return std::nullopt;
    }

    const SpeakerVoiceprintEvidence* containing_vad = nullptr;
    for (const auto& candidate : voiceprint_) {
      if (candidate.kind != "vad" || !candidate.embedding_available ||
          !candidate.robust_gallery_complete ||
          candidate.start > item.start + tolerance ||
          candidate.end + tolerance < item.end) {
        continue;
      }
      if (containing_vad != nullptr) return std::nullopt;
      containing_vad = &candidate;
    }
    if (containing_vad == nullptr) return std::nullopt;
    const double vad_duration = containing_vad->end - containing_vad->start;
    const auto vad_session =
        select(containing_vad->session_scores, vad_duration, true);
    const auto vad_robust =
        select(containing_vad->robust_scores, vad_duration, true);
    if (!vad_session || !vad_robust ||
        vad_session->speaker_id != candidate_identity ||
        vad_robust->speaker_id != candidate_identity) {
      return std::nullopt;
    }
    return candidate_identity;
  };
  auto bracketed_primary_unit_challenge = [&](const auto& item)
      -> std::optional<std::string> {
    const double duration = item.end - item.start;
    if (item.kind != "aligned_unit" || item.embedding_available ||
        item.text_id != text.id || item.source_start < 0 ||
        item.source_end > character_count ||
        item.source_end <= item.source_start || duration <= 0.0 ||
        duration + 1e-9 >= config_.voiceprint_primary_consensus_min_sec) {
      return std::nullopt;
    }

    std::string current_identity;
    for (int index = item.source_start; index < item.source_end; ++index) {
      if (labels[index].speaker_id.empty() ||
          labels[index].reason.rfind("voiceprint_direct_", 0) != 0) {
        return std::nullopt;
      }
      if (current_identity.empty()) {
        current_identity = labels[index].speaker_id;
      } else if (labels[index].speaker_id != current_identity) {
        return std::nullopt;
      }
    }

    const auto alignment = align_.find(text.id);
    if (alignment == align_.end()) return std::nullopt;
    std::optional<std::size_t> unit_index;
    for (std::size_t index = 0; index < alignment->second.units.size();
         ++index) {
      const auto& unit = alignment->second.units[index];
      if (!NearEqual(unit.start, item.start) || !NearEqual(unit.end, item.end)) {
        continue;
      }
      if (unit_index) return std::nullopt;
      unit_index = index;
    }
    if (!unit_index) return std::nullopt;

    const ComprehensiveTimeline::AlignUnitSeg* previous_unit = nullptr;
    for (std::size_t index = *unit_index; index > 0; --index) {
      const auto& unit = alignment->second.units[index - 1];
      if (unit.end > unit.start) {
        previous_unit = &unit;
        break;
      }
    }
    const ComprehensiveTimeline::AlignUnitSeg* next_unit = nullptr;
    for (std::size_t index = *unit_index + 1;
         index < alignment->second.units.size(); ++index) {
      const auto& unit = alignment->second.units[index];
      if (unit.end > unit.start) {
        next_unit = &unit;
        break;
      }
    }
    if (previous_unit == nullptr || next_unit == nullptr) {
      return std::nullopt;
    }

    std::set<std::string> covering_slots;
    for (const auto& segment : speakers_) {
      if (segment.speaker_id != current_identity) continue;
      std::vector<std::pair<double, double>> intervals;
      for (const auto& same_slot : speakers_) {
        if (same_slot.speaker != segment.speaker ||
            same_slot.speaker_id != current_identity) {
          continue;
        }
        const double overlap =
            Overlap(item.start, item.end, same_slot.start, same_slot.end);
        if (overlap > 0.0) {
          intervals.push_back({std::max(item.start, same_slot.start),
                               std::min(item.end, same_slot.end)});
        }
      }
      if (CoveredDuration(MergeIntervals(std::move(intervals))) + 1e-9 >=
          duration) {
        covering_slots.insert(segment.speaker);
      }
    }
    if (covering_slots.size() != 1) return std::nullopt;
    const std::string current_slot = *covering_slots.begin();
    for (const auto& segment : speakers_) {
      if (Overlap(item.start, item.end, segment.start, segment.end) <= 1e-9) {
        continue;
      }
      if (segment.speaker != current_slot ||
          segment.speaker_id != current_identity) {
        return std::nullopt;
      }
    }

    const SpeakerSeg* candidate_run = nullptr;
    int overlapping_primary_count = 0;
    for (const auto& primary : primary_speakers_) {
      if (Overlap(item.start, item.end, primary.start, primary.end) <= 1e-9) {
        continue;
      }
      ++overlapping_primary_count;
      if (primary.start <= item.start + 1e-9 &&
          primary.end + 1e-9 >= item.end) {
        candidate_run = &primary;
      }
    }
    if (overlapping_primary_count != 1 || candidate_run == nullptr ||
        candidate_run->speaker_id.empty() ||
        candidate_run->speaker_id == current_identity ||
        candidate_run->end <= candidate_run->start ||
        candidate_run->end - candidate_run->start + 1e-9 >=
            config_.voiceprint_primary_consensus_min_sec) {
      return std::nullopt;
    }

    const SpeakerSeg* previous_primary = nullptr;
    const SpeakerSeg* next_primary = nullptr;
    for (const auto& primary : primary_speakers_) {
      if (&primary == candidate_run) continue;
      if (NearEqual(primary.end, candidate_run->start)) {
        if (previous_primary != nullptr) return std::nullopt;
        previous_primary = &primary;
      }
      if (NearEqual(primary.start, candidate_run->end)) {
        if (next_primary != nullptr) return std::nullopt;
        next_primary = &primary;
      }
    }
    if (previous_primary == nullptr || next_primary == nullptr ||
        previous_primary->speaker_id != current_identity ||
        next_primary->speaker_id != current_identity ||
        Overlap(previous_unit->start, previous_unit->end,
                candidate_run->start, candidate_run->end) > 1e-9 ||
        Overlap(next_unit->start, next_unit->end, candidate_run->start,
                candidate_run->end) > 1e-9) {
      return std::nullopt;
    }
    return candidate_run->speaker_id;
  };

  struct VadAlignedIslandSelection {
    int source_start = 0;
    int source_end = 0;
    std::string speaker_id;
  };
  auto isolated_vad_aligned_island_challenge =
      [&](const SpeakerVoiceprintEvidence& vad)
      -> std::optional<VadAlignedIslandSelection> {
    const double vad_duration = vad.end - vad.start;
    if (vad.kind != "vad" || vad_duration <= 0.0 ||
        !vad.embedding_available || !vad.robust_gallery_complete) {
      return std::nullopt;
    }
    const auto session = select(vad.session_scores, vad_duration, true);
    const auto robust = select(vad.robust_scores, vad_duration, true);
    if (!session || !robust || session->speaker_id != robust->speaker_id) {
      return std::nullopt;
    }

    const SpeakerVoiceprintEvidence* previous_vad = nullptr;
    const SpeakerVoiceprintEvidence* next_vad = nullptr;
    for (const auto& candidate : voiceprint_) {
      if (&candidate == &vad || candidate.kind != "vad") continue;
      if (Overlap(vad.start, vad.end, candidate.start, candidate.end) > 1e-9) {
        return std::nullopt;
      }
      if (candidate.end <= vad.start + 1e-9 &&
          (previous_vad == nullptr ||
           candidate.end > previous_vad->end)) {
        previous_vad = &candidate;
      }
      if (candidate.start + 1e-9 >= vad.end &&
          (next_vad == nullptr || candidate.start < next_vad->start)) {
        next_vad = &candidate;
      }
    }
    if (previous_vad == nullptr || next_vad == nullptr ||
        vad.start - previous_vad->end + 1e-9 <
            config_.align_snap_pause_sec ||
        next_vad->start - vad.end + 1e-9 <
            config_.align_snap_pause_sec) {
      return std::nullopt;
    }

    std::vector<const SpeakerVoiceprintEvidence*> units;
    for (const auto& candidate : voiceprint_) {
      if (candidate.kind != "aligned_unit" ||
          candidate.end <= candidate.start) {
        continue;
      }
      if (Overlap(vad.start, vad.end, candidate.start, candidate.end) <= 1e-9) {
        continue;
      }
      if (candidate.start + 1e-9 < vad.start ||
          candidate.end > vad.end + 1e-9 || candidate.text_id != text.id) {
        return std::nullopt;
      }
      units.push_back(&candidate);
    }
    if (static_cast<int>(units.size()) <
        config_.voiceprint_four_view_min_aligned_units) {
      return std::nullopt;
    }
    std::sort(units.begin(), units.end(), [](const auto* left,
                                             const auto* right) {
      if (left->source_start != right->source_start) {
        return left->source_start < right->source_start;
      }
      return left->source_end < right->source_end;
    });
    if (units.front()->source_start < 0 ||
        units.back()->source_end > character_count) {
      return std::nullopt;
    }
    for (std::size_t index = 1; index < units.size(); ++index) {
      if (units[index - 1]->source_end != units[index]->source_start ||
          units[index]->start + 1e-9 < units[index - 1]->end) {
        return std::nullopt;
      }
    }

    const int source_start = units.front()->source_start;
    const int source_end = units.back()->source_end;
    const double island_start = units.front()->start;
    const double island_end = units.back()->end;
    std::string current_identity;
    for (int index = source_start; index < source_end; ++index) {
      if (labels[index].speaker_id.empty() ||
          labels[index].reason.rfind("voiceprint_direct_", 0) != 0) {
        return std::nullopt;
      }
      if (current_identity.empty()) {
        current_identity = labels[index].speaker_id;
      } else if (labels[index].speaker_id != current_identity) {
        return std::nullopt;
      }
    }
    if (current_identity.empty() || current_identity == session->speaker_id) {
      return std::nullopt;
    }

    std::set<std::string> covering_slots;
    for (const auto& segment : speakers_) {
      if (segment.speaker_id != current_identity) continue;
      std::vector<std::pair<double, double>> intervals;
      for (const auto& same_slot : speakers_) {
        if (same_slot.speaker != segment.speaker ||
            same_slot.speaker_id != current_identity) {
          continue;
        }
        const double overlap = Overlap(island_start, island_end,
                                       same_slot.start, same_slot.end);
        if (overlap > 0.0) {
          intervals.push_back({std::max(island_start, same_slot.start),
                               std::min(island_end, same_slot.end)});
        }
      }
      if (CoveredDuration(MergeIntervals(std::move(intervals))) + 1e-9 >=
          island_end - island_start) {
        covering_slots.insert(segment.speaker);
      }
    }
    if (covering_slots.size() != 1) return std::nullopt;
    const std::string& current_slot = *covering_slots.begin();
    for (const auto& segment : speakers_) {
      if (Overlap(island_start, island_end, segment.start, segment.end) <=
          1e-9) {
        continue;
      }
      if (segment.speaker != current_slot ||
          segment.speaker_id != current_identity) {
        return std::nullopt;
      }
    }

    const SpeakerSeg* covering_primary = nullptr;
    int overlapping_primary_count = 0;
    for (const auto& primary : primary_speakers_) {
      if (Overlap(island_start, island_end, primary.start, primary.end) <=
          1e-9) {
        continue;
      }
      ++overlapping_primary_count;
      if (primary.start <= island_start + 1e-9 &&
          primary.end + 1e-9 >= island_end) {
        covering_primary = &primary;
      }
    }
    if (overlapping_primary_count != 1 || covering_primary == nullptr ||
        covering_primary->speaker_id != current_identity) {
      return std::nullopt;
    }
    return VadAlignedIslandSelection{source_start, source_end,
                                     session->speaker_id};
  };

  struct PrimaryOnsetIslandSelection {
    int source_start = 0;
    int source_end = 0;
    std::string speaker_id;
  };
  auto primary_onset_aligned_island_challenge =
      [&](const SpeakerSeg& candidate_run)
      -> std::optional<PrimaryOnsetIslandSelection> {
    const double run_duration = candidate_run.end - candidate_run.start;
    if (candidate_run.speaker_id.empty() ||
        run_duration + 1e-9 < config_.voiceprint_primary_consensus_min_sec ||
        run_duration + 1e-9 >= config_.voiceprint_short_max_sec) {
      return std::nullopt;
    }

    const SpeakerSeg* previous_primary = nullptr;
    const SpeakerSeg* next_primary = nullptr;
    for (const auto& primary : primary_speakers_) {
      if (&primary == &candidate_run) continue;
      if (Overlap(candidate_run.start, candidate_run.end, primary.start,
                  primary.end) > 1e-9) {
        return std::nullopt;
      }
      if (primary.end <= candidate_run.start + 1e-9 &&
          (previous_primary == nullptr ||
           primary.end > previous_primary->end)) {
        previous_primary = &primary;
      }
      if (primary.start + 1e-9 >= candidate_run.end &&
          (next_primary == nullptr || primary.start < next_primary->start)) {
        next_primary = &primary;
      }
    }
    if (previous_primary == nullptr || next_primary == nullptr ||
        candidate_run.start - previous_primary->end + 1e-9 <
            config_.align_snap_pause_sec ||
        !NearEqual(next_primary->start, candidate_run.end) ||
        next_primary->end + 1e-9 <
            candidate_run.end +
                config_.voiceprint_primary_consensus_min_sec) {
      return std::nullopt;
    }

    std::vector<const SpeakerVoiceprintEvidence*> units;
    for (const auto& item : voiceprint_) {
      if (item.kind != "aligned_unit" || item.end <= item.start) continue;
      if (Overlap(candidate_run.start, candidate_run.end, item.start,
                  item.end) <= 1e-9) {
        continue;
      }
      if (item.start + 1e-9 < candidate_run.start ||
          item.end > candidate_run.end + 1e-9 || item.text_id != text.id) {
        return std::nullopt;
      }
      units.push_back(&item);
    }
    if (static_cast<int>(units.size()) <
        config_.voiceprint_four_view_min_aligned_units) {
      return std::nullopt;
    }
    std::sort(units.begin(), units.end(), [](const auto* left,
                                             const auto* right) {
      if (left->source_start != right->source_start) {
        return left->source_start < right->source_start;
      }
      return left->source_end < right->source_end;
    });
    if (units.front()->source_start < 0 ||
        units.back()->source_end > character_count) {
      return std::nullopt;
    }
    std::optional<std::size_t> source_split;
    for (std::size_t index = 1; index < units.size(); ++index) {
      if (units[index - 1]->source_end > units[index]->source_start ||
          units[index]->start + 1e-9 < units[index - 1]->end) {
        return std::nullopt;
      }
      if (units[index - 1]->source_end < units[index]->source_start) {
        if (source_split) return std::nullopt;
        source_split = index;
      }
    }
    if (!source_split) return std::nullopt;

    const int full_source_start = units.front()->source_start;
    const int source_end = units.back()->source_end;
    const int source_start = units[*source_split]->source_start;
    const double island_start = units[*source_split]->start;
    const double island_end = units.back()->end;
    std::string current_identity;
    for (int index = full_source_start; index < source_end; ++index) {
      if (labels[index].speaker_id.empty() ||
          labels[index].reason.rfind("voiceprint_", 0) != 0) {
        return std::nullopt;
      }
      if (current_identity.empty()) {
        current_identity = labels[index].speaker_id;
      } else if (labels[index].speaker_id != current_identity) {
        return std::nullopt;
      }
    }
    if (current_identity.empty() ||
        current_identity == candidate_run.speaker_id ||
        previous_primary->speaker_id != current_identity ||
        next_primary->speaker_id != current_identity) {
      return std::nullopt;
    }

    std::set<std::string> candidate_slots;
    for (const auto& segment : speakers_) {
      if (Overlap(island_start, island_end, segment.start, segment.end) <=
          1e-9) {
        continue;
      }
      if (segment.speaker_id != current_identity &&
          segment.speaker_id != candidate_run.speaker_id) {
        return std::nullopt;
      }
      if (segment.speaker_id == candidate_run.speaker_id) {
        candidate_slots.insert(segment.speaker);
      }
    }
    if (candidate_slots.size() != 1) return std::nullopt;
    std::vector<std::pair<double, double>> candidate_intervals;
    for (const auto& segment : speakers_) {
      if (segment.speaker != *candidate_slots.begin() ||
          segment.speaker_id != candidate_run.speaker_id) {
        continue;
      }
      const double overlap =
          Overlap(island_start, island_end, segment.start, segment.end);
      if (overlap > 0.0) {
        candidate_intervals.push_back({std::max(island_start, segment.start),
                                       std::min(island_end, segment.end)});
      }
    }
    if (CoveredDuration(MergeIntervals(std::move(candidate_intervals))) +
            1e-9 <
        island_end - island_start) {
      return std::nullopt;
    }

    const double tolerance = config_.align_boundary_split_tolerance_sec;
    const SpeakerVoiceprintEvidence* containing_vad = nullptr;
    for (const auto& item : voiceprint_) {
      if (item.kind != "vad" || !item.embedding_available ||
          !item.robust_gallery_complete ||
          std::abs(item.start - candidate_run.start) > tolerance + 1e-9 ||
          item.start > island_start + 1e-9 ||
          item.end + 1e-9 < island_end ||
          item.end + 1e-9 <
              candidate_run.end +
                  config_.voiceprint_primary_consensus_min_sec) {
        continue;
      }
      if (containing_vad != nullptr) return std::nullopt;
      containing_vad = &item;
    }
    if (containing_vad == nullptr) return std::nullopt;

    const SpeakerVoiceprintEvidence* previous_vad = nullptr;
    for (const auto& item : voiceprint_) {
      if (item.kind != "vad" || &item == containing_vad ||
          item.end > containing_vad->start + 1e-9) {
        continue;
      }
      if (previous_vad == nullptr || item.end > previous_vad->end) {
        previous_vad = &item;
      }
    }
    if (previous_vad == nullptr ||
        containing_vad->start - previous_vad->end + 1e-9 <
            config_.align_snap_pause_sec) {
      return std::nullopt;
    }
    const double vad_duration = containing_vad->end - containing_vad->start;
    const auto vad_session = rank(containing_vad->session_scores, vad_duration);
    const auto vad_robust = rank(containing_vad->robust_scores, vad_duration);
    if (!vad_session || !vad_robust ||
        vad_session->speaker_id != current_identity ||
        vad_robust->speaker_id != current_identity) {
      return std::nullopt;
    }
    return PrimaryOnsetIslandSelection{source_start, source_end,
                                       candidate_run.speaker_id};
  };

  auto isolated_no_vad_interval_challenge =
      [&](const SpeakerVoiceprintEvidence& item)
      -> std::optional<std::string> {
    const double duration = item.end - item.start;
    if (item.kind != "business_interval" || item.embedding_available ||
        item.text_id != text.id || item.source_start < 0 ||
        item.source_end > character_count ||
        item.source_end <= item.source_start || duration <= 0.0 ||
        duration + 1e-9 >= config_.voiceprint_primary_consensus_min_sec) {
      return std::nullopt;
    }

    std::string current_identity;
    for (int index = item.source_start; index < item.source_end; ++index) {
      if (labels[index].speaker_id.empty() || labels[index].voiceprint) {
        return std::nullopt;
      }
      if (current_identity.empty()) {
        current_identity = labels[index].speaker_id;
      } else if (labels[index].speaker_id != current_identity) {
        return std::nullopt;
      }
    }

    std::set<std::string> covering_slots;
    for (const auto& segment : speakers_) {
      if (segment.speaker_id != current_identity) continue;
      std::vector<std::pair<double, double>> intervals;
      for (const auto& same_slot : speakers_) {
        if (same_slot.speaker != segment.speaker ||
            same_slot.speaker_id != current_identity) {
          continue;
        }
        const double overlap =
            Overlap(item.start, item.end, same_slot.start, same_slot.end);
        if (overlap > 0.0) {
          intervals.push_back({std::max(item.start, same_slot.start),
                               std::min(item.end, same_slot.end)});
        }
      }
      if (CoveredDuration(MergeIntervals(std::move(intervals))) + 1e-9 >=
          duration) {
        covering_slots.insert(segment.speaker);
      }
    }
    if (covering_slots.size() != 1) return std::nullopt;
    const std::string current_slot = *covering_slots.begin();
    for (const auto& segment : speakers_) {
      if (Overlap(item.start, item.end, segment.start, segment.end) <= 1e-9) {
        continue;
      }
      if (segment.speaker != current_slot ||
          segment.speaker_id != current_identity) {
        return std::nullopt;
      }
    }
    const auto initial = initial_identities.find(current_slot);
    if (initial == initial_identities.end() || initial->second.second.empty() ||
        initial->second.second == current_identity) {
      return std::nullopt;
    }
    const std::string candidate_identity = initial->second.second;

    const SpeakerSeg* covering_primary = nullptr;
    int overlapping_primary_count = 0;
    for (const auto& primary : primary_speakers_) {
      if (Overlap(item.start, item.end, primary.start, primary.end) <= 1e-9) {
        continue;
      }
      ++overlapping_primary_count;
      if (primary.start <= item.start + 1e-9 &&
          primary.end + 1e-9 >= item.end) {
        covering_primary = &primary;
      }
    }
    if (overlapping_primary_count != 1 || covering_primary == nullptr ||
        covering_primary->speaker != current_slot ||
        covering_primary->speaker_id != current_identity) {
      return std::nullopt;
    }

    const SpeakerVoiceprintEvidence* previous_vad = nullptr;
    const SpeakerVoiceprintEvidence* next_vad = nullptr;
    for (const auto& candidate : voiceprint_) {
      if (candidate.kind != "vad") continue;
      if (Overlap(item.start, item.end, candidate.start, candidate.end) >
          1e-9) {
        return std::nullopt;
      }
      if (candidate.end <= item.start + 1e-9 &&
          (previous_vad == nullptr ||
           candidate.end > previous_vad->end)) {
        previous_vad = &candidate;
      }
      if (candidate.start + 1e-9 >= item.end &&
          (next_vad == nullptr || candidate.start < next_vad->start)) {
        next_vad = &candidate;
      }
    }
    if (previous_vad == nullptr || next_vad == nullptr ||
        item.start - previous_vad->end + 1e-9 <
            config_.align_snap_pause_sec ||
        next_vad->start - item.end + 1e-9 <
            config_.align_snap_pause_sec) {
      return std::nullopt;
    }

    int aligned_unit_count = 0;
    const SpeakerVoiceprintEvidence* previous_unit = nullptr;
    const SpeakerVoiceprintEvidence* next_unit = nullptr;
    for (const auto& candidate : voiceprint_) {
      if (candidate.kind != "aligned_unit" ||
          candidate.end <= candidate.start) {
        continue;
      }
      if (Overlap(item.start, item.end, candidate.start, candidate.end) >
          1e-9) {
        if (candidate.text_id != text.id ||
            candidate.source_start < item.source_start ||
            candidate.source_end > item.source_end ||
            candidate.start + 1e-9 < item.start ||
            candidate.end > item.end + 1e-9) {
          return std::nullopt;
        }
        ++aligned_unit_count;
        continue;
      }
      if (candidate.end <= item.start + 1e-9 &&
          (previous_unit == nullptr ||
           candidate.end > previous_unit->end)) {
        previous_unit = &candidate;
      }
      if (candidate.start + 1e-9 >= item.end &&
          (next_unit == nullptr || candidate.start < next_unit->start)) {
        next_unit = &candidate;
      }
    }
    if (aligned_unit_count < config_.voiceprint_four_view_min_aligned_units ||
        previous_unit == nullptr || next_unit == nullptr ||
        item.start - previous_unit->end + 1e-9 <
            config_.align_snap_pause_sec ||
        next_unit->start - item.end + 1e-9 <
            config_.align_snap_pause_sec) {
      return std::nullopt;
    }
    return candidate_identity;
  };
  auto subminimum_native_cross_scale_challenge = [&](const auto& item)
      -> std::optional<std::string> {
    const double interval_duration = item.end - item.start;
    if (item.kind != "business_interval" || item.text_id != text.id ||
        item.source_start < 0 || item.source_end > character_count ||
        item.source_end <= item.source_start || item.embedding_available ||
        interval_duration <= 0.0 ||
        interval_duration + 1e-9 >=
            config_.voiceprint_primary_consensus_min_sec) {
      return std::nullopt;
    }

    std::string current_identity;
    for (int index = item.source_start; index < item.source_end; ++index) {
      if (labels[index].speaker_id.empty() || !labels[index].voiceprint ||
          labels[index].reason != "voiceprint_phrase_session") {
        return std::nullopt;
      }
      if (current_identity.empty()) {
        current_identity = labels[index].speaker_id;
      } else if (labels[index].speaker_id != current_identity) {
        return std::nullopt;
      }
    }

    const SpeakerSeg* covering_primary = nullptr;
    for (const auto& primary : primary_speakers_) {
      if (Overlap(item.start, item.end, primary.start, primary.end) <= 1e-9) {
        continue;
      }
      if (covering_primary != nullptr || primary.speaker_id.empty() ||
          primary.speaker_id == current_identity ||
          primary.start > item.start + 1e-9 ||
          primary.end + 1e-9 < item.end) {
        return std::nullopt;
      }
      covering_primary = &primary;
    }
    if (covering_primary == nullptr) return std::nullopt;
    const std::string native_slot = covering_primary->speaker;
    const std::string native_identity = covering_primary->speaker_id;

    using ActivityKey = std::pair<std::string, std::string>;
    std::map<ActivityKey, std::vector<std::pair<double, double>>>
        activity_intervals;
    for (const auto& segment : speakers_) {
      const double overlap =
          Overlap(item.start, item.end, segment.start, segment.end);
      if (overlap <= 1e-9) continue;
      activity_intervals[{segment.speaker, segment.speaker_id}].push_back(
          {std::max(item.start, segment.start),
           std::min(item.end, segment.end)});
    }
    if (activity_intervals.size() != 2) return std::nullopt;
    bool native_activity_found = false;
    std::string competitor_identity;
    for (auto& [activity, intervals] : activity_intervals) {
      if (activity.second.empty() || activity.second == current_identity ||
          CoveredDuration(MergeIntervals(std::move(intervals))) + 1e-9 <
              interval_duration) {
        return std::nullopt;
      }
      if (activity.first == native_slot &&
          activity.second == native_identity) {
        native_activity_found = true;
      } else {
        if (activity.second == native_identity ||
            !competitor_identity.empty()) {
          return std::nullopt;
        }
        competitor_identity = activity.second;
      }
    }
    if (!native_activity_found || competitor_identity.empty()) {
      return std::nullopt;
    }

    const double tolerance = config_.align_boundary_split_tolerance_sec;
    const SpeakerVoiceprintEvidence* containing_phrase = nullptr;
    const SpeakerVoiceprintEvidence* containing_vad = nullptr;
    for (const auto& candidate : voiceprint_) {
      const bool contains_time =
          candidate.start <= item.start + tolerance &&
          candidate.end + tolerance >= item.end;
      if (!contains_time) continue;
      if (candidate.kind == "punctuation_phrase" &&
          candidate.text_id == item.text_id &&
          candidate.source_start <= item.source_start &&
          candidate.source_end >= item.source_end) {
        if (containing_phrase != nullptr || !candidate.embedding_available ||
            !candidate.robust_gallery_complete) {
          return std::nullopt;
        }
        containing_phrase = &candidate;
      } else if (candidate.kind == "vad") {
        if (containing_vad != nullptr || !candidate.embedding_available ||
            !candidate.robust_gallery_complete) {
          return std::nullopt;
        }
        containing_vad = &candidate;
      }
    }
    if (containing_phrase == nullptr || containing_vad == nullptr) {
      return std::nullopt;
    }

    const double phrase_duration =
        containing_phrase->end - containing_phrase->start;
    const auto phrase_session =
        rank(containing_phrase->session_scores, phrase_duration);
    const auto phrase_robust =
        rank(containing_phrase->robust_scores, phrase_duration);
    if (!phrase_session || !phrase_robust ||
        phrase_session->speaker_id != current_identity ||
        phrase_robust->speaker_id != current_identity ||
        !phrase_session->score_pass || !phrase_session->margin_pass ||
        !phrase_robust->score_pass || phrase_robust->margin_pass) {
      return std::nullopt;
    }

    struct RankedPair {
      Selection first;
      std::string second_id;
    };
    auto ranked_pair = [&](const auto& scores,
                           double duration) -> std::optional<RankedPair> {
      const auto selected = rank(scores, duration);
      if (!selected || scores.size() < 2) return std::nullopt;
      auto ordered = scores;
      std::sort(ordered.begin(), ordered.end(), [](const auto& left,
                                                   const auto& right) {
        if (!NearEqual(left.score, right.score)) {
          return left.score > right.score;
        }
        return left.speaker_id < right.speaker_id;
      });
      return RankedPair{*selected, ordered[1].speaker_id};
    };
    const double vad_duration = containing_vad->end - containing_vad->start;
    const auto vad_session =
        ranked_pair(containing_vad->session_scores, vad_duration);
    const auto vad_robust =
        ranked_pair(containing_vad->robust_scores, vad_duration);
    if (!vad_session || !vad_robust ||
        vad_session->first.speaker_id != native_identity ||
        vad_robust->first.speaker_id != native_identity ||
        vad_session->second_id != current_identity ||
        vad_robust->second_id != current_identity ||
        !vad_session->first.score_pass || vad_session->first.margin_pass ||
        !vad_robust->first.score_pass || vad_robust->first.margin_pass) {
      return std::nullopt;
    }

    const auto alignment = align_.find(text.id);
    if (alignment == align_.end()) return std::nullopt;
    int positive_unit_count = 0;
    for (const auto& unit : alignment->second.units) {
      if (unit.end <= unit.start ||
          Overlap(item.start, item.end, unit.start, unit.end) <= 1e-9) {
        continue;
      }
      if (unit.start + 1e-9 < item.start ||
          unit.end > item.end + 1e-9) {
        return std::nullopt;
      }
      ++positive_unit_count;
    }
    if (positive_unit_count != 1) return std::nullopt;
    return native_identity;
  };
  auto complete_source_aligned_vad_challenge = [&](const auto& item)
      -> std::optional<std::string> {
    const double outer_duration = item.end - item.start;
    if (item.kind != "complete_source" || item.text_id != text.id ||
        item.source_start != 0 || item.source_end != character_count ||
        !item.embedding_available || !item.robust_gallery_complete ||
        outer_duration + 1e-9 < config_.voiceprint_short_max_sec) {
      return std::nullopt;
    }

    std::vector<const SpeakerVoiceprintEvidence*> aligned_units;
    for (const auto* candidate : source_evidence) {
      if (candidate->kind == "aligned_unit" &&
          candidate->end > candidate->start) {
        aligned_units.push_back(candidate);
      }
    }
    if (aligned_units.empty()) return std::nullopt;

    std::vector<int> source_partition_count(character_count, 0);
    double aligned_start = 0.0;
    double aligned_end = 0.0;
    bool aligned_found = false;
    for (const auto* candidate : source_evidence) {
      if (candidate->kind != "business_interval" ||
          candidate->end <= candidate->start) {
        continue;
      }
      bool anchored = false;
      for (const auto* unit : aligned_units) {
        if (unit->source_start >= candidate->source_start &&
            unit->source_end <= candidate->source_end &&
            unit->start + 1e-9 >= candidate->start &&
            unit->end <= candidate->end + 1e-9) {
          anchored = true;
          break;
        }
      }
      if (!anchored) return std::nullopt;
      for (int index = candidate->source_start;
           index < candidate->source_end; ++index) {
        ++source_partition_count[index];
      }
      if (!aligned_found) {
        aligned_start = candidate->start;
        aligned_end = candidate->end;
        aligned_found = true;
      } else {
        aligned_start = std::min(aligned_start, candidate->start);
        aligned_end = std::max(aligned_end, candidate->end);
      }
    }
    if (!aligned_found) return std::nullopt;
    for (int index = 0; index < character_count; ++index) {
      if (source_partition_count[index] != 1) {
        return std::nullopt;
      }
    }
    const double aligned_duration = aligned_end - aligned_start;
    if (aligned_duration + 1e-9 <
            config_.voiceprint_primary_consensus_min_sec ||
        aligned_duration + 1e-9 >= config_.voiceprint_short_max_sec) {
      return std::nullopt;
    }

    const auto outer_session = rank(item.session_scores, outer_duration);
    const auto outer_robust = rank(item.robust_scores, outer_duration);
    const auto aligned_session = rank(item.session_scores, aligned_duration);
    const auto aligned_robust = rank(item.robust_scores, aligned_duration);
    if (!outer_session || !outer_robust || !aligned_session ||
        !aligned_robust ||
        aligned_session->speaker_id != aligned_robust->speaker_id ||
        !aligned_session->score_pass || !aligned_session->margin_pass ||
        !aligned_robust->score_pass || !aligned_robust->margin_pass ||
        outer_session->speaker_id != aligned_session->speaker_id ||
        outer_robust->speaker_id != aligned_session->speaker_id ||
        outer_session->score_pass || !outer_session->margin_pass ||
        outer_robust->score_pass || !outer_robust->margin_pass) {
      return std::nullopt;
    }
    const std::string candidate_identity = aligned_session->speaker_id;

    std::string incumbent_identity;
    int candidate_first = character_count;
    int candidate_last = -1;
    for (int index = 0; index < character_count; ++index) {
      if (labels[index].speaker_id.empty()) return std::nullopt;
      if (labels[index].speaker_id == candidate_identity) {
        if (labels[index].voiceprint ||
            (labels[index].reason != "primary_speaker_tie_break" &&
             labels[index].reason !=
                 "primary_speaker_overlap_refinement")) {
          return std::nullopt;
        }
        candidate_first = std::min(candidate_first, index);
        candidate_last = std::max(candidate_last, index);
        continue;
      }
      if (incumbent_identity.empty()) {
        incumbent_identity = labels[index].speaker_id;
      } else if (labels[index].speaker_id != incumbent_identity) {
        return std::nullopt;
      }
    }
    if (incumbent_identity.empty() || candidate_last < candidate_first ||
        (candidate_first != 0 && candidate_last != character_count - 1)) {
      return std::nullopt;
    }
    for (int index = candidate_first; index <= candidate_last; ++index) {
      if (labels[index].speaker_id != candidate_identity) {
        return std::nullopt;
      }
    }

    auto coverage_by_identity = [&](const std::vector<SpeakerSeg>& segments) {
      std::map<std::string, std::vector<std::pair<double, double>>> intervals;
      for (const auto& segment : segments) {
        const double overlap = Overlap(aligned_start, aligned_end,
                                       segment.start, segment.end);
        if (overlap <= 1e-9 || segment.speaker_id.empty()) continue;
        intervals[segment.speaker_id].push_back(
            {std::max(aligned_start, segment.start),
             std::min(aligned_end, segment.end)});
      }
      std::map<std::string, double> coverage;
      for (auto& [identity, spans] : intervals) {
        coverage[identity] =
            CoveredDuration(MergeIntervals(std::move(spans)));
      }
      return coverage;
    };
    const auto activity_coverage = coverage_by_identity(speakers_);
    if (activity_coverage.size() != 2 ||
        activity_coverage.count(candidate_identity) == 0 ||
        activity_coverage.count(incumbent_identity) == 0 ||
        activity_coverage.at(incumbent_identity) + 1e-9 < aligned_duration ||
        activity_coverage.at(candidate_identity) <= 1e-9 ||
        activity_coverage.at(candidate_identity) + 1e-9 >= aligned_duration) {
      return std::nullopt;
    }

    const auto primary_coverage = coverage_by_identity(primary_speakers_);
    if (primary_coverage.size() != 2 ||
        primary_coverage.count(candidate_identity) == 0 ||
        primary_coverage.count(incumbent_identity) == 0 ||
        primary_coverage.at(candidate_identity) <= 1e-9 ||
        primary_coverage.at(incumbent_identity) <= 1e-9) {
      return std::nullopt;
    }
    std::vector<std::pair<double, double>> primary_union;
    for (const auto& primary : primary_speakers_) {
      if (primary.speaker_id != candidate_identity &&
          primary.speaker_id != incumbent_identity) {
        continue;
      }
      const double overlap = Overlap(aligned_start, aligned_end,
                                     primary.start, primary.end);
      if (overlap > 1e-9) {
        primary_union.push_back({std::max(aligned_start, primary.start),
                                 std::min(aligned_end, primary.end)});
      }
    }
    if (CoveredDuration(MergeIntervals(std::move(primary_union))) + 1e-9 <
        aligned_duration) {
      return std::nullopt;
    }

    auto top_two = [](const auto& scores)
        -> std::optional<std::pair<std::string, std::string>> {
      if (scores.size() < 2) return std::nullopt;
      auto ordered = scores;
      std::sort(ordered.begin(), ordered.end(), [](const auto& left,
                                                   const auto& right) {
        if (!NearEqual(left.score, right.score)) {
          return left.score > right.score;
        }
        return left.speaker_id < right.speaker_id;
      });
      return std::make_pair(ordered[0].speaker_id, ordered[1].speaker_id);
    };

    const SpeakerVoiceprintEvidence* phrase = nullptr;
    const double tolerance = config_.align_boundary_split_tolerance_sec;
    const SpeakerVoiceprintEvidence* containing_vad = nullptr;
    for (const auto& candidate : voiceprint_) {
      if (candidate.kind == "punctuation_phrase" &&
          candidate.text_id == text.id) {
        if (phrase != nullptr || !candidate.embedding_available ||
            !candidate.robust_gallery_complete) {
          return std::nullopt;
        }
        phrase = &candidate;
      } else if (candidate.kind == "vad" &&
                 candidate.start <= aligned_start + tolerance &&
                 candidate.end + tolerance >= aligned_end) {
        if (containing_vad != nullptr || !candidate.embedding_available ||
            !candidate.robust_gallery_complete) {
          return std::nullopt;
        }
        containing_vad = &candidate;
      }
    }
    if (phrase == nullptr || containing_vad == nullptr) {
      return std::nullopt;
    }

    const double phrase_duration = phrase->end - phrase->start;
    const auto phrase_session = rank(phrase->session_scores, phrase_duration);
    const auto phrase_robust = rank(phrase->robust_scores, phrase_duration);
    const auto phrase_session_pair = top_two(phrase->session_scores);
    const auto phrase_robust_pair = top_two(phrase->robust_scores);
    auto pair_contains = [&](const auto& pair) {
      return pair.first == candidate_identity ||
             pair.second == candidate_identity;
    };
    if (!phrase_session || !phrase_robust || !phrase_session_pair ||
        !phrase_robust_pair || !phrase_session->score_pass ||
        phrase_session->margin_pass || !phrase_robust->score_pass ||
        phrase_robust->margin_pass ||
        !pair_contains(*phrase_session_pair) ||
        !pair_contains(*phrase_robust_pair)) {
      return std::nullopt;
    }

    const double vad_duration = containing_vad->end - containing_vad->start;
    const auto vad_session = rank(containing_vad->session_scores, vad_duration);
    const auto vad_robust = rank(containing_vad->robust_scores, vad_duration);
    if (!vad_session || !vad_robust ||
        vad_session->speaker_id != candidate_identity ||
        vad_robust->speaker_id != candidate_identity ||
        !vad_session->score_pass || !vad_session->margin_pass ||
        !vad_robust->score_pass || !vad_robust->margin_pass) {
      return std::nullopt;
    }
    return candidate_identity;
  };

  struct AdjacentPrefixSelection {
    int source_start = 0;
    int source_end = 0;
    std::string speaker_id;
  };
  auto adjacent_primary_supported_prefix_challenge = [&](const auto& item)
      -> std::optional<AdjacentPrefixSelection> {
    const double pair_duration = item.end - item.start;
    if (item.kind != "adjacent_business_pair" || item.text_id != text.id ||
        item.source_start != 0 || item.source_end > character_count ||
        item.source_end <= item.source_start || !item.embedding_available ||
        !item.robust_gallery_complete ||
        pair_duration + 1e-9 <
            config_.voiceprint_primary_consensus_min_sec ||
        pair_duration + 1e-9 >= config_.voiceprint_short_max_sec) {
      return std::nullopt;
    }

    std::vector<const SpeakerVoiceprintEvidence*> components;
    for (const auto* candidate : source_evidence) {
      if (candidate->kind != "business_interval" ||
          candidate->source_start < item.source_start ||
          candidate->source_end > item.source_end ||
          candidate->end <= candidate->start) {
        continue;
      }
      components.push_back(candidate);
    }
    std::sort(components.begin(), components.end(), [](const auto* left,
                                                       const auto* right) {
      if (left->source_start != right->source_start) {
        return left->source_start < right->source_start;
      }
      return left->source_end < right->source_end;
    });
    if (components.size() != 2) return std::nullopt;
    const auto& leading = *components[0];
    const auto& following = *components[1];
    const double leading_duration = leading.end - leading.start;
    const double following_duration = following.end - following.start;
    if (leading.source_start != item.source_start ||
        leading.source_end != following.source_start ||
        following.source_end != item.source_end ||
        !NearEqual(leading.start, item.start) ||
        !NearEqual(leading.end, following.start) ||
        !NearEqual(following.end, item.end) || leading.embedding_available ||
        !following.embedding_available ||
        !following.robust_gallery_complete || leading_duration <= 0.0 ||
        leading_duration + 1e-9 >=
            config_.voiceprint_primary_consensus_min_sec ||
        following_duration + 1e-9 <
            config_.voiceprint_primary_consensus_min_sec) {
      return std::nullopt;
    }

    const auto pair_session = select(item.session_scores, pair_duration, true);
    const auto pair_robust = select(item.robust_scores, pair_duration, true);
    if (!pair_session || !pair_robust ||
        pair_session->speaker_id != pair_robust->speaker_id) {
      return std::nullopt;
    }
    const std::string candidate_identity = pair_session->speaker_id;

    std::string incumbent_identity;
    for (int index = leading.source_start; index < leading.source_end;
         ++index) {
      const bool native_provenance =
          labels[index].reason == "sole_diar_support" ||
          labels[index].reason == "competing_diar_interval_policy";
      if (labels[index].speaker_id.empty() || labels[index].voiceprint ||
          !native_provenance) {
        return std::nullopt;
      }
      if (incumbent_identity.empty()) {
        incumbent_identity = labels[index].speaker_id;
      } else if (labels[index].speaker_id != incumbent_identity) {
        return std::nullopt;
      }
    }
    if (incumbent_identity.empty() ||
        incumbent_identity == candidate_identity) {
      return std::nullopt;
    }
    for (int index = following.source_start; index < following.source_end;
         ++index) {
      const bool primary_selected =
          labels[index].reason == "primary_speaker_tie_break" ||
          labels[index].reason == "primary_speaker_overlap_refinement";
      if (labels[index].voiceprint || !primary_selected ||
          labels[index].speaker_id != candidate_identity) {
        return std::nullopt;
      }
    }

    const double tolerance = config_.align_boundary_split_tolerance_sec;
    const SpeakerSeg* candidate_primary = nullptr;
    for (const auto& primary : primary_speakers_) {
      if (Overlap(item.start, item.end, primary.start, primary.end) <= 1e-9) {
        continue;
      }
      if (candidate_primary != nullptr ||
          primary.speaker_id != candidate_identity ||
          primary.start + 1e-9 < item.start ||
          primary.start > leading.end + tolerance ||
          primary.end + tolerance < following.end) {
        return std::nullopt;
      }
      candidate_primary = &primary;
    }
    if (candidate_primary == nullptr) return std::nullopt;

    std::map<std::string, std::vector<std::pair<double, double>>>
        activity_by_identity;
    std::map<std::string, double> activity_first_start;
    std::map<std::string, double> activity_confidence_total;
    std::map<std::string, double> activity_confidence_weight;
    for (const auto& segment : speakers_) {
      const double overlap =
          Overlap(item.start, item.end, segment.start, segment.end);
      if (overlap <= 1e-9) continue;
      if (segment.speaker_id.empty()) return std::nullopt;
      activity_by_identity[segment.speaker_id].push_back(
          {std::max(item.start, segment.start),
           std::min(item.end, segment.end)});
      activity_confidence_total[segment.speaker_id] += segment.conf * overlap;
      activity_confidence_weight[segment.speaker_id] += overlap;
      const auto position = activity_first_start.find(segment.speaker_id);
      if (position == activity_first_start.end()) {
        activity_first_start[segment.speaker_id] = segment.start;
      } else {
        position->second = std::min(position->second, segment.start);
      }
    }
    if (activity_by_identity.size() < 2 || activity_by_identity.size() > 3 ||
        activity_by_identity.count(candidate_identity) == 0 ||
        activity_by_identity.count(incumbent_identity) == 0) {
      return std::nullopt;
    }
    const double candidate_activity = CoveredDuration(MergeIntervals(
        activity_by_identity.at(candidate_identity)));
    const double incumbent_activity = CoveredDuration(MergeIntervals(
        activity_by_identity.at(incumbent_identity)));
    if (activity_first_start.at(candidate_identity) + 1e-9 < item.start ||
        activity_first_start.at(candidate_identity) >
            leading.end + tolerance ||
        candidate_activity + tolerance < following_duration ||
        incumbent_activity + tolerance < pair_duration) {
      return std::nullopt;
    }
    if (activity_by_identity.size() == 3) {
      const double candidate_confidence =
          activity_confidence_total.at(candidate_identity) /
          activity_confidence_weight.at(candidate_identity);
      const double incumbent_confidence =
          activity_confidence_total.at(incumbent_identity) /
          activity_confidence_weight.at(incumbent_identity);
      for (const auto& [identity, intervals] : activity_by_identity) {
        if (identity == candidate_identity || identity == incumbent_identity) {
          continue;
        }
        const double third_coverage =
            CoveredDuration(MergeIntervals(intervals));
        const double third_confidence =
            activity_confidence_total.at(identity) /
            activity_confidence_weight.at(identity);
        if (third_coverage + tolerance < pair_duration ||
            third_confidence + 1e-9 >= candidate_confidence ||
            third_confidence + 1e-9 >= incumbent_confidence) {
          return std::nullopt;
        }
      }
    }

    int leading_units = 0;
    int following_units = 0;
    for (const auto* candidate : source_evidence) {
      if (candidate->kind != "aligned_unit" ||
          candidate->end <= candidate->start ||
          candidate->source_end <= item.source_start ||
          candidate->source_start >= item.source_end) {
        continue;
      }
      const bool inside_leading =
          candidate->source_start >= leading.source_start &&
          candidate->source_end <= leading.source_end &&
          candidate->start + 1e-9 >= leading.start &&
          candidate->end <= leading.end + 1e-9;
      const bool inside_following =
          candidate->source_start >= following.source_start &&
          candidate->source_end <= following.source_end &&
          candidate->start + 1e-9 >= following.start &&
          candidate->end <= following.end + 1e-9;
      if (inside_leading == inside_following) return std::nullopt;
      if (inside_leading) {
        ++leading_units;
      } else {
        ++following_units;
      }
    }
    if (leading_units == 0 || following_units == 0 ||
        leading_units + following_units <
            config_.voiceprint_four_view_min_aligned_units) {
      return std::nullopt;
    }

    const SpeakerVoiceprintEvidence* next_interval = nullptr;
    for (const auto* candidate : source_evidence) {
      if (candidate->kind != "business_interval" ||
          candidate->source_start != item.source_end ||
          candidate->end <= candidate->start) {
        continue;
      }
      if (next_interval != nullptr) return std::nullopt;
      next_interval = candidate;
    }
    if (next_interval == nullptr ||
        next_interval->start + 1e-9 < item.end) {
      return std::nullopt;
    }
    for (int index = next_interval->source_start;
         index < next_interval->source_end; ++index) {
      const bool primary_selected =
          labels[index].reason == "primary_speaker_tie_break" ||
          labels[index].reason == "primary_speaker_overlap_refinement";
      if (labels[index].voiceprint || !primary_selected ||
          labels[index].speaker_id != incumbent_identity) {
        return std::nullopt;
      }
    }
    return AdjacentPrefixSelection{leading.source_start, leading.source_end,
                                   candidate_identity};
  };

  for (const auto* item : evidence) {
    const double duration = item->end - item->start;
    const auto initial_slot = initial_slot_phrase_challenge(*item);
    if (initial_slot) {
      apply(*item, *initial_slot,
            "voiceprint_phrase_initial_slot_dual_gallery_override");
      continue;
    }

    const auto four_view = four_view_margin_challenge(*item);
    if (four_view) {
      apply(*item, *four_view,
            "voiceprint_phrase_vad_four_view_margin_override");
      continue;
    }

    const auto short_initial_slot = short_initial_slot_vad_challenge(*item);
    if (short_initial_slot) {
      apply(*item, *short_initial_slot,
            "voiceprint_phrase_short_initial_slot_vad_override");
      continue;
    }

    const auto initial_slot_near_tie =
        initial_slot_four_view_near_tie_challenge(*item);
    if (initial_slot_near_tie) {
      apply(*item, *initial_slot_near_tie,
            "voiceprint_phrase_initial_slot_four_view_near_tie_override");
      continue;
    }

    const auto cross_scale_near_tie =
        cross_scale_symmetric_near_tie_challenge(*item);
    if (cross_scale_near_tie) {
      apply(*item, *cross_scale_near_tie,
            "voiceprint_phrase_cross_scale_symmetric_near_tie_override");
      continue;
    }

    const auto exact_interval_primary_conflict =
        exact_interval_primary_conflict_challenge(*item);
    if (exact_interval_primary_conflict) {
      apply(*item, *exact_interval_primary_conflict,
            "voiceprint_interval_primary_conflict_vad_abstention_override");
      continue;
    }

    const auto adjacent_phrase = adjacent_phrase_continuation_challenge(*item);
    if (adjacent_phrase) {
      apply(*item, *adjacent_phrase,
            "voiceprint_direct_adjacent_phrase_anchor");
      continue;
    }

    const auto short_initial_direct =
        short_initial_slot_direct_challenge(*item);
    if (short_initial_direct) {
      apply(*item, *short_initial_direct,
            "voiceprint_phrase_short_initial_slot_direct_override");
      continue;
    }

    const auto session = select(item->session_scores, duration, true);
    if (!session) continue;
    const auto robust = item->robust_gallery_complete
                            ? select(item->robust_scores, duration, true)
                            : std::nullopt;
    const bool dual_agreement =
        robust && robust->speaker_id == session->speaker_id;
    if (native_views_preserve_current(*item, session->speaker_id)) {
      continue;
    }
    if (item->kind == "business_interval") {
      const std::string reason =
          duration < config_.voiceprint_short_max_sec
              ? "voiceprint_direct_short"
              : "voiceprint_direct_regular";
      const std::string label = speaker_label(session->speaker_id);
      for (int index = item->source_start; index < item->source_end; ++index) {
        const bool conflicts_with_primary =
            labels[index].reason.rfind("primary_speaker_", 0) == 0 &&
            !labels[index].speaker_id.empty() &&
            labels[index].speaker_id != session->speaker_id;
        if (conflicts_with_primary) continue;
        labels[index].speaker = label;
        labels[index].speaker_id = session->speaker_id;
        labels[index].reason = reason;
        labels[index].voiceprint = true;
      }
      continue;
    }
    const bool direct_conflict =
        has_conflicting_direct_anchor(*item, session->speaker_id);
    const bool strong_conflict_override =
        session->score + 1e-9f >= config_.voiceprint_regular_min_score;
    if (item->kind == "aligned_unit") {
      if (dual_agreement &&
          !has_conflicting_primary_label(*item, session->speaker_id) &&
          (!direct_conflict || strong_conflict_override)) {
        apply(*item, session->speaker_id,
              direct_conflict
                  ? "voiceprint_aligned_unit_dual_gallery_override"
                  : "voiceprint_aligned_unit_dual_gallery");
      }
    } else if (item->kind == "punctuation_phrase") {
      // A complete phrase measured independently by both galleries is more
      // specific than a containing business interval. A session-only phrase
      // still abstains on conflict because the two views are correlated.
      const bool primary_activity_consensus =
          dual_agreement && primary_activity_phrase_consensus(
                                *item, session->speaker_id);
      if (direct_conflict && !primary_activity_consensus &&
          (!dual_agreement || !strong_conflict_override)) {
        continue;
      }
      apply(*item, session->speaker_id,
            primary_activity_consensus && direct_conflict
                ? "voiceprint_phrase_primary_activity_dual_gallery"
                : dual_agreement
                      ? (direct_conflict
                             ? "voiceprint_phrase_dual_gallery_override"
                             : "voiceprint_phrase_dual_gallery")
                      : "voiceprint_phrase_session");
    } else if (item->kind == "complete_source" && dual_agreement) {
      bool one_current_identity = true;
      std::string current;
      for (int index = item->source_start; index < item->source_end; ++index) {
        if (labels[index].speaker_id.empty()) continue;
        if (current.empty()) current = labels[index].speaker_id;
        if (labels[index].speaker_id != current) one_current_identity = false;
      }
      if (one_current_identity &&
          (current.empty() || current == session->speaker_id)) {
        apply(*item, session->speaker_id,
              "voiceprint_complete_source_dual_gallery");
      }
    }
  }

  for (const auto& item : voiceprint_) {
    const auto selected = isolated_subminimum_unit_vad_challenge(item);
    if (selected) {
      apply(item, *selected,
            "voiceprint_aligned_unit_isolated_initial_slot_vad_override");
    }
  }
  for (const auto& item : voiceprint_) {
    const auto selected = bracketed_primary_unit_challenge(item);
    if (!selected) continue;
    const std::string label = speaker_label(*selected);
    for (int index = item.source_start; index < item.source_end; ++index) {
      labels[index].speaker = label;
      labels[index].speaker_id = *selected;
      labels[index].reason =
          "primary_speaker_bracketed_aligned_unit_override";
      labels[index].voiceprint = false;
    }
  }
  for (const auto& item : voiceprint_) {
    const auto selected = isolated_vad_aligned_island_challenge(item);
    if (!selected) continue;
    const std::string label = speaker_label(selected->speaker_id);
    for (int index = selected->source_start; index < selected->source_end;
         ++index) {
      labels[index].speaker = label;
      labels[index].speaker_id = selected->speaker_id;
      labels[index].reason =
          "voiceprint_vad_isolated_aligned_island_override";
      labels[index].voiceprint = true;
    }
  }
  for (const auto& primary : primary_speakers_) {
    const auto selected = primary_onset_aligned_island_challenge(primary);
    if (!selected) continue;
    const std::string label = speaker_label(selected->speaker_id);
    for (int index = selected->source_start; index < selected->source_end;
         ++index) {
      labels[index].speaker = label;
      labels[index].speaker_id = selected->speaker_id;
      labels[index].reason =
          "primary_speaker_pause_onset_aligned_island_override";
      labels[index].voiceprint = false;
    }
  }
  for (const auto& item : voiceprint_) {
    const auto selected = isolated_no_vad_interval_challenge(item);
    if (!selected) continue;
    const std::string label = speaker_label(*selected);
    for (int index = item.source_start; index < item.source_end; ++index) {
      labels[index].speaker = label;
      labels[index].speaker_id = *selected;
      labels[index].reason =
          "sortformer_initial_slot_isolated_no_vad_interval_override";
      labels[index].voiceprint = false;
    }
  }
  for (const auto* item : source_evidence) {
    const auto selected = subminimum_native_cross_scale_challenge(*item);
    if (!selected) continue;
    apply(*item, *selected,
          "voiceprint_subminimum_native_cross_scale_restore");
  }
  for (const auto* item : source_evidence) {
    const auto selected = complete_source_aligned_vad_challenge(*item);
    if (!selected) continue;
    apply(*item, *selected,
          "voiceprint_complete_source_aligned_vad_closure");
  }
  for (const auto* item : source_evidence) {
    const auto selected =
        adjacent_primary_supported_prefix_challenge(*item);
    if (!selected) continue;
    const std::string label = speaker_label(selected->speaker_id);
    for (int index = selected->source_start; index < selected->source_end;
         ++index) {
      labels[index].speaker = label;
      labels[index].speaker_id = selected->speaker_id;
      labels[index].reason =
          "voiceprint_adjacent_primary_supported_prefix_restore";
      labels[index].voiceprint = true;
    }
  }

  struct CharacterTime {
    bool available = false;
    double start = 0.0;
    double end = 0.0;
  };
  std::vector<CharacterTime> character_times(character_count);
  const auto alignment = align_.find(text.id);
  if (alignment != align_.end()) {
    std::size_t source_cursor = 0;
    for (const auto& unit : alignment->second.units) {
      const std::vector<std::size_t> unit_offsets = Utf8Offsets(unit.text);
      std::vector<std::size_t> matched_indices;
      for (std::size_t unit_index = 0; unit_index + 1 < unit_offsets.size();
           ++unit_index) {
        const std::string codepoint = unit.text.substr(
            unit_offsets[unit_index],
            unit_offsets[unit_index + 1] - unit_offsets[unit_index]);
        std::size_t found = std::string::npos;
        for (std::size_t source_index = source_cursor;
             source_index + 1 < offsets.size(); ++source_index) {
          if (text.text.substr(offsets[source_index],
                               offsets[source_index + 1] -
                                   offsets[source_index]) == codepoint) {
            found = source_index;
            break;
          }
        }
        if (found == std::string::npos) break;
        matched_indices.push_back(found);
        source_cursor = found + 1;
      }
      if (unit.end > unit.start && !matched_indices.empty()) {
        const double step =
            (unit.end - unit.start) / matched_indices.size();
        for (std::size_t index = 0; index < matched_indices.size(); ++index) {
          character_times[matched_indices[index]] = {
              true, unit.start + index * step,
              unit.start + (index + 1) * step};
        }
      }
    }
  }

  std::vector<int> bounds{0};
  for (int index = 1; index < character_count; ++index) {
    if (labels[index - 1].speaker_id != labels[index].speaker_id ||
        labels[index - 1].reason != labels[index].reason) {
      bounds.push_back(index);
    }
  }
  bounds.push_back(character_count);

  struct ProjectedPiece {
    int source_start = 0;
    int source_end = 0;
    double start = 0.0;
    double end = 0.0;
  };
  std::vector<ProjectedPiece> projected_pieces;
  projected_pieces.reserve(bounds.size() - 1);
  for (std::size_t piece = 0; piece + 1 < bounds.size(); ++piece) {
    const int source_start = bounds[piece];
    const int source_end = bounds[piece + 1];
    double start = 0.0;
    double end = 0.0;
    bool timed = false;
    std::set<std::size_t> owners;
    for (int index = source_start; index < source_end; ++index) {
      owners.insert(labels[index].owner);
      if (!character_times[index].available) continue;
      if (!timed) {
        start = character_times[index].start;
        end = character_times[index].end;
        timed = true;
      } else {
        start = std::min(start, character_times[index].start);
        end = std::max(end, character_times[index].end);
      }
    }
    if (!timed) {
      std::optional<double> previous_end;
      for (int index = source_start - 1; index >= 0; --index) {
        if (character_times[index].available) {
          previous_end = character_times[index].end;
          break;
        }
      }
      std::optional<double> next_start;
      for (int index = source_end; index < character_count; ++index) {
        if (character_times[index].available) {
          next_start = character_times[index].start;
          break;
        }
      }
      double owner_start = entries[*owners.begin()].start;
      double owner_end = entries[*owners.begin()].end;
      for (std::size_t owner : owners) {
        owner_start = std::min(owner_start, entries[owner].start);
        owner_end = std::max(owner_end, entries[owner].end);
      }
      if (previous_end && next_start && *next_start > *previous_end) {
        start = *previous_end;
        end = *next_start;
      } else if (next_start) {
        end = *next_start;
        start = std::min(owner_start, end - 1.0 / time_base_.sample_rate());
      } else if (previous_end) {
        start = *previous_end;
        end = std::max(owner_end, start + 1.0 / time_base_.sample_rate());
      } else {
        start = owner_start;
        end = owner_end;
      }
      timed = end > start;
    }
    if (!timed || end <= start) return entries;
    projected_pieces.push_back({source_start, source_end, start, end});
  }

  const double sample_period = 1.0 / time_base_.sample_rate();
  for (std::size_t index = 1; index < projected_pieces.size(); ++index) {
    auto& previous = projected_pieces[index - 1];
    auto& current = projected_pieces[index];
    if (current.start + 1e-9 >= previous.end) continue;

    const double lower = previous.start + sample_period;
    const double upper = current.end - sample_period;
    if (upper < lower) return entries;
    const double midpoint = 0.5 * (previous.end + current.start);
    const double boundary = std::clamp(midpoint, lower, upper);
    previous.end = boundary;
    current.start = boundary;
  }

  std::vector<Entry> projected;
  projected.reserve(projected_pieces.size());
  for (const auto& piece : projected_pieces) {
    const auto& label = labels[piece.source_start];
    Entry entry = MakeEntry(
        piece.start, piece.end, label.speaker, label.speaker_id,
        text.text.substr(offsets[piece.source_start],
                         offsets[piece.source_end] -
                             offsets[piece.source_start]),
        text.id, "forced_alignment");
    if (label.reason == "primary_speaker_bracketed_aligned_unit_override") {
      entry.speaker_support = "strong";
      entry.speaker_uncertain = false;
      entry.speaker_decision.speaker_source =
          "sortformer_primary_top1+forced_alignment";
      entry.speaker_decision.reason = label.reason;
    } else if (label.reason ==
               "primary_speaker_pause_onset_aligned_island_override") {
      entry.speaker_support = "strong";
      entry.speaker_uncertain = false;
      entry.speaker_decision.speaker_source =
          "sortformer_activity+primary_top1+vad_boundary+forced_alignment";
      entry.speaker_decision.reason = label.reason;
    } else if (label.reason ==
               "sortformer_initial_slot_isolated_no_vad_interval_override") {
      entry.speaker_support = "strong";
      entry.speaker_uncertain = false;
      entry.speaker_decision.speaker_source =
          "sortformer_initial_slot+activity+primary_top1+vad_gap+"
          "forced_alignment";
      entry.speaker_decision.reason = label.reason;
    } else if (label.voiceprint) {
      entry.speaker_support = "strong";
      entry.speaker_uncertain = false;
      if (label.reason ==
          "voiceprint_phrase_initial_slot_four_view_near_tie_override") {
        entry.speaker_decision.speaker_source =
            "sortformer_initial_slot+titanet_phrase+vad_session+"
            "robust_gallery+forced_alignment";
      } else if (label.reason ==
                 "voiceprint_phrase_cross_scale_symmetric_near_tie_override") {
        entry.speaker_decision.speaker_source =
            "sortformer_activity+primary_top1+titanet_phrase+vad_session+"
            "robust_gallery+forced_alignment";
      } else if (
          label.reason ==
          "voiceprint_interval_primary_conflict_vad_abstention_override") {
        entry.speaker_decision.speaker_source =
            "sortformer_activity+primary_top1+titanet_interval+vad_session+"
            "robust_gallery+forced_alignment";
      } else if (label.reason ==
                 "voiceprint_subminimum_native_cross_scale_restore") {
        entry.speaker_decision.speaker_source =
            "sortformer_activity+primary_top1+titanet_phrase+vad_session+"
            "robust_gallery+forced_alignment";
      } else if (
          label.reason ==
          "voiceprint_complete_source_aligned_vad_closure") {
        entry.speaker_decision.speaker_source =
            "sortformer_activity+primary_top1+titanet_complete_source+"
            "titanet_phrase+vad_session+robust_gallery+forced_alignment";
      } else if (
          label.reason ==
          "voiceprint_adjacent_primary_supported_prefix_restore") {
        entry.speaker_decision.speaker_source =
            "sortformer_activity+primary_top1+titanet_adjacent_pair+"
            "robust_gallery+forced_alignment";
      } else if (label.reason.find("initial_slot") != std::string::npos) {
        entry.speaker_decision.speaker_source =
            "sortformer_initial_slot+titanet_session+robust_gallery";
      } else if (label.reason.find("vad_four_view") != std::string::npos) {
        entry.speaker_decision.speaker_source =
            "titanet_phrase+vad_session+robust_gallery";
      } else if (label.reason.find("vad_isolated_aligned_island") !=
                 std::string::npos) {
        entry.speaker_decision.speaker_source =
            "titanet_vad_session+robust_gallery+forced_alignment";
      } else {
        entry.speaker_decision.speaker_source =
            label.reason.find("dual_gallery") != std::string::npos
                ? "titanet_session+robust_gallery"
                : "titanet_session_gallery";
      }
      entry.speaker_decision.reason = label.reason;
    }
    projected.push_back(std::move(entry));
  }
  return projected.empty() ? entries : projected;
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
