#include "pipeline/speaker_evidence_stage.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "pipeline/speaker_identity_stage.h"

namespace orator {
namespace pipeline {
namespace {

using Timeline = ComprehensiveTimeline;

std::vector<std::string> Utf8Codepoints(const std::string& text) {
  std::vector<std::string> output;
  for (std::size_t offset = 0; offset < text.size();) {
    const unsigned char byte = static_cast<unsigned char>(text[offset]);
    std::size_t length = 1;
    if (byte >= 0xF0) {
      length = 4;
    } else if (byte >= 0xE0) {
      length = 3;
    } else if (byte >= 0xC0) {
      length = 2;
    }
    if (offset + length > text.size()) length = 1;
    output.push_back(text.substr(offset, length));
    offset += length;
  }
  return output;
}

struct CharacterTime {
  bool available = false;
  double start = 0.0;
  double end = 0.0;
};

std::optional<std::vector<CharacterTime>> AlignCharacters(
    const std::vector<std::string>& source,
    const std::vector<Timeline::AlignUnitSeg>& units) {
  std::vector<CharacterTime> times(source.size());
  std::size_t cursor = 0;
  for (const auto& unit : units) {
    for (const auto& codepoint : Utf8Codepoints(unit.text)) {
      auto found = std::find(source.begin() + static_cast<long>(cursor),
                             source.end(), codepoint);
      if (found == source.end()) return std::nullopt;
      const std::size_t index =
          static_cast<std::size_t>(std::distance(source.begin(), found));
      times[index] = {unit.end > unit.start, unit.start, unit.end};
      cursor = index + 1;
    }
  }
  return times;
}

std::vector<std::pair<int, int>> PhraseRanges(
    const std::vector<std::string>& source,
    const std::set<std::string>& punctuation) {
  std::vector<std::pair<int, int>> ranges;
  int start = 0;
  auto visible = [&](int begin, int end) {
    for (int index = begin; index < end; ++index) {
      if (punctuation.count(source[index]) == 0 &&
          source[index] != " " && source[index] != "\t" &&
          source[index] != "\n" && source[index] != "\r") {
        return true;
      }
    }
    return false;
  };
  for (int index = 0; index < static_cast<int>(source.size()); ++index) {
    if (punctuation.count(source[index]) == 0) continue;
    if (visible(start, index + 1)) ranges.push_back({start, index + 1});
    start = index + 1;
  }
  if (start < static_cast<int>(source.size()) &&
      visible(start, static_cast<int>(source.size()))) {
    ranges.push_back({start, static_cast<int>(source.size())});
  }
  return ranges;
}

bool IsWhitespace(const std::string& codepoint) {
  return codepoint == " " || codepoint == "\t" || codepoint == "\n" ||
         codepoint == "\r";
}

std::vector<std::string> VisibleCodepoints(
    const std::vector<std::string>& source, int source_start, int source_end,
    const std::set<std::string>& punctuation) {
  std::vector<std::string> output;
  for (int index = source_start; index < source_end; ++index) {
    if (punctuation.count(source[index]) == 0 &&
        !IsWhitespace(source[index])) {
      output.push_back(source[index]);
    }
  }
  return output;
}

bool IsStrictVisibleSuffix(const std::vector<std::string>& source,
                           const std::pair<int, int>& preceding,
                           const std::pair<int, int>& following,
                           const std::set<std::string>& punctuation) {
  const auto preceding_visible = VisibleCodepoints(
      source, preceding.first, preceding.second, punctuation);
  const auto following_visible = VisibleCodepoints(
      source, following.first, following.second, punctuation);
  return !following_visible.empty() &&
         following_visible.size() < preceding_visible.size() &&
         std::equal(following_visible.rbegin(), following_visible.rend(),
                    preceding_visible.rbegin());
}

std::optional<std::pair<double, double>> TimeForRange(
    const std::vector<CharacterTime>& times, int source_start,
    int source_end) {
  double start = 0.0;
  double end = 0.0;
  bool found = false;
  for (int index = source_start; index < source_end; ++index) {
    if (!times[index].available) continue;
    if (!found) {
      start = times[index].start;
      end = times[index].end;
      found = true;
    } else {
      start = std::min(start, times[index].start);
      end = std::max(end, times[index].end);
    }
  }
  if (!found || end <= start) return std::nullopt;
  return std::make_pair(start, end);
}

std::vector<std::string> ActiveSpeakerIds(
    const std::vector<Timeline::SpeakerInput>& diarization) {
  std::set<std::string> ids;
  for (const auto& segment : diarization) {
    if (!segment.speaker_id.empty()) ids.insert(segment.speaker_id);
  }
  return {ids.begin(), ids.end()};
}

Timeline::SpeakerVoiceprintEvidence MakeQuery(const std::string& evidence_id,
                                              const std::string& kind,
                                              long text_id, int source_start,
                                              int source_end, double start,
                                              double end) {
  Timeline::SpeakerVoiceprintEvidence output;
  output.evidence_id = evidence_id;
  output.kind = kind;
  output.text_id = text_id;
  output.source_start = source_start;
  output.source_end = source_end;
  output.start = start;
  output.end = end;
  return output;
}

Timeline::SpeakerVoiceprintEvidence ConvertEvidence(
    const std::string& evidence_id, const std::string& kind, long text_id,
    int source_start, int source_end, double start, double end,
    const SpeakerIdentityStage::SpanEvidence& source) {
  Timeline::SpeakerVoiceprintEvidence output;
  output.evidence_id = evidence_id;
  output.kind = kind;
  output.text_id = text_id;
  output.source_start = source_start;
  output.source_end = source_end;
  output.start = start;
  output.end = end;
  output.embedding_available = source.embedding_available;
  output.session_gallery_complete = source.session_gallery_complete;
  output.robust_gallery_complete = source.robust_gallery_complete;
  for (const auto& score : source.session_scores) {
    output.session_scores.push_back({score.speaker_id, score.score});
  }
  for (const auto& score : source.robust_scores) {
    output.robust_scores.push_back({score.speaker_id, score.score});
  }
  return output;
}

}  // namespace

SpeakerEvidenceStage::SpeakerEvidenceStage(SpeakerIdentityStage* identity,
                                           Config config)
    : identity_(identity), config_(std::move(config)) {
  if (identity_ == nullptr) {
    throw std::invalid_argument("SpeakerEvidenceStage requires identity stage");
  }
  config_.min_embed_sec = std::max(0.0, config_.min_embed_sec);
  config_.edge_margin_sec = std::max(0.0, config_.edge_margin_sec);
  config_.max_embed_window_sec =
      std::max(config_.min_embed_sec, config_.max_embed_window_sec);
  config_.phrase_min_sec =
      std::max(config_.min_embed_sec, config_.phrase_min_sec);
  config_.phrase_max_sec =
      std::max(config_.phrase_min_sec, config_.phrase_max_sec);
  config_.short_max_sec =
      std::max(config_.min_embed_sec, config_.short_max_sec);
  config_.boundary_tolerance_sec =
      std::max(0.0, config_.boundary_tolerance_sec);
  config_.minimum_gallery_size = std::max(2, config_.minimum_gallery_size);
  config_.precompute_interval_sec =
      std::max(0.0, config_.precompute_interval_sec);
  config_.precompute_max_spans_per_cycle =
      std::max(1, config_.precompute_max_spans_per_cycle);
}

SpeakerEvidenceStage::~SpeakerEvidenceStage() { StopPrecompute(false); }

void SpeakerEvidenceStage::StartPrecompute(ComprehensiveTimeline* timeline,
                                           std::function<bool()> ready) {
  if (!config_.enabled || config_.precompute_interval_sec <= 0.0 ||
      timeline == nullptr) {
    return;
  }
  std::lock_guard<std::mutex> lock(precompute_mutex_);
  if (precompute_thread_.joinable()) return;
  timeline_ = timeline;
  precompute_ready_ = std::move(ready);
  precompute_stop_ = false;
  precompute_drain_ = false;
  precomputed_spans_.clear();
  precompute_thread_ = std::thread([this] { PrecomputeLoop(); });
}

void SpeakerEvidenceStage::StopPrecompute(bool drain) {
  {
    std::lock_guard<std::mutex> lock(precompute_mutex_);
    if (!precompute_thread_.joinable()) return;
    precompute_stop_ = true;
    precompute_drain_ = drain;
  }
  precompute_cv_.notify_all();
  precompute_thread_.join();
  std::lock_guard<std::mutex> lock(precompute_mutex_);
  timeline_ = nullptr;
  precompute_ready_ = {};
}

std::size_t SpeakerEvidenceStage::precomputed_span_count() const {
  std::lock_guard<std::mutex> lock(precompute_mutex_);
  return precomputed_spans_.size();
}

void SpeakerEvidenceStage::PrecomputeLoop() {
  const auto interval =
      std::chrono::duration<double>(config_.precompute_interval_sec);
  for (;;) {
    bool stop = false;
    bool drain = false;
    ComprehensiveTimeline* timeline = nullptr;
    std::function<bool()> ready;
    {
      std::unique_lock<std::mutex> lock(precompute_mutex_);
      precompute_cv_.wait_for(lock, interval,
                              [this] { return precompute_stop_; });
      stop = precompute_stop_;
      drain = precompute_drain_;
      timeline = timeline_;
      ready = precompute_ready_;
    }
    if (timeline != nullptr && (!stop || drain) &&
        (drain || !ready || ready())) {
      const std::size_t max_spans =
          drain ? 0
                : static_cast<std::size_t>(
                      config_.precompute_max_spans_per_cycle);
      Precompute(timeline->SnapshotSpeakerEvidenceInputs(), max_spans);
    }
    if (stop) return;
  }
}

std::vector<std::pair<int, int>> SpeakerEvidenceStage::SplitPartialPhraseEdges(
    int source_start, int source_end,
    const std::vector<std::pair<int, int>>& phrase_ranges) {
  if (source_end <= source_start) return {};
  std::vector<int> bounds{source_start, source_end};
  for (const auto& phrase : phrase_ranges) {
    if (source_start > phrase.first && source_start < phrase.second &&
        phrase.second < source_end) {
      bounds.push_back(phrase.second);
    }
    if (source_end > phrase.first && source_end < phrase.second &&
        phrase.first > source_start) {
      bounds.push_back(phrase.first);
    }
  }
  std::sort(bounds.begin(), bounds.end());
  bounds.erase(std::unique(bounds.begin(), bounds.end()), bounds.end());
  std::vector<std::pair<int, int>> output;
  output.reserve(bounds.size() - 1);
  for (std::size_t index = 0; index + 1 < bounds.size(); ++index) {
    if (bounds[index + 1] > bounds[index]) {
      output.push_back({bounds[index], bounds[index + 1]});
    }
  }
  return output;
}

std::vector<Timeline::SpeakerVoiceprintEvidence>
SpeakerEvidenceStage::BuildAdjacentBusinessPairs(
    const std::vector<Timeline::SpeakerVoiceprintEvidence>& intervals,
    double min_embed_sec, double short_max_sec) {
  std::map<long, std::vector<const Timeline::SpeakerVoiceprintEvidence*>>
      by_text;
  for (const auto& interval : intervals) {
    if (interval.kind == "business_interval" &&
        interval.source_end > interval.source_start &&
        interval.end > interval.start) {
      by_text[interval.text_id].push_back(&interval);
    }
  }

  std::vector<Timeline::SpeakerVoiceprintEvidence> output;
  for (auto& [text_id, text_intervals] : by_text) {
    std::stable_sort(text_intervals.begin(), text_intervals.end(),
                     [](const auto* left, const auto* right) {
                       if (left->source_start != right->source_start) {
                         return left->source_start < right->source_start;
                       }
                       return left->source_end < right->source_end;
                     });
    int ordinal = 0;
    for (std::size_t index = 0; index + 1 < text_intervals.size(); ++index) {
      const auto& leading = *text_intervals[index];
      const auto& following = *text_intervals[index + 1];
      const double leading_duration = leading.end - leading.start;
      const double following_duration = following.end - following.start;
      const double combined_duration = following.end - leading.start;
      if (leading.source_end != following.source_start ||
          std::abs(leading.end - following.start) > 1e-6 ||
          leading_duration + 1e-9 >= min_embed_sec ||
          following_duration + 1e-9 < min_embed_sec ||
          combined_duration + 1e-9 < min_embed_sec ||
          combined_duration + 1e-9 >= short_max_sec) {
        continue;
      }
      Timeline::SpeakerVoiceprintEvidence pair;
      pair.evidence_id = "adjacent_business_pair:" +
                         std::to_string(text_id) + ":" +
                         std::to_string(ordinal++);
      pair.kind = "adjacent_business_pair";
      pair.text_id = text_id;
      pair.source_start = leading.source_start;
      pair.source_end = following.source_end;
      pair.start = leading.start;
      pair.end = following.end;
      output.push_back(std::move(pair));
    }
  }
  return output;
}

std::vector<Timeline::SpeakerInput>
SpeakerEvidenceStage::BuildPrimarySpeaker(
    const Timeline::TrackSnapshot& snapshot) {
  std::vector<Timeline::SpeakerInput> output;
  if (!config_.enabled) return output;

  // Primary raw Sortformer view. Consecutive frames are coalesced only when
  // their reset-aware local slot and resolved global identity are unchanged.
  struct PrimaryRun {
    double start = 0.0;
    double end = 0.0;
    int local = -1;
    std::string speaker_id;
    double confidence_total = 0.0;
    int frame_count = 0;
  };
  std::optional<PrimaryRun> run;
  auto flush_primary = [&] {
    if (!run || run->end <= run->start) return;
    output.push_back(
        {run->start, run->end,
         "speaker_" + std::to_string(run->local),
         static_cast<float>(run->confidence_total / run->frame_count),
         run->speaker_id});
    run.reset();
  };
  for (const auto& block : snapshot.diar_frames) {
    for (int frame = 0; frame < block.num_frames; ++frame) {
      int best_slot = 0;
      float best_score = block.probabilities[
          static_cast<std::size_t>(frame) * block.num_speakers];
      for (int slot = 1; slot < block.num_speakers; ++slot) {
        const float score = block.probabilities[
            static_cast<std::size_t>(frame) * block.num_speakers + slot];
        if (score > best_score) {
          best_slot = slot;
          best_score = score;
        }
      }
      const double start = block.start + frame * block.frame_period_sec;
      const double end = start + block.frame_period_sec;
      if (best_score + 1e-9f < config_.frame_activity_threshold) {
        flush_primary();
        continue;
      }
      const int local = block.local_speaker_offset + best_slot;
      const std::string speaker_id =
          identity_->IdentityAt(local, 0.5 * (start + end));
      if (speaker_id.empty()) {
        flush_primary();
        continue;
      }
      if (!run || run->local != local || run->speaker_id != speaker_id ||
          std::abs(run->end - start) > 1e-6) {
        flush_primary();
        run = PrimaryRun{start, end, local, speaker_id, best_score, 1};
      } else {
        run->end = end;
        run->confidence_total += best_score;
        ++run->frame_count;
      }
    }
  }
  flush_primary();
  return output;
}

std::vector<Timeline::SpeakerVoiceprintEvidence>
SpeakerEvidenceStage::BuildVoiceprintQueries(
    const Timeline::SpeakerEvidenceSnapshot& snapshot) const {
  std::vector<Timeline::SpeakerVoiceprintEvidence> output;
  if (!config_.enabled) return output;

  std::map<long, Timeline::AlignGroup> align_by_text;
  for (const auto& group : snapshot.align) align_by_text[group.text_id] = group;
  std::map<long, std::vector<std::string>> source_by_text;
  std::map<long, std::vector<CharacterTime>> character_times_by_text;
  for (const auto& text : snapshot.asr) {
    const auto alignment = align_by_text.find(text.id);
    if (alignment == align_by_text.end()) continue;
    std::vector<std::string> source = Utf8Codepoints(text.text);
    auto times = AlignCharacters(source, alignment->second.units);
    if (!times) continue;
    source_by_text.emplace(text.id, std::move(source));
    character_times_by_text.emplace(text.id, std::move(*times));
  }

  const std::vector<std::string> punctuation_codepoints =
      Utf8Codepoints(config_.punctuation);
  const std::set<std::string> punctuation(punctuation_codepoints.begin(),
                                          punctuation_codepoints.end());
  std::map<long, std::vector<std::pair<int, int>>> phrase_ranges_by_text;
  for (const auto& [text_id, source] : source_by_text) {
    phrase_ranges_by_text.emplace(text_id, PhraseRanges(source, punctuation));
  }

  std::map<long, std::vector<Timeline::Entry>> entries_by_text;
  for (const auto& entry : snapshot.business_speaker) {
    entries_by_text[entry.text_id].push_back(entry);
  }
  std::vector<Timeline::SpeakerVoiceprintEvidence> business_intervals;
  for (auto& [text_id, entries] : entries_by_text) {
    std::stable_sort(entries.begin(), entries.end(),
                     [](const Timeline::Entry& left,
                        const Timeline::Entry& right) {
                       return left.start < right.start;
                     });
    int source_cursor = 0;
    int ordinal = 0;
    for (const auto& entry : entries) {
      const int codepoints =
          static_cast<int>(Utf8Codepoints(entry.text).size());
      const int source_end = source_cursor + codepoints;
      std::vector<std::pair<int, int>> query_ranges = {
          {source_cursor, source_end}};
      const auto phrase_ranges = phrase_ranges_by_text.find(text_id);
      if (phrase_ranges != phrase_ranges_by_text.end()) {
        query_ranges = SplitPartialPhraseEdges(
            source_cursor, source_end, phrase_ranges->second);
      }
      for (const auto& range : query_ranges) {
        double query_start = entry.start;
        double query_end = entry.end;
        const auto times = character_times_by_text.find(text_id);
        if (times != character_times_by_text.end() && range.first >= 0 &&
            range.second <= static_cast<int>(times->second.size())) {
          const auto aligned =
              TimeForRange(times->second, range.first, range.second);
          if (aligned) {
            query_start = aligned->first;
            query_end = aligned->second;
          }
        }
        auto query = MakeQuery("business_interval:" + std::to_string(text_id) +
                                   ":" + std::to_string(ordinal++),
                               "business_interval", text_id, range.first,
                               range.second, query_start, query_end);
        business_intervals.push_back(query);
        output.push_back(std::move(query));
      }
      source_cursor = source_end;
    }
  }
  for (auto pair : BuildAdjacentBusinessPairs(
           business_intervals, config_.min_embed_sec, config_.short_max_sec)) {
    output.push_back(std::move(pair));
  }

  std::vector<Timeline::SpeakerInput> primary = snapshot.primary_speaker;
  std::stable_sort(primary.begin(), primary.end(),
                   [](const auto& left, const auto& right) {
                     if (left.start != right.start) {
                       return left.start < right.start;
                     }
                     return left.end < right.end;
                   });

  for (const auto& text : snapshot.asr) {
    const auto alignment = align_by_text.find(text.id);
    const auto source_position = source_by_text.find(text.id);
    const auto times_position = character_times_by_text.find(text.id);
    if (alignment == align_by_text.end() ||
        source_position == source_by_text.end() ||
        times_position == character_times_by_text.end()) {
      continue;
    }
    const auto& source = source_position->second;
    const auto& character_times = times_position->second;

    int unit_ordinal = 0;
    std::size_t unit_source_cursor = 0;
    for (const auto& unit : alignment->second.units) {
      if (unit.end <= unit.start) continue;
      const auto unit_text = Utf8Codepoints(unit.text);
      auto found = std::search(
          source.begin() + static_cast<long>(unit_source_cursor), source.end(),
          unit_text.begin(), unit_text.end());
      if (found == source.end()) continue;
      const int source_start =
          static_cast<int>(std::distance(source.begin(), found));
      const int source_end = source_start + static_cast<int>(unit_text.size());
      unit_source_cursor = static_cast<std::size_t>(source_end);
      output.push_back(MakeQuery("aligned_unit:" + std::to_string(text.id) +
                                     ":" + std::to_string(unit_ordinal++),
                                 "aligned_unit", text.id, source_start,
                                 source_end, unit.start, unit.end));
    }

    int phrase_ordinal = 0;
    const auto phrase_ranges = phrase_ranges_by_text.find(text.id);
    if (phrase_ranges == phrase_ranges_by_text.end()) continue;
    for (const auto& range : phrase_ranges->second) {
      const auto time = TimeForRange(character_times, range.first, range.second);
      if (!time) continue;
      const double duration = time->second - time->first;
      if (duration + 1e-9 < config_.phrase_min_sec ||
          duration > config_.phrase_max_sec + 1e-9) {
        continue;
      }
      output.push_back(
          MakeQuery("punctuation_phrase:" + std::to_string(text.id) + ":" +
                        std::to_string(phrase_ordinal++),
                    "punctuation_phrase", text.id, range.first, range.second,
                    time->first, time->second));
    }

    output.push_back(MakeQuery(
        "complete_source:" + std::to_string(text.id), "complete_source",
        text.id, 0, static_cast<int>(source.size()), text.start, text.end));

    struct EchoCandidate {
      int source_start = 0;
      int source_end = 0;
      double start = 0.0;
      double end = 0.0;
    };
    std::vector<EchoCandidate> echo_candidates;
    bool echo_ambiguous = false;
    for (std::size_t phrase_index = 0;
         phrase_index + 1 < phrase_ranges->second.size(); ++phrase_index) {
      const auto& preceding = phrase_ranges->second[phrase_index];
      const auto& following = phrase_ranges->second[phrase_index + 1];
      if (preceding.second != following.first ||
          !IsStrictVisibleSuffix(source, preceding, following, punctuation) ||
          !TimeForRange(character_times, following.first, following.second)) {
        continue;
      }

      struct Candidate {
        double start = 0.0;
        double end = 0.0;
      };
      std::vector<Candidate> candidates;
      std::vector<int> positive_indices;
      for (int index = preceding.first; index < preceding.second; ++index) {
        if (character_times[index].available) positive_indices.push_back(index);
      }
      for (std::size_t time_index = 0;
           time_index + 1 < positive_indices.size(); ++time_index) {
        const auto& left = character_times[positive_indices[time_index]];
        const auto& right = character_times[positive_indices[time_index + 1]];
        if (right.start <= left.end + 1e-9) continue;
        for (std::size_t primary_index = 1;
             primary_index + 1 < primary.size(); ++primary_index) {
          const auto& outer_left = primary[primary_index - 1];
          const auto& middle = primary[primary_index];
          const auto& outer_right = primary[primary_index + 1];
          const double middle_duration = middle.end - middle.start;
          if (outer_left.speaker_id.empty() || middle.speaker_id.empty() ||
              outer_left.speaker_id != outer_right.speaker_id ||
              outer_left.speaker_id == middle.speaker_id ||
              std::abs(outer_left.end - middle.start) >
                  config_.boundary_tolerance_sec + 1e-9 ||
              std::abs(middle.end - outer_right.start) >
                  config_.boundary_tolerance_sec + 1e-9 ||
              outer_left.end - outer_left.start + 1e-9 <
                  config_.min_embed_sec ||
              outer_right.end - outer_right.start + 1e-9 <
                  config_.min_embed_sec ||
              middle_duration + 1e-9 < config_.min_embed_sec ||
              middle_duration + 1e-9 >= config_.short_max_sec ||
              middle.start + 1e-9 < left.end ||
              middle.end > right.start + 1e-9) {
            continue;
          }
          candidates.push_back({middle.start, middle.end});
        }
      }
      if (candidates.empty()) continue;
      if (candidates.size() != 1) {
        echo_ambiguous = true;
        break;
      }
      echo_candidates.push_back(
          {following.first, following.second, candidates.front().start,
           candidates.front().end});
    }
    if (!echo_ambiguous && echo_candidates.size() == 1) {
      const auto& candidate = echo_candidates.front();
      output.push_back(MakeQuery(
          "primary_alignment_gap_echo:" + std::to_string(text.id) + ":0",
          "primary_alignment_gap_echo", text.id, candidate.source_start,
          candidate.source_end, candidate.start, candidate.end));
    }
  }

  int vad_ordinal = 0;
  for (const auto& vad : snapshot.vad) {
    if (vad.end - vad.start + 1e-9 < config_.min_embed_sec) continue;
    output.push_back(MakeQuery("vad:" + std::to_string(vad_ordinal++), "vad",
                               -1, 0, 0, vad.start, vad.end));
  }
  return output;
}

void SpeakerEvidenceStage::Precompute(
    const Timeline::SpeakerEvidenceSnapshot& snapshot, std::size_t max_spans) {
  if (ActiveSpeakerIds(snapshot.diarization).size() <
      static_cast<std::size_t>(config_.minimum_gallery_size)) {
    return;
  }
  std::size_t computed = 0;
  for (const auto& query : BuildVoiceprintQueries(snapshot)) {
    if (query.end - query.start + 1e-9 < config_.min_embed_sec) continue;
    const auto key =
        std::make_pair(static_cast<long>(std::llround(query.start * 1000000.0)),
                       static_cast<long>(std::llround(query.end * 1000000.0)));
    {
      std::lock_guard<std::mutex> lock(precompute_mutex_);
      if (precomputed_spans_.count(key) != 0) continue;
    }
    if (!identity_->PrecomputeSpan(
            query.start, query.end, config_.min_embed_sec,
            config_.edge_margin_sec, config_.max_embed_window_sec)) {
      continue;
    }
    std::lock_guard<std::mutex> lock(precompute_mutex_);
    precomputed_spans_.insert(key);
    ++computed;
    if (max_spans > 0 && computed >= max_spans) return;
  }
}

std::vector<Timeline::SpeakerVoiceprintEvidence>
SpeakerEvidenceStage::BuildVoiceprint(
    const Timeline::SpeakerEvidenceSnapshot& snapshot) {
  std::vector<Timeline::SpeakerVoiceprintEvidence> output;
  if (!config_.enabled) return output;
  const std::vector<std::string> active_ids =
      ActiveSpeakerIds(snapshot.diarization);
  if (active_ids.size() <
      static_cast<std::size_t>(config_.minimum_gallery_size)) {
    return output;
  }

  using SpanKey = std::pair<long, long>;
  std::map<SpanKey, SpeakerIdentityStage::SpanEvidence> cache;
  auto evaluate = [&](double start, double end)
      -> const SpeakerIdentityStage::SpanEvidence& {
    const SpanKey key = {static_cast<long>(std::llround(start * 1000000.0)),
                         static_cast<long>(std::llround(end * 1000000.0))};
    auto [position, inserted] = cache.emplace(
        key, SpeakerIdentityStage::SpanEvidence{});
    if (inserted) {
      position->second = identity_->EvaluateSpan(
          start, end, active_ids, config_.min_embed_sec,
          config_.edge_margin_sec, config_.max_embed_window_sec);
    }
    return position->second;
  };

  for (const auto& query : BuildVoiceprintQueries(snapshot)) {
    output.push_back(ConvertEvidence(query.evidence_id, query.kind,
                                     query.text_id, query.source_start,
                                     query.source_end, query.start, query.end,
                                     evaluate(query.start, query.end)));
  }
  return output;
}

}  // namespace pipeline
}  // namespace orator
