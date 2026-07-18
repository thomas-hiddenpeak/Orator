#include <cstdio>
#include <string>

#include "core/time_base.h"
#include "pipeline/comprehensive_timeline.h"
#include "pipeline/json_util.h"

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
  using Timeline = orator::pipeline::ComprehensiveTimeline;

  Timeline timeline;
  int evidence_notifications = 0;
  int final_notifications = 0;
  bool callback_saw_committed_final = false;
  const long evidence_subscription =
      timeline.SubscribeEvidence([&](const Timeline::EvidenceUpdate& update) {
        ++evidence_notifications;
        if (update.track == Timeline::EvidenceTrack::kAsrFinal) {
          const auto committed = timeline.FindAsrFinal(update.record_id);
          callback_saw_committed_final =
              committed && committed->text == "immutable final";
        }
      });
  const long final_subscription =
      timeline.SubscribeAsrFinals([&](const Timeline::RawTextSeg& segment) {
        ++final_notifications;
        CHECK(segment.id == 7 && segment.text == "immutable final",
              "ASR subscriber receives the committed typed record");
      });

  const auto inserted =
      timeline.DepositAsrFinal({7, 1.0, 2.0, "immutable final"});
  CHECK(inserted == Timeline::DepositResult::kInserted,
        "first ASR final is inserted");
  CHECK(callback_saw_committed_final,
        "evidence callback runs after the ASR record commits");
  CHECK(final_notifications == 1, "ASR final subscriber runs once");

  const auto unchanged =
      timeline.DepositAsrFinal({7, 1.0, 2.0, "immutable final"});
  CHECK(unchanged == Timeline::DepositResult::kUnchanged,
        "identical ASR repeat is idempotent");
  CHECK(final_notifications == 1,
        "idempotent ASR repeat does not notify downstream pipelines");

  const auto conflict =
      timeline.DepositAsrFinal({7, 1.0, 2.5, "mutated final"});
  CHECK(conflict == Timeline::DepositResult::kConflict,
        "conflicting ASR repeat is rejected");
  const auto raw_after_conflict = timeline.FindAsrFinal(7);
  CHECK(raw_after_conflict && raw_after_conflict->end == 2.0 &&
            raw_after_conflict->text == "immutable final",
        "rejected ASR conflict cannot mutate the raw track");

  const Timeline::AlignGroup alignment{
      7, 1.0, 2.0, {{1.0, 1.4, "immutable"}, {1.4, 2.0, " final"}}};
  CHECK(timeline.DepositAlignment(alignment) ==
            Timeline::DepositResult::kInserted,
        "first alignment group is inserted");
  CHECK(timeline.DepositAlignment(alignment) ==
            Timeline::DepositResult::kUnchanged,
        "identical alignment repeat is idempotent");
  Timeline::AlignGroup conflicting_alignment = alignment;
  conflicting_alignment.units[0].text = "changed";
  CHECK(timeline.DepositAlignment(conflicting_alignment) ==
            Timeline::DepositResult::kConflict,
        "conflicting alignment repeat is rejected");
  CHECK(timeline.FindAlignment(7)->units[0].text == "immutable",
        "rejected alignment conflict cannot mutate the raw track");

  timeline.DepositDiarization({{0.0, 3.0, "speaker_0", 0.9f, "voice_0"}});
  const auto raw_before_business = timeline.SnapshotTracks();
  Timeline::Entry business_entry;
  business_entry.start = 1.0;
  business_entry.end = 2.0;
  business_entry.speaker = "speaker_0";
  business_entry.speaker_id = "voice_0";
  business_entry.text = "immutable final";
  business_entry.text_id = 7;
  timeline.DepositBusinessSpeakerRevision({1.0, 2.0, {business_entry}});

  const auto with_business = timeline.SnapshotTracks();
  CHECK(with_business.business_speaker.size() == 1,
        "business-speaker pipeline owns a distinct stored track");
  CHECK(with_business.asr.size() == raw_before_business.asr.size() &&
            with_business.asr[0].text == raw_before_business.asr[0].text,
        "business revision does not mutate the ASR raw track");
  CHECK(with_business.diarization.size() ==
                raw_before_business.diarization.size() &&
            with_business.diarization[0].speaker ==
                raw_before_business.diarization[0].speaker,
        "business revision does not mutate the diarization raw track");
  CHECK(with_business.align.size() == raw_before_business.align.size() &&
            with_business.align[0].units[0].text ==
                raw_before_business.align[0].units[0].text,
        "business revision does not mutate the alignment raw track");

  business_entry.speaker = "speaker_1";
  timeline.DepositBusinessSpeakerRevision({1.0, 2.0, {business_entry}});
  CHECK(timeline.Snapshot()[0].speaker == "speaker_1",
        "business pipeline can revise only its own track");
  CHECK(timeline.FindAsrFinal(7)->text == "immutable final",
        "business re-attribution leaves finalized ASR immutable");

  Timeline::Entry one_sample_entry = business_entry;
  one_sample_entry.start = 1.0;
  one_sample_entry.end =
      one_sample_entry.start + orator::core::TimeBase(16000).SecondsAt(1);
  const std::string one_sample_json =
      orator::pipeline::SerializeRevisionToJson(
          {one_sample_entry.start, one_sample_entry.end, {one_sample_entry}},
          "business_speaker");
  CHECK(one_sample_json.find(
            "\"dirty_start\":1.000000000,\"dirty_end\":1.000062500") !=
            std::string::npos,
        "revision JSON preserves a positive one-sample dirty interval");
  CHECK(one_sample_json.find(
            "\"start\":1.000000000,\"end\":1.000062500") !=
            std::string::npos,
        "revision JSON preserves a positive one-sample business entry");

  timeline.DepositVad({2.0, 2.5});
  timeline.AdvanceVadHorizon(3.0);
  timeline.UpdateVadState(true, 2.75, 2.20, 2.50);
  const auto first_vad = timeline.SnapshotVadEvidence();
  timeline.UpdateVadState(false, 2.50);
  timeline.DepositVad({3.0, 3.5});
  timeline.AdvanceVadHorizon(2.0);
  const auto second_vad = timeline.SnapshotVadEvidence();
  CHECK(first_vad.segments && first_vad.segments->size() == 1,
        "earlier VAD snapshot remains immutable after a later deposit");
  CHECK(second_vad.segments && second_vad.segments->size() == 2,
        "latest VAD snapshot contains both segments");
  CHECK(second_vad.horizon == 3.0,
        "VAD decision horizon advances monotonically");
  CHECK(second_vad.in_speech && second_vad.state_observed_at == 2.75 &&
            second_vad.active_start == 2.20 &&
            second_vad.active_horizon == 2.50,
        "VAD active onset and stable frontier are typed evidence");
  timeline.UpdateVadState(false, 3.75);
  const auto inactive_vad = timeline.SnapshotVadEvidence();
  CHECK(!inactive_vad.in_speech && inactive_vad.active_start < -1e8 &&
            inactive_vad.active_horizon < -1e8,
        "inactive VAD state clears provisional active evidence");

  Timeline::DiarFrameBlock frame_block;
  frame_block.start = 0.0;
  frame_block.frame_period_sec = 0.08;
  frame_block.num_frames = 2;
  frame_block.num_speakers = 2;
  frame_block.probabilities = {0.8f, 0.2f, 0.7f, 0.3f};
  CHECK(timeline.DepositDiarFrameBlock(frame_block) ==
            Timeline::DepositResult::kInserted,
        "first raw diar frame block is inserted");
  Timeline::DiarFrameBlock next_block = frame_block;
  next_block.start = 0.16;
  CHECK(timeline.DepositDiarFrameBlock(next_block) ==
            Timeline::DepositResult::kInserted,
        "contiguous raw diar frame block is appended");
  Timeline::DiarFrameBlock overlapping_block = frame_block;
  overlapping_block.start = 0.08;
  CHECK(timeline.DepositDiarFrameBlock(overlapping_block) ==
            Timeline::DepositResult::kConflict,
        "overlapping raw diar frame block is rejected");
  CHECK(timeline.SnapshotDiarFrames().size() == 2,
        "rejected frame block cannot mutate the raw frame track");

  Timeline::SpeakerVoiceprintEvidence voiceprint;
  voiceprint.evidence_id = "phrase:7:0";
  voiceprint.kind = "punctuation_phrase";
  voiceprint.text_id = 7;
  voiceprint.source_start = 0;
  voiceprint.source_end = 9;
  voiceprint.start = 1.0;
  voiceprint.end = 2.0;
  voiceprint.embedding_available = true;
  voiceprint.robust_gallery_complete = true;
  voiceprint.session_scores = {{"voice_0", 0.8f}, {"voice_1", 0.5f}};
  voiceprint.robust_scores = {{"voice_0", 0.75f}, {"voice_1", 0.45f}};
  timeline.DepositSpeakerVoiceprint({voiceprint});
  const auto voiceprint_snapshot = timeline.SnapshotSpeakerVoiceprint();
  CHECK(voiceprint_snapshot.size() == 1 &&
            voiceprint_snapshot[0].evidence_id == "phrase:7:0",
        "typed voiceprint evidence is committed without a decision");
  CHECK(timeline.FindAsrFinal(7)->text == "immutable final",
        "voiceprint evidence cannot mutate finalized ASR");

  timeline.UnsubscribeAsrFinals(final_subscription);
  timeline.Clear();
  const auto cleared = timeline.SnapshotTracks();
  CHECK(cleared.diarization.empty() && cleared.asr.empty() &&
            cleared.vad.empty() && cleared.align.empty() &&
            cleared.diar_frames.empty() &&
            cleared.speaker_voiceprint.empty() &&
            cleared.business_speaker.empty(),
        "session reset clears every stored track");
  CHECK(evidence_notifications >= 5,
        "typed evidence subscriber observes committed track updates and reset");
  timeline.UnsubscribeEvidence(evidence_subscription);

  if (failures == 0) {
    std::printf("ComprehensiveTimeline test PASSED\n");
    return 0;
  }
  std::printf("ComprehensiveTimeline test FAILED (%d checks)\n", failures);
  return 1;
}
