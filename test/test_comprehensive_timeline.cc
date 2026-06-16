#include <cassert>
#include <cstdio>
#include <string>

#include "pipeline/comprehensive_timeline.h"

using orator::pipeline::ComprehensiveTimeline;

static int g_fail = 0;
#define CHECK(cond, msg)                                  \
  do {                                                    \
    if (!(cond)) {                                        \
      std::printf("FAIL: %s\n", msg);                     \
      ++g_fail;                                           \
    }                                                     \
  } while (0)

int main() {
  std::printf("Testing ComprehensiveTimeline (Spec 004 Phase 5: diar-driven view)...\n");

  // ---- 1. Out-of-order upserts: text arrives before its speaker ----
  {
    ComprehensiveTimeline tl;
    // Text 0 at [0,5) arrives first; no speaker yet -> attributed "unknown".
    auto r0 = tl.UpsertText(0, 0.0, 5.0, "hello");
    CHECK(r0.size() == 1, "first text yields one revision (new entry)");
    CHECK(r0[0].entries.size() == 1 && r0[0].entries[0].speaker == "unknown",
          "text with no speaker attributed unknown");

    // Speaker covering [0,5) arrives -> attribution flips to speaker_0, revision.
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
    ComprehensiveTimeline tl;
    // Speaker_0 [0,3), Speaker_1 [3,10).
    tl.UpsertSpeaker(0.0, 3.0, "speaker_0", 0.9f);
    tl.UpsertSpeaker(3.0, 10.0, "speaker_1", 0.9f);
    // Text [2,9) crosses the boundary at 3.0 -> two pieces: [2,3) spk0, [3,9) spk1.
    auto r = tl.UpsertText(0, 2.0, 9.0, "spanning text");
    CHECK(r.size() == 1, "text yields one revision");
    CHECK(r[0].entries.size() == 2, "text crossing a diar boundary splits in two");
    if (r[0].entries.size() == 2) {
      CHECK(r[0].entries[0].speaker == "speaker_0", "first piece -> speaker_0");
      CHECK(r[0].entries[1].speaker == "speaker_1", "second piece -> speaker_1");
      CHECK(r[0].entries[0].end == 3.0 && r[0].entries[1].start == 3.0,
            "pieces split exactly at the diarization boundary");
      CHECK(r[0].entries[0].text + r[0].entries[1].text == "spanning text",
            "the split pieces concatenate back to the original text");
    }
  }

  // ---- 3. A later speaker update flips a prior attribution (revisable) ----
  {
    ComprehensiveTimeline tl;
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

  // ---- 4. Incrementality: a speaker update far from a text does not revise ----
  {
    ComprehensiveTimeline tl;
    tl.UpsertSpeaker(0.0, 5.0, "speaker_0", 0.9f);
    tl.UpsertText(0, 0.0, 5.0, "a");
    // Speaker far away [100,105) -> no overlap with the text -> no revision.
    auto r = tl.UpsertSpeaker(100.0, 105.0, "speaker_1", 0.9f);
    CHECK(r.empty(), "non-overlapping speaker update touches nothing");
  }

  // ---- 4b. Pure alignment: text with no overlapping speaker is "unknown",
  // NOT borrowed from the nearest segment (Spec 004 invariant). ----
  {
    ComprehensiveTimeline tl;
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

  // ---- 5. Snapshot coalesces consecutive same-speaker entries ----
  {
    ComprehensiveTimeline tl;
    tl.UpsertSpeaker(0.0, 20.0, "speaker_0", 0.9f);
    tl.UpsertText(0, 0.0, 5.0, "one");
    tl.UpsertText(1, 5.0, 10.0, "two");
    tl.UpsertText(2, 10.0, 15.0, "three");
    auto snap = tl.Snapshot();
    CHECK(snap.size() == 1, "three same-speaker texts coalesce to one turn");
    CHECK(snap[0].text == "onetwothree", "coalesced text joined in order");
    CHECK(snap[0].start == 0.0 && snap[0].end == 15.0, "coalesced span");
  }

  // ---- 6. ReplaceSpeakers: diarization's whole-view delivery re-projects text ----
  {
    ComprehensiveTimeline tl;
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
        ComprehensiveTimeline a;
        ComprehensiveTimeline b;

        // Order A: diar baseline -> text -> text revision -> diar refinement.
        a.ReplaceSpeakers({{0.0, 4.0, "speaker_0", 0.8f},
               {4.0, 8.0, "speaker_1", 0.8f}});
        a.UpsertText(7, 2.0, 6.0, "abcd");
        a.UpsertText(7, 2.0, 7.0, "abcdef");
        a.ReplaceSpeakers({{0.0, 3.0, "speaker_0", 0.9f},
               {3.0, 8.0, "speaker_1", 0.9f}});

        // Order B: text and revision arrive first, diar view arrives later.
        b.UpsertText(7, 2.0, 6.0, "abcd");
        b.UpsertText(7, 2.0, 7.0, "abcdef");
        b.ReplaceSpeakers({{0.0, 4.0, "speaker_0", 0.8f},
               {4.0, 8.0, "speaker_1", 0.8f}});
        b.ReplaceSpeakers({{0.0, 3.0, "speaker_0", 0.9f},
               {3.0, 8.0, "speaker_1", 0.9f}});

        const auto sa = a.Snapshot();
        const auto sb = b.Snapshot();
        CHECK(sa.size() == 2, "final view has two diar-driven turns");
        CHECK(sb.size() == 2, "final view has two diar-driven turns (order B)");
        if (sa.size() == 2) {
      CHECK(sa[0].speaker == "speaker_0" && sa[0].start == 2.0 && sa[0].end == 3.0,
        "order A first turn is speaker_0 [2,3)");
      CHECK(sa[1].speaker == "speaker_1" && sa[1].start == 3.0 && sa[1].end == 7.0,
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

  if (g_fail == 0) {
    std::printf("ComprehensiveTimeline test PASSED\n");
    return 0;
  }
  std::printf("ComprehensiveTimeline test FAILED (%d checks)\n", g_fail);
  return 1;
}
