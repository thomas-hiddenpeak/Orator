#include "pipeline/business_speaker_pipeline.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
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
        a[i].speaker_uncertain != b[i].speaker_uncertain) {
      return false;
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
  texts_.clear();
  align_.clear();
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
  texts_.reserve(snapshot.asr.size());
  for (const auto& segment : snapshot.asr) {
    texts_.push_back({segment.id, segment.start, segment.end, segment.text});
  }
  for (const auto& group : snapshot.align) {
    align_[group.text_id] = group;
  }
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
  double best_span = 0.0;
  float best_confidence = -1.0f;
  const SpeakerSeg* best = nullptr;
  for (const auto& segment : speakers_) {
    const double overlap = Overlap(start, end, segment.start, segment.end);
    if (overlap <= 0.0) continue;
    const double span = segment.end - segment.start;
    bool better = overlap > best_overlap + 1e-9;
    if (!better && overlap > best_overlap - 1e-9 && best != nullptr) {
      if (span < best_span - 1e-9 ||
          (span < best_span + 1e-9 && segment.conf > best_confidence)) {
        better = true;
      }
    }
    if (better) {
      best_overlap = overlap;
      best_span = span;
      best_confidence = segment.conf;
      best = &segment;
    }
  }
  if (best == nullptr) return {"unknown", ""};
  return {best->speaker, best->speaker_id};
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

BusinessSpeakerPipeline::Entry BusinessSpeakerPipeline::MakeEntry(
    double start, double end, const std::string& speaker,
    const std::string& speaker_id, std::string text, long text_id) const {
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
  std::vector<double> bounds;
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
  std::sort(bounds.begin(), bounds.end());
  bounds.erase(std::unique(bounds.begin(), bounds.end(),
                           [](double a, double b) { return NearEqual(a, b); }),
               bounds.end());

  struct Turn {
    double start = 0.0;
    double end = 0.0;
    std::string speaker;
    std::string speaker_id;
  };
  std::vector<Turn> turns;
  turns.reserve(bounds.size());
  for (std::size_t i = 0; i + 1 < bounds.size(); ++i) {
    const double start = bounds[i];
    const double end = bounds[i + 1];
    if (end - start <= 1e-9) continue;
    const SpeakerAttr attribution = AttributeInterval(start, end);
    if (!turns.empty() && turns.back().speaker == attribution.speaker &&
        turns.back().speaker_id == attribution.speaker_id) {
      turns.back().end = end;
    } else {
      turns.push_back(
          {start, end, attribution.speaker, attribution.speaker_id});
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
          merged.back().speaker_id == turn.speaker_id) {
        merged.back().end = turn.end;
      } else {
        merged.push_back(turn);
      }
    }
    turns.swap(merged);
  }

  if (turns.empty()) {
    return {MakeEntry(text.start, text.end, "unknown", "", text.text, text.id)};
  }
  if (turns.size() == 1) {
    return {MakeEntry(turns[0].start, turns[0].end, turns[0].speaker,
                      turns[0].speaker_id, text.text, text.id)};
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

    std::size_t i = 0;
    while (i < units.size()) {
      std::size_t end = i;
      if (config_.align_snap_pause_sec > 0.0) {
        while (end + 1 < units.size()) {
          const double gap = units[end + 1].start - units[end].end;
          if (gap > config_.align_snap_pause_sec ||
              boundary_near_gap(units[end].end, units[end + 1].start)) {
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
                                    (*slices)[turn], text.id));
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
                                std::move(slice), text.id));
    used = end;
  }
  return entries;
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
