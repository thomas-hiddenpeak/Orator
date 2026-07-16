#include <cstdio>
#include <mutex>
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

class OneShotVad final : public orator::core::IVad {
 public:
  void Initialize(const orator::core::VadConfig&) override {}
  void LoadWeights(const std::string&) override {}
  void Reset() override { drained_ = false; }
  std::string name() const override { return "one_shot_vad"; }
  void Push(const float*, int) override {}
  void DrainSegments(
      bool, std::vector<orator::core::VadSegmentResult>* segments) override {
    if (drained_) return;
    segments->push_back({1600, 3200});
    drained_ = true;
  }
  bool is_in_speech() const override { return false; }
  double compute_sec() const override { return 0.0; }

 private:
  bool drained_ = false;
};

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
  std::string business_protocol_data;
  protocol.SubscribeInternal(
      TopicPattern{orator::protocol::kBusinessSpeakerRevision.to_string()},
      [&evidence, &business_mirrored_after_commit,
       &business_protocol_data](const Message& message) {
        const auto tracks = evidence.SnapshotTracks();
        business_mirrored_after_commit =
            tracks.asr.size() == 1 && tracks.business_speaker.size() == 1;
        business_protocol_data = message.data;
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
  std::string final_protocol_data;
  std::string partial_protocol_data;
  protocol.SubscribeInternal(
      TopicPattern{"asr/+"},
      [&typed_final_committed, &final_mirrored_after_commit, &partial_mirrors,
       &final_protocol_data, &partial_protocol_data](const Message& message) {
        if (message.topic == orator::protocol::kAsrTranscript.to_string()) {
          final_mirrored_after_commit = typed_final_committed;
          final_protocol_data = message.data;
        } else if (message.topic ==
                   orator::protocol::kAsrTranscriptPartial.to_string()) {
          ++partial_mirrors;
          partial_protocol_data = message.data;
        }
      });

  orator::pipeline::HandleTextSink(evidence, &protocol, asr_handle.get(), 4,
                                   1.0, 2.0, "final", true);

  CHECK(typed_final_committed, "ASR final commits to typed evidence");
  CHECK(observed_final.id == 4 && observed_final.text == "final",
        "typed ASR subscriber receives the original record");
  CHECK(final_mirrored_after_commit,
        "protocol ASR final is mirrored after typed commit");
  CHECK(final_protocol_data.find("\"type\":\"asr\"") != std::string::npos &&
            final_protocol_data.find("\"text_id\":4") != std::string::npos,
        "protocol ASR final is self-describing and keeps text_id");
  CHECK(evidence.SnapshotTracks().asr.size() == 1,
        "typed ASR track contains one final");
  CHECK(business_mirrored_after_commit,
        "business revision mirrors only after its typed track commits");
  CHECK(business_protocol_data.find("\"speaker_decision\":{") !=
                std::string::npos &&
            business_protocol_data.find("\"reason\":\"no_diar_support\"") !=
                std::string::npos,
        "business revision serializes structured speaker-decision evidence");

  orator::pipeline::HandleTextSink(evidence, &protocol, asr_handle.get(), 5,
                                   2.0, 2.5, "partial", false);
  CHECK(partial_mirrors == 1, "ASR partial remains available on protocol");
  CHECK(partial_protocol_data.find("\"type\":\"asr_partial\"") !=
                std::string::npos &&
            partial_protocol_data.find("\"text_id\":5") != std::string::npos,
        "protocol ASR partial is self-describing and keeps text_id");
  CHECK(evidence.SnapshotTracks().asr.size() == 1,
        "ASR partial does not enter the finalized typed track");

  PipelineDescriptor align_descriptor;
  align_descriptor.name = "align";
  align_descriptor.version = "1.0.0";
  align_descriptor.produces = {orator::protocol::kAlignUnits};
  auto align_handle = protocol.RegisterPipeline(std::move(align_descriptor));

  bool align_mirrored_after_commit = false;
  std::string align_protocol_data;
  protocol.SubscribeInternal(
      TopicPattern{orator::protocol::kAlignUnits.to_string()},
      [&evidence, &align_mirrored_after_commit,
       &align_protocol_data](const Message& message) {
        const auto tracks = evidence.SnapshotTracks();
        align_mirrored_after_commit =
            tracks.align.size() == 1 && tracks.align[0].text_id == 4;
        align_protocol_data = message.data;
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
  CHECK(align_protocol_data.find("\"type\":\"align\"") != std::string::npos &&
            align_protocol_data.find("\"text_id\":4") != std::string::npos,
        "protocol alignment is self-describing and keeps text_id");

  PipelineDescriptor diar_descriptor;
  diar_descriptor.name = "diar";
  diar_descriptor.version = "1.0.0";
  diar_descriptor.produces = {orator::protocol::kDiarSpeakerSegment};
  auto diar_handle = protocol.RegisterPipeline(std::move(diar_descriptor));
  std::mutex state_mutex;
  std::vector<orator::core::DiarSegment> last_segments;
  const std::vector<orator::core::DiarSegment> diar_segments = {
      {1.0, 2.0, 2, "spk_2", 0.9f},
      {0.1, 0.2, 3, "spk_3", 0.8f},
      {1.0, 1.5, 1, "spk_1", 0.700123429f},
      {1.0, 1.5, 0, "spk_0", 0.6f},
  };
  orator::pipeline::HandleSpeakerSink(evidence, state_mutex, last_segments,
                                      &protocol, diar_handle.get(), emit,
                                      diar_segments);
  const auto diarization = evidence.SnapshotTracks().diarization;
  CHECK(diarization.size() == 4, "diarization commits to the typed track");
  CHECK(last_segments.size() == 4 &&
            last_segments[0].local_speaker == 3 &&
            last_segments[1].local_speaker == 0 &&
            last_segments[2].local_speaker == 1 &&
            last_segments[3].local_speaker == 2,
        "speaker sink canonicalizes overlapping segment order");
  CHECK(diarization.size() == last_segments.size() &&
            diarization[0].speaker == "speaker_3" &&
            diarization[1].speaker == "speaker_0" &&
            diarization[2].speaker == "speaker_1" &&
            diarization[3].speaker == "speaker_2",
        "terminal diarization preserves the live canonical order");
  CHECK(events.back().find("\"type\":\"diar\"") != std::string::npos,
        "diarization emits a self-describing live event");
  const std::string& diar_event = events.back();
  CHECK(diar_event.find("\"confidence\":0.700123429") != std::string::npos,
        "live diarization retains round-trip float confidence precision");
  const auto first = diar_event.find("\"start\":0.100000000");
  const auto overlap_zero = diar_event.find(
      "\"start\":1.000000000,\"end\":1.500000000,"
      "\"speaker\":\"speaker_0\"");
  const auto overlap_one = diar_event.find(
      "\"start\":1.000000000,\"end\":1.500000000,"
      "\"speaker\":\"speaker_1\"");
  const auto longest = diar_event.find(
      "\"start\":1.000000000,\"end\":2.000000000,"
      "\"speaker\":\"speaker_2\"");
  CHECK(first < overlap_zero && overlap_zero < overlap_one &&
            overlap_one < longest,
        "live diarization uses terminal canonical ordering");

  PipelineDescriptor vad_descriptor;
  vad_descriptor.name = "vad";
  vad_descriptor.version = "1.0.0";
  vad_descriptor.produces = {orator::protocol::kVadSpeechSegment};
  auto vad_handle = protocol.RegisterPipeline(std::move(vad_descriptor));
  OneShotVad vad;
  std::vector<orator::core::VadSegmentResult> vad_segments;
  orator::pipeline::HandleVadDrain(&vad, evidence, &protocol, vad_handle.get(),
                                   emit, orator::core::TimeBase(16000),
                                   &vad_segments, false);
  CHECK(evidence.SnapshotTracks().vad.size() == 1,
        "VAD commits to the typed track");
  CHECK(events.back().find("\"type\":\"vad\"") != std::string::npos,
        "VAD emits a self-describing live event");

  if (failures == 0) {
    std::printf("test_typed_evidence_flow PASSED\n");
    return 0;
  }
  std::printf("test_typed_evidence_flow FAILED (%d checks)\n", failures);
  return 1;
}
