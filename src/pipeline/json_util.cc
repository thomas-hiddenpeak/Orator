#include "pipeline/json_util.h"

#include "pipeline/comprehensive_timeline.h"

#include <cstdio>
#include <stdexcept>
#include <string>

namespace orator {
namespace pipeline {
namespace {

int SpeakerIndex(const std::string& speaker) {
  if (speaker.rfind("speaker_", 0) != 0) return -1;
  try {
    return std::stoi(speaker.substr(8));
  } catch (const std::invalid_argument&) {
    return -1;
  } catch (const std::out_of_range&) {
    return -1;
  }
}

}  // namespace

std::string SerializeSpeakerDecisionToJson(
    const ComprehensiveTimeline::SpeakerDecisionAudit& decision) {
  char buf[256];
  std::string out = ",\"speaker_decision\":{";
  out += "\"speaker_source\":\"" + JsonEscape(decision.speaker_source) +
         "\",\"text_projection_source\":\"" +
         JsonEscape(decision.text_projection_source) + "\",\"reason\":\"" +
         JsonEscape(decision.reason) + "\"";
  std::snprintf(buf, sizeof(buf),
                ",\"overlap_margin_sec\":%.3f,"
                "\"confidence_margin\":%.6f,\"candidates\":[",
                decision.overlap_margin_sec, decision.confidence_margin);
  out += buf;
  for (std::size_t i = 0; i < decision.candidates.size(); ++i) {
    const auto& candidate = decision.candidates[i];
    std::snprintf(buf, sizeof(buf),
                  "{\"speaker\":%d,\"overlap_sec\":%.3f,"
                  "\"coverage_ratio\":%.6f,\"confidence\":%.6f,"
                  "\"island_count\":%d,\"selected\":%s",
                  SpeakerIndex(candidate.speaker), candidate.overlap_sec,
                  candidate.coverage_ratio, candidate.confidence,
                  candidate.island_count,
                  candidate.selected ? "true" : "false");
    out += buf;
    if (!candidate.speaker_id.empty()) {
      out += ",\"speaker_id\":\"" + JsonEscape(candidate.speaker_id) + "\"";
    }
    out += "}";
    if (i + 1 < decision.candidates.size()) out += ",";
  }
  out += "]}";
  return out;
}

std::string SerializeRevisionToJson(const ComprehensiveTimeline::Revision& r,
                                    const char* source) {
  char buf[256];
  std::string out = "{\"type\":\"revision\",\"source\":\"";
  out += source;
  out += "\",";
  std::snprintf(buf, sizeof(buf),
                "\"dirty_start\":%.9f,\"dirty_end\":%.9f,\"entries\":[",
                r.dirty_start, r.dirty_end);
  out += buf;
  for (size_t i = 0; i < r.entries.size(); ++i) {
    const auto& e = r.entries[i];
    int spk_idx = -1;
    if (e.speaker.size() > 8 && e.speaker.substr(0, 8) == "speaker_") {
      try {
        spk_idx = std::stoi(e.speaker.substr(8));
      } catch (const std::invalid_argument&) {
        spk_idx = -1;
      } catch (const std::out_of_range&) {
        spk_idx = -1;
      }
    }
    std::snprintf(buf, sizeof(buf),
                  "{\"start\":%.9f,\"end\":%.9f,\"text_id\":%ld,\"speaker\":%d",
                  e.start, e.end, e.text_id, spk_idx);
    out += buf;
    // Spec 010: surface the resolved global voiceprint identity live. Prefer
    // the per-entry id because one diarizer-local label can map to different
    // global identities over time after local drift splitting.
    if (!e.speaker_id.empty()) {
      out += ",\"speaker_id\":\"" + e.speaker_id + "\"";
    }
    std::snprintf(
        buf, sizeof(buf),
        ",\"speaker_support\":\"%s\","
        "\"speaker_uncertain\":%s,"
        "\"diar_overlap_sec\":%.3f,"
        "\"diar_total_overlap_sec\":%.3f,"
        "\"diar_coverage_ratio\":%.3f,"
        "\"diar_total_coverage_ratio\":%.3f,"
        "\"diar_max_gap_sec\":%.3f,"
        "\"diar_island_count\":%d",
        e.speaker_support.c_str(), e.speaker_uncertain ? "true" : "false",
        e.diar_overlap_sec, e.diar_total_overlap_sec, e.diar_coverage_ratio,
        e.diar_total_coverage_ratio, e.diar_max_gap_sec, e.diar_island_count);
    out += buf;
    out += SerializeSpeakerDecisionToJson(e.speaker_decision);
    out += ",\"text\":\"" + JsonEscape(e.text) + "\"}";
    if (i + 1 < r.entries.size()) out += ",";
  }
  out += "]}";
  return out;
}

}  // namespace pipeline
}  // namespace orator
