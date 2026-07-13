#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "pipeline/auditory_stream_subscriptions.h"
#include "pipeline/business_speaker_pipeline.h"
#include "pipeline/comprehensive_timeline.h"
#include "protocol/pipeline_registry.h"
#include "protocol/protocol_timeline.h"
#include "protocol/topic.h"

namespace {

int failures = 0;

#define CHECK(condition, message)         \
  do {                                    \
    if (!(condition)) {                   \
      std::printf("FAIL: %s\n", message); \
      ++failures;                         \
    }                                     \
  } while (0)

}  // namespace

int main() {
  using orator::pipeline::ComprehensiveTimeline;
  using orator::protocol::Message;
  using orator::protocol::PipelineDescriptor;
  using orator::protocol::ProtocolTimeline;
  using orator::protocol::TopicPattern;

  ComprehensiveTimeline evidence;
  ProtocolTimeline protocol;
  std::vector<std::string> events;
  const auto emit = [&events](const std::string& event) {
    events.push_back(event);
  };

  PipelineDescriptor asr_descriptor;
  asr_descriptor.name = "asr";
  asr_descriptor.version = "1.0.0";
  asr_descriptor.produces = {orator::protocol::kAsrTranscript,
                             orator::protocol::kAsrTranscriptPartial};
  auto asr_handle = protocol.RegisterPipeline(std::move(asr_descriptor));

  PipelineDescriptor business_descriptor;
  business_descriptor.name = "business_speaker";
  business_descriptor.version = "1.0.0";
  business_descriptor.produces = {orator::protocol::kBusinessSpeakerRevision};
  auto business_handle =
      protocol.RegisterPipeline(std::move(business_descriptor));

  bool business_mirrored_after_commit = false;
  protocol.SubscribeInternal(
      TopicPattern{orator::protocol::kBusinessSpeakerRevision.to_string()},
      [&evidence, &business_mirrored_after_commit](const Message&) {
        const auto tracks = evidence.SnapshotTracks();
        business_mirrored_after_commit =
            tracks.asr.size() == 1 && tracks.business_speaker.size() == 1;
      });

  orator::pipeline::BusinessSpeakerPipeline business(
      &evidence, orator::pipeline::BusinessSpeakerPipeline::Config{},
      orator::core::TimeBase(16000),
      [&evidence, &protocol, &business_handle,
       &emit](const ComprehensiveTimeline::Revision& revision) {
        orator::pipeline::HandleBusinessSpeakerRevision(
            &protocol, business_handle.get(), emit, revision);
      });
  business.Start();

  bool typed_final_committed = false;
  ComprehensiveTimeline::RawTextSeg observed_final;
  evidence.SubscribeAsrFinals(
      [&typed_final_committed,
       &observed_final](const ComprehensiveTimeline::RawTextSeg& segment) {
        typed_final_committed = true;
        observed_final = segment;
      });

  bool final_mirrored_after_commit = false;
  int partial_mirrors = 0;
  protocol.SubscribeInternal(
      TopicPattern{"asr/+"},
      [&typed_final_committed, &final_mirrored_after_commit,
       &partial_mirrors](const Message& message) {
        if (message.topic == orator::protocol::kAsrTranscript.to_string()) {
          final_mirrored_after_commit = typed_final_committed;
        } else if (message.topic ==
                   orator::protocol::kAsrTranscriptPartial.to_string()) {
          ++partial_mirrors;
        }
      });

  orator::pipeline::HandleTextSink(evidence, &protocol, asr_handle.get(), 4,
                                   1.0, 2.0, "final", true);

  CHECK(typed_final_committed, "ASR final commits to typed evidence");
  CHECK(observed_final.id == 4 && observed_final.text == "final",
        "typed ASR subscriber receives the original record");
  CHECK(final_mirrored_after_commit,
        "protocol ASR final is mirrored after typed commit");
  CHECK(evidence.SnapshotTracks().asr.size() == 1,
        "typed ASR track contains one final");
  CHECK(business_mirrored_after_commit,
        "business revision mirrors only after its typed track commits");

  orator::pipeline::HandleTextSink(evidence, &protocol, asr_handle.get(), 5,
                                   2.0, 2.5, "partial", false);
  CHECK(partial_mirrors == 1, "ASR partial remains available on protocol");
  CHECK(evidence.SnapshotTracks().asr.size() == 1,
        "ASR partial does not enter the finalized typed track");

  PipelineDescriptor align_descriptor;
  align_descriptor.name = "align";
  align_descriptor.version = "1.0.0";
  align_descriptor.produces = {orator::protocol::kAlignUnits};
  auto align_handle = protocol.RegisterPipeline(std::move(align_descriptor));

  bool align_mirrored_after_commit = false;
  protocol.SubscribeInternal(
      TopicPattern{orator::protocol::kAlignUnits.to_string()},
      [&evidence, &align_mirrored_after_commit](const Message&) {
        const auto tracks = evidence.SnapshotTracks();
        align_mirrored_after_commit =
            tracks.align.size() == 1 && tracks.align[0].text_id == 4;
      });

  const std::vector<orator::core::AlignUnit> units = {{"f", 1.0, 1.4},
                                                      {"inal", 1.4, 2.0}};
  orator::pipeline::HandleAlignSink(evidence, &protocol, align_handle.get(),
                                    emit, 4, 1.0, 2.0, units);

  const auto final_tracks = evidence.SnapshotTracks();
  CHECK(final_tracks.align.size() == 1, "alignment commits to the typed track");
  CHECK(final_tracks.align[0].text_id == final_tracks.asr[0].id,
        "alignment retains the source ASR text ID");
  CHECK(align_mirrored_after_commit,
        "protocol alignment is mirrored after typed commit");

  if (failures == 0) {
    std::printf("test_typed_evidence_flow PASSED\n");
    return 0;
  }
  std::printf("test_typed_evidence_flow FAILED (%d checks)\n", failures);
  return 1;
}
