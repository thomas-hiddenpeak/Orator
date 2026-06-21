#include "pipeline/json_util.h"

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

}  // namespace pipeline
}  // namespace orator
