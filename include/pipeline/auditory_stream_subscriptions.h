#pragma once

// Free functions for AuditoryStream protocol subscription and pipeline sink
// callbacks. Extracted from auditory_stream.cc to keep the controller focused
// on lifecycle. Each function receives only the AuditoryStream members it needs
// as explicit parameters.

#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "core/stages.h"
#include "core/time_base.h"
#include "core/types.h"

namespace orator {

// Emit callback for revision JSON strings.
using RevisionEmitter = std::function<void(const std::string&)>;

namespace protocol {
class Message;
class PipelineHandle;
class ProtocolTimeline;
}  // namespace protocol

namespace pipeline {

class ComprehensiveTimeline;

// VAD subscription: parse {"start":..., "end":...} → comp_.AddVad()
void HandleVadSubscription(ComprehensiveTimeline& comp,
                           std::mutex& comp_mutex,
                           const protocol::Message& msg);

// Diar subscription: parse {"segments":[{...},...]} → comp_.ReplaceSpeakers()
// emit_rev is called outside comp_mutex for each revision produced.
void HandleDiarSubscription(ComprehensiveTimeline& comp,
                            std::mutex& comp_mutex,
                            const protocol::Message& msg,
                            RevisionEmitter emit_rev);

// ASR subscription: parse {"id":..., "start":..., "end":..., "text":"..."}
// → comp_.UpsertText(). emit_rev is called outside comp_mutex for each
// revision produced.
void HandleAsrSubscription(ComprehensiveTimeline& comp,
                           std::mutex& comp_mutex,
                           const protocol::Message& msg,
                           RevisionEmitter emit_rev);

// Speaker sink callback: diarization worker → protocol publish
void HandleSpeakerSink(std::mutex& comp_mutex,
                       std::vector<core::DiarSegment>& last_segments,
                       protocol::ProtocolTimeline* protocol_timeline,
                       protocol::PipelineHandle* diar_handle,
                       const std::vector<core::DiarSegment>& segs);

// Text sink callback: ASR worker → protocol publish
void HandleTextSink(protocol::ProtocolTimeline* protocol_timeline,
                    protocol::PipelineHandle* asr_handle,
                    long id, double start, double end,
                    const std::string& text);

// VAD drain: extract segments from IVad and publish to protocol timeline.
void HandleVadDrain(core::IVad* vad_detector,
                    protocol::ProtocolTimeline* protocol_timeline,
                    protocol::PipelineHandle* vad_handle,
                    const core::TimeBase& tb,
                    std::vector<core::VadSegmentResult>* segs,
                    bool finalize);

// Publish a VAD progress/horizon heartbeat: the absolute time (common clock)
// up to which VAD has processed and confirmed silence, so ASR can skip
// confirmed silence at real-time pacing. Frequent + lossy (AT_MOST_ONCE).
void PublishVadProgress(protocol::ProtocolTimeline* protocol_timeline,
                        protocol::PipelineHandle* vad_handle,
                        double horizon_sec);

}  // namespace pipeline
}  // namespace orator
