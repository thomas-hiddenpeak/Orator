#pragma once

// Minimal JSON string escaping and serialization for the timeline/event
// serializers. Shared so the ASR worker (incremental events) and the
// controller (timeline document) escape UTF-8 text identically without
// duplicating the logic.

#include <string>

#include "io/json_escape.h"
#include "pipeline/comprehensive_timeline.h"

namespace orator {
namespace pipeline {

// Escape a UTF-8 string for embedding in a JSON string value (quotes,
// backslash, control chars). Multi-byte UTF-8 bytes pass through unchanged.
inline std::string JsonEscape(const std::string& s) {
  return io::JsonEscape(s);
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
