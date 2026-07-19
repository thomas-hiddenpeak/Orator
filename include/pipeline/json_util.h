#pragma once

// Minimal JSON string escaping and serialization for the timeline/event
// serializers. Shared so the ASR worker (incremental events) and the
// controller (timeline document) escape UTF-8 text identically without
// duplicating the logic.

#include <cstdio>
#include <string>

#include "pipeline/comprehensive_timeline.h"

namespace orator {
namespace pipeline {

// Escape a UTF-8 string for embedding in a JSON string value (quotes,
// backslash, control chars). Multi-byte UTF-8 bytes pass through unchanged.
inline std::string JsonEscape(const std::string& s) {
  std::string o;
  o.reserve(s.size() + 8);
  for (char c : s) {
    switch (c) {
      case '"':
        o += "\\\"";
        break;
      case '\\':
        o += "\\\\";
        break;
      case '\n':
        o += "\\n";
        break;
      case '\r':
        o += "\\r";
        break;
      case '\t':
        o += "\\t";
        break;
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

std::string SerializeRevisionToJson(
    const ComprehensiveTimeline::Revision& revision, const char* source);

// Serialize the structured attribution audit shared by live revisions and the
// terminal business-speaker track. The returned fragment starts with a comma.
std::string SerializeSpeakerDecisionToJson(
    const ComprehensiveTimeline::SpeakerDecisionAudit& decision);

// Serialize one complete speaker-voiceprint evidence object without assuming
// a maximum identifier or formatted-record length.
std::string SerializeSpeakerVoiceprintEvidenceToJson(
    const ComprehensiveTimeline::SpeakerVoiceprintEvidence& evidence);

}  // namespace pipeline
}  // namespace orator
