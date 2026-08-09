#pragma once

#include <cstdio>
#include <string>

namespace orator {
namespace io {

// Escape UTF-8 text for one JSON string value. Multi-byte bytes pass through;
// only JSON syntax and ASCII control bytes are rewritten.
inline std::string JsonEscape(const std::string& value) {
  std::string escaped;
  escaped.reserve(value.size() + 8);
  for (char c : value) {
    switch (c) {
      case '"':
        escaped += "\\\"";
        break;
      case '\\':
        escaped += "\\\\";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char buffer[8];
          std::snprintf(buffer, sizeof(buffer), "\\u%04x",
                        static_cast<unsigned char>(c));
          escaped += buffer;
        } else {
          escaped += c;
        }
    }
  }
  return escaped;
}

}  // namespace io
}  // namespace orator
