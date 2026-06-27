#include "pipeline/json_util.h"

#include "pipeline/comprehensive_timeline.h"

#include <cstdio>
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

std::string SerializeRevisionToJson(const ComprehensiveTimeline::Revision& r,
                                    const char* source) {
  char buf[160];
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
                  "{\"start\":%.3f,\"end\":%.3f,\"text_id\":%ld,\"speaker\":%d,"
                  "\"text\":\"",
                  e.start, e.end, e.text_id, spk_idx);
    out += std::string(buf) + JsonEscape(e.text) + "\"}";
    if (i + 1 < r.entries.size()) out += ",";
  }
  out += "]}";
  return out;
}

}  // namespace pipeline
}  // namespace orator
