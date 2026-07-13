// Typed evidence deposit and protocol-mirroring callbacks for AuditoryStream.
//
// Extracted from auditory_stream.cc to keep the controller focused on
// lifecycle. Pipeline records are committed to ComprehensiveTimeline as typed
// values before they are serialized for protocol persistence and transport.

#include "pipeline/auditory_stream_subscriptions.h"

#include "core/log.h"
#include "core/time_base.h"
#include "core/types.h"
#include "pipeline/comprehensive_timeline.h"
#include "pipeline/json_util.h"
#include "protocol/pipeline_registry.h"
#include "protocol/protocol_timeline.h"
#include "protocol/storage.h"
#include "protocol/topic.h"
#include "protocol/topic_router.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace orator {
namespace pipeline {
namespace {

bool DiarSegmentLess(const core::DiarSegment& a,
                     const core::DiarSegment& b) {
  constexpr double kTimeEpsilon = 1e-9;
  if (std::abs(a.start_sec - b.start_sec) > kTimeEpsilon) {
    return a.start_sec < b.start_sec;
  }
  if (a.end_sec != b.end_sec) return a.end_sec < b.end_sec;
  if (a.local_speaker != b.local_speaker) {
    return a.local_speaker < b.local_speaker;
  }
  return a.speaker_id < b.speaker_id;
}

}  // namespace

// ---------------------------------------------------------------------------
// Speaker sink callback: typed deposit first, then protocol mirror.
// ---------------------------------------------------------------------------
void HandleSpeakerSink(ComprehensiveTimeline& comp, std::mutex& state_mutex,
                       std::vector<core::DiarSegment>& last_segments,
                       protocol::ProtocolTimeline* protocol_timeline,
                       protocol::PipelineHandle* diar_handle,
                       const EventEmitter& emit,
                       const std::vector<core::DiarSegment>& segs) {
  std::vector<core::DiarSegment> ordered_segments = segs;
  std::stable_sort(ordered_segments.begin(), ordered_segments.end(),
                   DiarSegmentLess);

  std::string segments_json = "[";
  std::vector<ComprehensiveTimeline::SpeakerInput> typed_segments;
  typed_segments.reserve(ordered_segments.size());
  {
    std::lock_guard<std::mutex> lk(state_mutex);
    last_segments = ordered_segments;
  }
  for (size_t i = 0; i < ordered_segments.size(); ++i) {
    const auto& s = ordered_segments[i];
    // Keep "speaker" as the diarizer-local label (preserves the integer
    // mapping consumers derive from "speaker_<n>"); expose the resolved
    // global voiceprint identity, when any, in a separate "speaker_id" field
    // (Spec 010, backward compatible).
    const std::string label = "speaker_" + std::to_string(s.local_speaker);
    typed_segments.push_back(
        {s.start_sec, s.end_sec, label, s.confidence, s.speaker_id});
    char b[200];
    if (s.speaker_id.empty()) {
      std::snprintf(b, sizeof(b),
                    "{\"start\":%.3f,\"end\":%.3f,\"speaker\":\"%s\","
                    "\"confidence\":%.3f}",
                    s.start_sec, s.end_sec, label.c_str(), s.confidence);
    } else {
      std::snprintf(b, sizeof(b),
                    "{\"start\":%.3f,\"end\":%.3f,\"speaker\":\"%s\","
                    "\"speaker_id\":\"%s\",\"confidence\":%.3f}",
                    s.start_sec, s.end_sec, label.c_str(), s.speaker_id.c_str(),
                    s.confidence);
    }
    segments_json += b;
    if (i + 1 < ordered_segments.size()) segments_json += ",";
  }
  segments_json += "]";

  comp.DepositDiarization(typed_segments);

  protocol::Message msg;
  msg.topic = protocol::kDiarSpeakerSegment.to_string();
  msg.pipeline = "diar";
  msg.pipeline_version = "1.0.0";
  msg.timestamp_sec =
      ordered_segments.empty() ? 0.0 : ordered_segments[0].start_sec;
  msg.qos = static_cast<uint8_t>(protocol::QoS::AT_LEAST_ONCE);
  msg.schema_version = 1;
  msg.data = "{\"type\":\"diar\",\"source\":\"sortformer\",\"segments\":" +
             segments_json + "}";

  protocol_timeline->Publish(*diar_handle, protocol::kDiarSpeakerSegment, msg,
                             protocol::QoS::AT_LEAST_ONCE);
  if (emit) emit(msg.data);
}

// ---------------------------------------------------------------------------
// Text sink callback: typed final deposit first, then protocol mirror.
// ---------------------------------------------------------------------------
void HandleTextSink(ComprehensiveTimeline& comp,
                    protocol::ProtocolTimeline* protocol_timeline,
                    protocol::PipelineHandle* asr_handle, long id, double start,
                    double end, const std::string& text, bool is_final) {
  // Finals go to asr/transcript, in-progress partials to
  // asr/transcript_partial. Business fusion and forced alignment consume only
  // the typed finalized track, so each segment is recorded once.
  if (is_final) {
    const auto result = comp.DepositAsrFinal({id, start, end, text});
    if (result == ComprehensiveTimeline::DepositResult::kConflict ||
        result == ComprehensiveTimeline::DepositResult::kInvalid) {
      LOG_ERROR("[timeline] rejected ASR final id=%ld\n", id);
      return;
    }
    if (result == ComprehensiveTimeline::DepositResult::kUnchanged) return;
  }
  const protocol::Topic& topic =
      is_final ? protocol::kAsrTranscript : protocol::kAsrTranscriptPartial;
  protocol::Message msg;
  msg.topic = topic.to_string();
  msg.pipeline = "asr";
  msg.pipeline_version = "1.0.0";
  msg.timestamp_sec = start;
  msg.qos = static_cast<uint8_t>(protocol::QoS::AT_LEAST_ONCE);
  msg.schema_version = 1;
  msg.data = "{\"type\":\"" + std::string(is_final ? "asr" : "asr_partial") +
             "\",\"id\":" + std::to_string(id) +
             ",\"text_id\":" + std::to_string(id) +
             ",\"start\":" + std::to_string(start) +
             ",\"end\":" + std::to_string(end) + ",\"text\":\"" +
             JsonEscape(text) + "\"}";
  protocol_timeline->Publish(*asr_handle, topic, msg,
                             protocol::QoS::AT_LEAST_ONCE);
}

// ---------------------------------------------------------------------------
// Align sink callback: typed deposit first, then protocol mirror + WS emit.
// ---------------------------------------------------------------------------
void HandleAlignSink(ComprehensiveTimeline& comp,
                     protocol::ProtocolTimeline* protocol_timeline,
                     protocol::PipelineHandle* align_handle,
                     const EventEmitter& emit, long id, double seg_start,
                     double seg_end,
                     const std::vector<core::AlignUnit>& units) {
  std::string units_json = "[";
  std::vector<ComprehensiveTimeline::AlignUnitSeg> typed_units;
  typed_units.reserve(units.size());
  for (size_t i = 0; i < units.size(); ++i) {
    const auto& u = units[i];
    typed_units.push_back({u.start_sec, u.end_sec, u.text});
    char b[256];
    std::snprintf(b, sizeof(b), "{\"text\":\"%s\",\"start\":%.3f,\"end\":%.3f}",
                  JsonEscape(u.text).c_str(), u.start_sec, u.end_sec);
    units_json += b;
    if (i + 1 < units.size()) units_json += ",";
  }
  units_json += "]";

  std::string payload = "{\"type\":\"align\",\"id\":" + std::to_string(id) +
                        ",\"text_id\":" + std::to_string(id) +
                        ",\"start\":" + std::to_string(seg_start) +
                        ",\"end\":" + std::to_string(seg_end) +
                        ",\"units\":" + units_json + "}";

  const auto result =
      comp.DepositAlignment({id, seg_start, seg_end, std::move(typed_units)});
  if (result == ComprehensiveTimeline::DepositResult::kConflict ||
      result == ComprehensiveTimeline::DepositResult::kInvalid) {
    LOG_ERROR("[timeline] rejected alignment id=%ld\n", id);
    return;
  }
  if (result == ComprehensiveTimeline::DepositResult::kUnchanged) return;

  protocol::Message msg;
  msg.topic = protocol::kAlignUnits.to_string();
  msg.pipeline = "align";
  msg.pipeline_version = "1.0.0";
  msg.timestamp_sec = seg_start;
  msg.qos = static_cast<uint8_t>(protocol::QoS::AT_LEAST_ONCE);
  msg.schema_version = 1;
  msg.data = payload;
  protocol_timeline->Publish(*align_handle, protocol::kAlignUnits, msg,
                             protocol::QoS::AT_LEAST_ONCE);

  if (emit) emit(payload);
}

void HandleBusinessSpeakerRevision(
    protocol::ProtocolTimeline* protocol_timeline,
    protocol::PipelineHandle* business_handle, const EventEmitter& emit,
    const ComprehensiveTimeline::Revision& revision) {
  const std::string payload =
      SerializeRevisionToJson(revision, "business_speaker");

  protocol::Message message;
  message.topic = protocol::kBusinessSpeakerRevision.to_string();
  message.pipeline = "business_speaker";
  message.pipeline_version = "1.0.0";
  message.timestamp_sec = revision.dirty_start;
  message.qos = static_cast<uint8_t>(protocol::QoS::AT_LEAST_ONCE);
  message.schema_version = 1;
  message.data = payload;
  protocol_timeline->Publish(*business_handle,
                             protocol::kBusinessSpeakerRevision, message,
                             protocol::QoS::AT_LEAST_ONCE);
  if (emit) emit(payload);
}

// ---------------------------------------------------------------------------
// VAD drain: typed deposit first, then protocol mirror.
// ---------------------------------------------------------------------------
void HandleVadDrain(core::IVad* vad_detector, ComprehensiveTimeline& comp,
                    protocol::ProtocolTimeline* protocol_timeline,
                    protocol::PipelineHandle* vad_handle,
                    const EventEmitter& emit, const core::TimeBase& tb,
                    std::vector<core::VadSegmentResult>* segs, bool finalize) {
  segs->clear();
  vad_detector->DrainSegments(finalize, segs);
  for (const auto& sp : *segs) {
    const double s = tb.SecondsAt(sp.start_sample);
    const double e = tb.SecondsAt(sp.end_sample);
    comp.DepositVad({s, e});
    protocol::Message msg;
    msg.topic = protocol::kVadSpeechSegment.to_string();
    msg.pipeline = "vad";
    msg.pipeline_version = "1.0.0";
    msg.timestamp_sec = s;
    msg.qos = static_cast<uint8_t>(protocol::QoS::AT_LEAST_ONCE);
    msg.schema_version = 1;
    char buf[128];
    std::snprintf(buf, sizeof(buf),
                  "{\"type\":\"vad\",\"start\":%.3f,\"end\":%.3f,"
                  "\"source\":\"silero_gpu\"}",
                  s, e);
    msg.data = buf;
    protocol_timeline->Publish(*vad_handle, protocol::kVadSpeechSegment, msg,
                               protocol::QoS::AT_LEAST_ONCE);
    if (emit) emit(msg.data);
  }
}

void PublishVadProgress(ComprehensiveTimeline& comp,
                        protocol::ProtocolTimeline* protocol_timeline,
                        protocol::PipelineHandle* vad_handle,
                        double horizon_sec) {
  if (horizon_sec < 0.0) return;
  comp.AdvanceVadHorizon(horizon_sec);
  protocol::Message msg;
  msg.topic = protocol::kVadProgress.to_string();
  msg.pipeline = "vad";
  msg.pipeline_version = "1.0.0";
  msg.timestamp_sec = horizon_sec;
  msg.qos = static_cast<uint8_t>(protocol::QoS::AT_MOST_ONCE);
  msg.schema_version = 1;
  char buf[64];
  std::snprintf(buf, sizeof(buf),
                "{\"type\":\"vad_progress\",\"horizon\":%.3f}", horizon_sec);
  msg.data = buf;
  // AT_MOST_ONCE: a frequent, lossy heartbeat -- the next one supersedes it.
  protocol_timeline->Publish(*vad_handle, protocol::kVadProgress, msg,
                             protocol::QoS::AT_MOST_ONCE);
}

}  // namespace pipeline
}  // namespace orator
