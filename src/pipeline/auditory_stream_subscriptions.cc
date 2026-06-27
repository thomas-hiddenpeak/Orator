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

// Shared revision serializer defined in src/pipeline/json_util.cc.
std::string SerializeRevisionToJson(
    const ComprehensiveTimeline::Revision& r, const char* source);

// ---------------------------------------------------------------------------
// Local helper: serialize a Revision to JSON and emit via callback.
// Uses the shared SerializeRevisionToJson from json_util.
// ---------------------------------------------------------------------------
namespace {
void DoEmitRevision(const ComprehensiveTimeline::Revision& r,
                    const char* source, RevisionEmitter emit_rev) {
  emit_rev(SerializeRevisionToJson(r, source));
}
}  // namespace

// ---------------------------------------------------------------------------
// VAD subscription: parse {"start":..., "end":...} → comp_.AddVad()
// ---------------------------------------------------------------------------
void HandleVadSubscription(ComprehensiveTimeline& comp,
                           std::mutex& comp_mutex,
                           const protocol::Message& msg) {
  // Only speech segments populate the timeline; vad/progress heartbeats (same
  // vad/+ subscription) carry no segment and must not create vad entries.
  if (msg.topic != protocol::kVadSpeechSegment.to_string()) return;
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
                            const protocol::Message& msg,
                            RevisionEmitter emit_rev) {
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

  std::vector<ComprehensiveTimeline::Revision> revs;
  {
    std::lock_guard<std::mutex> lk(comp_mutex);
    revs = comp.ReplaceSpeakers(speakers);
  }
  for (const auto& r : revs) {
    DoEmitRevision(r, "diar", emit_rev);
  }
}

// ---------------------------------------------------------------------------
// ASR subscription: parse {"id":..., "start":..., "end":..., "text":"..."}
// → comp_.UpsertText()
// ---------------------------------------------------------------------------
void HandleAsrSubscription(ComprehensiveTimeline& comp,
                           std::mutex& comp_mutex,
                           const protocol::Message& msg,
                           RevisionEmitter emit_rev) {
  const std::string& data = msg.data;

  long id = JsonParseLong(data, "id");
  double start = JsonParseNum(data, "start");
  double end = JsonParseNum(data, "end");
  std::string text = JsonParseStr(data, "text");

  if (id < 0) return;

  std::vector<ComprehensiveTimeline::Revision> revs;
  {
    std::lock_guard<std::mutex> lk(comp_mutex);
    revs = comp.UpsertText(id, start, end, text);
  }
  for (const auto& r : revs) {
    DoEmitRevision(r, "asr", emit_rev);
  }
}

// ---------------------------------------------------------------------------
// Align subscription: parse {"id":..,"start":..,"end":..,"units":[{...}]}
// → comp_.UpsertAlign(). Brings the forced-alignment per-unit timestamps into
// the comprehensive timeline's align track. Times are already on the common
// time base (the align worker offset them); this is a pure container deposit.
// ---------------------------------------------------------------------------
void HandleAlignSubscription(ComprehensiveTimeline& comp,
                             std::mutex& comp_mutex,
                             const protocol::Message& msg) {
  const std::string& data = msg.data;
  const long id = JsonParseLong(data, "id");
  if (id < 0) return;
  const double seg_start = JsonParseNum(data, "start");
  const double seg_end = JsonParseNum(data, "end");

  auto arr_start = data.find("\"units\":[");
  if (arr_start == std::string::npos) return;
  arr_start += 9;  // len of "\"units\":["
  int depth = 1;
  size_t pos = arr_start;
  while (pos < data.size() && depth > 0) {
    if (data[pos] == '[') depth++;
    else if (data[pos] == ']') depth--;
    pos++;
  }
  if (depth != 0) return;
  const std::string units_json = data.substr(arr_start, pos - arr_start - 1);

  std::vector<ComprehensiveTimeline::AlignUnitSeg> units;
  size_t obj_pos = 0;
  while (obj_pos < units_json.size()) {
    size_t brace = units_json.find('{', obj_pos);
    if (brace == std::string::npos) break;
    size_t brace_end = units_json.find('}', brace);
    if (brace_end == std::string::npos) break;
    const std::string obj = units_json.substr(brace, brace_end - brace + 1);

    ComprehensiveTimeline::AlignUnitSeg u;
    u.text = JsonParseStr(obj, "text");
    u.start = JsonParseNum(obj, "start");
    u.end = JsonParseNum(obj, "end");
    units.push_back(std::move(u));

    obj_pos = brace_end + 1;
  }

  std::lock_guard<std::mutex> lk(comp_mutex);
  comp.UpsertAlign(id, seg_start, seg_end, units);
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
                    const std::string& text, bool is_final) {
  // Finals go to asr/transcript, in-progress partials to asr/transcript_partial.
  // The comprehensive timeline subscribes to asr/+ (both, in-place revision by
  // id); the forced aligner subscribes to asr/transcript only, so it aligns each
  // segment exactly once against its finalized text.
  const protocol::Topic& topic =
      is_final ? protocol::kAsrTranscript : protocol::kAsrTranscriptPartial;
  protocol::Message msg;
  msg.topic = topic.to_string();
  msg.pipeline = "asr";
  msg.pipeline_version = "1.0.0";
  msg.timestamp_sec = start;
  msg.qos = static_cast<uint8_t>(protocol::QoS::AT_LEAST_ONCE);
  msg.schema_version = 1;
  msg.data = "{\"id\":" + std::to_string(id)
             + ",\"start\":" + std::to_string(start)
             + ",\"end\":" + std::to_string(end)
             + ",\"text\":\"" + JsonEscape(text) + "\"}";
  protocol_timeline->Publish(*asr_handle, topic, msg,
                             protocol::QoS::AT_LEAST_ONCE);
}

// ---------------------------------------------------------------------------
// Align sink callback: forced-alignment worker → protocol publish + WS emit
// Builds the align/units message (per-unit timestamps for one transcript
// segment) and publishes it; `emit` forwards the same JSON to the transport.
// ---------------------------------------------------------------------------
void HandleAlignSink(protocol::ProtocolTimeline* protocol_timeline,
                     protocol::PipelineHandle* align_handle,
                     const RevisionEmitter& emit,
                     long id, double seg_start, double seg_end,
                     const std::vector<core::AlignUnit>& units) {
  std::string units_json = "[";
  for (size_t i = 0; i < units.size(); ++i) {
    const auto& u = units[i];
    char b[256];
    std::snprintf(b, sizeof(b),
                  "{\"text\":\"%s\",\"start\":%.3f,\"end\":%.3f}",
                  JsonEscape(u.text).c_str(), u.start_sec, u.end_sec);
    units_json += b;
    if (i + 1 < units.size()) units_json += ",";
  }
  units_json += "]";

  std::string payload = "{\"id\":" + std::to_string(id)
      + ",\"start\":" + std::to_string(seg_start)
      + ",\"end\":" + std::to_string(seg_end)
      + ",\"units\":" + units_json + "}";

  protocol::Message msg;
  msg.topic = protocol::kAlignUnits.to_string();
  msg.pipeline = "align";
  msg.pipeline_version = "1.0.0";
  msg.timestamp_sec = seg_start;
  msg.qos = static_cast<uint8_t>(protocol::QoS::AT_LEAST_ONCE);
  msg.schema_version = 1;
  msg.data = payload;
  protocol_timeline->Publish(*align_handle, protocol::kAlignUnits,
                             msg, protocol::QoS::AT_LEAST_ONCE);

  if (emit) emit("{\"type\":\"align\"," + payload.substr(1));
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

void PublishVadProgress(protocol::ProtocolTimeline* protocol_timeline,
                        protocol::PipelineHandle* vad_handle,
                        double horizon_sec) {
  if (horizon_sec < 0.0) return;
  protocol::Message msg;
  msg.topic = protocol::kVadProgress.to_string();
  msg.pipeline = "vad";
  msg.pipeline_version = "1.0.0";
  msg.timestamp_sec = horizon_sec;
  msg.qos = static_cast<uint8_t>(protocol::QoS::AT_MOST_ONCE);
  msg.schema_version = 1;
  char buf[64];
  std::snprintf(buf, sizeof(buf), "{\"horizon\":%.3f}", horizon_sec);
  msg.data = buf;
  // AT_MOST_ONCE: a frequent, lossy heartbeat -- the next one supersedes it.
  protocol_timeline->Publish(*vad_handle, protocol::kVadProgress, msg,
                             protocol::QoS::AT_MOST_ONCE);
}

}  // namespace pipeline
}  // namespace orator
