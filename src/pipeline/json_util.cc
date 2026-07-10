#include "pipeline/json_util.h"

#include "pipeline/comprehensive_timeline.h"

#include <cstdio>
#include <map>
#include <stdexcept>
#include <string>

namespace orator {
namespace pipeline {

double JsonParseNum(const std::string& data, const char* key) {
  std::string search = "\"" + std::string(key) + "\":";
  auto kp = data.find(search);
  if (kp == std::string::npos) return 0.0;
  kp += search.size();
  auto ve = data.find_first_of(",}", kp);
  if (ve == std::string::npos) return 0.0;
  try {
    return std::stod(data.substr(kp, ve - kp));
  } catch (const std::exception&) {
    return 0.0;
  }
}

std::string JsonParseStr(const std::string& data, const char* key) {
  std::string search = "\"" + std::string(key) + "\":";
  auto kp = data.find(search);
  if (kp == std::string::npos) return "";
  kp += search.size();
  if (kp >= data.size() || data[kp] != '"') return "";
  kp++;  // skip opening quote
  // Handle escaped quotes within the string value
  std::string result;
  while (kp < data.size()) {
    if (data[kp] == '\\' && kp + 1 < data.size()) {
      result += data[kp + 1];
      kp += 2;
    } else if (data[kp] == '"') {
      break;
    } else {
      result += data[kp];
      kp++;
    }
  }
  return result;
}

long JsonParseLong(const std::string& data, const char* key) {
  std::string search = "\"" + std::string(key) + "\":";
  auto kp = data.find(search);
  if (kp == std::string::npos) return -1;
  kp += search.size();
  auto ve = data.find_first_of(",}", kp);
  if (ve == std::string::npos) return -1;
  try {
    return static_cast<long>(std::stol(data.substr(kp, ve - kp)));
  } catch (const std::exception&) {
    return -1;
  }
}

std::string SerializeRevisionToJson(
    const ComprehensiveTimeline::Revision& r, const char* source,
    const std::map<std::string, std::string>* label_ids) {
  char buf[256];
  std::string out = "{\"type\":\"revision\",\"source\":\"";
  out += source;
  out += "\",";
  std::snprintf(buf, sizeof(buf),
                "\"dirty_start\":%.3f,\"dirty_end\":%.3f,\"entries\":[",
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
                  "{\"start\":%.3f,\"end\":%.3f,\"text_id\":%ld,\"speaker\":%d",
                  e.start, e.end, e.text_id, spk_idx);
    out += buf;
    // Spec 010: surface the resolved global voiceprint identity live. Prefer
    // the per-entry id because one diarizer-local label can map to different
    // global identities over time after local drift splitting.
    if (!e.speaker_id.empty()) {
      out += ",\"speaker_id\":\"" + e.speaker_id + "\"";
    } else if (label_ids) {
      auto it = label_ids->find(e.speaker);
      if (it != label_ids->end() && !it->second.empty())
        out += ",\"speaker_id\":\"" + it->second + "\"";
    }
    std::snprintf(buf, sizeof(buf),
                  ",\"speaker_support\":\"%s\","
                  "\"diar_overlap_sec\":%.3f,"
                  "\"diar_total_overlap_sec\":%.3f,"
                  "\"diar_coverage_ratio\":%.3f,"
                  "\"diar_total_coverage_ratio\":%.3f,"
                  "\"diar_max_gap_sec\":%.3f,"
                  "\"diar_island_count\":%d",
                  e.speaker_support.c_str(), e.diar_overlap_sec,
                  e.diar_total_overlap_sec, e.diar_coverage_ratio,
                  e.diar_total_coverage_ratio, e.diar_max_gap_sec,
                  e.diar_island_count);
    out += buf;
    out += ",\"text\":\"" + JsonEscape(e.text) + "\"}";
    if (i + 1 < r.entries.size()) out += ",";
  }
  out += "]}";
  return out;
}

}  // namespace pipeline
}  // namespace orator
