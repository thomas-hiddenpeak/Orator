#include <cassert>
#include <cstdio>
#include <string>

#include "test_comprehensive_timeline_access.h"

using orator::pipeline::ComprehensiveTimeline;
using orator::pipeline::TestComprehensiveTimeline;

static int g_fail = 0;
#define CHECK(cond, msg)              \
  do {                                \
    if (!(cond)) {                    \
      std::printf("FAIL: %s\n", msg); \
      ++g_fail;                       \
    }                                 \
  } while (0)

int main() {
  std::printf(
      "Testing ComprehensiveTimeline (Spec 004 Phase 5: diar-driven "
      "view)...\n");

  // ---- 1. Out-of-order upserts: text arrives before its speaker ----
  {
    TestComprehensiveTimeline tl;
    // Text 0 at [0,5) arrives first; no speaker yet -> attributed "unknown".
    auto r0 = tl.UpsertText(0, 0.0, 5.0, "hello");
    CHECK(r0.size() == 1, "first text yields one revision (new entry)");
    CHECK(r0[0].entries.size() == 1 && r0[0].entries[0].speaker == "unknown",
          "text with no speaker attributed unknown");

    // Speaker covering [0,5) arrives -> attribution flips to speaker_0,
    // revision.
    auto r1 = tl.UpsertSpeaker(0.0, 5.0, "speaker_0", 0.9f);
    CHECK(r1.size() == 1, "speaker upsert flips attribution -> one revision");
    CHECK(r1[0].entries[0].speaker == "speaker_0", "flip to speaker_0");

    // Re-deposit the SAME speaker over the same range -> no attribution change.
    auto r2 = tl.UpsertSpeaker(0.0, 5.0, "speaker_0", 0.9f);
    CHECK(r2.empty(), "unchanged attribution yields no revision");
  }

  // ---- 2. A text spanning two speakers is SPLIT at the diarization boundary
  // (diar-driven view), not attributed wholly to the max-overlap speaker. ----
  {
    TestComprehensiveTimeline tl;
    // Speaker_0 [0,3), Speaker_1 [3,10).
    tl.UpsertSpeaker(0.0, 3.0, "speaker_0", 0.9f);
    tl.UpsertSpeaker(3.0, 10.0, "speaker_1", 0.9f);
    // Text [2,9) crosses the boundary at 3.0 -> two pieces: [2,3) spk0, [3,9)
    // spk1.
    auto r = tl.UpsertText(0, 2.0, 9.0, "spanning text");
    CHECK(r.size() == 1, "text yields one revision");
    CHECK(r[0].entries.size() == 2,
          "text crossing a diar boundary splits in two");
    if (r[0].entries.size() == 2) {
      CHECK(r[0].entries[0].speaker == "speaker_0", "first piece -> speaker_0");
      CHECK(r[0].entries[1].speaker == "speaker_1",
            "second piece -> speaker_1");
      CHECK(r[0].entries[0].end == 3.0 && r[0].entries[1].start == 3.0,
            "pieces split exactly at the diarization boundary");
      CHECK(r[0].entries[0].text + r[0].entries[1].text == "spanning text",
            "the split pieces concatenate back to the original text");
    }
  }

  // ---- 3. A later speaker update flips a prior attribution (revisable) ----
  {
    TestComprehensiveTimeline tl;
    tl.UpsertSpeaker(0.0, 10.0, "speaker_0", 0.6f);
    auto r0 = tl.UpsertText(0, 4.0, 6.0, "mid");
    CHECK(r0[0].entries[0].speaker == "speaker_0", "initial attribution spk0");
    // A more-overlapping speaker_1 segment arrives exactly over [4,6).
    auto r1 = tl.UpsertSpeaker(4.0, 6.0, "speaker_1", 0.95f);
    CHECK(r1.size() == 1, "overlapping speaker update yields a revision");
    if (r1.size() == 1 && !r1[0].entries.empty()) {
      CHECK(r1[0].entries[0].speaker == "speaker_1",
            "attribution revised to speaker_1 (tighter segment wins on tie)");
      CHECK(r1[0].dirty_start == 4.0 && r1[0].dirty_end == 6.0,
            "revision dirty range matches the text span");
    }
  }

  // ---- 4. Incrementality: a speaker update far from a text does not revise
  // ----
  {
    TestComprehensiveTimeline tl;
    tl.UpsertSpeaker(0.0, 5.0, "speaker_0", 0.9f);
    tl.UpsertText(0, 0.0, 5.0, "a");
    // Speaker far away [100,105) -> no overlap with the text -> no revision.
    auto r = tl.UpsertSpeaker(100.0, 105.0, "speaker_1", 0.9f);
    CHECK(r.empty(), "non-overlapping speaker update touches nothing");
  }

  // ---- 4b. Pure alignment: text with no overlapping speaker is "unknown",
  // NOT borrowed from the nearest segment (Spec 004 invariant). ----
  {
    TestComprehensiveTimeline tl;
    // A speaker exists only at [20,25); text at [0,5) has NO overlap.
    tl.UpsertSpeaker(20.0, 25.0, "speaker_0", 0.9f);
    auto r = tl.UpsertText(0, 0.0, 5.0, "head");
    CHECK(r.size() == 1, "text yields one revision");
    if (r.size() == 1 && !r[0].entries.empty())
      CHECK(r[0].entries[0].speaker == "unknown",
            "no time overlap -> unknown (not nearest-borrowed)");
    // When diarization later covers [0,5), the attribution is revised.
    auto r2 = tl.UpsertSpeaker(0.0, 5.0, "speaker_1", 0.9f);
    CHECK(r2.size() == 1, "covering speaker revises the unknown entry");
    if (r2.size() == 1 && !r2[0].entries.empty())
      CHECK(r2[0].entries[0].speaker == "speaker_1",
            "unknown revised to the covering speaker");
  }

  // ---- 5. Snapshot preserves ASR text_id/final boundaries even when
  // consecutive entries have the same speaker. Speaker-turn aggregation is a
  // presentation concern, not the terminal accuracy view. ----
  {
    TestComprehensiveTimeline tl;
    tl.UpsertSpeaker(0.0, 20.0, "speaker_0", 0.9f);
    tl.UpsertText(0, 0.0, 5.0, "one");
    tl.UpsertText(1, 5.0, 10.0, "two");
    tl.UpsertText(2, 10.0, 15.0, "three");
    auto snap = tl.Snapshot();
    CHECK(snap.size() == 3,
          "three same-speaker texts remain three source text entries");
    if (snap.size() == 3) {
      CHECK(snap[0].text == "one" && snap[0].text_id == 0,
            "first ASR text boundary preserved");
      CHECK(snap[1].text == "two" && snap[1].text_id == 1,
            "second ASR text boundary preserved");
      CHECK(snap[2].text == "three" && snap[2].text_id == 2,
            "third ASR text boundary preserved");
      CHECK(snap[0].start == 0.0 && snap[0].end == 5.0,
            "first source span preserved");
      CHECK(snap[1].start == 5.0 && snap[1].end == 10.0,
            "second source span preserved");
      CHECK(snap[2].start == 10.0 && snap[2].end == 15.0,
            "third source span preserved");
    }
  }

  // ---- 6. ReplaceSpeakers: diarization's whole-view delivery re-projects text
  // ----
  {
    TestComprehensiveTimeline tl;
    tl.UpsertText(0, 0.0, 5.0, "x");
    tl.UpsertText(1, 5.0, 10.0, "y");
    // First diar view: one speaker covering everything -> both texts -> spk0.
    auto r1 = tl.ReplaceSpeakers({{0.0, 10.0, "speaker_0", 0.8f}});
    CHECK(r1.size() == 2, "ReplaceSpeakers re-projects both texts (new->spk0)");
    // Refined diar view: spk1 owns [5,10) -> text 1 flips, text 0 unchanged.
    auto r2 = tl.ReplaceSpeakers(
        {{0.0, 5.0, "speaker_0", 0.9f}, {5.0, 10.0, "speaker_1", 0.9f}});
    CHECK(r2.size() == 1, "refined diar view revises only the changed text");
    if (r2.size() == 1 && !r2[0].entries.empty())
      CHECK(r2[0].entries[0].speaker == "speaker_1",
            "text 1 re-attributed to speaker_1 after diar refinement");
  }

  // ---- 7. Interleaved convergence: ASR self-revisions and diar updates in
  // different arrival orders must converge to the SAME final view. ----
  {
    TestComprehensiveTimeline a;
    TestComprehensiveTimeline b;

    // Order A: diar baseline -> text -> text revision -> diar refinement.
    a.ReplaceSpeakers(
        {{0.0, 4.0, "speaker_0", 0.8f}, {4.0, 8.0, "speaker_1", 0.8f}});
    a.UpsertText(7, 2.0, 6.0, "abcd");
    a.UpsertText(7, 2.0, 7.0, "abcdef");
    a.ReplaceSpeakers(
        {{0.0, 3.0, "speaker_0", 0.9f}, {3.0, 8.0, "speaker_1", 0.9f}});

    // Order B: text and revision arrive first, diar view arrives later.
    b.UpsertText(7, 2.0, 6.0, "abcd");
    b.UpsertText(7, 2.0, 7.0, "abcdef");
    b.ReplaceSpeakers(
        {{0.0, 4.0, "speaker_0", 0.8f}, {4.0, 8.0, "speaker_1", 0.8f}});
    b.ReplaceSpeakers(
        {{0.0, 3.0, "speaker_0", 0.9f}, {3.0, 8.0, "speaker_1", 0.9f}});

    const auto sa = a.Snapshot();
    const auto sb = b.Snapshot();
    CHECK(sa.size() == 2, "final view has two diar-driven turns");
    CHECK(sb.size() == 2, "final view has two diar-driven turns (order B)");
    if (sa.size() == 2) {
      CHECK(sa[0].speaker == "speaker_0" && sa[0].start == 2.0 &&
                sa[0].end == 3.0,
            "order A first turn is speaker_0 [2,3)");
      CHECK(sa[1].speaker == "speaker_1" && sa[1].start == 3.0 &&
                sa[1].end == 7.0,
            "order A second turn is speaker_1 [3,7)");
      CHECK(sa[0].text + sa[1].text == "abcdef",
            "order A split preserves full revised text");
    }
    if (sa.size() == sb.size() && sa.size() == 2) {
      CHECK(sa[0].speaker == sb[0].speaker && sa[1].speaker == sb[1].speaker,
            "arrival order does not change final speaker attribution");
      CHECK(sa[0].text == sb[0].text && sa[1].text == sb[1].text,
            "arrival order does not change final text split");
      CHECK(sa[0].start == sb[0].start && sa[0].end == sb[0].end &&
                sa[1].start == sb[1].start && sa[1].end == sb[1].end,
            "arrival order does not change final time spans");
    }
  }

  // ---- 8. Alignment run-coherence: a continuous (gapless) run of aligned
  // units that straddles a diarization boundary is attributed WHOLE to one
  // speaker (the run-midpoint's), not split mid-utterance. The speaker boundary
  // is effectively snapped to the surrounding pause. ----
  {
    TestComprehensiveTimeline tl;
    tl.UpsertText(0, 0.0, 1.2, "ABCD");
    // Diar boundary at 1.0 falls INSIDE the run (unit D spans 0.9-1.2).
    tl.ReplaceSpeakers(
        {{0.0, 1.0, "speaker_0", 0.9f}, {1.0, 4.0, "speaker_1", 0.9f}});
    // Four gapless units -> one run [0,1.2], midpoint 0.6 in speaker_0's turn.
    tl.UpsertAlign(
        0, 0.0, 1.2,
        {{0.0, 0.3, "A"}, {0.3, 0.6, "B"}, {0.6, 0.9, "C"}, {0.9, 1.2, "D"}});
    auto snap = tl.Snapshot();
    CHECK(snap.size() == 1, "gapless run not split across the diar boundary");
    if (snap.size() == 1) {
      CHECK(snap[0].speaker == "speaker_0",
            "whole run attributed to the run-midpoint speaker");
      CHECK(snap[0].text == "ABCD", "run text kept intact (no mid-run cut)");
    }
  }

  // ---- 9. A diarizer-local speaker label can resolve to different global ids
  // over time; the comprehensive entries must keep the per-interval id instead
  // of serializing through the label's last mapping. ----
  {
    TestComprehensiveTimeline tl;
    tl.UpsertText(0, 0.0, 4.0, "ABCD");
    tl.ReplaceSpeakers({{0.0, 2.0, "speaker_0", 0.8f, "spk_a"},
                        {2.0, 4.0, "speaker_0", 0.8f, "spk_b"}});
    auto snap = tl.Snapshot();
    CHECK(snap.size() == 2,
          "same local speaker with different global ids stays split");
    if (snap.size() == 2) {
      CHECK(snap[0].speaker == "speaker_0" && snap[0].speaker_id == "spk_a",
            "first interval keeps spk_a");
      CHECK(snap[1].speaker == "speaker_0" && snap[1].speaker_id == "spk_b",
            "second interval keeps spk_b");
      CHECK(snap[0].text + snap[1].text == "ABCD",
            "global-id split preserves text");
    }
  }

  // ---- 10. Alignment snap keeps ordinary short pauses coherent, but a diar
  // boundary near an alignment gap forces a split. A 0.16s gap is below the
  // 0.25s run-coherence threshold, yet it still splits because the speaker
  // boundary sits inside the gap. ----
  {
    TestComprehensiveTimeline tl;
    tl.set_align_snap_pause_sec(0.25);
    tl.set_align_boundary_split_tolerance_sec(0.08);
    tl.UpsertText(0, 0.0, 4.0, "ABCDEF");
    tl.ReplaceSpeakers({{0.0, 2.5, "speaker_3", 0.9f, "spk_3"},
                        {2.5, 4.0, "speaker_0", 0.9f, "spk_0"}});
    tl.UpsertAlign(0, 0.0, 4.0,
                   {{0.0, 0.3, "A"},
                    {0.3, 0.6, "B"},
                    {2.0, 2.4, "C"},
                    {2.56, 2.66, "D"},
                    {2.66, 2.9, "E"},
                    {2.9, 3.2, "F"}});
    auto snap = tl.Snapshot();
    CHECK(snap.size() == 2, "diar boundary near 0.16s gap splits speaker runs");
    if (snap.size() == 2) {
      CHECK(snap[0].speaker_id == "spk_3" && snap[0].text == "ABC",
            "pre-gap text remains with first speaker");
      CHECK(snap[1].speaker_id == "spk_0" && snap[1].text == "DEF",
            "post-gap text moves to second speaker");
    }
  }

  // ---- 11. Speaker-support diagnostics flag sparse diar evidence without
  // changing the selected speaker. This exposes the tail failure mode where a
  // long comprehensive entry is created from two short same-speaker islands.
  {
    TestComprehensiveTimeline tl;
    tl.UpsertText(0, 0.0, 5.0, "ABCDE");
    tl.ReplaceSpeakers({{0.0, 1.0, "speaker_0", 0.9f, "spk_0"},
                        {4.0, 5.0, "speaker_0", 0.9f, "spk_0"}});
    auto snap = tl.Snapshot();
    CHECK(snap.size() == 1,
          "same-speaker gap-fill still produces one comprehensive entry");
    if (snap.size() == 1) {
      CHECK(snap[0].speaker_id == "spk_0",
            "gap-filled entry keeps the selected speaker");
      CHECK(snap[0].speaker_support == "weak",
            "sparse selected-speaker support is labelled weak");
      CHECK(snap[0].speaker_uncertain,
            "weak selected-speaker support is marked uncertain");
      CHECK(snap[0].diar_island_count == 2,
            "two selected-speaker islands are counted");
      CHECK(snap[0].diar_overlap_sec == 2.0,
            "selected-speaker overlap is measured in seconds");
      CHECK(snap[0].diar_max_gap_sec == 3.0,
            "largest selected-speaker evidence gap is exposed");
    }
  }

  // ---- 12. Fully covered speaker intervals are labelled strong. ----
  {
    TestComprehensiveTimeline tl;
    tl.set_gap_fill_enabled(false);
    tl.UpsertText(0, 0.0, 5.0, "ABCDE");
    tl.ReplaceSpeakers({{0.0, 1.0, "speaker_0", 0.9f, "spk_0"},
                        {4.0, 5.0, "speaker_0", 0.9f, "spk_0"}});
    const auto snap = tl.Snapshot();
    bool has_unknown = false;
    for (const auto& entry : snap) {
      if (entry.speaker == "unknown") has_unknown = true;
    }
    CHECK(has_unknown, "TOML-disabled gap fill preserves unsupported interval");
  }

  // ---- 13. Fully covered speaker intervals are labelled strong. ----
  {
    TestComprehensiveTimeline tl;
    tl.UpsertText(0, 0.0, 3.0, "ABC");
    tl.ReplaceSpeakers({{0.0, 3.0, "speaker_1", 0.9f, "spk_1"}});
    auto snap = tl.Snapshot();
    CHECK(snap.size() == 1, "fully covered entry exists");
    if (snap.size() == 1) {
      CHECK(snap[0].speaker_support == "strong",
            "complete selected-speaker coverage is labelled strong");
      CHECK(!snap[0].speaker_uncertain,
            "strong selected-speaker support is not marked uncertain");
      CHECK(snap[0].diar_coverage_ratio == 1.0,
            "complete selected-speaker coverage ratio is 1.0");
      CHECK(snap[0].diar_island_count == 1,
            "complete coverage has one selected-speaker island");
    }
  }

  // ---- 14. Typed evidence reads are immutable and subscriptions carry the
  // committed ASR final rather than a protocol-serialized reconstruction. ----
  {
    ComprehensiveTimeline tl;
    int notifications = 0;
    ComprehensiveTimeline::RawTextSeg observed;
    const long subscription = tl.SubscribeAsrFinals(
        [&notifications,
         &observed](const ComprehensiveTimeline::RawTextSeg& segment) {
          ++notifications;
          observed = segment;
        });

    tl.DepositAsrFinal({7, 1.0, 2.0, "typed final"});
    CHECK(notifications == 1, "typed ASR final subscriber called once");
    CHECK(observed.id == 7 && observed.start == 1.0 && observed.end == 2.0 &&
              observed.text == "typed final",
          "subscriber receives the committed typed ASR record");

    tl.UnsubscribeAsrFinals(subscription);
    tl.DepositAsrFinal({8, 2.0, 3.0, "after unsubscribe"});
    CHECK(notifications == 1, "unsubscribed ASR reader receives no updates");

    tl.DepositVad({1.0, 1.5});
    tl.AdvanceVadHorizon(2.0);
    const auto first = tl.SnapshotVadEvidence();
    tl.DepositVad({2.0, 2.5});
    tl.AdvanceVadHorizon(1.0);
    const auto second = tl.SnapshotVadEvidence();
    CHECK(first.segments && first.segments->size() == 1,
          "earlier VAD snapshot remains immutable after a later deposit");
    CHECK(second.segments && second.segments->size() == 2,
          "latest VAD snapshot contains both typed segments");
    CHECK(second.horizon == 2.0, "VAD decision horizon advances monotonically");
  }

  if (g_fail == 0) {
    std::printf("ComprehensiveTimeline test PASSED\n");
    return 0;
  }
  std::printf("ComprehensiveTimeline test FAILED (%d checks)\n", g_fail);
  return 1;
}
