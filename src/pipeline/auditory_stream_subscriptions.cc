// Protocol subscription bridge callbacks for AuditoryStream.
//
// Extracted from auditory_stream.cc to keep the controller focused on lifecycle.
// Each function handles one protocol subscription or pipeline sink callback,
// receiving only the AuditoryStream members it needs as explicit parameters.

#include "pipeline/auditory_stream_subscriptions.h"

#include "core/time_base.h"
#include "core/types.h"
#include "pipeline/comprehensive_timeline.h"
#include "pipeline/json_util.h"
#include "protocol/pipeline_registry.h"
#include "protocol/protocol_timeline.h"
#include "protocol/storage.h"
#include "protocol/topic.h"
#include "protocol/topic_router.h"

#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

namespace orator {
namespace pipeline {

// ---------------------------------------------------------------------------
// VAD subscription: parse {"start":..., "end":...} → comp_.AddVad()
// ---------------------------------------------------------------------------
void HandleVadSubscription(ComprehensiveTimeline& comp,
                           std::mutex& comp_mutex,
                           const protocol::Message& msg) {
  double start = JsonParseNum(msg.data, "start");
  double end = JsonParseNum(msg.data, "end");
  std::lock_guard<std::mutex> lk(comp_mutex);
  comp.AddVad(start, end);
}

// ---------------------------------------------------------------------------
// Diar subscription: parse {"segments":[{...},...]} → comp_.ReplaceSpeakers()
// ---------------------------------------------------------------------------
void HandleDiarSubscription(ComprehensiveTimeline& comp,
                            std::mutex& comp_mutex,
                            const protocol::Message& msg) {
  const std::string& data = msg.data;
  auto seg_start = data.find("\"segments\":[");
  if (seg_start == std::string::npos) return;
  seg_start += 12;  // len of "\"segments\":["
  // Find matching closing bracket for the segments array
  int bracket_depth = 1;
  size_t pos = seg_start;
  while (pos < data.size() && bracket_depth > 0) {
    if (data[pos] == '[') bracket_depth++;
    else if (data[pos] == ']') bracket_depth--;
    pos++;
  }
  if (bracket_depth != 0) return;
  std::string seg_json = data.substr(seg_start, pos - seg_start - 1);

  std::vector<ComprehensiveTimeline::SpeakerInput> speakers;
  // Parse each {"start":...,"end":...,"speaker":"...","confidence":...} object
  size_t obj_pos = 0;
  while (obj_pos < seg_json.size()) {
    size_t brace = seg_json.find('{', obj_pos);
    if (brace == std::string::npos) break;
    size_t brace_end = seg_json.find('}', brace);
    if (brace_end == std::string::npos) break;
    std::string obj = seg_json.substr(brace, brace_end - brace + 1);

    ComprehensiveTimeline::SpeakerInput si;
    si.start = JsonParseNum(obj, "start");
    si.end = JsonParseNum(obj, "end");
    si.speaker = JsonParseStr(obj, "speaker");
    si.conf = static_cast<float>(JsonParseNum(obj, "confidence"));
    speakers.push_back(si);

    obj_pos = brace_end + 1;
  }

  std::lock_guard<std::mutex> lk(comp_mutex);
  comp.ReplaceSpeakers(speakers);
}

// ---------------------------------------------------------------------------
// ASR subscription: parse {"id":..., "start":..., "end":..., "text":"..."}
// → comp_.UpsertText()
// ---------------------------------------------------------------------------
void HandleAsrSubscription(ComprehensiveTimeline& comp,
                           std::mutex& comp_mutex,
                           const protocol::Message& msg) {
  const std::string& data = msg.data;

  long id = JsonParseLong(data, "id");
  double start = JsonParseNum(data, "start");
  double end = JsonParseNum(data, "end");
  std::string text = JsonParseStr(data, "text");

  if (id < 0) return;

  std::lock_guard<std::mutex> lk(comp_mutex);
  comp.UpsertText(id, start, end, text);
}

// ---------------------------------------------------------------------------
// Speaker sink callback: diarization worker → protocol publish
// Stores last_segments_ under comp_mutex_ for Serialize(), then publishes
// to the protocol timeline on the diar/speaker_segment topic.
// ---------------------------------------------------------------------------
void HandleSpeakerSink(std::mutex& comp_mutex,
                       std::vector<core::DiarSegment>& last_segments,
                       protocol::ProtocolTimeline* protocol_timeline,
                       protocol::PipelineHandle* diar_handle,
                       const std::vector<core::DiarSegment>& segs) {
  std::string segments_json = "[";
  {
    std::lock_guard<std::mutex> lk(comp_mutex);
    last_segments = segs;
    for (size_t i = 0; i < segs.size(); ++i) {
      const auto& s = segs[i];
      const std::string label =
          s.speaker_id.empty()
              ? ("speaker_" + std::to_string(s.local_speaker))
              : s.speaker_id;
      char b[160];
      std::snprintf(b, sizeof(b),
                    "{\"start\":%.3f,\"end\":%.3f,\"speaker\":\"%s\","
                    "\"confidence\":%.3f}",
                    s.start_sec, s.end_sec, label.c_str(), s.confidence);
      segments_json += b;
      if (i + 1 < segs.size()) segments_json += ",";
    }
  }
  segments_json += "]";

  protocol::Message msg;
  msg.topic = protocol::kDiarSpeakerSegment.to_string();
  msg.pipeline = "diar";
  msg.pipeline_version = "1.0.0";
  msg.timestamp_sec = segs.empty() ? 0.0 : segs[0].start_sec;
  msg.qos = static_cast<uint8_t>(protocol::QoS::AT_LEAST_ONCE);
  msg.schema_version = 1;
  msg.data = "{\"source\":\"sortformer\",\"segments\":" + segments_json + "}";

  protocol_timeline->Publish(*diar_handle, protocol::kDiarSpeakerSegment,
                             msg, protocol::QoS::AT_LEAST_ONCE);
}

// ---------------------------------------------------------------------------
// Text sink callback: ASR worker → protocol publish
// Builds a protocol message from the ASR text segment and publishes it on
// the asr/transcript topic.
// ---------------------------------------------------------------------------
void HandleTextSink(protocol::ProtocolTimeline* protocol_timeline,
                    protocol::PipelineHandle* asr_handle,
                    long id, double start, double end,
                    const std::string& text) {
  protocol::Message msg;
  msg.topic = protocol::kAsrTranscript.to_string();
  msg.pipeline = "asr";
  msg.pipeline_version = "1.0.0";
  msg.timestamp_sec = start;
  msg.qos = static_cast<uint8_t>(protocol::QoS::AT_LEAST_ONCE);
  msg.schema_version = 1;
  msg.data = "{\"id\":" + std::to_string(id)
             + ",\"start\":" + std::to_string(start)
             + ",\"end\":" + std::to_string(end)
             + ",\"text\":\"" + JsonEscape(text) + "\"}";
  protocol_timeline->Publish(*asr_handle, protocol::kAsrTranscript,
                             msg, protocol::QoS::AT_LEAST_ONCE);
}

// ---------------------------------------------------------------------------
// VAD drain: extract segments from IVad and publish to protocol timeline.
// ---------------------------------------------------------------------------
void HandleVadDrain(core::IVad* vad_detector,
                    protocol::ProtocolTimeline* protocol_timeline,
                    protocol::PipelineHandle* vad_handle,
                    const core::TimeBase& tb,
                    std::vector<core::VadSegmentResult>* segs,
                    bool finalize) {
  segs->clear();
  vad_detector->DrainSegments(finalize, segs);
  for (const auto& sp : *segs) {
    const double s = tb.SecondsAt(sp.start_sample);
    const double e = tb.SecondsAt(sp.end_sample);
    protocol::Message msg;
    msg.topic = protocol::kVadSpeechSegment.to_string();
    msg.pipeline = "vad";
    msg.pipeline_version = "1.0.0";
    msg.timestamp_sec = s;
    msg.qos = static_cast<uint8_t>(protocol::QoS::AT_LEAST_ONCE);
    msg.schema_version = 1;
    char buf[128];
    std::snprintf(buf, sizeof(buf),
                  "{\"start\":%.3f,\"end\":%.3f,\"source\":\"silero_gpu\"}",
                  s, e);
    msg.data = buf;
    protocol_timeline->Publish(*vad_handle, protocol::kVadSpeechSegment,
                                msg, protocol::QoS::AT_LEAST_ONCE);
  }
}

}  // namespace pipeline
}  // namespace orator
