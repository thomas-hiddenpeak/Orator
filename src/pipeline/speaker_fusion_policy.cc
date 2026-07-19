#include "speaker_fusion_policy.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <set>
#include <utility>

#include "business_speaker_utils.h"

namespace orator {
namespace pipeline {

using business_speaker_internal::CoveredDuration;
using business_speaker_internal::MergeIntervals;
using business_speaker_internal::NearEqual;
using business_speaker_internal::Overlap;
using business_speaker_internal::Utf8Offsets;

std::vector<BusinessSpeakerPipeline::Entry> SpeakerFusionPolicy::Apply(
    const BusinessSpeakerPipeline& pipeline,
    const BusinessSpeakerPipeline::TextSeg& text,
    std::vector<BusinessSpeakerPipeline::Entry> entries) {
  using Entry = BusinessSpeakerPipeline::Entry;
  using SpeakerSeg = BusinessSpeakerPipeline::SpeakerSeg;
  using SpeakerVoiceprintEvidence =
      BusinessSpeakerPipeline::SpeakerVoiceprintEvidence;

  if (entries.empty() || text.text.empty()) return entries;
  std::string reconstructed;
  for (const auto& entry : entries) reconstructed += entry.text;
  if (reconstructed != text.text) return entries;

  const std::vector<std::size_t> offsets = Utf8Offsets(text.text);
  const int character_count = static_cast<int>(offsets.size()) - 1;
  struct CharacterLabel {
    std::string speaker;
    std::string speaker_id;
    std::string base_speaker_id;
    std::string base_reason;
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
                        entries[owner].speaker_id,
                        entries[owner].speaker_decision.reason,
                        entries[owner].speaker_decision.reason, owner, false});
    }
  }
  if (static_cast<int>(labels.size()) != character_count) return entries;

  struct CharacterTime {
    bool available = false;
    double start = 0.0;
    double end = 0.0;
  };
  std::vector<CharacterTime> character_times(character_count);
  const auto alignment = pipeline.align_.find(text.id);
  if (alignment != pipeline.align_.end()) {
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

  struct Selection {
    std::string speaker_id;
    float score = 0.0f;
    float margin = 0.0f;
    bool score_pass = false;
    bool margin_pass = false;
  };
  struct RankedPair {
    Selection first;
    std::string second_id;
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
    const bool short_span = duration < pipeline.config_.voiceprint_short_max_sec;
    const float score_gate = short_span
                                 ? pipeline.config_.voiceprint_short_min_score
                                 : pipeline.config_.voiceprint_regular_min_score;
    const float margin_gate = short_span
                                  ? pipeline.config_.voiceprint_short_min_margin
                                  : pipeline.config_.voiceprint_regular_min_margin;
    const float margin = ranked[0].score - ranked[1].score;
    return Selection{ranked[0].speaker_id, ranked[0].score, margin,
                     ranked[0].score + 1e-9f >= score_gate,
                     margin + 1e-9f >= margin_gate};
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
  auto has_minimum_aligned_units = [&](double start, double end) {
    const auto alignment = pipeline.align_.find(text.id);
    if (alignment == pipeline.align_.end()) return false;
    int aligned_unit_count = 0;
    for (const auto& unit : alignment->second.units) {
      if (unit.end > unit.start && unit.start + 1e-9 >= start &&
          unit.end <= end + 1e-9) {
        ++aligned_unit_count;
      }
    }
    return aligned_unit_count >=
           pipeline.config_.voiceprint_four_view_min_aligned_units;
  };

  auto speaker_label = [&](const std::string& speaker_id) {
    for (const auto& segment : pipeline.speakers_) {
      if (segment.speaker_id == speaker_id) return segment.speaker;
    }
    for (const auto& segment : pipeline.primary_speakers_) {
      if (segment.speaker_id == speaker_id) return segment.speaker;
    }
    return std::string("speaker_voiceprint");
  };

  std::map<std::string, std::pair<double, std::string>> initial_identities;
  for (const auto& segment : pipeline.speakers_) {
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
  const auto text_evidence_position =
      pipeline.voiceprint_by_text_.find(text.id);
  const std::vector<SpeakerVoiceprintEvidence> empty_text_evidence;
  const auto& text_voiceprint =
      text_evidence_position == pipeline.voiceprint_by_text_.end()
          ? empty_text_evidence
          : text_evidence_position->second;
  for (const auto& item : text_voiceprint) {
    if (item.source_start < 0 || item.source_end > character_count ||
        item.source_end <= item.source_start) {
      continue;
    }
    source_evidence.push_back(&item);
    if (item.embedding_available) evidence.push_back(&item);
  }
  std::vector<const SpeakerVoiceprintEvidence*> relevant_voiceprint;
  relevant_voiceprint.reserve(pipeline.voiceprint_vad_.size() +
                              text_voiceprint.size());
  for (const auto& item : pipeline.voiceprint_vad_) {
    relevant_voiceprint.push_back(&item);
  }
  for (const auto& item : text_voiceprint) {
    relevant_voiceprint.push_back(&item);
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
  auto has_sustained_native_handoff = [&](const auto& item,
                                          const std::string& selected) {
    struct NativeRun {
      std::string speaker_id;
      int source_start = 0;
      int source_end = 0;
    };
    std::vector<NativeRun> runs;
    for (int index = item.source_start; index < item.source_end; ++index) {
      const std::string& speaker_id = labels[index].base_speaker_id;
      if (speaker_id.empty()) return false;
      if (runs.empty() || runs.back().speaker_id != speaker_id) {
        runs.push_back({speaker_id, index, index + 1});
      } else {
        runs.back().source_end = index + 1;
      }
    }
    if (runs.size() != 2 ||
        (runs[0].speaker_id != selected &&
         runs[1].speaker_id != selected)) {
      return false;
    }
    const NativeRun& competing =
        runs[0].speaker_id == selected ? runs[1] : runs[0];

    std::vector<std::pair<double, double>> aligned_intervals;
    std::set<std::size_t> competing_owners;
    for (int index = competing.source_start; index < competing.source_end;
         ++index) {
      competing_owners.insert(labels[index].owner);
      if (character_times[index].available) {
        aligned_intervals.push_back(
            {character_times[index].start, character_times[index].end});
      }
    }
    for (const std::size_t owner : competing_owners) {
      const auto& base = entries[owner];
      if (base.speaker_id != competing.speaker_id ||
          (base.speaker_decision.reason != "primary_speaker_tie_break" &&
           base.speaker_decision.reason !=
               "primary_speaker_overlap_refinement")) {
        return false;
      }
    }
    return CoveredDuration(MergeIntervals(std::move(aligned_intervals))) +
               1e-9 >=
           pipeline.config_.voiceprint_primary_consensus_min_sec;
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
  const auto corroborated_handoffs =
      pipeline.FindCorroboratedStraddledHandoffs(text);
  auto preserves_corroborated_handoff = [&](const auto& item,
                                            const std::string& selected) {
    if (item.kind != "business_interval" || corroborated_handoffs.empty()) {
      return false;
    }

    struct BaseRun {
      std::string speaker_id;
      int source_start = 0;
      int source_end = 0;
    };
    std::vector<BaseRun> runs;
    for (int index = item.source_start; index < item.source_end; ++index) {
      const std::string& speaker_id = labels[index].base_speaker_id;
      if (speaker_id.empty()) return false;
      if (runs.empty() || runs.back().speaker_id != speaker_id) {
        runs.push_back({speaker_id, index, index + 1});
      } else {
        runs.back().source_end = index + 1;
      }
    }
    if (runs.size() != 2 || runs[0].speaker_id == runs[1].speaker_id ||
        selected != runs[1].speaker_id) {
      return false;
    }

    const auto handoff = std::find_if(
        corroborated_handoffs.begin(), corroborated_handoffs.end(),
        [&](const auto& candidate) {
          return candidate.boundary > item.start + 1e-9 &&
                 candidate.boundary < item.end - 1e-9 &&
                 candidate.preceding_speaker_id == runs[0].speaker_id &&
                 candidate.following_speaker_id == runs[1].speaker_id;
        });
    if (handoff == corroborated_handoffs.end()) return false;

    std::vector<std::pair<double, double>> aligned_intervals;
    for (int index = runs[0].source_start; index < runs[0].source_end;
         ++index) {
      if (character_times[index].available) {
        aligned_intervals.push_back(
            {character_times[index].start, character_times[index].end});
      }
    }
    aligned_intervals = MergeIntervals(std::move(aligned_intervals));
    const double minimum =
        pipeline.config_.voiceprint_primary_consensus_min_sec;
    if (CoveredDuration(aligned_intervals) + 1e-9 < minimum) return false;

    auto aligned_coverage = [&](const std::vector<SpeakerSeg>& track) {
      std::vector<std::pair<double, double>> intersections;
      for (const auto& interval : aligned_intervals) {
        for (const auto& segment : track) {
          if (segment.speaker_id != runs[0].speaker_id) continue;
          const double start = std::max(interval.first, segment.start);
          const double end = std::min(interval.second, segment.end);
          if (end > start) intersections.push_back({start, end});
        }
      }
      return CoveredDuration(MergeIntervals(std::move(intersections)));
    };
    return aligned_coverage(pipeline.speakers_) + 1e-9 >= minimum &&
           aligned_coverage(pipeline.primary_speakers_) + 1e-9 >= minimum;
  };
  auto local_coverage = [&](double start, double end,
                            const std::string& local_speaker) {
    std::vector<std::pair<double, double>> intervals;
    for (const auto& segment : pipeline.speakers_) {
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
    if (duration + 1e-9 < pipeline.config_.voiceprint_primary_consensus_min_sec) {
      return false;
    }
    const double primary_coverage =
        identity_coverage(pipeline.primary_speakers_, item.start, item.end, selected);
    if (primary_coverage + 1e-9 < duration) return false;
    for (const auto& segment : pipeline.primary_speakers_) {
      if (segment.speaker_id != selected &&
          Overlap(item.start, item.end, segment.start, segment.end) > 1e-9) {
        return false;
      }
    }
    const double activity_coverage =
        identity_coverage(pipeline.speakers_, item.start, item.end, selected);
    return activity_coverage + 1e-9 >=
           pipeline.config_.voiceprint_primary_consensus_min_sec;
  };
  auto native_views_preserve_current = [&](const auto& item,
                                           const std::string& selected) {
    const double duration = item.end - item.start;
    if ((item.kind != "business_interval" &&
         item.kind != "punctuation_phrase") ||
        duration + 1e-9 <
            pipeline.config_.voiceprint_primary_consensus_min_sec ||
        duration + 1e-9 >= pipeline.config_.voiceprint_short_max_sec) {
      return false;
    }

    int overlapping_vad_count = 0;
    bool has_containing_vad = false;
    const double tolerance =
        pipeline.config_.align_boundary_split_tolerance_sec;
    for (const auto& evidence_item : pipeline.voiceprint_vad_) {
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
    return complete_uncontested_coverage(pipeline.speakers_) &&
           complete_uncontested_coverage(pipeline.primary_speakers_);
  };
  auto preserves_exact_cross_scale_primary_return =
      [&](int source_index, const std::string& selected) {
        if (source_index < 0 || source_index >= character_count ||
            selected.empty() || !character_times[source_index].available) {
          return false;
        }
        const auto& label = labels[source_index];
        const bool primary_base =
            label.base_reason == "primary_speaker_tie_break" ||
            label.base_reason == "primary_speaker_overlap_refinement";
        if (!primary_base || label.base_speaker_id.empty() ||
            label.base_speaker_id == selected) {
          return false;
        }

        const double midpoint =
            0.5 * (character_times[source_index].start +
                   character_times[source_index].end);
        const SpeakerSeg* candidate = nullptr;
        for (const auto& primary : pipeline.primary_speakers_) {
          if (primary.speaker_id != label.base_speaker_id ||
              midpoint < primary.start - 1e-9 ||
              midpoint >= primary.end - 1e-9) {
            continue;
          }
          if (candidate != nullptr) return false;
          candidate = &primary;
        }
        if (candidate == nullptr) return false;

        const double duration = candidate->end - candidate->start;
        if (duration + 1e-9 <
                pipeline.config_.voiceprint_primary_consensus_min_sec ||
            duration + 1e-9 >=
                pipeline.config_.voiceprint_short_max_sec) {
          return false;
        }

        const SpeakerSeg* previous = nullptr;
        const SpeakerSeg* following = nullptr;
        for (const auto& primary : pipeline.primary_speakers_) {
          if (&primary == candidate) continue;
          if (Overlap(candidate->start, candidate->end, primary.start,
                      primary.end) > 1e-9) {
            return false;
          }
          if (primary.end <= candidate->start + 1e-9 &&
              (previous == nullptr || primary.end > previous->end + 1e-9)) {
            previous = &primary;
          }
          if (primary.start + 1e-9 >= candidate->end &&
              (following == nullptr ||
               primary.start < following->start - 1e-9)) {
            following = &primary;
          }
        }
        if (previous == nullptr || following == nullptr ||
            previous->speaker_id != selected ||
            following->speaker_id != selected) {
          return false;
        }

        const SpeakerVoiceprintEvidence* exact_interval = nullptr;
        for (const auto* item : source_evidence) {
          if (item->kind != "business_interval" ||
              !NearEqual(item->start, candidate->start) ||
              !NearEqual(item->end, candidate->end)) {
            continue;
          }
          if (exact_interval != nullptr) return false;
          exact_interval = item;
        }
        if (exact_interval == nullptr ||
            !exact_interval->embedding_available ||
            !exact_interval->robust_gallery_complete) {
          return false;
        }
        const auto interval_session =
            select(exact_interval->session_scores, duration, true);
        const auto interval_robust =
            select(exact_interval->robust_scores, duration, true);
        if (!interval_session || !interval_robust ||
            interval_session->speaker_id != candidate->speaker_id ||
            interval_robust->speaker_id != candidate->speaker_id) {
          return false;
        }

        for (const auto& activity : pipeline.speakers_) {
          if (Overlap(candidate->start, candidate->end, activity.start,
                      activity.end) <= 1e-9) {
            continue;
          }
          if (activity.speaker_id != selected &&
              activity.speaker_id != candidate->speaker_id) {
            return false;
          }
        }
        return identity_coverage(pipeline.speakers_, candidate->start,
                                 candidate->end, candidate->speaker_id) +
                   1e-9 >=
               duration;
      };
  auto apply = [&](const auto& item, const std::string& selected,
                   const std::string& reason,
                   bool preserve_exact_primary_returns = false) {
    const std::string label = speaker_label(selected);
    for (int index = item.source_start; index < item.source_end; ++index) {
      if (preserve_exact_primary_returns &&
          preserves_exact_cross_scale_primary_return(index, selected)) {
        continue;
      }
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
        duration + 1e-9 < pipeline.config_.voiceprint_short_max_sec ||
        duration > pipeline.config_.voiceprint_phrase_max_sec + 1e-9 ||
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

    for (const auto& segment : pipeline.speakers_) {
      if (segment.speaker == covered_local) continue;
      if (Overlap(item.start, item.end, segment.start, segment.end) > 1e-9) {
        return std::nullopt;
      }
    }
    return session->speaker_id;
  };
  auto partition_invariant_regular_initial_slot_challenge =
      [&](const auto& item) -> std::optional<std::string> {
    const double phrase_duration = item.end - item.start;
    if (item.kind != "punctuation_phrase" || item.text_id != text.id ||
        phrase_duration + 1e-9 <
            pipeline.config_.voiceprint_short_max_sec ||
        phrase_duration > pipeline.config_.voiceprint_phrase_max_sec + 1e-9 ||
        !item.embedding_available || !item.robust_gallery_complete ||
        !has_minimum_aligned_units(item.start, item.end)) {
      return std::nullopt;
    }

    std::string current_identity;
    for (int index = item.source_start; index < item.source_end; ++index) {
      if (labels[index].speaker_id.empty() || labels[index].voiceprint ||
          labels[index].reason != "sole_diar_support" ||
          labels[index].base_reason != "sole_diar_support" ||
          labels[index].base_speaker_id != labels[index].speaker_id) {
        return std::nullopt;
      }
      if (current_identity.empty()) {
        current_identity = labels[index].speaker_id;
      } else if (labels[index].speaker_id != current_identity) {
        return std::nullopt;
      }
    }
    if (current_identity.empty()) return std::nullopt;

    auto uncontested_covering_slot = [&](const auto& segments)
        -> std::optional<std::string> {
      std::set<std::string> slots;
      for (const auto& segment : segments) {
        if (Overlap(item.start, item.end, segment.start, segment.end) <= 1e-9) {
          continue;
        }
        if (segment.speaker.empty() ||
            segment.speaker_id != current_identity) {
          return std::nullopt;
        }
        slots.insert(segment.speaker);
      }
      if (slots.size() != 1 ||
          identity_coverage(segments, item.start, item.end,
                            current_identity) +
                  1e-9 <
              phrase_duration) {
        return std::nullopt;
      }
      return *slots.begin();
    };
    const auto activity_slot =
        uncontested_covering_slot(pipeline.speakers_);
    const auto primary_slot =
        uncontested_covering_slot(pipeline.primary_speakers_);
    if (!activity_slot || !primary_slot || *activity_slot != *primary_slot) {
      return std::nullopt;
    }

    const auto initial = initial_identities.find(*activity_slot);
    if (initial == initial_identities.end() || initial->second.second.empty() ||
        initial->second.second == current_identity) {
      return std::nullopt;
    }
    const std::string& initial_identity = initial->second.second;

    const auto phrase_session =
        ranked_pair(item.session_scores, phrase_duration);
    const auto phrase_robust =
        ranked_pair(item.robust_scores, phrase_duration);
    if (!phrase_session || !phrase_robust ||
        phrase_session->first.speaker_id != current_identity ||
        phrase_session->second_id != initial_identity ||
        phrase_robust->first.speaker_id != current_identity ||
        phrase_robust->second_id != initial_identity ||
        phrase_session->first.score_pass ||
        phrase_robust->first.score_pass ||
        phrase_session->first.margin_pass ==
            phrase_robust->first.margin_pass) {
      return std::nullopt;
    }

    const SpeakerVoiceprintEvidence* containing_vad = nullptr;
    for (const auto& candidate : pipeline.voiceprint_vad_) {
      if (candidate.kind != "vad" ||
          candidate.start > item.start + 1e-9 ||
          candidate.end + 1e-9 < item.end) {
        continue;
      }
      if (containing_vad != nullptr) return std::nullopt;
      containing_vad = &candidate;
    }

    const SpeakerVoiceprintEvidence* complete_source = nullptr;
    for (const auto& candidate : text_voiceprint) {
      if (candidate.kind != "complete_source" ||
          candidate.text_id != item.text_id ||
          candidate.source_start > item.source_start ||
          candidate.source_end < item.source_end ||
          candidate.start > item.start + 1e-9 ||
          candidate.end + 1e-9 < item.end) {
        continue;
      }
      if (complete_source != nullptr) return std::nullopt;
      complete_source = &candidate;
    }

    auto outer_view_abstains_for_initial = [&](const auto* outer) {
      if (outer == nullptr || !outer->embedding_available ||
          !outer->robust_gallery_complete) {
        return false;
      }
      const double duration = outer->end - outer->start;
      if (duration + 1e-9 < pipeline.config_.voiceprint_short_max_sec) {
        return false;
      }
      const auto session = ranked_pair(outer->session_scores, duration);
      const auto robust = ranked_pair(outer->robust_scores, duration);
      return session && robust &&
             session->first.speaker_id == initial_identity &&
             session->second_id == current_identity &&
             !session->first.score_pass && !session->first.margin_pass &&
             robust->first.speaker_id == initial_identity &&
             robust->second_id == current_identity &&
             !robust->first.score_pass && !robust->first.margin_pass;
    };
    if (!outer_view_abstains_for_initial(containing_vad) ||
        !outer_view_abstains_for_initial(complete_source)) {
      return std::nullopt;
    }
    return initial_identity;
  };
  auto future_epoch_phrase_challenge = [&](const auto& item)
      -> std::optional<std::string> {
    const double duration = item.end - item.start;
    if (item.kind != "punctuation_phrase" ||
        pipeline.config_.voiceprint_future_epoch_lookahead_sec <= 0.0 ||
        duration + 1e-9 <
            pipeline.config_.voiceprint_primary_consensus_min_sec ||
        duration > pipeline.config_.voiceprint_phrase_max_sec + 1e-9 ||
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
    for (const auto& segment : pipeline.speakers_) {
      if (segment.speaker_id == current_identity &&
          local_coverage(item.start, item.end, segment.speaker) + 1e-9 >=
              duration) {
        covering_slots.insert(segment.speaker);
      }
    }
    if (covering_slots.size() != 1) return std::nullopt;
    const std::string current_slot = *covering_slots.begin();
    for (const auto& segment : pipeline.speakers_) {
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
    for (const auto& primary : pipeline.primary_speakers_) {
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

    const SpeakerSeg* future_epoch = nullptr;
    for (const auto& segment : pipeline.speakers_) {
      if (segment.speaker != current_slot || segment.speaker_id.empty() ||
          segment.speaker_id == current_identity ||
          segment.start + 1e-9 < item.end ||
          segment.start - item.end >
              pipeline.config_.voiceprint_future_epoch_lookahead_sec + 1e-9) {
        continue;
      }
      if (future_epoch == nullptr || segment.start < future_epoch->start) {
        future_epoch = &segment;
      }
    }
    const double minimum =
        pipeline.config_.voiceprint_primary_consensus_min_sec;
    if (future_epoch == nullptr ||
        future_epoch->end - future_epoch->start + 1e-9 < minimum) {
      return std::nullopt;
    }

    std::vector<std::pair<double, double>> future_primary_intervals;
    for (const auto& primary : pipeline.primary_speakers_) {
      if (primary.speaker != current_slot ||
          primary.speaker_id != future_epoch->speaker_id) {
        continue;
      }
      const double start = std::max(future_epoch->start, primary.start);
      const double end = std::min(future_epoch->end, primary.end);
      if (end > start) future_primary_intervals.push_back({start, end});
    }
    if (CoveredDuration(MergeIntervals(std::move(future_primary_intervals))) +
            1e-9 <
        minimum) {
      return std::nullopt;
    }

    if (!has_minimum_aligned_units(item.start, item.end)) {
      return std::nullopt;
    }
    const auto session = ranked_pair(item.session_scores, duration);
    const auto robust = ranked_pair(item.robust_scores, duration);
    if (!session || !robust ||
        robust->first.speaker_id != future_epoch->speaker_id ||
        !robust->first.score_pass || !robust->first.margin_pass ||
        session->first.speaker_id == current_identity ||
        robust->first.speaker_id == current_identity ||
        (session->first.speaker_id != future_epoch->speaker_id &&
         session->second_id != future_epoch->speaker_id)) {
      return std::nullopt;
    }
    return future_epoch->speaker_id;
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

    if (!has_minimum_aligned_units(item.start, item.end)) {
      return std::nullopt;
    }

    const double tolerance = pipeline.config_.align_boundary_split_tolerance_sec;
    const SpeakerSeg* covering_primary = nullptr;
    for (const auto& primary : pipeline.primary_speakers_) {
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
    for (const auto& candidate : pipeline.voiceprint_vad_) {
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
        duration + 1e-9 < pipeline.config_.voiceprint_primary_consensus_min_sec ||
        duration + 1e-9 >= pipeline.config_.voiceprint_short_max_sec ||
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

    const double tolerance = pipeline.config_.align_boundary_split_tolerance_sec;
    const SpeakerSeg* covering_primary = nullptr;
    for (const auto& primary : pipeline.primary_speakers_) {
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
    for (const auto& segment : pipeline.speakers_) {
      if (segment.speaker_id == current_identity) {
        current_slots.insert(segment.speaker);
      }
    }
    std::string current_slot;
    for (const auto& local_slot : current_slots) {
      std::vector<std::pair<double, double>> intervals;
      for (const auto& segment : pipeline.speakers_) {
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

    if (!has_minimum_aligned_units(item.start, item.end)) {
      return std::nullopt;
    }

    const SpeakerVoiceprintEvidence* containing_vad = nullptr;
    for (const auto& candidate : pipeline.voiceprint_vad_) {
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
            pipeline.config_.voiceprint_primary_consensus_min_sec ||
        phrase_duration + 1e-9 >= pipeline.config_.voiceprint_short_max_sec ||
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
    for (const auto& segment : pipeline.speakers_) {
      if (segment.speaker_id == current_identity &&
          local_coverage(item.start, item.end, segment.speaker) + 1e-9 >=
              phrase_duration) {
        covering_slots.insert(segment.speaker);
      }
    }
    if (covering_slots.size() != 1) return std::nullopt;
    const std::string current_slot = *covering_slots.begin();
    for (const auto& segment : pipeline.speakers_) {
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
    for (const auto& primary : pipeline.primary_speakers_) {
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

    const double tolerance =
        pipeline.config_.align_boundary_split_tolerance_sec;
    const SpeakerVoiceprintEvidence* containing_vad = nullptr;
    for (const auto& candidate : pipeline.voiceprint_vad_) {
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

    if (!has_minimum_aligned_units(item.start, item.end)) {
      return std::nullopt;
    }

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
            pipeline.config_.voiceprint_primary_consensus_min_sec ||
        phrase_duration + 1e-9 >= pipeline.config_.voiceprint_short_max_sec ||
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
    for (const auto& segment : pipeline.speakers_) {
      if (segment.speaker_id == current_identity &&
          local_coverage(item.start, item.end, segment.speaker) + 1e-9 >=
              phrase_duration) {
        covering_slots.insert(segment.speaker);
      }
    }
    if (covering_slots.size() != 1) return std::nullopt;
    const std::string current_slot = *covering_slots.begin();
    for (const auto& segment : pipeline.speakers_) {
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
    for (const auto& primary : pipeline.primary_speakers_) {
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

    const double tolerance =
        pipeline.config_.align_boundary_split_tolerance_sec;
    const SpeakerVoiceprintEvidence* containing_vad = nullptr;
    for (const auto& candidate : pipeline.voiceprint_vad_) {
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

    if (!has_minimum_aligned_units(item.start, item.end)) {
      return std::nullopt;
    }

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
  // Keep a native phrase only when every typed scale exposes the same
  // challenger/current abstention pattern under the existing gates.
  auto partition_invariant_cross_scale_preserves_current =
      [&](const auto& item, const std::string& challenger_identity) {
        const double phrase_duration = item.end - item.start;
        if (item.kind != "punctuation_phrase" ||
            item.text_id != text.id || challenger_identity.empty() ||
            phrase_duration + 1e-9 <
                pipeline.config_.voiceprint_primary_consensus_min_sec ||
            phrase_duration + 1e-9 >=
                pipeline.config_.voiceprint_short_max_sec ||
            !item.embedding_available || !item.robust_gallery_complete ||
            !has_minimum_aligned_units(item.start, item.end)) {
          return false;
        }

        std::string current_identity;
        for (int index = item.source_start; index < item.source_end; ++index) {
          if (labels[index].speaker_id.empty() || labels[index].voiceprint ||
              labels[index].base_speaker_id != labels[index].speaker_id) {
            return false;
          }
          if (current_identity.empty()) {
            current_identity = labels[index].speaker_id;
          } else if (labels[index].speaker_id != current_identity) {
            return false;
          }
        }
        if (current_identity.empty() ||
            current_identity == challenger_identity) {
          return false;
        }

        auto uncontested_native_slot = [&](const auto& segments,
                                            std::string* slot) {
          std::set<std::string> overlapping_slots;
          for (const auto& segment : segments) {
            if (Overlap(item.start, item.end, segment.start, segment.end) <=
                1e-9) {
              continue;
            }
            if (segment.speaker_id != current_identity ||
                segment.speaker.empty()) {
              return false;
            }
            overlapping_slots.insert(segment.speaker);
          }
          if (overlapping_slots.size() != 1 ||
              identity_coverage(segments, item.start, item.end,
                                current_identity) +
                      1e-9 <
                  phrase_duration) {
            return false;
          }
          *slot = *overlapping_slots.begin();
          return true;
        };
        std::string activity_slot;
        std::string primary_slot;
        if (!uncontested_native_slot(pipeline.speakers_, &activity_slot) ||
            !uncontested_native_slot(pipeline.primary_speakers_,
                                     &primary_slot) ||
            activity_slot != primary_slot) {
          return false;
        }

        const auto phrase_session =
            ranked_pair(item.session_scores, phrase_duration);
        const auto phrase_robust =
            ranked_pair(item.robust_scores, phrase_duration);
        if (!phrase_session || !phrase_robust ||
            phrase_session->first.speaker_id != challenger_identity ||
            phrase_session->second_id != current_identity ||
            !phrase_session->first.score_pass ||
            !phrase_session->first.margin_pass ||
            phrase_robust->first.speaker_id != challenger_identity ||
            phrase_robust->second_id != current_identity ||
            !phrase_robust->first.score_pass ||
            phrase_robust->first.margin_pass) {
          return false;
        }

        const double tolerance =
            pipeline.config_.align_boundary_split_tolerance_sec;
        const SpeakerVoiceprintEvidence* containing_vad = nullptr;
        for (const auto& candidate : pipeline.voiceprint_vad_) {
          if (candidate.kind != "vad" ||
              candidate.start > item.start + tolerance ||
              candidate.end + tolerance < item.end) {
            continue;
          }
          if (containing_vad != nullptr) return false;
          containing_vad = &candidate;
        }
        if (containing_vad == nullptr ||
            !containing_vad->embedding_available ||
            !containing_vad->robust_gallery_complete) {
          return false;
        }
        const double vad_duration = containing_vad->end - containing_vad->start;
        const auto vad_session =
            ranked_pair(containing_vad->session_scores, vad_duration);
        const auto vad_robust =
            ranked_pair(containing_vad->robust_scores, vad_duration);
        if (!vad_session || !vad_robust ||
            vad_session->first.speaker_id != challenger_identity ||
            vad_session->second_id != current_identity ||
            !vad_session->first.score_pass ||
            !vad_session->first.margin_pass ||
            vad_robust->first.speaker_id != current_identity ||
            vad_robust->second_id != challenger_identity ||
            !vad_robust->first.score_pass ||
            vad_robust->first.margin_pass) {
          return false;
        }

        const SpeakerVoiceprintEvidence* containing_interval = nullptr;
        const SpeakerVoiceprintEvidence* complete_source = nullptr;
        for (const auto& candidate : text_voiceprint) {
          if (candidate.text_id != item.text_id ||
              (candidate.kind != "business_interval" &&
               candidate.kind != "complete_source") ||
              candidate.source_start > item.source_start ||
              candidate.source_end < item.source_end ||
              candidate.start > item.start + tolerance ||
              candidate.end + tolerance < item.end) {
            continue;
          }
          auto** destination = candidate.kind == "business_interval"
                                   ? &containing_interval
                                   : &complete_source;
          if (*destination != nullptr) return false;
          *destination = &candidate;
        }

        auto broad_view_supports_current = [&](const auto* broad) {
          if (broad == nullptr || !broad->embedding_available ||
              !broad->robust_gallery_complete) {
            return false;
          }
          const double duration = broad->end - broad->start;
          if (duration + 1e-9 <
              pipeline.config_.voiceprint_short_max_sec) {
            return false;
          }
          const auto session = ranked_pair(broad->session_scores, duration);
          const auto robust = ranked_pair(broad->robust_scores, duration);
          return session && robust &&
                 session->first.speaker_id == current_identity &&
                 session->second_id == challenger_identity &&
                 !session->first.score_pass &&
                 session->first.margin_pass &&
                 robust->first.speaker_id == current_identity &&
                 robust->second_id == challenger_identity &&
                 !robust->first.score_pass && robust->first.margin_pass;
        };
        return broad_view_supports_current(containing_interval) &&
               broad_view_supports_current(complete_source);
      };
  // Let exact phrase and VAD evidence challenge a coarse direct write only
  // when activity and primary expose the bounded three-identity conflict.
  auto exact_phrase_vad_direct_conflict_challenge = [&](const auto& item)
      -> std::optional<std::string> {
    const double phrase_duration = item.end - item.start;
    if (item.kind != "punctuation_phrase" || item.text_id != text.id ||
        !item.embedding_available || !item.robust_gallery_complete ||
        phrase_duration + 1e-9 <
            pipeline.config_.voiceprint_primary_consensus_min_sec ||
        phrase_duration + 1e-9 >=
            pipeline.config_.voiceprint_short_max_sec ||
        !has_minimum_aligned_units(item.start, item.end)) {
      return std::nullopt;
    }

    std::string current_identity;
    for (int index = item.source_start; index < item.source_end; ++index) {
      const bool direct =
          labels[index].reason == "voiceprint_direct_short" ||
          labels[index].reason == "voiceprint_direct_regular";
      if (!direct || !labels[index].voiceprint ||
          labels[index].speaker_id.empty()) {
        return std::nullopt;
      }
      if (current_identity.empty()) {
        current_identity = labels[index].speaker_id;
      } else if (labels[index].speaker_id != current_identity) {
        return std::nullopt;
      }
    }
    if (current_identity.empty()) return std::nullopt;

    const double tolerance =
        pipeline.config_.align_boundary_split_tolerance_sec;
    const SpeakerVoiceprintEvidence* containing_interval = nullptr;
    for (const auto& candidate : text_voiceprint) {
      if (candidate.kind != "business_interval" ||
          candidate.text_id != item.text_id ||
          candidate.source_start > item.source_start ||
          candidate.source_end < item.source_end ||
          candidate.start > item.start + tolerance ||
          candidate.end + tolerance < item.end) {
        continue;
      }
      if (containing_interval != nullptr) return std::nullopt;
      containing_interval = &candidate;
    }
    if (containing_interval == nullptr ||
        !containing_interval->embedding_available ||
        !containing_interval->robust_gallery_complete) {
      return std::nullopt;
    }
    const double interval_duration =
        containing_interval->end - containing_interval->start;
    if (interval_duration + 1e-9 <
        pipeline.config_.voiceprint_short_max_sec) {
      return std::nullopt;
    }
    for (int index = item.source_start; index < item.source_end; ++index) {
      if (labels[index].reason != "voiceprint_direct_regular") {
        return std::nullopt;
      }
    }
    const auto interval_session =
        select(containing_interval->session_scores, interval_duration, true);
    const auto interval_robust =
        select(containing_interval->robust_scores, interval_duration, true);
    if (!interval_session || !interval_robust ||
        interval_session->speaker_id != current_identity ||
        interval_robust->speaker_id != current_identity) {
      return std::nullopt;
    }

    const auto phrase_session =
        select(item.session_scores, phrase_duration, true);
    const auto phrase_robust =
        select(item.robust_scores, phrase_duration, true);
    if (!phrase_session || !phrase_robust ||
        phrase_session->speaker_id.empty() ||
        phrase_session->speaker_id == current_identity ||
        phrase_robust->speaker_id != phrase_session->speaker_id) {
      return std::nullopt;
    }
    const std::string candidate_identity = phrase_session->speaker_id;

    const SpeakerVoiceprintEvidence* containing_vad = nullptr;
    for (const auto& candidate : pipeline.voiceprint_vad_) {
      if (candidate.kind != "vad" ||
          candidate.start > item.start + tolerance ||
          candidate.end + tolerance < item.end) {
        continue;
      }
      if (containing_vad != nullptr) return std::nullopt;
      containing_vad = &candidate;
    }
    if (containing_vad == nullptr ||
        !containing_vad->embedding_available ||
        !containing_vad->robust_gallery_complete) {
      return std::nullopt;
    }
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

    const SpeakerSeg* covering_primary = nullptr;
    for (const auto& segment : pipeline.primary_speakers_) {
      if (Overlap(item.start, item.end, segment.start, segment.end) <= 1e-9) {
        continue;
      }
      if (covering_primary != nullptr) return std::nullopt;
      covering_primary = &segment;
    }
    if (covering_primary == nullptr || covering_primary->speaker.empty() ||
        covering_primary->speaker_id.empty() ||
        covering_primary->start > item.start + tolerance ||
        covering_primary->end + tolerance < item.end) {
      return std::nullopt;
    }
    const std::string primary_identity = covering_primary->speaker_id;
    if (primary_identity == current_identity ||
        primary_identity == candidate_identity) {
      return std::nullopt;
    }

    std::map<std::string, std::set<std::string>> activity_slots;
    for (const auto& segment : pipeline.speakers_) {
      if (Overlap(item.start, item.end, segment.start, segment.end) <= 1e-9) {
        continue;
      }
      if (segment.speaker.empty() || segment.speaker_id.empty() ||
          (segment.speaker_id != candidate_identity &&
           segment.speaker_id != primary_identity)) {
        return std::nullopt;
      }
      activity_slots[segment.speaker_id].insert(segment.speaker);
    }
    const auto candidate_slots = activity_slots.find(candidate_identity);
    const auto primary_slots = activity_slots.find(primary_identity);
    if (activity_slots.size() != 2 || candidate_slots == activity_slots.end() ||
        primary_slots == activity_slots.end() ||
        candidate_slots->second.size() != 1 ||
        primary_slots->second.size() != 1 ||
        *primary_slots->second.begin() != covering_primary->speaker ||
        identity_coverage(pipeline.speakers_, item.start, item.end,
                          candidate_identity) +
                1e-9 <
            phrase_duration ||
        identity_coverage(pipeline.speakers_, item.start, item.end,
                          primary_identity) +
                1e-9 <
            phrase_duration) {
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
            pipeline.config_.voiceprint_primary_consensus_min_sec ||
        interval_duration + 1e-9 >= pipeline.config_.voiceprint_short_max_sec) {
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
    for (const auto& segment : pipeline.speakers_) {
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
    for (const auto& primary : pipeline.primary_speakers_) {
      if (Overlap(item.start, item.end, primary.start, primary.end) <= 1e-9) {
        continue;
      }
      if (covering_primary != nullptr || primary.speaker != current_slot ||
          primary.speaker_id != current_identity ||
          primary.start > item.start + 1e-9 || primary.end + 1e-9 < item.end) {
        return std::nullopt;
      }
      covering_primary = &primary;
    }
    if (covering_primary == nullptr) return std::nullopt;

    const double tolerance =
        pipeline.config_.align_boundary_split_tolerance_sec;
    const SpeakerVoiceprintEvidence* containing_vad = nullptr;
    for (const auto& evidence_item : pipeline.voiceprint_vad_) {
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

    if (!has_minimum_aligned_units(item.start, item.end)) {
      return std::nullopt;
    }
    return candidate_identity;
  };
  auto adjacent_phrase_continuation_challenge = [&](const auto& item)
      -> std::optional<std::string> {
    const double duration = item.end - item.start;
    if (item.kind != "business_interval" ||
        duration + 1e-9 < pipeline.config_.voiceprint_primary_consensus_min_sec ||
        duration + 1e-9 >= pipeline.config_.voiceprint_short_max_sec ||
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

    const double tolerance =
        pipeline.config_.align_boundary_split_tolerance_sec;
    const SpeakerVoiceprintEvidence* anchor = nullptr;
    for (const auto& candidate : text_voiceprint) {
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
        item.start + pipeline.config_.voiceprint_primary_consensus_min_sec;
    const double continuous_duration = continuous_end - anchor->start;
    if (identity_coverage(pipeline.speakers_, anchor->start, continuous_end,
                          anchor_identity) +
            1e-9 <
        continuous_duration) {
      return std::nullopt;
    }

    const SpeakerSeg* covering_primary = nullptr;
    for (const auto& primary : pipeline.primary_speakers_) {
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

    if (!has_minimum_aligned_units(item.start, item.end)) {
      return std::nullopt;
    }
    return anchor_identity;
  };
  auto short_initial_slot_direct_challenge = [&](const auto& item)
      -> std::optional<std::string> {
    const double duration = item.end - item.start;
    if (item.kind != "punctuation_phrase" ||
        duration + 1e-9 < pipeline.config_.voiceprint_primary_consensus_min_sec ||
        duration + 1e-9 >= pipeline.config_.voiceprint_short_max_sec ||
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
    for (const auto& segment : pipeline.speakers_) {
      if (segment.speaker_id == current_identity &&
          local_coverage(item.start, item.end, segment.speaker) + 1e-9 >=
              duration) {
        covering_slots.insert(segment.speaker);
      }
    }
    if (covering_slots.size() != 1) return std::nullopt;
    const std::string current_slot = *covering_slots.begin();
    for (const auto& segment : pipeline.speakers_) {
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

    const double tolerance = pipeline.config_.align_boundary_split_tolerance_sec;
    const SpeakerSeg* covering_primary = nullptr;
    for (const auto& primary : pipeline.primary_speakers_) {
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

    if (!has_minimum_aligned_units(item.start, item.end)) {
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
        duration + 1e-9 >= pipeline.config_.voiceprint_primary_consensus_min_sec) {
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

    const auto alignment = pipeline.align_.find(text.id);
    if (alignment == pipeline.align_.end()) return std::nullopt;
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
    const double boundary_tolerance =
        pipeline.config_.align_boundary_split_tolerance_sec;
    if (previous_unit == nullptr || next_unit == nullptr ||
        item.start - previous_unit->end + boundary_tolerance + 1e-9 <
            pipeline.config_.align_snap_pause_sec ||
        next_unit->start - item.end + boundary_tolerance + 1e-9 <
            pipeline.config_.align_snap_pause_sec) {
      return std::nullopt;
    }

    std::set<std::string> covering_slots;
    for (const auto& segment : pipeline.speakers_) {
      if (segment.speaker_id != current_identity) continue;
      std::vector<std::pair<double, double>> intervals;
      for (const auto& same_slot : pipeline.speakers_) {
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
    for (const auto& segment : pipeline.speakers_) {
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

    const double tolerance = pipeline.config_.align_boundary_split_tolerance_sec;
    const SpeakerSeg* covering_primary = nullptr;
    int overlapping_primary_count = 0;
    for (const auto& primary : pipeline.primary_speakers_) {
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
    for (const auto& candidate : pipeline.voiceprint_vad_) {
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
        duration + 1e-9 >= pipeline.config_.voiceprint_primary_consensus_min_sec) {
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

    const auto alignment = pipeline.align_.find(text.id);
    if (alignment == pipeline.align_.end()) return std::nullopt;
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
    for (const auto& segment : pipeline.speakers_) {
      if (segment.speaker_id != current_identity) continue;
      std::vector<std::pair<double, double>> intervals;
      for (const auto& same_slot : pipeline.speakers_) {
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
    for (const auto& segment : pipeline.speakers_) {
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
    for (const auto& primary : pipeline.primary_speakers_) {
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
            pipeline.config_.voiceprint_primary_consensus_min_sec) {
      return std::nullopt;
    }

    const SpeakerSeg* previous_primary = nullptr;
    const SpeakerSeg* next_primary = nullptr;
    for (const auto& primary : pipeline.primary_speakers_) {
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
    for (const auto& candidate : pipeline.voiceprint_vad_) {
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
            pipeline.config_.align_snap_pause_sec ||
        next_vad->start - vad.end + 1e-9 <
            pipeline.config_.align_snap_pause_sec) {
      return std::nullopt;
    }

    std::vector<const SpeakerVoiceprintEvidence*> units;
    for (const auto& candidate : pipeline.voiceprint_aligned_units_) {
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
        pipeline.config_.voiceprint_four_view_min_aligned_units) {
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
    for (const auto& segment : pipeline.speakers_) {
      if (segment.speaker_id != current_identity) continue;
      std::vector<std::pair<double, double>> intervals;
      for (const auto& same_slot : pipeline.speakers_) {
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
    for (const auto& segment : pipeline.speakers_) {
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
    for (const auto& primary : pipeline.primary_speakers_) {
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
        run_duration + 1e-9 < pipeline.config_.voiceprint_primary_consensus_min_sec ||
        run_duration + 1e-9 >= pipeline.config_.voiceprint_short_max_sec) {
      return std::nullopt;
    }

    const SpeakerSeg* previous_primary = nullptr;
    const SpeakerSeg* next_primary = nullptr;
    for (const auto& primary : pipeline.primary_speakers_) {
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
            pipeline.config_.align_snap_pause_sec ||
        !NearEqual(next_primary->start, candidate_run.end) ||
        next_primary->end + 1e-9 <
            candidate_run.end +
                pipeline.config_.voiceprint_primary_consensus_min_sec) {
      return std::nullopt;
    }

    std::vector<const SpeakerVoiceprintEvidence*> units;
    for (const auto& item : pipeline.voiceprint_aligned_units_) {
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
        pipeline.config_.voiceprint_four_view_min_aligned_units) {
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
    for (const auto& segment : pipeline.speakers_) {
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
    for (const auto& segment : pipeline.speakers_) {
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
    if (CoveredDuration(MergeIntervals(std::move(candidate_intervals))) + 1e-9 <
        island_end - island_start) {
      return std::nullopt;
    }

    const double tolerance =
        pipeline.config_.align_boundary_split_tolerance_sec;
    const SpeakerVoiceprintEvidence* containing_vad = nullptr;
    for (const auto& item : pipeline.voiceprint_vad_) {
      if (item.kind != "vad" || !item.embedding_available ||
          !item.robust_gallery_complete ||
          std::abs(item.start - candidate_run.start) > tolerance + 1e-9 ||
          item.start > island_start + 1e-9 || item.end + 1e-9 < island_end ||
          item.end + 1e-9 <
              candidate_run.end +
                  pipeline.config_.voiceprint_primary_consensus_min_sec) {
        continue;
      }
      if (containing_vad != nullptr) return std::nullopt;
      containing_vad = &item;
    }
    if (containing_vad == nullptr) return std::nullopt;

    const SpeakerVoiceprintEvidence* previous_vad = nullptr;
    for (const auto& item : pipeline.voiceprint_vad_) {
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
            pipeline.config_.align_snap_pause_sec) {
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
        duration + 1e-9 >= pipeline.config_.voiceprint_primary_consensus_min_sec) {
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
    for (const auto& segment : pipeline.speakers_) {
      if (segment.speaker_id != current_identity) continue;
      std::vector<std::pair<double, double>> intervals;
      for (const auto& same_slot : pipeline.speakers_) {
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
    for (const auto& segment : pipeline.speakers_) {
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
    for (const auto& primary : pipeline.primary_speakers_) {
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
    for (const auto& candidate : pipeline.voiceprint_vad_) {
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
            pipeline.config_.align_snap_pause_sec ||
        next_vad->start - item.end + 1e-9 <
            pipeline.config_.align_snap_pause_sec) {
      return std::nullopt;
    }

    int aligned_unit_count = 0;
    const SpeakerVoiceprintEvidence* previous_unit = nullptr;
    const SpeakerVoiceprintEvidence* next_unit = nullptr;
    for (const auto& candidate : pipeline.voiceprint_aligned_units_) {
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
    if (aligned_unit_count < pipeline.config_.voiceprint_four_view_min_aligned_units ||
        previous_unit == nullptr || next_unit == nullptr ||
        item.start - previous_unit->end + 1e-9 <
            pipeline.config_.align_snap_pause_sec ||
        next_unit->start - item.end + 1e-9 <
            pipeline.config_.align_snap_pause_sec) {
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
            pipeline.config_.voiceprint_primary_consensus_min_sec) {
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
    for (const auto& primary : pipeline.primary_speakers_) {
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
    for (const auto& segment : pipeline.speakers_) {
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

    const double tolerance =
        pipeline.config_.align_boundary_split_tolerance_sec;
    const SpeakerVoiceprintEvidence* containing_phrase = nullptr;
    const SpeakerVoiceprintEvidence* containing_vad = nullptr;
    for (const auto* candidate : relevant_voiceprint) {
      const bool contains_time = candidate->start <= item.start + tolerance &&
                                 candidate->end + tolerance >= item.end;
      if (!contains_time) continue;
      if (candidate->kind == "punctuation_phrase" &&
          candidate->text_id == item.text_id &&
          candidate->source_start <= item.source_start &&
          candidate->source_end >= item.source_end) {
        if (containing_phrase != nullptr || !candidate->embedding_available ||
            !candidate->robust_gallery_complete) {
          return std::nullopt;
        }
        containing_phrase = candidate;
      } else if (candidate->kind == "vad") {
        if (containing_vad != nullptr || !candidate->embedding_available ||
            !candidate->robust_gallery_complete) {
          return std::nullopt;
        }
        containing_vad = candidate;
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

    const auto alignment = pipeline.align_.find(text.id);
    if (alignment == pipeline.align_.end()) return std::nullopt;
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
        outer_duration + 1e-9 < pipeline.config_.voiceprint_short_max_sec) {
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
            pipeline.config_.voiceprint_primary_consensus_min_sec ||
        aligned_duration + 1e-9 >= pipeline.config_.voiceprint_short_max_sec) {
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
    const auto activity_coverage = coverage_by_identity(pipeline.speakers_);
    if (activity_coverage.size() != 2 ||
        activity_coverage.count(candidate_identity) == 0 ||
        activity_coverage.count(incumbent_identity) == 0 ||
        activity_coverage.at(incumbent_identity) + 1e-9 < aligned_duration ||
        activity_coverage.at(candidate_identity) <= 1e-9 ||
        activity_coverage.at(candidate_identity) + 1e-9 >= aligned_duration) {
      return std::nullopt;
    }

    const auto primary_coverage = coverage_by_identity(pipeline.primary_speakers_);
    if (primary_coverage.size() != 2 ||
        primary_coverage.count(candidate_identity) == 0 ||
        primary_coverage.count(incumbent_identity) == 0 ||
        primary_coverage.at(candidate_identity) <= 1e-9 ||
        primary_coverage.at(incumbent_identity) <= 1e-9) {
      return std::nullopt;
    }
    std::vector<std::pair<double, double>> primary_union;
    for (const auto& primary : pipeline.primary_speakers_) {
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
      std::sort(ordered.begin(), ordered.end(),
                [](const auto& left, const auto& right) {
                  if (!NearEqual(left.score, right.score)) {
                    return left.score > right.score;
                  }
                  return left.speaker_id < right.speaker_id;
                });
      return std::make_pair(ordered[0].speaker_id, ordered[1].speaker_id);
    };

    const SpeakerVoiceprintEvidence* phrase = nullptr;
    const double tolerance =
        pipeline.config_.align_boundary_split_tolerance_sec;
    const SpeakerVoiceprintEvidence* containing_vad = nullptr;
    for (const auto* candidate : relevant_voiceprint) {
      if (candidate->kind == "punctuation_phrase" &&
          candidate->text_id == text.id) {
        if (phrase != nullptr || !candidate->embedding_available ||
            !candidate->robust_gallery_complete) {
          return std::nullopt;
        }
        phrase = candidate;
      } else if (candidate->kind == "vad" &&
                 candidate->start <= aligned_start + tolerance &&
                 candidate->end + tolerance >= aligned_end) {
        if (containing_vad != nullptr || !candidate->embedding_available ||
            !candidate->robust_gallery_complete) {
          return std::nullopt;
        }
        containing_vad = candidate;
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
            pipeline.config_.voiceprint_primary_consensus_min_sec ||
        pair_duration + 1e-9 >= pipeline.config_.voiceprint_short_max_sec) {
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
            pipeline.config_.voiceprint_primary_consensus_min_sec ||
        following_duration + 1e-9 <
            pipeline.config_.voiceprint_primary_consensus_min_sec) {
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

    const double tolerance = pipeline.config_.align_boundary_split_tolerance_sec;
    const SpeakerSeg* candidate_primary = nullptr;
    for (const auto& primary : pipeline.primary_speakers_) {
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
    for (const auto& segment : pipeline.speakers_) {
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
            pipeline.config_.voiceprint_four_view_min_aligned_units) {
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

  struct DelayedClauseSelection {
    int source_start = 0;
    int source_end = 0;
    std::string speaker_id;
  };
  auto delayed_subminimum_clause_challenges = [&] {
    std::vector<DelayedClauseSelection> selections;
    if (pipeline.config_.voiceprint_punctuation.empty() ||
        pipeline.config_.align_snap_pause_sec <= 0.0 ||
        pipeline.config_.voiceprint_primary_consensus_min_sec <= 0.0) {
      return selections;
    }

    std::set<std::string> punctuation;
    const auto punctuation_offsets =
        Utf8Offsets(pipeline.config_.voiceprint_punctuation);
    for (std::size_t index = 0; index + 1 < punctuation_offsets.size();
         ++index) {
      punctuation.insert(pipeline.config_.voiceprint_punctuation.substr(
          punctuation_offsets[index],
          punctuation_offsets[index + 1] - punctuation_offsets[index]));
    }
    std::vector<std::string> source;
    source.reserve(character_count);
    for (int index = 0; index < character_count; ++index) {
      source.push_back(text.text.substr(offsets[index],
                                        offsets[index + 1] - offsets[index]));
    }
    auto visible = [&](int start, int end) {
      for (int index = start; index < end; ++index) {
        if (punctuation.count(source[index]) == 0 && source[index] != " " &&
            source[index] != "\t" && source[index] != "\n" &&
            source[index] != "\r") {
          return true;
        }
      }
      return false;
    };
    std::vector<std::pair<int, int>> phrases;
    int phrase_start = 0;
    for (int index = 0; index < character_count; ++index) {
      if (punctuation.count(source[index]) == 0) continue;
      if (visible(phrase_start, index + 1)) {
        phrases.push_back({phrase_start, index + 1});
      }
      phrase_start = index + 1;
    }
    if (phrase_start < character_count &&
        visible(phrase_start, character_count)) {
      phrases.push_back({phrase_start, character_count});
    }

    std::vector<const SpeakerVoiceprintEvidence*> aligned_units;
    for (const auto& item : pipeline.voiceprint_aligned_units_) {
      if (item.kind == "aligned_unit" && item.text_id == text.id &&
          item.end > item.start && item.source_start >= 0 &&
          item.source_end <= character_count &&
          item.source_end > item.source_start) {
        aligned_units.push_back(&item);
      }
    }
    std::sort(aligned_units.begin(), aligned_units.end(),
              [](const auto* left, const auto* right) {
                if (left->source_start != right->source_start) {
                  return left->source_start < right->source_start;
                }
                return left->source_end < right->source_end;
              });
    if (aligned_units.empty()) return selections;

    auto units_for = [&](int source_start, int source_end) {
      std::vector<const SpeakerVoiceprintEvidence*> units;
      for (const auto* unit : aligned_units) {
        if (unit->source_end <= source_start ||
            unit->source_start >= source_end) {
          continue;
        }
        if (unit->source_start < source_start ||
            unit->source_end > source_end) {
          return std::vector<const SpeakerVoiceprintEvidence*>{};
        }
        units.push_back(unit);
      }
      return units;
    };
    auto aligned_duration = [&](const auto& units) {
      std::vector<std::pair<double, double>> intervals;
      intervals.reserve(units.size());
      for (const auto* unit : units) {
        intervals.push_back({unit->start, unit->end});
      }
      return CoveredDuration(MergeIntervals(std::move(intervals)));
    };
    auto allowed_current_reason = [](const std::string& reason) {
      return reason == "sole_diar_support" ||
             reason == "competing_diar_interval_policy" ||
             reason.rfind("primary_speaker_", 0) == 0 ||
             reason.rfind("voiceprint_direct_", 0) == 0;
    };

    const double minimum =
        pipeline.config_.voiceprint_primary_consensus_min_sec;
    const double pause = pipeline.config_.align_snap_pause_sec;
    const double tolerance =
        pipeline.config_.align_boundary_split_tolerance_sec;
    for (std::size_t phrase_index = 0; phrase_index < phrases.size();
         ++phrase_index) {
      const int source_start = phrases[phrase_index].first;
      std::size_t following_index = phrase_index;
      int source_end = source_start;
      bool contiguous = true;
      while (following_index < phrases.size()) {
        if (following_index > phrase_index &&
            phrases[following_index - 1].second !=
                phrases[following_index].first) {
          contiguous = false;
          break;
        }
        const auto phrase_units = units_for(phrases[following_index].first,
                                            phrases[following_index].second);
        if (aligned_duration(phrase_units) + 1e-9 >= minimum) break;
        source_end = phrases[following_index].second;
        ++following_index;
      }
      if (!contiguous || following_index >= phrases.size() ||
          source_end <= source_start ||
          phrases[following_index].first != source_end) {
        continue;
      }

      const auto group_units = units_for(source_start, source_end);
      if (static_cast<int>(group_units.size()) <
              pipeline.config_.voiceprint_four_view_min_aligned_units ||
          aligned_duration(group_units) + 1e-9 >= minimum) {
        continue;
      }
      const double group_start = group_units.front()->start;
      const double group_end = group_units.back()->end;
      if (group_end <= group_start || source_start <= 0 ||
          source_end >= character_count) {
        continue;
      }

      bool exact_embedding_available = false;
      for (const auto* item : source_evidence) {
        if (item->kind == "punctuation_phrase" && item->embedding_available &&
            item->source_start >= source_start &&
            item->source_end <= source_end) {
          exact_embedding_available = true;
          break;
        }
      }
      if (exact_embedding_available) continue;

      const std::string incumbent = labels[source_start].speaker_id;
      if (incumbent.empty() ||
          labels[source_start - 1].speaker_id != incumbent ||
          labels[source_end].speaker_id != incumbent) {
        continue;
      }
      bool uniform_current = true;
      for (int index = source_start; index < source_end; ++index) {
        if (labels[index].speaker_id != incumbent ||
            !allowed_current_reason(labels[index].reason)) {
          uniform_current = false;
          break;
        }
      }
      if (!uniform_current) continue;

      const SpeakerVoiceprintEvidence* previous_unit = nullptr;
      for (const auto* unit : aligned_units) {
        if (unit->source_end <= source_start) {
          previous_unit = unit;
        } else {
          break;
        }
      }
      if (previous_unit == nullptr ||
          group_start - previous_unit->end + 1e-9 < pause ||
          identity_coverage(pipeline.speakers_, previous_unit->start,
                            previous_unit->end, incumbent) +
                  1e-9 <
              previous_unit->end - previous_unit->start) {
        continue;
      }

      const SpeakerSeg* activity_return = nullptr;
      const SpeakerSeg* primary_return = nullptr;
      for (const auto& segment : pipeline.speakers_) {
        if (segment.speaker_id != incumbent ||
            std::abs(segment.start - group_start) > tolerance + 1e-9 ||
            segment.start > group_start + 1e-9 ||
            segment.end + 1e-9 < group_end) {
          continue;
        }
        if (activity_return != nullptr) {
          activity_return = nullptr;
          break;
        }
        activity_return = &segment;
      }
      for (const auto& segment : pipeline.primary_speakers_) {
        if (segment.speaker_id != incumbent ||
            std::abs(segment.start - group_start) > tolerance + 1e-9 ||
            segment.start > group_start + 1e-9 ||
            segment.end + 1e-9 < group_end) {
          continue;
        }
        if (primary_return != nullptr) {
          primary_return = nullptr;
          break;
        }
        primary_return = &segment;
      }
      if (activity_return == nullptr || primary_return == nullptr) continue;

      struct NativeIsland {
        std::string speaker_id;
        double activity_start = 0.0;
        double activity_end = 0.0;
      };
      std::vector<NativeIsland> native_islands;
      std::set<std::string> candidate_ids;
      for (const auto& segment : pipeline.speakers_) {
        if (!segment.speaker_id.empty() && segment.speaker_id != incumbent) {
          candidate_ids.insert(segment.speaker_id);
        }
      }
      for (const auto& candidate_id : candidate_ids) {
        std::vector<std::pair<double, double>> intersections;
        double activity_start = group_start;
        double activity_end = previous_unit->end;
        bool candidate_on_group = false;
        for (const auto& activity : pipeline.speakers_) {
          if (activity.speaker_id != candidate_id) continue;
          if (Overlap(group_start, group_end, activity.start, activity.end) >
              1e-9) {
            candidate_on_group = true;
          }
          const double clipped_start =
              std::max(previous_unit->end, activity.start);
          const double clipped_end = std::min(group_start, activity.end);
          if (clipped_end <= clipped_start) continue;
          activity_start = std::min(activity_start, clipped_start);
          activity_end = std::max(activity_end, clipped_end);
          for (const auto& primary : pipeline.primary_speakers_) {
            if (primary.speaker_id != candidate_id) continue;
            const double start = std::max(clipped_start, primary.start);
            const double end = std::min(clipped_end, primary.end);
            if (end > start) intersections.push_back({start, end});
          }
        }
        if (candidate_on_group ||
            CoveredDuration(MergeIntervals(std::move(intersections))) + 1e-9 <
                minimum ||
            activity_return->start - activity_end + 1e-9 < pause) {
          continue;
        }
        native_islands.push_back({candidate_id, activity_start, activity_end});
      }
      if (native_islands.size() != 1) continue;
      const auto& native_island = native_islands.front();

      std::vector<const SpeakerVoiceprintEvidence*> vad;
      for (const auto& item : pipeline.voiceprint_vad_) {
        if (item.kind == "vad" && item.end > item.start) vad.push_back(&item);
      }
      std::sort(vad.begin(), vad.end(),
                [](const auto* left, const auto* right) {
                  if (!NearEqual(left->start, right->start)) {
                    return left->start < right->start;
                  }
                  return left->end < right->end;
                });
      int matching_vad_gaps = 0;
      for (std::size_t index = 0; index + 1 < vad.size(); ++index) {
        const auto& leading = *vad[index];
        const auto& following = *vad[index + 1];
        if (following.start - leading.end + 1e-9 < pause ||
            leading.end <= previous_unit->end + 1e-9 ||
            following.start + 1e-9 >= group_start ||
            Overlap(native_island.activity_start, native_island.activity_end,
                    leading.start, leading.end) <= 1e-9 ||
            Overlap(activity_return->start, group_end, following.start,
                    following.end) <= 1e-9) {
          continue;
        }
        ++matching_vad_gaps;
      }
      if (matching_vad_gaps != 1) continue;

      selections.push_back(
          {source_start, source_end, native_island.speaker_id});
      phrase_index = following_index - 1;
    }
    return selections;
  };

  for (const auto* item : evidence) {
    const double duration = item->end - item->start;
    const auto initial_slot = initial_slot_phrase_challenge(*item);
    if (initial_slot) {
      apply(*item, *initial_slot,
            "voiceprint_phrase_initial_slot_dual_gallery_override");
      continue;
    }

    const auto partition_invariant_initial_slot =
        partition_invariant_regular_initial_slot_challenge(*item);
    if (partition_invariant_initial_slot) {
      apply(*item, *partition_invariant_initial_slot,
            "voiceprint_phrase_partition_invariant_regular_initial_slot_"
            "override");
      continue;
    }

    const auto future_epoch = future_epoch_phrase_challenge(*item);
    if (future_epoch) {
      apply(*item, *future_epoch,
            "voiceprint_phrase_future_epoch_robust_override");
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

    const auto exact_phrase_vad_direct_conflict =
        exact_phrase_vad_direct_conflict_challenge(*item);
    if (exact_phrase_vad_direct_conflict) {
      apply(*item, *exact_phrase_vad_direct_conflict,
            "voiceprint_phrase_vad_dual_gallery_direct_override");
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
    if (partition_invariant_cross_scale_preserves_current(
            *item, session->speaker_id)) {
      continue;
    }
    if (native_views_preserve_current(*item, session->speaker_id)) {
      continue;
    }
    if (item->kind == "business_interval") {
      const std::string reason =
          duration < pipeline.config_.voiceprint_short_max_sec
              ? "voiceprint_direct_short"
              : "voiceprint_direct_regular";
      const std::string label = speaker_label(session->speaker_id);
      const bool preserve_handoff =
          preserves_corroborated_handoff(*item, session->speaker_id);
      for (int index = item->source_start; index < item->source_end; ++index) {
        if (preserve_handoff &&
            labels[index].base_speaker_id != session->speaker_id) {
          continue;
        }
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
        session->score + 1e-9f >= pipeline.config_.voiceprint_regular_min_score;
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
      if (!primary_activity_consensus &&
          has_sustained_native_handoff(*item, session->speaker_id)) {
        continue;
      }
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
                      : "voiceprint_phrase_session",
            /*preserve_exact_primary_returns=*/true);
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
              "voiceprint_complete_source_dual_gallery",
              /*preserve_exact_primary_returns=*/true);
      }
    }
  }

  for (const auto& item : pipeline.voiceprint_aligned_units_) {
    const auto selected = isolated_subminimum_unit_vad_challenge(item);
    if (selected) {
      apply(item, *selected,
            "voiceprint_aligned_unit_isolated_initial_slot_vad_override");
    }
  }
  for (const auto& item : pipeline.voiceprint_aligned_units_) {
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
  for (const auto& item : pipeline.voiceprint_vad_) {
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
  for (const auto& primary : pipeline.primary_speakers_) {
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
  for (const auto& item : pipeline.voiceprint_business_intervals_) {
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
  for (const auto& selected : delayed_subminimum_clause_challenges()) {
    const std::string label = speaker_label(selected.speaker_id);
    for (int index = selected.source_start; index < selected.source_end;
         ++index) {
      labels[index].speaker = label;
      labels[index].speaker_id = selected.speaker_id;
      labels[index].reason =
          "sortformer_delayed_subminimum_clause_group_override";
      labels[index].voiceprint = false;
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
        start = std::min(owner_start, end - 1.0 / pipeline.time_base_.sample_rate());
      } else if (previous_end) {
        start = *previous_end;
        end = std::max(owner_end, start + 1.0 / pipeline.time_base_.sample_rate());
      } else {
        start = owner_start;
        end = owner_end;
      }
      timed = end > start;
    }
    if (!timed || end <= start) return entries;
    projected_pieces.push_back({source_start, source_end, start, end});
  }

  const double sample_period = 1.0 / pipeline.time_base_.sample_rate();
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
    Entry entry = pipeline.MakeEntry(
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
    } else if (label.reason ==
               "sortformer_delayed_subminimum_clause_group_override") {
      entry.speaker_support = "strong";
      entry.speaker_uncertain = false;
      entry.speaker_decision.speaker_source =
          "sortformer_activity+primary_top1+vad_boundary+forced_alignment";
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
          "voiceprint_phrase_vad_dual_gallery_direct_override") {
        entry.speaker_decision.speaker_source =
            "sortformer_activity+titanet_phrase+vad_session+robust_gallery+"
            "forced_alignment";
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
          "voiceprint_phrase_partition_invariant_regular_initial_slot_"
          "override") {
        entry.speaker_decision.speaker_source =
            "sortformer_initial_slot+activity+primary_top1+titanet_phrase+"
            "titanet_vad+titanet_complete_source+robust_gallery+"
            "forced_alignment";
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

}  // namespace pipeline
}  // namespace orator
