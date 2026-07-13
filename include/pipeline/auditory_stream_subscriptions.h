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
#include "pipeline/comprehensive_timeline.h"

namespace orator {

// Emit callback for self-describing live-event JSON strings.
using EventEmitter = std::function<void(const std::string&)>;

namespace protocol {
class PipelineHandle;
class ProtocolTimeline;
}  // namespace protocol

namespace pipeline {

// Speaker sink callback: diarization worker → typed track deposit → protocol
// mirror. Business revisions are produced independently from the committed
// evidence by BusinessSpeakerPipeline.
void HandleSpeakerSink(ComprehensiveTimeline& comp, std::mutex& state_mutex,
                       std::vector<core::DiarSegment>& last_segments,
                       protocol::ProtocolTimeline* protocol_timeline,
                       protocol::PipelineHandle* diar_handle,
                       const EventEmitter& emit,
                       const std::vector<core::DiarSegment>& segs);

// Text sink callback: ASR worker → typed final deposit → protocol mirror.
// Partials are transport-only and never enter the finalized ASR track.
void HandleTextSink(ComprehensiveTimeline& comp,
                    protocol::ProtocolTimeline* protocol_timeline,
                    protocol::PipelineHandle* asr_handle, long id, double start,
                    double end, const std::string& text, bool is_final);

// Align sink callback: forced-alignment worker → typed track deposit → protocol
// mirror + WS event.
void HandleAlignSink(ComprehensiveTimeline& comp,
                     protocol::ProtocolTimeline* protocol_timeline,
                     protocol::PipelineHandle* align_handle,
                     const EventEmitter& emit, long id, double seg_start,
                     double seg_end, const std::vector<core::AlignUnit>& units);

// Mirror an already committed business-speaker revision to protocol and the
// live WebSocket event stream.
void HandleBusinessSpeakerRevision(
    protocol::ProtocolTimeline* protocol_timeline,
    protocol::PipelineHandle* business_handle, const EventEmitter& emit,
    const ComprehensiveTimeline::Revision& revision);

// VAD drain: extract segments, deposit typed evidence, then mirror to protocol.
void HandleVadDrain(core::IVad* vad_detector, ComprehensiveTimeline& comp,
                    protocol::ProtocolTimeline* protocol_timeline,
                    protocol::PipelineHandle* vad_handle,
                    const EventEmitter& emit, const core::TimeBase& tb,
                    std::vector<core::VadSegmentResult>* segs, bool finalize);

// Publish a VAD progress/horizon heartbeat: the absolute time (common clock)
// up to which VAD has processed and confirmed silence, so ASR can skip
// confirmed silence at real-time pacing. Frequent + lossy (AT_MOST_ONCE).
void PublishVadProgress(ComprehensiveTimeline& comp,
                        protocol::ProtocolTimeline* protocol_timeline,
                        protocol::PipelineHandle* vad_handle,
                        double horizon_sec);

}  // namespace pipeline
}  // namespace orator
