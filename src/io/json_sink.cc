#include "io/json_sink.h"

#include <sstream>

namespace orator {
namespace io {

namespace {

std::string EscapeJson(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (char c : s) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out += c;
    }
  }
  return out;
}

std::string FormatTime(double seconds) {
  std::ostringstream oss;
  oss.setf(std::ios::fixed);
  oss.precision(3);
  oss << seconds;
  return oss.str();
}

}  // namespace

std::string TimelineToJson(const core::Timeline& timeline, bool pretty) {
  std::ostringstream oss;
  const std::string nl = pretty ? "\n" : "";
  const std::string ind1 = pretty ? "  " : "";
  const std::string ind2 = pretty ? "    " : "";
  const std::string sp = pretty ? " " : "";

  oss << "{" << nl << ind1 << "\"segments\":" << sp << "[";
  for (size_t i = 0; i < timeline.segments.size(); ++i) {
    const auto& seg = timeline.segments[i];
    oss << nl << ind2 << "{";
    oss << "\"start\":" << sp << FormatTime(seg.start_sec) << "," << sp;
    oss << "\"end\":" << sp << FormatTime(seg.end_sec) << "," << sp;
    oss << "\"speaker_id\":" << sp << "\"" << EscapeJson(seg.speaker_id)
        << "\"," << sp;
    oss << "\"text\":" << sp << "\"" << EscapeJson(seg.text) << "\"}";
    if (i + 1 < timeline.segments.size()) oss << ",";
  }
  if (!timeline.segments.empty()) oss << nl << ind1;
  oss << "]" << nl << "}";
  return oss.str();
}

void JsonSink::Consume(const core::Timeline& timeline) {
  out_ << TimelineToJson(timeline, pretty_) << std::endl;
}

}  // namespace io
}  // namespace orator
