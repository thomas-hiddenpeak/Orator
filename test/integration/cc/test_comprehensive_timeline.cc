#include <cstdio>
#include <string>

#include "pipeline/comprehensive_timeline.h"

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

  timeline.DepositVad({2.0, 2.5});
  timeline.AdvanceVadHorizon(3.0);
  timeline.UpdateVadState(true, 2.75);
  const auto first_vad = timeline.SnapshotVadEvidence();
  timeline.DepositVad({3.0, 3.5});
  timeline.AdvanceVadHorizon(2.0);
  const auto second_vad = timeline.SnapshotVadEvidence();
  CHECK(first_vad.segments && first_vad.segments->size() == 1,
        "earlier VAD snapshot remains immutable after a later deposit");
  CHECK(second_vad.segments && second_vad.segments->size() == 2,
        "latest VAD snapshot contains both segments");
  CHECK(second_vad.horizon == 3.0,
        "VAD decision horizon advances monotonically");
  CHECK(second_vad.in_speech && second_vad.state_observed_at == 2.75,
        "VAD activity state is available as typed evidence");

  timeline.UnsubscribeAsrFinals(final_subscription);
  timeline.Clear();
  const auto cleared = timeline.SnapshotTracks();
  CHECK(cleared.diarization.empty() && cleared.asr.empty() &&
            cleared.vad.empty() && cleared.align.empty() &&
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
