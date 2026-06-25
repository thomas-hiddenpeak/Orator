#pragma once

// Minimal JSON string escaping and serialization for the timeline/event
// serializers. Shared so the ASR worker (incremental events) and the
// controller (timeline document) escape UTF-8 text identically without
// duplicating the logic.
//
// Also provides JSON key-value parsing helpers.

#include <cstdio>
#include <string>

namespace orator {
namespace pipeline {

// Escape a UTF-8 string for embedding in a JSON string value (quotes,
// backslash, control chars). Multi-byte UTF-8 bytes pass through unchanged.
inline std::string JsonEscape(const std::string& s) {
  std::string o;
  o.reserve(s.size() + 8);
  for (char c : s) {
    switch (c) {
      case '"': o += "\\\""; break;
      case '\\': o += "\\\\"; break;
      case '\n': o += "\\n"; break;
      case '\r': o += "\\r"; break;
      case '\t': o += "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", c);
          o += buf;
        } else {
          o += c;
        }
    }
  }
  return o;
}

// Parse a numeric value for a JSON key. Returns 0.0 on failure.
double JsonParseNum(const std::string& data, const char* key);

// Parse a string value for a JSON key. Returns "" on failure.
// Handles escaped quotes within the string value.
std::string JsonParseStr(const std::string& data, const char* key);

// Parse a long integer value for a JSON key. Returns -1 on failure.
long JsonParseLong(const std::string& data, const char* key);

}  // namespace pipeline
}  // namespace orator
