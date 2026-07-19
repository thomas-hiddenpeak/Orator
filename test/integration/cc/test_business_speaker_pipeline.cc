#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>

#include "test_business_speaker_pipeline_access.h"

using orator::pipeline::TestBusinessSpeakerPipeline;
using orator::pipeline::BusinessSpeakerPipeline;
using orator::pipeline::ComprehensiveTimeline;

static int g_fail = 0;
#define CHECK(cond, msg)              \
  do {                                \
    if (!(cond)) {                    \
      std::printf("FAIL: %s\n", msg); \
      ++g_fail;                       \
    }                                 \
  } while (0)

int main() {
  std::printf("Testing BusinessSpeakerPipeline typed fusion view...\n");

  // ---- 1. Out-of-order upserts: text arrives before its speaker ----
  {
    TestBusinessSpeakerPipeline tl;
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

  // ---- 3b. FR16ZZ: confidence-first equal-overlap arbitration prevents a
  // weak micro segment from overriding a stronger primary segment. ----
  {
    orator::pipeline::BusinessSpeakerPipeline::Config config;
    config.speaker_overlap_tie_policy =
        orator::pipeline::BusinessSpeakerPipeline::SpeakerOverlapTiePolicy::
            kHigherConfidence;
    TestBusinessSpeakerPipeline tl(config);
    tl.UpsertSpeaker(0.0, 10.0, "speaker_primary", 0.9f);
    tl.UpsertSpeaker(4.0, 4.1, "speaker_micro", 0.5f);
    auto revision = tl.UpsertText(0, 4.0, 4.1, "x");
    CHECK(revision.size() == 1 && revision[0].entries.size() == 1,
          "confidence policy produces one atomic overlap entry");
    if (revision.size() == 1 && revision[0].entries.size() == 1) {
      CHECK(revision[0].entries[0].speaker == "speaker_primary",
            "higher-confidence primary wins an equal-overlap tie");
    }
  }

  // ---- 3c. FR16AAB: the primary track may arbitrate only an exact activity
  // overlap tie. It contributes identity, but never contributes a boundary or
  // replaces an activity interval with a unique winner. ----
  {
    using Pipeline = orator::pipeline::BusinessSpeakerPipeline;
    Pipeline::Config config;
    config.speaker_overlap_tie_policy =
        Pipeline::SpeakerOverlapTiePolicy::kPrimarySpeaker;
    TestBusinessSpeakerPipeline tl(config);
    tl.ReplaceSpeakers({{0.0, 10.0, "speaker_0", 0.9f, "spk_a"},
                        {4.0, 4.1, "speaker_1", 0.5f, "spk_b"}});
    tl.ReplacePrimarySpeakers(
        {{0.0, 10.0, "speaker_0", 0.8f, "spk_a"}});
    const auto revision = tl.UpsertText(0, 4.0, 4.1, "x");
    CHECK(revision.size() == 1 && revision[0].entries.size() == 1,
          "primary arbitration produces one atomic overlap entry");
    if (revision.size() == 1 && revision[0].entries.size() == 1) {
      const auto& entry = revision[0].entries[0];
      CHECK(entry.speaker_id == "spk_a",
            "primary identity wins an exact activity overlap tie");
      CHECK(entry.speaker_decision.reason == "primary_speaker_tie_break",
            "primary arbitration remains explicit in decision audit");
      CHECK(entry.speaker_decision.speaker_source ==
                "sortformer_activity+primary_top1",
            "decision audit names both contributing evidence tracks");
    }
  }

  // ---- 3d. Primary disagreement cannot replace a unique activity winner. ----
  {
    using Pipeline = orator::pipeline::BusinessSpeakerPipeline;
    Pipeline::Config config;
    config.speaker_overlap_tie_policy =
        Pipeline::SpeakerOverlapTiePolicy::kPrimarySpeaker;
    TestBusinessSpeakerPipeline tl(config);
    tl.ReplaceSpeakers({{0.0, 0.8, "speaker_0", 0.9f, "spk_a"},
                        {0.8, 1.0, "speaker_1", 0.9f, "spk_b"}});
    tl.ReplacePrimarySpeakers(
        {{0.0, 1.0, "speaker_1", 0.9f, "spk_b"}});
    const auto revision = tl.UpsertText(0, 0.0, 1.0, "x");
    CHECK(revision.size() == 1 && revision[0].entries.size() == 2,
          "activity boundaries remain authoritative with primary evidence");
    if (revision.size() == 1 && revision[0].entries.size() == 2) {
      CHECK(revision[0].entries[0].speaker_id == "spk_a",
            "primary disagreement does not replace a unique activity winner");
      CHECK(revision[0].entries[1].speaker_id == "spk_b",
            "second unique activity interval remains unchanged");
    }
  }

  // ---- 3e. Ambiguous primary evidence abstains and preserves the established
  // shorter-span fallback. ----
  {
    using Pipeline = orator::pipeline::BusinessSpeakerPipeline;
    Pipeline::Config config;
    config.speaker_overlap_tie_policy =
        Pipeline::SpeakerOverlapTiePolicy::kPrimarySpeaker;
    TestBusinessSpeakerPipeline tl(config);
    tl.ReplaceSpeakers({{0.0, 10.0, "speaker_0", 0.9f, "spk_a"},
                        {4.0, 4.1, "speaker_1", 0.5f, "spk_b"}});
    tl.ReplacePrimarySpeakers(
        {{4.0, 4.1, "speaker_0", 0.9f, "spk_a"},
         {4.0, 4.1, "speaker_1", 0.9f, "spk_b"}});
    const auto revision = tl.UpsertText(0, 4.0, 4.1, "x");
    CHECK(revision.size() == 1 && revision[0].entries.size() == 1 &&
              revision[0].entries[0].speaker_id == "spk_b",
          "ambiguous primary evidence falls back to the shorter activity span");
  }

  // ---- 3f. Primary boundaries are not business-view split boundaries. ----
  {
    using Pipeline = orator::pipeline::BusinessSpeakerPipeline;
    Pipeline::Config config;
    config.speaker_overlap_tie_policy =
        Pipeline::SpeakerOverlapTiePolicy::kPrimarySpeaker;
    TestBusinessSpeakerPipeline tl(config);
    tl.ReplaceSpeakers({{0.0, 10.0, "speaker_0", 0.9f, "spk_a"}});
    tl.ReplacePrimarySpeakers({{0.0, 5.0, "speaker_0", 0.9f, "spk_a"},
                               {5.0, 10.0, "speaker_1", 0.9f, "spk_b"}});
    const auto revision = tl.UpsertText(0, 0.0, 10.0, "whole");
    CHECK(revision.size() == 1 && revision[0].entries.size() == 1 &&
              revision[0].entries[0].speaker_id == "spk_a",
          "primary-only boundary cannot split or relabel an activity span");
  }

  // ---- 3f2. A primary transition may refine only the range where two
  // resolved activity identities are simultaneously active. ----
  {
    using Pipeline = orator::pipeline::BusinessSpeakerPipeline;
    Pipeline::Config config;
    config.align_snap_pause_sec = 0.25;
    config.speaker_overlap_tie_policy =
        Pipeline::SpeakerOverlapTiePolicy::kPrimarySpeaker;
    TestBusinessSpeakerPipeline tl(config);
    tl.ReplaceSpeakers({{0.0, 1.0, "speaker_0", 0.9f, "spk_a"},
                        {0.2, 0.9, "speaker_1", 0.9f, "spk_b"}});
    tl.ReplacePrimarySpeakers(
        {{0.0, 0.5, "speaker_0", 0.9f, "spk_a"},
         {0.5, 0.9, "speaker_1", 0.9f, "spk_b"}});
    tl.UpsertText(0, 0.0, 1.0, "甲乙丙丁");
    tl.UpsertAlign(0, 0.0, 1.0,
                   {{0.0, 0.2, "甲"},
                    {0.2, 0.4, "乙"},
                    {0.4, 0.7, "丙"},
                    {0.9, 1.0, "丁"}});
    const auto snap = tl.Snapshot();
    const auto refined = std::find_if(
        snap.begin(), snap.end(), [](const auto& entry) {
          return entry.speaker_decision.reason ==
                 "primary_speaker_overlap_refinement";
        });
    const auto switched = std::find_if(
        snap.begin(), snap.end(), [](const auto& entry) {
          return entry.speaker_id == "spk_b" && entry.text == "丙";
        });
    CHECK(refined != snap.end() && refined->speaker_id == "spk_a" &&
              refined->text == "甲乙" && switched != snap.end(),
          "primary transition refines a competing activity overlap");
    CHECK(!snap.empty() && snap.front().speaker_id == "spk_a" &&
              snap.back().speaker_id == "spk_a" &&
              snap.back().text == "丁",
          "primary refinement leaves unique activity ranges unchanged");
  }

  // ---- 3g. Activity and primary tracks converge independent of arrival
  // order, including the audit reason. ----
  {
    using Pipeline = orator::pipeline::BusinessSpeakerPipeline;
    Pipeline::Config config;
    config.speaker_overlap_tie_policy =
        Pipeline::SpeakerOverlapTiePolicy::kPrimarySpeaker;
    const std::vector<TestBusinessSpeakerPipeline::SpeakerInput> activity = {
        {0.0, 10.0, "speaker_0", 0.9f, "spk_a"},
        {4.0, 4.1, "speaker_1", 0.5f, "spk_b"}};
    const std::vector<TestBusinessSpeakerPipeline::SpeakerInput> primary = {
        {0.0, 10.0, "speaker_0", 0.8f, "spk_a"}};
    TestBusinessSpeakerPipeline a(config);
    TestBusinessSpeakerPipeline b(config);
    a.ReplaceSpeakers(activity);
    a.ReplacePrimarySpeakers(primary);
    a.UpsertText(0, 4.0, 4.1, "x");
    b.UpsertText(0, 4.0, 4.1, "x");
    b.ReplacePrimarySpeakers(primary);
    b.ReplaceSpeakers(activity);
    const auto left = a.Snapshot();
    const auto right = b.Snapshot();
    CHECK(left.size() == 1 && right.size() == 1 &&
              left[0].speaker_id == right[0].speaker_id &&
              left[0].speaker_decision.reason ==
                  right[0].speaker_decision.reason,
          "primary arbitration converges across evidence arrival orders");
  }

  // ---- 2. A text spanning two speakers is SPLIT at the diarization boundary
  // (diar-driven view), not attributed wholly to the max-overlap speaker. ----
  {
    TestBusinessSpeakerPipeline tl;
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
    TestBusinessSpeakerPipeline tl;
    tl.UpsertSpeaker(0.0, 10.0, "speaker_0", 0.6f);
    auto r0 = tl.UpsertText(0, 4.0, 6.0, "mid");
    CHECK(r0[0].entries[0].speaker == "speaker_0", "initial attribution spk0");
    // A more-overlapping speaker_1 segment arrives exactly over [4,6).
    auto r1 = tl.UpsertSpeaker(4.0, 6.0, "speaker_1", 0.95f);
    CHECK(r1.size() == 1, "overlapping speaker update yields a revision");
    if (r1.size() == 1 && !r1[0].entries.empty()) {
      CHECK(r1[0].entries[0].speaker == "speaker_1",
            "attribution revised to speaker_1 (tighter segment wins on tie)");
      const auto& decision = r1[0].entries[0].speaker_decision;
      CHECK(decision.reason == "competing_diar_interval_policy",
            "overlapping decision records its tie policy");
      CHECK(decision.candidates.size() == 2,
            "selected and rejected diar candidates are retained");
      if (decision.candidates.size() == 2) {
        CHECK(decision.candidates[0].selected &&
                  decision.candidates[0].speaker == "speaker_1",
              "selected candidate is explicit and ordered first");
        CHECK(!decision.candidates[1].selected &&
                  decision.candidates[1].speaker == "speaker_0",
              "rejected overlapping candidate remains auditable");
      }
      CHECK(decision.overlap_margin_sec == 0.0,
            "equal-overlap candidates expose a zero overlap margin");
      CHECK(std::abs(decision.confidence_margin - 0.35) < 1e-6,
            "selected-versus-rejected confidence margin is recorded");
      CHECK(r1[0].dirty_start == 4.0 && r1[0].dirty_end == 6.0,
            "revision dirty range matches the text span");
    }
  }

  // ---- 4. Incrementality: a speaker update far from a text does not revise
  // ----
  {
    TestBusinessSpeakerPipeline tl;
    tl.UpsertSpeaker(0.0, 5.0, "speaker_0", 0.9f);
    tl.UpsertText(0, 0.0, 5.0, "a");
    // Speaker far away [100,105) -> no overlap with the text -> no revision.
    auto r = tl.UpsertSpeaker(100.0, 105.0, "speaker_1", 0.9f);
    CHECK(r.empty(), "non-overlapping speaker update touches nothing");
  }

  // ---- 4b. Pure alignment: text with no overlapping speaker is "unknown",
  // NOT borrowed from the nearest segment (Spec 004 invariant). ----
  {
    TestBusinessSpeakerPipeline tl;
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
    TestBusinessSpeakerPipeline tl;
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
    TestBusinessSpeakerPipeline tl;
    tl.UpsertText(0, 0.0, 5.0, "x");
    tl.UpsertText(1, 5.0, 10.0, "y");
    // First diar view: one speaker covering everything -> both texts -> spk0.
    auto r1 = tl.ReplaceSpeakers({{0.0, 10.0, "speaker_0", 0.8f}});
    CHECK(r1.size() == 2, "ReplaceSpeakers re-projects both texts (new->spk0)");
    // Refined diar view: spk1 owns [5,10). Text 1 flips; text 0 keeps its
    // speaker but receives revised confidence evidence.
    auto r2 = tl.ReplaceSpeakers(
        {{0.0, 5.0, "speaker_0", 0.9f}, {5.0, 10.0, "speaker_1", 0.9f}});
    CHECK(r2.size() == 2,
          "refined diar view revises changed attribution and audit evidence");
    bool found_reattribution = false;
    for (const auto& revision : r2) {
      if (!revision.entries.empty() && revision.entries[0].text_id == 1 &&
          revision.entries[0].speaker == "speaker_1") {
        found_reattribution = true;
      }
    }
    CHECK(found_reattribution,
          "text 1 re-attributed to speaker_1 after diar refinement");
  }

  // ---- 7. Interleaved convergence: immutable ASR and revisable diar evidence
  // in different arrival orders must converge to the SAME final view. ----
  {
    TestBusinessSpeakerPipeline a;
    TestBusinessSpeakerPipeline b;

    // Order A: diar baseline -> finalized text -> diar refinement.
    a.ReplaceSpeakers(
        {{0.0, 4.0, "speaker_0", 0.8f}, {4.0, 8.0, "speaker_1", 0.8f}});
    a.UpsertText(7, 2.0, 7.0, "abcdef");
    a.ReplaceSpeakers(
        {{0.0, 3.0, "speaker_0", 0.9f}, {3.0, 8.0, "speaker_1", 0.9f}});

    // Order B: finalized text arrives first, diar views arrive later.
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
    TestBusinessSpeakerPipeline tl;
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
    TestBusinessSpeakerPipeline tl;
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

  // ---- 8b. FR16AAX: when activity and primary start the same new stable
  // identity inside a long aligned unit, the next complete unit starts a new
  // alignment run. Neither source alone nor a conflicting identity may split
  // the gapless run. ----
  {
    using Pipeline = orator::pipeline::BusinessSpeakerPipeline;
    Pipeline::Config config;
    config.align_snap_pause_sec = 0.25;
    config.align_boundary_split_tolerance_sec = 0.08;
    config.speaker_overlap_tie_policy =
        Pipeline::SpeakerOverlapTiePolicy::kPrimarySpeaker;
    const std::vector<TestBusinessSpeakerPipeline::SpeakerInput> activity = {
        {0.0, 1.4, "speaker_0", 0.9f, "spk_a"},
        {1.9, 3.0, "speaker_1", 0.9f, "spk_b"}};
    const std::vector<TestBusinessSpeakerPipeline::SpeakerInput> primary = {
        {0.0, 1.34, "speaker_0", 0.9f, "spk_a"},
        {1.96, 3.0, "speaker_1", 0.9f, "spk_b"}};
    const std::vector<ComprehensiveTimeline::AlignUnitSeg> alignment = {
        {0.0, 0.5, "A"},
        {0.5, 1.0, "B"},
        {1.0, 2.1, "C"},
        {2.1, 2.5, "D"}};

    TestBusinessSpeakerPipeline ordered(config);
    ordered.ReplaceSpeakers(activity);
    ordered.ReplacePrimarySpeakers(primary);
    ordered.UpsertText(0, 0.0, 3.0, "ABCD");
    ordered.UpsertAlign(0, 0.0, 3.0, alignment);
    const auto split = ordered.Snapshot();
    CHECK(split.size() == 2 && split[0].speaker_id == "spk_a" &&
              split[0].text == "ABC" && split[1].speaker_id == "spk_b" &&
              split[1].text == "D",
          "dual-Sortformer transition splits after the crossing aligned unit");
    if (split.size() == 2) {
      CHECK(split[0].text + split[1].text == "ABCD",
            "corroborated transition preserves source text byte-for-byte");
    }

    TestBusinessSpeakerPipeline reversed(config);
    reversed.UpsertAlign(0, 0.0, 3.0, alignment);
    reversed.UpsertText(0, 0.0, 3.0, "ABCD");
    reversed.ReplacePrimarySpeakers(primary);
    reversed.ReplaceSpeakers(activity);
    const auto reversed_split = reversed.Snapshot();
    CHECK(reversed_split.size() == split.size() &&
              reversed_split.size() == 2 &&
              reversed_split[0].speaker_id == split[0].speaker_id &&
              reversed_split[0].text == split[0].text &&
              reversed_split[1].speaker_id == split[1].speaker_id &&
              reversed_split[1].text == split[1].text,
          "corroborated transition converges across evidence arrival order");

    TestBusinessSpeakerPipeline activity_only(config);
    activity_only.ReplaceSpeakers(activity);
    activity_only.UpsertText(0, 0.0, 3.0, "ABCD");
    activity_only.UpsertAlign(0, 0.0, 3.0, alignment);
    const auto activity_only_snap = activity_only.Snapshot();
    CHECK(activity_only_snap.size() == 1 &&
              activity_only_snap[0].speaker_id == "spk_a" &&
              activity_only_snap[0].text == "ABCD",
          "activity-only transition preserves gapless run coherence");

    TestBusinessSpeakerPipeline primary_only(config);
    primary_only.ReplaceSpeakers(
        {{0.0, 3.0, "speaker_0", 0.9f, "spk_a"}});
    primary_only.ReplacePrimarySpeakers(primary);
    primary_only.UpsertText(0, 0.0, 3.0, "ABCD");
    primary_only.UpsertAlign(0, 0.0, 3.0, alignment);
    const auto primary_only_snap = primary_only.Snapshot();
    CHECK(primary_only_snap.size() == 1 &&
              primary_only_snap[0].speaker_id == "spk_a" &&
              primary_only_snap[0].text == "ABCD",
          "primary-only transition cannot create a business split");

    TestBusinessSpeakerPipeline conflicting(config);
    conflicting.ReplaceSpeakers(activity);
    conflicting.ReplacePrimarySpeakers(
        {{0.0, 1.34, "speaker_0", 0.9f, "spk_a"},
         {1.96, 3.0, "speaker_2", 0.9f, "spk_c"}});
    conflicting.UpsertText(0, 0.0, 3.0, "ABCD");
    conflicting.UpsertAlign(0, 0.0, 3.0, alignment);
    const auto conflicting_snap = conflicting.Snapshot();
    CHECK(conflicting_snap.size() == 1 &&
              conflicting_snap[0].speaker_id == "spk_a" &&
              conflicting_snap[0].text == "ABCD",
          "conflicting primary identity preserves gapless run coherence");

    TestBusinessSpeakerPipeline insufficient_gap(config);
    insufficient_gap.ReplaceSpeakers(
        {{0.0, 1.7, "speaker_0", 0.9f, "spk_a"},
         {1.9, 3.0, "speaker_1", 0.9f, "spk_b"}});
    insufficient_gap.ReplacePrimarySpeakers(
        {{0.0, 1.76, "speaker_0", 0.9f, "spk_a"},
         {1.96, 3.0, "speaker_1", 0.9f, "spk_b"}});
    insufficient_gap.UpsertText(0, 0.0, 3.0, "ABCD");
    insufficient_gap.UpsertAlign(0, 0.0, 3.0, alignment);
    const auto insufficient_gap_snap = insufficient_gap.Snapshot();
    CHECK(insufficient_gap_snap.size() == 1 &&
              insufficient_gap_snap[0].speaker_id == "spk_a" &&
              insufficient_gap_snap[0].text == "ABCD",
          "sub-pause native gap preserves gapless run coherence");

    TestBusinessSpeakerPipeline short_primary(config);
    short_primary.ReplaceSpeakers(activity);
    short_primary.ReplacePrimarySpeakers(
        {{0.0, 1.34, "speaker_0", 0.9f, "spk_a"},
         {1.96, 2.25, "speaker_1", 0.9f, "spk_b"}});
    short_primary.UpsertText(0, 0.0, 3.0, "ABCD");
    short_primary.UpsertAlign(0, 0.0, 3.0, alignment);
    const auto short_primary_snap = short_primary.Snapshot();
    CHECK(short_primary_snap.size() == 1 &&
              short_primary_snap[0].speaker_id == "spk_a" &&
              short_primary_snap[0].text == "ABCD",
          "subminimum primary run preserves gapless run coherence");

    TestBusinessSpeakerPipeline contested(config);
    contested.ReplaceSpeakers(
        {{0.0, 1.4, "speaker_0", 0.9f, "spk_a"},
         {1.9, 3.0, "speaker_1", 0.9f, "spk_b"},
         {2.1, 2.3, "speaker_2", 0.9f, "spk_c"}});
    contested.ReplacePrimarySpeakers(primary);
    contested.UpsertText(0, 0.0, 3.0, "ABCD");
    contested.UpsertAlign(0, 0.0, 3.0, alignment);
    const auto contested_snap = contested.Snapshot();
    CHECK(contested_snap.size() == 1 &&
              contested_snap[0].speaker_id == "spk_a" &&
              contested_snap[0].text == "ABCD",
          "competing minimum-run identity preserves gapless run coherence");
  }

  // ---- 10. Alignment snap keeps ordinary short pauses coherent, but a diar
  // boundary near an alignment gap forces a split. A 0.16s gap is below the
  // 0.25s run-coherence threshold, yet it still splits because the speaker
  // boundary sits inside the gap. ----
  {
    TestBusinessSpeakerPipeline tl;
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
      CHECK(snap[0].speaker_decision.text_projection_source ==
                    "forced_alignment" &&
                snap[1].speaker_decision.text_projection_source ==
                    "forced_alignment",
            "speaker decisions identify forced-alignment text projection");
    }
  }

  // ---- 11. Speaker-support diagnostics flag sparse diar evidence without
  // changing the selected speaker. This exposes the tail failure mode where a
  // long comprehensive entry is created from two short same-speaker islands.
  {
    TestBusinessSpeakerPipeline tl;
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
      CHECK(snap[0].speaker_decision.reason == "same_speaker_gap_fill",
            "gap-filled attribution records its decision reason");
      CHECK(snap[0].speaker_decision.candidates.size() == 1 &&
                snap[0].speaker_decision.candidates[0].island_count == 2,
            "gap-fill audit retains both selected evidence islands");
    }
  }

  // ---- 12. Fully covered speaker intervals are labelled strong. ----
  {
    TestBusinessSpeakerPipeline tl;
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
    TestBusinessSpeakerPipeline tl;
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

  // ---- 14. Alignment chooses speaker boundaries without replacing the raw
  // ASR text. Punctuation omitted by the aligner must remain byte-exact. ----
  {
    TestBusinessSpeakerPipeline tl;
    const std::string source = "甲，乙。丙？丁。";
    tl.UpsertText(0, 0.0, 4.0, source);
    tl.ReplaceSpeakers({{0.0, 2.0, "speaker_0", 0.9f, "spk_0"},
                        {2.0, 4.0, "speaker_1", 0.9f, "spk_1"}});
    tl.UpsertAlign(0, 0.0, 4.0,
                   {{0.0, 0.5, "甲"},
                    {0.5, 1.0, "乙"},
                    {2.1, 2.5, "丙"},
                    {2.5, 3.0, "丁"}});
    const auto snap = tl.Snapshot();
    CHECK(snap.size() == 2,
          "alignment pause splits Chinese text at the speaker boundary");
    if (snap.size() == 2) {
      CHECK(snap[0].text == "甲，乙。",
            "first aligned slice preserves source punctuation");
      CHECK(snap[1].text == "丙？丁。",
            "second aligned slice preserves source punctuation");
      CHECK(snap[0].text + snap[1].text == source,
            "Chinese aligned slices reconstruct raw ASR text byte-for-byte");
    }
  }

  // ---- 15. A normalized alignment unit may span punctuation in the source.
  // Character-wise source mapping must still preserve all source bytes. ----
  {
    TestBusinessSpeakerPipeline tl;
    const std::string source = "hello,world again!";
    tl.UpsertText(0, 0.0, 3.0, source);
    tl.ReplaceSpeakers({{0.0, 1.5, "speaker_0", 0.9f, "spk_0"},
                        {1.5, 3.0, "speaker_1", 0.9f, "spk_1"}});
    tl.UpsertAlign(0, 0.0, 3.0,
                   {{0.0, 1.0, "helloworld"}, {2.0, 2.8, "again"}});
    const auto snap = tl.Snapshot();
    CHECK(snap.size() == 2,
          "normalized English alignment splits into two speaker turns");
    if (snap.size() == 2) {
      CHECK(snap[0].text + snap[1].text == source,
            "English aligned slices reconstruct raw ASR text byte-for-byte");
      CHECK(snap[0].text.find(',') != std::string::npos &&
                snap[1].text.find('!') != std::string::npos,
            "source punctuation remains present after aligned splitting");
    }
  }

  // ---- 16. Final direct voiceprint evidence may revise a diar attribution
  // while preserving the exact ASR source and exposing its decision source. ----
  {
    BusinessSpeakerPipeline::Config config;
    config.voiceprint_fusion_enabled = true;
    TestBusinessSpeakerPipeline tl(config);
    tl.UpsertText(0, 0.0, 1.0, "甲乙");
    tl.ReplaceSpeakers({{0.0, 1.0, "speaker_0", 0.9f, "spk_a"},
                        {1.1, 2.0, "speaker_1", 0.9f, "spk_b"}});
    ComprehensiveTimeline::SpeakerVoiceprintEvidence evidence;
    evidence.evidence_id = "business_interval:0:0";
    evidence.kind = "business_interval";
    evidence.text_id = 0;
    evidence.source_start = 0;
    evidence.source_end = 2;
    evidence.start = 0.0;
    evidence.end = 1.0;
    evidence.embedding_available = true;
    evidence.session_scores = {{"spk_a", 0.60f}, {"spk_b", 0.75f}};
    tl.ReplaceVoiceprint({evidence});
    const auto snap = tl.Snapshot();
    CHECK(snap.size() == 1, "direct voiceprint keeps one source span");
    if (snap.size() == 1) {
      CHECK(snap[0].speaker_id == "spk_b",
            "direct voiceprint revises the selected identity");
      CHECK(snap[0].text == "甲乙",
            "direct voiceprint preserves source text exactly");
      CHECK(snap[0].speaker_decision.reason == "voiceprint_direct_short",
            "direct voiceprint records a typed decision reason");
      CHECK(snap[0].speaker_decision.speaker_source ==
                    "titanet_session_gallery",
            "direct voiceprint records its model evidence source");
    }
  }

  // ---- 17. A dual-gallery exact phrase may revise a conflicting coarser
  // direct interval while preserving the source. ----
  {
    BusinessSpeakerPipeline::Config config;
    config.voiceprint_fusion_enabled = true;
    TestBusinessSpeakerPipeline tl(config);
    const std::string source = "甲，乙。";
    tl.UpsertText(0, 0.0, 2.0, source);
    tl.ReplaceSpeakers({{0.0, 2.0, "speaker_0", 0.9f, "spk_a"},
                        {2.1, 3.0, "speaker_1", 0.9f, "spk_b"}});
    tl.UpsertAlign(0, 0.0, 2.0,
                   {{0.0, 0.5, "甲"}, {1.0, 1.5, "乙"}});
    ComprehensiveTimeline::SpeakerVoiceprintEvidence direct;
    direct.evidence_id = "business_interval:0:0";
    direct.kind = "business_interval";
    direct.text_id = 0;
    direct.source_start = 0;
    direct.source_end = 2;
    direct.start = 0.0;
    direct.end = 0.8;
    direct.embedding_available = true;
    direct.session_scores = {{"spk_a", 0.75f}, {"spk_b", 0.60f}};
    ComprehensiveTimeline::SpeakerVoiceprintEvidence phrase;
    phrase.evidence_id = "punctuation_phrase:0:0";
    phrase.kind = "punctuation_phrase";
    phrase.text_id = 0;
    phrase.source_start = 0;
    phrase.source_end = 4;
    phrase.start = 0.0;
    phrase.end = 1.5;
    phrase.embedding_available = true;
    phrase.robust_gallery_complete = true;
    phrase.session_scores = {{"spk_a", 0.55f}, {"spk_b", 0.75f}};
    phrase.robust_scores = {{"spk_a", 0.54f}, {"spk_b", 0.73f}};
    tl.ReplaceVoiceprint({direct, phrase});
    const auto snap = tl.Snapshot();
    CHECK(!snap.empty(), "conflicting phrase still emits a business view");
    if (!snap.empty()) {
      CHECK(std::all_of(snap.begin(), snap.end(), [](const auto& entry) {
              return entry.speaker_id == "spk_b";
            }),
            "dual-gallery phrase supersedes the coarser direct interval");
      std::string rebuilt;
      for (const auto& entry : snap) rebuilt += entry.text;
      CHECK(rebuilt == source,
            "dual-gallery phrase revision preserves the full source");
    }
  }

  // ---- 18. A low-score dual-gallery phrase also abstains when a direct
  // interval conflicts. Agreement alone is insufficient for an override. ----
  {
    BusinessSpeakerPipeline::Config config;
    config.voiceprint_fusion_enabled = true;
    TestBusinessSpeakerPipeline tl(config);
    tl.UpsertText(0, 0.0, 1.0, "甲乙");
    tl.ReplaceSpeakers({{0.0, 1.0, "speaker_0", 0.9f, "spk_a"},
                        {1.1, 2.0, "speaker_1", 0.9f, "spk_b"}});
    ComprehensiveTimeline::SpeakerVoiceprintEvidence direct;
    direct.evidence_id = "business_interval:0:0";
    direct.kind = "business_interval";
    direct.text_id = 0;
    direct.source_start = 0;
    direct.source_end = 2;
    direct.start = 0.0;
    direct.end = 1.0;
    direct.embedding_available = true;
    direct.session_scores = {{"spk_a", 0.75f}, {"spk_b", 0.60f}};
    ComprehensiveTimeline::SpeakerVoiceprintEvidence phrase = direct;
    phrase.evidence_id = "punctuation_phrase:0:0";
    phrase.kind = "punctuation_phrase";
    phrase.robust_gallery_complete = true;
    phrase.session_scores = {{"spk_a", 0.15f}, {"spk_b", 0.27f}};
    phrase.robust_scores = {{"spk_a", 0.17f}, {"spk_b", 0.28f}};
    tl.ReplaceVoiceprint({direct, phrase});
    const auto snap = tl.Snapshot();
    CHECK(snap.size() == 1 && snap[0].speaker_id == "spk_a",
          "low-score dual phrase preserves the direct interval");
  }

  // ---- 18a. Generic phrase evidence cannot erase an aligned native speaker
  // handoff already present inside the exact phrase source range. ----
  {
    BusinessSpeakerPipeline::Config config;
    config.voiceprint_fusion_enabled = true;
    config.speaker_overlap_tie_policy =
        BusinessSpeakerPipeline::SpeakerOverlapTiePolicy::kPrimarySpeaker;
    TestBusinessSpeakerPipeline tl(config);
    const std::string source = "甲乙丙。";
    tl.UpsertText(0, 0.0, 1.0, source);
    tl.ReplaceSpeakers({{0.0, 1.0, "speaker_0", 0.9f, "spk_a"},
                        {0.2, 1.5, "speaker_1", 0.9f, "spk_b"}});
    tl.ReplacePrimarySpeakers({{0.0, 0.5, "speaker_0", 0.9f, "spk_a"},
                               {0.5, 1.0, "speaker_1", 0.9f, "spk_b"}});
    tl.UpsertAlign(0, 0.0, 1.0,
                   {{0.0, 0.2, "甲"},
                    {0.2, 0.4, "乙"},
                    {0.5, 0.95, "丙"}});
    ComprehensiveTimeline::SpeakerVoiceprintEvidence phrase;
    phrase.evidence_id = "punctuation_phrase:0:0";
    phrase.kind = "punctuation_phrase";
    phrase.text_id = 0;
    phrase.source_start = 0;
    phrase.source_end = 4;
    phrase.start = 0.0;
    phrase.end = 0.95;
    phrase.embedding_available = true;
    phrase.robust_gallery_complete = true;
    phrase.session_scores = {{"spk_a", 0.75f}, {"spk_b", 0.60f}};
    phrase.robust_scores = {{"spk_a", 0.74f}, {"spk_b", 0.59f}};
    tl.ReplaceVoiceprint({phrase});
    const auto snap = tl.Snapshot();
    std::vector<std::string> speaker_sequence;
    std::string rebuilt;
    for (const auto& entry : snap) {
      rebuilt += entry.text;
      if (speaker_sequence.empty() ||
          speaker_sequence.back() != entry.speaker_id) {
        speaker_sequence.push_back(entry.speaker_id);
      }
    }
    CHECK(speaker_sequence == std::vector<std::string>({"spk_a", "spk_b"}),
          "generic phrase preserves a native multi-identity handoff");
    CHECK(rebuilt == source,
          "handoff protection preserves the exact source text");
  }

  // ---- 18b. A subminimum primary fluctuation does not block an otherwise
  // eligible generic phrase decision. ----
  {
    using Pipeline = orator::pipeline::BusinessSpeakerPipeline;
    Pipeline::Config config;
    config.voiceprint_fusion_enabled = true;
    config.speaker_overlap_tie_policy =
        Pipeline::SpeakerOverlapTiePolicy::kPrimarySpeaker;
    TestBusinessSpeakerPipeline tl(config);
    const std::string source = "甲乙丙。";
    tl.UpsertText(0, 0.0, 1.2, source);
    tl.ReplaceSpeakers({{0.0, 1.2, "speaker_0", 0.9f, "spk_a"},
                        {0.5, 0.7, "speaker_1", 0.9f, "spk_b"}});
    tl.ReplacePrimarySpeakers({{0.0, 0.5, "speaker_0", 0.9f, "spk_a"},
                               {0.5, 0.7, "speaker_1", 0.9f, "spk_b"},
                               {0.7, 1.2, "speaker_0", 0.9f, "spk_a"}});
    tl.UpsertAlign(0, 0.0, 1.2,
                   {{0.0, 0.4, "甲"},
                    {0.5, 0.65, "乙"},
                    {0.8, 1.1, "丙"}});
    ComprehensiveTimeline::SpeakerVoiceprintEvidence phrase;
    phrase.evidence_id = "punctuation_phrase:0:0";
    phrase.kind = "punctuation_phrase";
    phrase.text_id = 0;
    phrase.source_start = 0;
    phrase.source_end = 4;
    phrase.start = 0.0;
    phrase.end = 1.1;
    phrase.embedding_available = true;
    phrase.robust_gallery_complete = true;
    phrase.session_scores = {{"spk_a", 0.75f}, {"spk_b", 0.60f}};
    phrase.robust_scores = {{"spk_a", 0.74f}, {"spk_b", 0.59f}};
    tl.ReplaceVoiceprint({phrase});
    const auto snap = tl.Snapshot();
    CHECK(!snap.empty() &&
              std::all_of(snap.begin(), snap.end(), [](const auto& entry) {
                return entry.speaker_id == "spk_a";
              }),
          "subminimum primary fluctuation keeps generic phrase behavior");
  }

  // ---- 18c. FR32 preserves only an exact business-interval/TitaNet-backed
  // short primary A-B-A return from an ordinary phrase repaint. ----
  {
    using Pipeline = orator::pipeline::BusinessSpeakerPipeline;
    struct Options {
      bool partial_candidate_activity = false;
      bool third_activity = false;
      bool different_following_primary = false;
      bool missing_candidate_alignment = false;
      bool regular_candidate = false;
      bool non_exact_interval = false;
      bool duplicate_exact_interval = false;
      bool missing_interval_embedding = false;
      bool incomplete_robust_gallery = false;
      bool gallery_disagreement = false;
      bool interval_gate_failure = false;
    };
    auto run_case = [](const Options& options) {
      Pipeline::Config config;
      config.voiceprint_fusion_enabled = true;
      if (options.interval_gate_failure) {
        config.voiceprint_short_min_score = 0.5f;
      }
      config.speaker_overlap_tie_policy =
          Pipeline::SpeakerOverlapTiePolicy::kPrimarySpeaker;
      TestBusinessSpeakerPipeline tl(config);

      const double candidate_end = options.regular_candidate ? 2.0 : 1.0;
      const double total_end = candidate_end + 0.4;
      tl.UpsertText(0, 0.0, total_end, "甲乙丙。");
      std::vector<TestBusinessSpeakerPipeline::SpeakerInput> activity = {
          {0.0, total_end, "speaker_0", 0.9f, "spk_a"},
          {options.partial_candidate_activity ? 0.5 : 0.4,
           options.partial_candidate_activity ? candidate_end - 0.1
                                              : candidate_end,
           "speaker_1", 0.9f, "spk_b"}};
      if (options.third_activity) {
        activity.push_back({0.6, 0.8, "speaker_2", 0.9f, "spk_c"});
      }
      tl.ReplaceSpeakers(activity);
      tl.ReplacePrimarySpeakers(
          {{0.0, 0.4, "speaker_0", 0.9f, "spk_a"},
           {0.4, candidate_end, "speaker_1", 0.9f, "spk_b"},
           {candidate_end, total_end, "speaker_0", 0.9f,
            options.different_following_primary ? "spk_c" : "spk_a"}});

      std::vector<ComprehensiveTimeline::AlignUnitSeg> units = {
          {0.1, 0.3, "甲"},
          {0.55, options.regular_candidate ? 1.4 : 0.85, "乙"},
          {candidate_end + 0.1, candidate_end + 0.3, "丙"}};
      if (options.missing_candidate_alignment) {
        units.erase(units.begin() + 1);
      }
      tl.UpsertAlign(0, 0.0, total_end, units);

      ComprehensiveTimeline::SpeakerVoiceprintEvidence interval;
      interval.evidence_id = "business_interval:0:0";
      interval.kind = "business_interval";
      interval.text_id = 0;
      interval.source_start = 1;
      interval.source_end = 2;
      interval.start = options.non_exact_interval ? 0.41 : 0.4;
      interval.end = candidate_end;
      interval.embedding_available = !options.missing_interval_embedding;
      interval.robust_gallery_complete =
          !options.incomplete_robust_gallery;
      const float candidate_score =
          options.interval_gate_failure ? 0.45f : 0.75f;
      interval.session_scores =
          {{"spk_b", candidate_score}, {"spk_a", 0.30f}};
      interval.robust_scores = options.gallery_disagreement
                                   ? std::vector<ComprehensiveTimeline::VoiceprintScore>(
                                         {{"spk_a", 0.75f}, {"spk_b", 0.30f}})
                                   : std::vector<ComprehensiveTimeline::VoiceprintScore>(
                                         {{"spk_b", candidate_score},
                                          {"spk_a", 0.30f}});

      ComprehensiveTimeline::SpeakerVoiceprintEvidence phrase;
      phrase.evidence_id = "punctuation_phrase:0:0";
      phrase.kind = "punctuation_phrase";
      phrase.text_id = 0;
      phrase.source_start = 0;
      phrase.source_end = 4;
      phrase.start = 0.1;
      phrase.end = candidate_end + 0.3;
      phrase.embedding_available = true;
      phrase.robust_gallery_complete = true;
      phrase.session_scores = {{"spk_a", 0.75f}, {"spk_b", 0.60f}};
      phrase.robust_scores = {{"spk_a", 0.74f}, {"spk_b", 0.59f}};
      std::vector<ComprehensiveTimeline::SpeakerVoiceprintEvidence> evidence = {
          interval, phrase};
      if (options.duplicate_exact_interval) {
        interval.evidence_id = "business_interval:0:duplicate";
        evidence.push_back(interval);
      }
      tl.ReplaceVoiceprint(evidence);
      return tl.Snapshot();
    };
    auto speaker_sequence = [](const auto& entries) {
      std::vector<std::string> ids;
      for (const auto& entry : entries) {
        if (ids.empty() || ids.back() != entry.speaker_id) {
          ids.push_back(entry.speaker_id);
        }
      }
      return ids;
    };
    auto rebuilt_source = [](const auto& entries) {
      std::string source;
      for (const auto& entry : entries) source += entry.text;
      return source;
    };

    const auto retained = run_case({});
    CHECK(speaker_sequence(retained) ==
              std::vector<std::string>({"spk_a", "spk_b", "spk_a"}),
          "exact cross-scale primary return survives ordinary phrase repaint");
    CHECK(rebuilt_source(retained) == "甲乙丙。",
          "primary-return protection preserves source text exactly");

    Options partial_activity;
    partial_activity.partial_candidate_activity = true;
    CHECK(speaker_sequence(run_case(partial_activity)) ==
              std::vector<std::string>({"spk_a"}),
          "incomplete activity coverage makes exact return guard abstain");

    Options third_activity;
    third_activity.third_activity = true;
    CHECK(speaker_sequence(run_case(third_activity)) ==
              std::vector<std::string>({"spk_a"}),
          "third activity identity makes exact return guard abstain");

    Options different_following;
    different_following.different_following_primary = true;
    CHECK(speaker_sequence(run_case(different_following)) ==
              std::vector<std::string>({"spk_a"}),
          "non-return primary topology keeps ordinary phrase behavior");

    Options missing_alignment;
    missing_alignment.missing_candidate_alignment = true;
    CHECK(speaker_sequence(run_case(missing_alignment)) ==
              std::vector<std::string>({"spk_a"}),
          "missing aligned candidate character cannot synthesize a return");

    Options regular_candidate;
    regular_candidate.regular_candidate = true;
    CHECK(speaker_sequence(run_case(regular_candidate)) ==
              std::vector<std::string>({"spk_a"}),
          "regular-duration primary run remains outside short-return guard");

    Options non_exact_interval;
    non_exact_interval.non_exact_interval = true;
    CHECK(speaker_sequence(run_case(non_exact_interval)) ==
              std::vector<std::string>({"spk_a"}),
          "non-exact business interval cannot corroborate a primary return");

    Options duplicate_interval;
    duplicate_interval.duplicate_exact_interval = true;
    CHECK(speaker_sequence(run_case(duplicate_interval)) ==
              std::vector<std::string>({"spk_a"}),
          "duplicate exact business intervals make corroboration ambiguous");

    Options missing_embedding;
    missing_embedding.missing_interval_embedding = true;
    CHECK(speaker_sequence(run_case(missing_embedding)) ==
              std::vector<std::string>({"spk_a"}),
          "missing exact-interval embedding makes return guard abstain");

    Options incomplete_gallery;
    incomplete_gallery.incomplete_robust_gallery = true;
    CHECK(speaker_sequence(run_case(incomplete_gallery)) ==
              std::vector<std::string>({"spk_a"}),
          "incomplete robust gallery makes return guard abstain");

    Options gallery_disagreement;
    gallery_disagreement.gallery_disagreement = true;
    CHECK(speaker_sequence(run_case(gallery_disagreement)) ==
              std::vector<std::string>({"spk_a"}),
          "exact-interval gallery disagreement makes return guard abstain");

    Options gate_failure;
    gate_failure.interval_gate_failure = true;
    CHECK(speaker_sequence(run_case(gate_failure)) ==
              std::vector<std::string>({"spk_a"}),
          "exact-interval score gate failure makes return guard abstain");
  }

  // ---- 18c2. FR33 keeps a native phrase when the narrow session challenge
  // conflicts with the complete cross-scale abstention topology. ----
  {
    using Entry = ComprehensiveTimeline::Entry;
    struct Options {
      bool phrase_robust_challenger = true;
      bool include_vad = true;
      int containing_vad_count = 1;
      bool vad_robust_complete = true;
      bool vad_session_eligible = true;
      bool vad_robust_current = true;
      bool include_interval = true;
      bool duplicate_interval = false;
      bool include_complete_source = true;
      bool duplicate_complete_source = false;
      bool broad_pair_consistent = true;
      bool broad_margin_passes = true;
      bool broad_score_abstains = true;
      bool competing_activity = false;
      bool enough_aligned_units = true;
    };
    auto run_case = [](const Options& options) -> std::vector<Entry> {
      BusinessSpeakerPipeline::Config config;
      config.voiceprint_fusion_enabled = true;
      config.speaker_overlap_tie_policy =
          BusinessSpeakerPipeline::SpeakerOverlapTiePolicy::kPrimarySpeaker;
      TestBusinessSpeakerPipeline tl(config);

      std::vector<TestBusinessSpeakerPipeline::SpeakerInput> activity = {
          {-2.0, -1.0, "slot_1", 0.9f, "spk_b"},
          {0.0, 2.2, "slot_0", 0.9f, "spk_a"}};
      if (options.competing_activity) {
        activity.push_back({0.6, 1.1, "slot_1", 0.9f, "spk_b"});
      }
      tl.ReplaceSpeakers(activity);
      tl.ReplacePrimarySpeakers(
          {{0.0, 2.2, "slot_0", 0.9f, "spk_a"}});
      tl.UpsertText(0, 0.0, 2.2, "甲乙。");
      if (options.enough_aligned_units) {
        tl.UpsertAlign(0, 0.0, 2.2,
                       {{0.5, 0.8, "甲"}, {0.8, 1.3, "乙。"}});
      } else {
        tl.UpsertAlign(0, 0.0, 2.2, {{0.5, 1.3, "甲乙。"}});
      }

      auto broad = [&](const std::string& kind, const std::string& id,
                       double start, double end) {
        ComprehensiveTimeline::SpeakerVoiceprintEvidence evidence;
        evidence.evidence_id = id;
        evidence.kind = kind;
        evidence.text_id = 0;
        evidence.source_start = 0;
        evidence.source_end = 3;
        evidence.start = start;
        evidence.end = end;
        evidence.embedding_available = true;
        evidence.robust_gallery_complete = true;
        const float second_score =
            options.broad_margin_passes ? 0.48f : 0.52f;
        const std::string second_id =
            options.broad_pair_consistent ? "spk_b" : "spk_c";
        evidence.session_scores =
            {{"spk_a", options.broad_score_abstains ? 0.54f : 0.60f},
             {second_id, second_score},
             {"spk_d", 0.10f}};
        evidence.robust_scores =
            {{"spk_a", options.broad_score_abstains ? 0.53f : 0.59f},
             {second_id, options.broad_margin_passes ? 0.47f : 0.51f},
             {"spk_d", 0.10f}};
        return evidence;
      };

      std::vector<ComprehensiveTimeline::SpeakerVoiceprintEvidence> evidence;
      if (options.include_interval) {
        auto interval = broad("business_interval", "business_interval:0:0",
                              0.4, 2.0);
        evidence.push_back(interval);
        if (options.duplicate_interval) {
          interval.evidence_id = "business_interval:0:duplicate";
          evidence.push_back(interval);
        }
      }

      ComprehensiveTimeline::SpeakerVoiceprintEvidence phrase;
      phrase.evidence_id = "punctuation_phrase:0:0";
      phrase.kind = "punctuation_phrase";
      phrase.text_id = 0;
      phrase.source_start = 0;
      phrase.source_end = 3;
      phrase.start = 0.5;
      phrase.end = 1.3;
      phrase.embedding_available = true;
      phrase.robust_gallery_complete = true;
      phrase.session_scores =
          {{"spk_b", 0.40f}, {"spk_a", 0.34f}, {"spk_c", 0.10f}};
      phrase.robust_scores = options.phrase_robust_challenger
                                 ? std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                       {"spk_b", 0.36f},
                                       {"spk_a", 0.34f},
                                       {"spk_c", 0.10f}}
                                 : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                       {"spk_a", 0.36f},
                                       {"spk_b", 0.34f},
                                       {"spk_c", 0.10f}};
      evidence.push_back(phrase);

      if (options.include_complete_source) {
        auto complete =
            broad("complete_source", "complete_source:0", 0.0, 2.2);
        evidence.push_back(complete);
        if (options.duplicate_complete_source) {
          complete.evidence_id = "complete_source:0:duplicate";
          evidence.push_back(complete);
        }
      }

      if (options.include_vad) {
        for (int index = 0; index < options.containing_vad_count; ++index) {
          ComprehensiveTimeline::SpeakerVoiceprintEvidence vad;
          vad.evidence_id = "vad:" + std::to_string(index);
          vad.kind = "vad";
          vad.text_id = -1;
          vad.start = 0.4 - index * 0.01;
          vad.end = 1.4 + index * 0.01;
          vad.embedding_available = true;
          vad.robust_gallery_complete = options.vad_robust_complete;
          vad.session_scores = options.vad_session_eligible
                                   ? std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                         {"spk_b", 0.42f},
                                         {"spk_a", 0.34f},
                                         {"spk_c", 0.10f}}
                                   : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                         {"spk_b", 0.36f},
                                         {"spk_a", 0.34f},
                                         {"spk_c", 0.10f}};
          vad.robust_scores = options.vad_robust_current
                                  ? std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                        {"spk_a", 0.39f},
                                        {"spk_b", 0.37f},
                                        {"spk_c", 0.10f}}
                                  : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                        {"spk_b", 0.39f},
                                        {"spk_a", 0.37f},
                                        {"spk_c", 0.10f}};
          evidence.push_back(vad);
        }
      }
      tl.ReplaceVoiceprint(evidence);
      return tl.Snapshot();
    };
    auto all_identity = [](const std::vector<Entry>& entries,
                           const std::string& speaker_id) {
      return !entries.empty() &&
             std::all_of(entries.begin(), entries.end(),
                         [&](const auto& entry) {
                           return entry.speaker_id == speaker_id;
                         });
    };

    CHECK(all_identity(run_case({}), "spk_a"),
          "complete cross-scale abstention preserves the native identity");

    Options phrase_disagreement;
    phrase_disagreement.phrase_robust_challenger = false;
    CHECK(all_identity(run_case(phrase_disagreement), "spk_b"),
          "changed robust phrase rank keeps ordinary session behavior");

    Options missing_vad;
    missing_vad.include_vad = false;
    CHECK(all_identity(run_case(missing_vad), "spk_b"),
          "missing containing VAD keeps ordinary session behavior");

    Options multiple_vad;
    multiple_vad.containing_vad_count = 2;
    CHECK(all_identity(run_case(multiple_vad), "spk_b"),
          "multiple containing VAD intervals make FR33 abstain");

    Options incomplete_vad;
    incomplete_vad.vad_robust_complete = false;
    CHECK(all_identity(run_case(incomplete_vad), "spk_b"),
          "incomplete VAD gallery makes FR33 abstain");

    Options weak_vad_session;
    weak_vad_session.vad_session_eligible = false;
    CHECK(all_identity(run_case(weak_vad_session), "spk_b"),
          "ineligible VAD session challenge makes FR33 abstain");

    Options vad_no_reversal;
    vad_no_reversal.vad_robust_current = false;
    CHECK(all_identity(run_case(vad_no_reversal), "spk_b"),
          "missing robust VAD reversal makes FR33 abstain");

    Options missing_interval;
    missing_interval.include_interval = false;
    CHECK(all_identity(run_case(missing_interval), "spk_b"),
          "missing broad interval keeps ordinary session behavior");

    Options duplicate_interval;
    duplicate_interval.duplicate_interval = true;
    CHECK(all_identity(run_case(duplicate_interval), "spk_b"),
          "duplicate broad intervals make FR33 abstain");

    Options missing_complete;
    missing_complete.include_complete_source = false;
    CHECK(all_identity(run_case(missing_complete), "spk_b"),
          "missing complete-source evidence makes FR33 abstain");

    Options duplicate_complete;
    duplicate_complete.duplicate_complete_source = true;
    CHECK(all_identity(run_case(duplicate_complete), "spk_b"),
          "duplicate complete-source evidence makes FR33 abstain");

    Options changed_broad_pair;
    changed_broad_pair.broad_pair_consistent = false;
    CHECK(all_identity(run_case(changed_broad_pair), "spk_b"),
          "changed broad top-two pair keeps ordinary session behavior");

    Options weak_broad_margin;
    weak_broad_margin.broad_margin_passes = false;
    CHECK(all_identity(run_case(weak_broad_margin), "spk_b"),
          "broad margin abstention does not satisfy FR33");

    Options eligible_broad_score;
    eligible_broad_score.broad_score_abstains = false;
    const auto eligible_broad_entries = run_case(eligible_broad_score);
    CHECK(all_identity(eligible_broad_entries, "spk_a") &&
              std::all_of(eligible_broad_entries.begin(),
                          eligible_broad_entries.end(), [](const auto& entry) {
                            return entry.speaker_decision.reason ==
                                   "voiceprint_complete_source_dual_gallery";
                          }),
          "eligible broad evidence keeps ordinary complete-source behavior");

    Options competing_activity;
    competing_activity.competing_activity = true;
    CHECK(all_identity(run_case(competing_activity), "spk_b"),
          "competing native activity makes FR33 abstain");

    Options insufficient_alignment;
    insufficient_alignment.enough_aligned_units = false;
    CHECK(all_identity(run_case(insufficient_alignment), "spk_b"),
          "insufficient alignment keeps ordinary session behavior");
  }

  // ---- 18c3. FR34 lets an exact phrase and unique containing VAD supersede
  // a coarse direct write only under the bounded three-identity topology. ----
  {
    using Entry = ComprehensiveTimeline::Entry;
    struct Options {
      bool include_interval = true;
      bool duplicate_interval = false;
      bool interval_contains = true;
      bool interval_embedding = true;
      bool interval_robust_complete = true;
      bool interval_session_eligible = true;
      bool interval_robust_current = true;
      bool phrase_robust_complete = true;
      bool phrase_margin_passes = true;
      bool phrase_robust_candidate = true;
      bool include_vad = true;
      int containing_vad_count = 1;
      bool vad_robust_complete = true;
      bool vad_session_eligible = true;
      bool vad_robust_candidate = true;
      bool enough_aligned_units = true;
      bool candidate_activity_covers = true;
      bool primary_activity_covers = true;
      bool extra_activity = false;
      bool include_primary = true;
      int overlapping_primary_count = 1;
      std::string primary_identity = "spk_c";
      bool protected_current_label = false;
    };
    auto run_case = [](const Options& options) -> std::vector<Entry> {
      BusinessSpeakerPipeline::Config config;
      config.voiceprint_fusion_enabled = true;
      config.speaker_overlap_tie_policy =
          BusinessSpeakerPipeline::SpeakerOverlapTiePolicy::kHigherConfidence;
      TestBusinessSpeakerPipeline tl(config);
      tl.UpsertText(0, 0.0, 2.4, "前甲乙。后");

      std::vector<TestBusinessSpeakerPipeline::SpeakerInput> activity = {
          {0.6, options.candidate_activity_covers ? 1.6 : 1.2, "slot_b",
           0.95f, "spk_b"},
          {0.6, options.primary_activity_covers ? 1.6 : 1.2, "slot_c",
           0.80f, "spk_c"}};
      if (options.extra_activity) {
        activity.push_back({0.8, 1.3, "slot_d", 0.70f, "spk_d"});
      }
      tl.ReplaceSpeakers(activity);

      std::vector<TestBusinessSpeakerPipeline::SpeakerInput> primary;
      if (options.include_primary) {
        for (int index = 0; index < options.overlapping_primary_count;
             ++index) {
          primary.push_back({0.6 + index * 0.02, 1.6 - index * 0.02,
                             index == 0 ? "slot_c" : "slot_d", 0.90f,
                             index == 0 ? options.primary_identity : "spk_d"});
        }
      }
      tl.ReplacePrimarySpeakers(primary);

      tl.UpsertAlign(
          0, 0.0, 2.4,
          options.enough_aligned_units
              ? std::vector<TestBusinessSpeakerPipeline::AlignUnitSeg>{
                    {0.2, 0.4, "前"}, {0.7, 0.9, "甲"},
                    {0.9, 1.1, "乙"}, {1.1, 1.5, "。"},
                    {1.8, 2.0, "后"}}
              : std::vector<TestBusinessSpeakerPipeline::AlignUnitSeg>{
                    {0.2, 0.4, "前"}, {0.7, 1.5, "甲乙。"},
                    {1.8, 2.0, "后"}});

      std::vector<ComprehensiveTimeline::SpeakerVoiceprintEvidence> evidence;
      if (options.include_interval) {
        ComprehensiveTimeline::SpeakerVoiceprintEvidence interval;
        interval.evidence_id = "business_interval:0:0";
        interval.kind = "business_interval";
        interval.text_id = 0;
        interval.source_start = 0;
        interval.source_end = 5;
        interval.start = 0.2;
        interval.end = options.interval_contains ? 2.0 : 1.0;
        interval.embedding_available = options.interval_embedding;
        interval.robust_gallery_complete = options.interval_robust_complete;
        interval.session_scores = options.interval_session_eligible
                                      ? std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                            {"spk_a", 0.70f},
                                            {"spk_b", 0.60f},
                                            {"spk_c", 0.20f}}
                                      : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                            {"spk_a", 0.54f},
                                            {"spk_b", 0.45f},
                                            {"spk_c", 0.20f}};
        interval.robust_scores = options.interval_robust_current
                                     ? std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                           {"spk_a", 0.68f},
                                           {"spk_b", 0.58f},
                                           {"spk_c", 0.20f}}
                                     : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                           {"spk_b", 0.68f},
                                           {"spk_a", 0.58f},
                                           {"spk_c", 0.20f}};
        evidence.push_back(interval);
        if (options.duplicate_interval) {
          interval.evidence_id = "business_interval:0:duplicate";
          evidence.push_back(interval);
        }
      }

      if (options.protected_current_label) {
        ComprehensiveTimeline::SpeakerVoiceprintEvidence unit;
        unit.evidence_id = "aligned_unit:0:protected";
        unit.kind = "aligned_unit";
        unit.text_id = 0;
        unit.source_start = 2;
        unit.source_end = 3;
        unit.start = 0.9;
        unit.end = 1.1;
        unit.embedding_available = true;
        unit.robust_gallery_complete = true;
        unit.session_scores = {{"spk_d", 0.75f}, {"spk_a", 0.40f}};
        unit.robust_scores = {{"spk_d", 0.72f}, {"spk_a", 0.39f}};
        evidence.push_back(unit);
      }

      ComprehensiveTimeline::SpeakerVoiceprintEvidence phrase;
      phrase.evidence_id = "punctuation_phrase:0:0";
      phrase.kind = "punctuation_phrase";
      phrase.text_id = 0;
      phrase.source_start = 1;
      phrase.source_end = 4;
      phrase.start = 0.7;
      phrase.end = 1.5;
      phrase.embedding_available = true;
      phrase.robust_gallery_complete = options.phrase_robust_complete;
      phrase.session_scores = options.phrase_margin_passes
                                  ? std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                        {"spk_b", 0.50f},
                                        {"spk_a", 0.35f},
                                        {"spk_c", 0.20f}}
                                  : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                        {"spk_b", 0.38f},
                                        {"spk_a", 0.36f},
                                        {"spk_c", 0.20f}};
      phrase.robust_scores = options.phrase_robust_candidate
                                 ? std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                       {"spk_b", 0.48f},
                                       {"spk_a", 0.34f},
                                       {"spk_c", 0.20f}}
                                 : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                       {"spk_c", 0.48f},
                                       {"spk_b", 0.34f},
                                       {"spk_a", 0.20f}};
      evidence.push_back(phrase);

      if (options.include_vad) {
        for (int index = 0; index < options.containing_vad_count; ++index) {
          ComprehensiveTimeline::SpeakerVoiceprintEvidence vad;
          vad.evidence_id = "vad:" + std::to_string(index);
          vad.kind = "vad";
          vad.text_id = -1;
          vad.start = 0.4 - index * 0.02;
          vad.end = 1.9 + index * 0.02;
          vad.embedding_available = true;
          vad.robust_gallery_complete = options.vad_robust_complete;
          vad.session_scores = options.vad_session_eligible
                                   ? std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                         {"spk_b", 0.70f},
                                         {"spk_c", 0.60f},
                                         {"spk_a", 0.20f}}
                                   : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                         {"spk_b", 0.54f},
                                         {"spk_c", 0.48f},
                                         {"spk_a", 0.20f}};
          vad.robust_scores = options.vad_robust_candidate
                                  ? std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                        {"spk_b", 0.68f},
                                        {"spk_c", 0.57f},
                                        {"spk_a", 0.20f}}
                                  : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                        {"spk_c", 0.68f},
                                        {"spk_b", 0.57f},
                                        {"spk_a", 0.20f}};
          evidence.push_back(vad);
        }
      }
      tl.ReplaceVoiceprint(evidence);
      return tl.Snapshot();
    };
    const std::string reason =
        "voiceprint_phrase_vad_dual_gallery_direct_override";
    auto has_exact_override = [&](const std::vector<Entry>& entries) {
      std::string reconstructed;
      int override_count = 0;
      bool exact = false;
      bool outside_preserved = true;
      for (const auto& entry : entries) {
        reconstructed += entry.text;
        if (entry.speaker_decision.reason == reason) {
          ++override_count;
          exact = entry.text == "甲乙。" && entry.speaker_id == "spk_b" &&
                  entry.speaker_decision.speaker_source ==
                      "sortformer_activity+titanet_phrase+vad_session+"
                      "robust_gallery+forced_alignment";
        } else if (entry.text.find("前") != std::string::npos ||
                   entry.text.find("后") != std::string::npos) {
          outside_preserved = outside_preserved &&
                              entry.speaker_id == "spk_a";
        }
      }
      return reconstructed == "前甲乙。后" && override_count == 1 && exact &&
             outside_preserved;
    };
    auto abstains = [&](const Options& options) {
      const auto entries = run_case(options);
      return std::none_of(entries.begin(), entries.end(), [&](const auto& entry) {
        return entry.speaker_decision.reason == reason;
      });
    };

    CHECK(has_exact_override(run_case({})),
          "exact phrase and unique VAD override only the coarse direct range");

    Options missing_interval;
    missing_interval.include_interval = false;
    CHECK(abstains(missing_interval), "missing coarse interval blocks FR34");
    Options duplicate_interval;
    duplicate_interval.duplicate_interval = true;
    CHECK(abstains(duplicate_interval), "duplicate coarse intervals block FR34");
    Options noncontaining_interval;
    noncontaining_interval.interval_contains = false;
    CHECK(abstains(noncontaining_interval),
          "noncontaining coarse interval blocks FR34");
    Options missing_interval_embedding;
    missing_interval_embedding.interval_embedding = false;
    CHECK(abstains(missing_interval_embedding),
          "missing coarse embedding blocks FR34");
    Options incomplete_interval_gallery;
    incomplete_interval_gallery.interval_robust_complete = false;
    CHECK(abstains(incomplete_interval_gallery),
          "incomplete coarse robust gallery blocks FR34");
    Options weak_interval;
    weak_interval.interval_session_eligible = false;
    CHECK(abstains(weak_interval), "coarse score abstention blocks FR34");
    Options disagreeing_interval;
    disagreeing_interval.interval_robust_current = false;
    CHECK(abstains(disagreeing_interval),
          "coarse gallery disagreement blocks FR34");

    Options incomplete_phrase_gallery;
    incomplete_phrase_gallery.phrase_robust_complete = false;
    CHECK(abstains(incomplete_phrase_gallery),
          "incomplete phrase robust gallery blocks FR34");
    Options weak_phrase;
    weak_phrase.phrase_margin_passes = false;
    CHECK(abstains(weak_phrase), "phrase margin abstention blocks FR34");
    Options disagreeing_phrase;
    disagreeing_phrase.phrase_robust_candidate = false;
    CHECK(abstains(disagreeing_phrase),
          "phrase gallery disagreement blocks FR34");

    Options missing_vad;
    missing_vad.include_vad = false;
    CHECK(abstains(missing_vad), "missing containing VAD blocks FR34");
    Options duplicate_vad;
    duplicate_vad.containing_vad_count = 2;
    CHECK(abstains(duplicate_vad), "duplicate containing VAD blocks FR34");
    Options incomplete_vad_gallery;
    incomplete_vad_gallery.vad_robust_complete = false;
    CHECK(abstains(incomplete_vad_gallery),
          "incomplete VAD robust gallery blocks FR34");
    Options weak_vad;
    weak_vad.vad_session_eligible = false;
    CHECK(abstains(weak_vad), "VAD score abstention blocks FR34");
    Options disagreeing_vad;
    disagreeing_vad.vad_robust_candidate = false;
    CHECK(abstains(disagreeing_vad),
          "VAD gallery disagreement blocks FR34");

    Options insufficient_alignment;
    insufficient_alignment.enough_aligned_units = false;
    CHECK(abstains(insufficient_alignment),
          "insufficient phrase alignment blocks FR34");
    Options candidate_gap;
    candidate_gap.candidate_activity_covers = false;
    CHECK(abstains(candidate_gap),
          "incomplete candidate activity coverage blocks FR34");
    Options primary_activity_gap;
    primary_activity_gap.primary_activity_covers = false;
    CHECK(abstains(primary_activity_gap),
          "incomplete primary-identity activity blocks FR34");
    Options extra_activity;
    extra_activity.extra_activity = true;
    CHECK(abstains(extra_activity), "a fourth activity identity blocks FR34");
    Options missing_primary;
    missing_primary.include_primary = false;
    CHECK(abstains(missing_primary), "missing primary conflict blocks FR34");
    Options duplicate_primary;
    duplicate_primary.overlapping_primary_count = 2;
    CHECK(abstains(duplicate_primary),
          "multiple overlapping primary segments block FR34");
    Options candidate_primary;
    candidate_primary.primary_identity = "spk_b";
    CHECK(abstains(candidate_primary),
          "candidate primary identity blocks the three-way conflict");
    Options current_primary;
    current_primary.primary_identity = "spk_a";
    CHECK(abstains(current_primary),
          "coarse current primary identity blocks the three-way conflict");
    Options protected_current;
    protected_current.protected_current_label = true;
    CHECK(abstains(protected_current),
          "a protected current subrange blocks the exact phrase override");
  }

  // ---- 18d. FR16ABN recovers only a delayed subminimum clause group when
  // activity, primary, and a typed VAD gap expose one intervening identity.
  // ----
  {
    using Entry = ComprehensiveTimeline::Entry;
    struct CaseOptions {
      bool regular_duration_group = false;
      bool include_second_unit = true;
      bool include_candidate_primary = true;
      bool candidate_separated = true;
      bool return_within_tolerance = true;
      bool valid_vad_gap = true;
      bool candidate_on_group = false;
      bool exact_embedding_available = false;
      bool competing_native_island = false;
      bool reverse_evidence = false;
    };
    auto run_case = [](const CaseOptions& options) -> std::vector<Entry> {
      BusinessSpeakerPipeline::Config config;
      config.voiceprint_fusion_enabled = true;
      config.speaker_overlap_tie_policy =
          BusinessSpeakerPipeline::SpeakerOverlapTiePolicy::kPrimarySpeaker;
      TestBusinessSpeakerPipeline tl(config);

      const double return_start = options.return_within_tolerance ? 2.94 : 2.80;
      const double candidate_end =
          options.candidate_on_group
              ? 3.04
              : (options.candidate_separated ? 2.00 : 2.80);
      std::vector<TestBusinessSpeakerPipeline::SpeakerInput> activity = {
          {0.0, 1.00, "slot_a", 0.9f, "spk_a"},
          {1.0, candidate_end, "slot_b", 0.9f, "spk_b"},
          {return_start, 4.40, "slot_a", 0.9f, "spk_a"}};
      if (options.competing_native_island) {
        activity.push_back({1.05, 1.85, "slot_c", 0.9f, "spk_c"});
      }
      tl.ReplaceSpeakers(activity);

      std::vector<TestBusinessSpeakerPipeline::SpeakerInput> primary = {
          {0.0, 1.0, "slot_a", 0.9f, "spk_a"},
          {return_start, 4.40, "slot_a", 0.9f, "spk_a"}};
      if (options.include_candidate_primary) {
        primary.push_back({1.02, 2.00, "slot_b", 0.9f, "spk_b"});
      }
      if (options.competing_native_island) {
        primary.push_back({1.08, 1.80, "slot_c", 0.9f, "spk_c"});
      }
      tl.ReplacePrimarySpeakers(primary);

      const std::string source = "甲。嗯，对。乙丙。";
      tl.UpsertText(0, 0.0, 4.40, source);
      const double first_short_end =
          options.regular_duration_group ? 3.25 : 3.08;
      const double second_short_end =
          options.regular_duration_group ? 3.50 : 3.16;
      tl.UpsertAlign(0, 0.0, 4.40,
                     {{0.60, 1.00, "甲"},
                      {3.00, first_short_end, "嗯"},
                      {first_short_end, second_short_end, "对"},
                      {3.70, 4.00, "乙"},
                      {4.00, 4.30, "丙"}});

      auto aligned_unit = [](int ordinal, int source_start, int source_end,
                             double start, double end) {
        ComprehensiveTimeline::SpeakerVoiceprintEvidence item;
        item.evidence_id = "aligned_unit:0:" + std::to_string(ordinal);
        item.kind = "aligned_unit";
        item.text_id = 0;
        item.source_start = source_start;
        item.source_end = source_end;
        item.start = start;
        item.end = end;
        return item;
      };
      std::vector<ComprehensiveTimeline::SpeakerVoiceprintEvidence> evidence = {
          aligned_unit(0, 0, 1, 0.60, 1.00),
          aligned_unit(1, 2, 3, 3.00, first_short_end),
          aligned_unit(3, 6, 7, 3.70, 4.00), aligned_unit(4, 7, 8, 4.00, 4.30)};
      if (options.include_second_unit) {
        evidence.push_back(
            aligned_unit(2, 4, 5, first_short_end, second_short_end));
      }

      ComprehensiveTimeline::SpeakerVoiceprintEvidence direct;
      direct.evidence_id = "business_interval:0:0";
      direct.kind = "business_interval";
      direct.text_id = 0;
      direct.source_start = 2;
      direct.source_end = 9;
      direct.start = 3.00;
      direct.end = 4.30;
      direct.embedding_available = true;
      direct.session_scores = {{"spk_a", 0.75f}, {"spk_b", 0.55f}};
      evidence.push_back(direct);

      auto vad = [](const std::string& id, double start, double end) {
        ComprehensiveTimeline::SpeakerVoiceprintEvidence item;
        item.evidence_id = id;
        item.kind = "vad";
        item.text_id = -1;
        item.start = start;
        item.end = end;
        return item;
      };
      evidence.push_back(vad("vad:0", 0.40, 2.10));
      evidence.push_back(
          vad("vad:1", options.valid_vad_gap ? 2.40 : 2.20, 4.40));

      if (options.exact_embedding_available) {
        ComprehensiveTimeline::SpeakerVoiceprintEvidence phrase;
        phrase.evidence_id = "punctuation_phrase:0:0";
        phrase.kind = "punctuation_phrase";
        phrase.text_id = 0;
        phrase.source_start = 2;
        phrase.source_end = 6;
        phrase.start = 3.00;
        phrase.end = second_short_end;
        phrase.embedding_available = true;
        phrase.robust_gallery_complete = true;
        phrase.session_scores = {{"spk_a", 0.75f}, {"spk_b", 0.55f}};
        phrase.robust_scores = {{"spk_a", 0.73f}, {"spk_b", 0.54f}};
        evidence.push_back(std::move(phrase));
      }
      if (options.reverse_evidence) {
        std::reverse(evidence.begin(), evidence.end());
      }
      tl.ReplaceVoiceprint(evidence);
      return tl.Snapshot();
    };

    const std::string reason =
        "sortformer_delayed_subminimum_clause_group_override";
    auto has_exact_recovery = [&](const std::vector<Entry>& entries) {
      std::string rebuilt;
      bool recovered = false;
      bool following_preserved = false;
      for (const auto& entry : entries) {
        rebuilt += entry.text;
        if (entry.text == "嗯，对。" && entry.speaker_id == "spk_b" &&
            entry.speaker_decision.reason == reason &&
            entry.speaker_decision.speaker_source ==
                "sortformer_activity+primary_top1+vad_boundary+"
                "forced_alignment") {
          recovered = true;
        }
        if (entry.text.find("乙丙") != std::string::npos &&
            entry.speaker_id == "spk_a") {
          following_preserved = true;
        }
      }
      return rebuilt == "甲。嗯，对。乙丙。" && recovered &&
             following_preserved;
    };
    auto abstains = [&](const CaseOptions& options) {
      const auto entries = run_case(options);
      return std::none_of(entries.begin(), entries.end(),
                          [&](const auto& entry) {
                            return entry.speaker_decision.reason == reason;
                          });
    };

    CHECK(has_exact_recovery(run_case({})),
          "a delayed subminimum clause group recovers the intervening native "
          "identity only on the exact source range");
    CaseOptions reversed;
    reversed.reverse_evidence = true;
    CHECK(has_exact_recovery(run_case(reversed)),
          "delayed clause recovery is independent of evidence arrival order");
    CaseOptions regular;
    regular.regular_duration_group = true;
    CHECK(abstains(regular),
          "a regular-duration clause group preserves current attribution");
    CaseOptions one_unit;
    one_unit.include_second_unit = false;
    CHECK(abstains(one_unit),
          "insufficient aligned units preserve current attribution");
    CaseOptions no_primary;
    no_primary.include_candidate_primary = false;
    CHECK(abstains(no_primary),
          "activity-only intervening evidence preserves current attribution");
    CaseOptions unseparated;
    unseparated.candidate_separated = false;
    CHECK(abstains(unseparated),
          "an unseparated native island preserves current attribution");
    CaseOptions late_return;
    late_return.return_within_tolerance = false;
    CHECK(abstains(late_return),
          "an incumbent return outside alignment tolerance abstains");
    CaseOptions no_vad_gap;
    no_vad_gap.valid_vad_gap = false;
    CHECK(abstains(no_vad_gap),
          "a missing configured VAD gap preserves current attribution");
    CaseOptions active_candidate;
    active_candidate.candidate_on_group = true;
    CHECK(abstains(active_candidate),
          "candidate activity on the delayed group preserves attribution");
    CaseOptions embeddable;
    embeddable.exact_embedding_available = true;
    CHECK(abstains(embeddable),
          "an independently embedded exact phrase preserves attribution");
    CaseOptions competing;
    competing.competing_native_island = true;
    CHECK(abstains(competing),
          "ambiguous intervening native identities preserve attribution");
  }

  // ---- 19. A session-only phrase still abstains when a direct interval
  // conflicts because it lacks an independent robust-gallery confirmation. ----
  {
    BusinessSpeakerPipeline::Config config;
    config.voiceprint_fusion_enabled = true;
    TestBusinessSpeakerPipeline tl(config);
    tl.UpsertText(0, 0.0, 1.0, "甲乙");
    tl.ReplaceSpeakers({{0.0, 1.0, "speaker_0", 0.9f, "spk_a"},
                        {1.1, 2.0, "speaker_1", 0.9f, "spk_b"}});
    ComprehensiveTimeline::SpeakerVoiceprintEvidence direct;
    direct.evidence_id = "business_interval:0:0";
    direct.kind = "business_interval";
    direct.text_id = 0;
    direct.source_start = 0;
    direct.source_end = 2;
    direct.start = 0.0;
    direct.end = 1.0;
    direct.embedding_available = true;
    direct.session_scores = {{"spk_a", 0.75f}, {"spk_b", 0.60f}};
    ComprehensiveTimeline::SpeakerVoiceprintEvidence phrase = direct;
    phrase.evidence_id = "punctuation_phrase:0:0";
    phrase.kind = "punctuation_phrase";
    phrase.session_scores = {{"spk_a", 0.55f}, {"spk_b", 0.75f}};
    tl.ReplaceVoiceprint({direct, phrase});
    const auto snap = tl.Snapshot();
    CHECK(snap.size() == 1 && snap[0].speaker_id == "spk_a",
          "session-only conflicting phrase preserves the direct interval");
  }

  // ---- 20. A session-only direct voiceprint cannot overwrite an exact
  // activity tie resolved by the independent primary Sortformer track. ----
  {
    using Pipeline = orator::pipeline::BusinessSpeakerPipeline;
    Pipeline::Config config;
    config.voiceprint_fusion_enabled = true;
    config.speaker_overlap_tie_policy =
        Pipeline::SpeakerOverlapTiePolicy::kPrimarySpeaker;
    TestBusinessSpeakerPipeline tl(config);
    tl.UpsertText(0, 0.0, 1.0, "甲乙");
    tl.ReplaceSpeakers({{0.0, 1.0, "speaker_0", 0.9f, "spk_a"},
                        {0.0, 1.0, "speaker_1", 0.9f, "spk_b"}});
    tl.ReplacePrimarySpeakers(
        {{0.0, 1.0, "speaker_0", 0.9f, "spk_a"}});
    ComprehensiveTimeline::SpeakerVoiceprintEvidence direct;
    direct.evidence_id = "business_interval:0:0";
    direct.kind = "business_interval";
    direct.text_id = 0;
    direct.source_start = 0;
    direct.source_end = 2;
    direct.start = 0.0;
    direct.end = 1.0;
    direct.embedding_available = true;
    direct.session_scores = {{"spk_a", 0.60f}, {"spk_b", 0.75f}};
    tl.ReplaceVoiceprint({direct});
    const auto snap = tl.Snapshot();
    CHECK(snap.size() == 1 && snap[0].speaker_id == "spk_a",
          "session-only direct evidence preserves primary tie arbitration");
  }

  // ---- 20b. A complete phrase may supersede a conflicting coarse direct
  // interval when primary, activity, and both phrase galleries agree. ----
  {
    using Pipeline = orator::pipeline::BusinessSpeakerPipeline;
    Pipeline::Config config;
    config.voiceprint_fusion_enabled = true;
    config.align_snap_pause_sec = 0.0;
    config.speaker_overlap_tie_policy =
        Pipeline::SpeakerOverlapTiePolicy::kPrimarySpeaker;
    TestBusinessSpeakerPipeline tl(config);
    const std::string source = "甲乙。";
    tl.UpsertText(0, 0.0, 1.0, source);
    tl.ReplaceSpeakers({{0.0, 1.0, "speaker_0", 0.9f, "spk_a"},
                        {0.0, 0.6, "speaker_1", 0.9f, "spk_b"}});
    tl.ReplacePrimarySpeakers(
        {{0.0, 0.8, "speaker_1", 0.9f, "spk_b"}});
    tl.UpsertAlign(0, 0.0, 1.0,
                   {{0.0, 0.4, "甲"}, {0.4, 0.8, "乙"}});
    ComprehensiveTimeline::SpeakerVoiceprintEvidence direct;
    direct.evidence_id = "business_interval:0:0";
    direct.kind = "business_interval";
    direct.text_id = 0;
    direct.source_start = 0;
    direct.source_end = 3;
    direct.start = 0.0;
    direct.end = 1.0;
    direct.embedding_available = true;
    direct.session_scores = {{"spk_a", 0.75f}, {"spk_b", 0.60f}};
    ComprehensiveTimeline::SpeakerVoiceprintEvidence phrase = direct;
    phrase.evidence_id = "punctuation_phrase:0:0";
    phrase.kind = "punctuation_phrase";
    phrase.start = 0.0;
    phrase.end = 0.8;
    phrase.robust_gallery_complete = true;
    phrase.session_scores = {{"spk_a", 0.15f}, {"spk_b", 0.27f}};
    phrase.robust_scores = {{"spk_a", 0.17f}, {"spk_b", 0.28f}};
    tl.ReplaceVoiceprint({direct, phrase});
    const auto snap = tl.Snapshot();
    CHECK(!snap.empty() &&
              std::all_of(snap.begin(), snap.end(), [](const auto& entry) {
                return entry.speaker_id == "spk_b";
              }),
          "primary/activity/dual-gallery phrase consensus wins locally");
    CHECK(!snap.empty() &&
              std::all_of(snap.begin(), snap.end(), [](const auto& entry) {
                return entry.speaker_decision.reason ==
                       "voiceprint_phrase_primary_activity_dual_gallery";
              }),
          "three-view phrase consensus exposes its decision reason");
  }

  // ---- 20bb. Short direct and phrase voiceprints spanning two disconnected
  // VAD islands cannot overwrite agreement between both native views. ----
  {
    BusinessSpeakerPipeline::Config config;
    config.voiceprint_fusion_enabled = true;
    config.align_snap_pause_sec = 0.0;
    TestBusinessSpeakerPipeline tl(config);
    const std::string source = "甲乙。";
    tl.UpsertText(0, 0.0, 1.0, source);
    tl.ReplaceSpeakers({{0.0, 1.0, "speaker_0", 0.9f, "spk_a"}});
    tl.ReplacePrimarySpeakers(
        {{0.0, 1.0, "speaker_0", 0.9f, "spk_a"}});
    tl.UpsertAlign(0, 0.0, 1.0,
                   {{0.0, 0.4, "甲"}, {0.4, 1.0, "乙"}});
    ComprehensiveTimeline::SpeakerVoiceprintEvidence direct;
    direct.evidence_id = "business_interval:0:0";
    direct.kind = "business_interval";
    direct.text_id = 0;
    direct.source_start = 0;
    direct.source_end = 3;
    direct.start = 0.0;
    direct.end = 1.0;
    direct.embedding_available = true;
    direct.session_scores = {{"spk_a", 0.60f}, {"spk_b", 0.75f}};
    ComprehensiveTimeline::SpeakerVoiceprintEvidence phrase = direct;
    phrase.evidence_id = "punctuation_phrase:0:0";
    phrase.kind = "punctuation_phrase";
    phrase.robust_gallery_complete = true;
    phrase.robust_scores = {{"spk_a", 0.59f}, {"spk_b", 0.74f}};
    ComprehensiveTimeline::SpeakerVoiceprintEvidence vad_a;
    vad_a.evidence_id = "vad:0";
    vad_a.kind = "vad";
    vad_a.text_id = -1;
    vad_a.start = 0.0;
    vad_a.end = 0.45;
    ComprehensiveTimeline::SpeakerVoiceprintEvidence vad_b = vad_a;
    vad_b.evidence_id = "vad:1";
    vad_b.start = 0.60;
    vad_b.end = 1.10;
    tl.ReplaceVoiceprint({direct, phrase, vad_a, vad_b});
    const auto snap = tl.Snapshot();
    CHECK(!snap.empty() &&
              std::all_of(snap.begin(), snap.end(), [](const auto& entry) {
                return entry.speaker_id == "spk_a";
              }),
          "native dual-view consensus preserves the current identity");
  }

  // ---- 20bc. One VAD interval containing the query leaves ordinary
  // voiceprint policy available even when both native views agree. ----
  {
    BusinessSpeakerPipeline::Config config;
    config.voiceprint_fusion_enabled = true;
    TestBusinessSpeakerPipeline tl(config);
    tl.UpsertText(0, 0.0, 1.0, "甲乙");
    tl.ReplaceSpeakers({{0.0, 1.0, "speaker_0", 0.9f, "spk_a"}});
    tl.ReplacePrimarySpeakers(
        {{0.0, 1.0, "speaker_0", 0.9f, "spk_a"}});
    ComprehensiveTimeline::SpeakerVoiceprintEvidence direct;
    direct.evidence_id = "business_interval:0:0";
    direct.kind = "business_interval";
    direct.text_id = 0;
    direct.source_start = 0;
    direct.source_end = 2;
    direct.start = 0.0;
    direct.end = 1.0;
    direct.embedding_available = true;
    direct.session_scores = {{"spk_a", 0.60f}, {"spk_b", 0.75f}};
    ComprehensiveTimeline::SpeakerVoiceprintEvidence vad;
    vad.evidence_id = "vad:0";
    vad.kind = "vad";
    vad.text_id = -1;
    vad.start = -0.1;
    vad.end = 1.1;
    tl.ReplaceVoiceprint({direct, vad});
    const auto snap = tl.Snapshot();
    CHECK(snap.size() == 1 && snap[0].speaker_id == "spk_b",
          "one containing VAD leaves direct voiceprint available");
  }

  // ---- 20bd. Competing primary coverage prevents fragmented-VAD native
  // consensus from being synthesized. ----
  {
    BusinessSpeakerPipeline::Config config;
    config.voiceprint_fusion_enabled = true;
    TestBusinessSpeakerPipeline tl(config);
    tl.UpsertText(0, 0.0, 1.0, "甲乙");
    tl.ReplaceSpeakers({{0.0, 1.0, "speaker_0", 0.9f, "spk_a"}});
    tl.ReplacePrimarySpeakers(
        {{0.0, 1.0, "speaker_0", 0.9f, "spk_a"},
         {0.4, 0.6, "speaker_1", 0.8f, "spk_b"}});
    ComprehensiveTimeline::SpeakerVoiceprintEvidence direct;
    direct.evidence_id = "business_interval:0:0";
    direct.kind = "business_interval";
    direct.text_id = 0;
    direct.source_start = 0;
    direct.source_end = 2;
    direct.start = 0.0;
    direct.end = 1.0;
    direct.embedding_available = true;
    direct.session_scores = {{"spk_a", 0.60f}, {"spk_b", 0.75f}};
    ComprehensiveTimeline::SpeakerVoiceprintEvidence vad_a;
    vad_a.evidence_id = "vad:0";
    vad_a.kind = "vad";
    vad_a.text_id = -1;
    vad_a.start = 0.0;
    vad_a.end = 0.45;
    ComprehensiveTimeline::SpeakerVoiceprintEvidence vad_b = vad_a;
    vad_b.evidence_id = "vad:1";
    vad_b.start = 0.60;
    vad_b.end = 1.10;
    tl.ReplaceVoiceprint({direct, vad_a, vad_b});
    const auto snap = tl.Snapshot();
    CHECK(snap.size() == 1 && snap[0].speaker_id == "spk_b",
          "competing primary coverage leaves direct voiceprint available");
  }

  // ---- 20c. Incomplete primary coverage makes the same low-score phrase
  // abstain, so it cannot expand from a partial primary run. ----
  {
    using Pipeline = orator::pipeline::BusinessSpeakerPipeline;
    Pipeline::Config config;
    config.voiceprint_fusion_enabled = true;
    config.align_snap_pause_sec = 0.0;
    config.speaker_overlap_tie_policy =
        Pipeline::SpeakerOverlapTiePolicy::kPrimarySpeaker;
    TestBusinessSpeakerPipeline tl(config);
    tl.UpsertText(0, 0.0, 1.0, "甲乙。");
    tl.ReplaceSpeakers({{0.0, 1.0, "speaker_0", 0.9f, "spk_a"},
                        {0.0, 0.6, "speaker_1", 0.9f, "spk_b"}});
    tl.ReplacePrimarySpeakers(
        {{0.0, 0.3, "speaker_1", 0.9f, "spk_b"}});
    tl.UpsertAlign(0, 0.0, 1.0,
                   {{0.0, 0.4, "甲"}, {0.4, 0.8, "乙"}});
    ComprehensiveTimeline::SpeakerVoiceprintEvidence direct;
    direct.evidence_id = "business_interval:0:0";
    direct.kind = "business_interval";
    direct.text_id = 0;
    direct.source_start = 0;
    direct.source_end = 3;
    direct.start = 0.0;
    direct.end = 1.0;
    direct.embedding_available = true;
    direct.session_scores = {{"spk_a", 0.75f}, {"spk_b", 0.60f}};
    ComprehensiveTimeline::SpeakerVoiceprintEvidence phrase = direct;
    phrase.evidence_id = "punctuation_phrase:0:0";
    phrase.kind = "punctuation_phrase";
    phrase.start = 0.0;
    phrase.end = 0.8;
    phrase.robust_gallery_complete = true;
    phrase.session_scores = {{"spk_a", 0.15f}, {"spk_b", 0.27f}};
    phrase.robust_scores = {{"spk_a", 0.17f}, {"spk_b", 0.28f}};
    tl.ReplaceVoiceprint({direct, phrase});
    const auto snap = tl.Snapshot();
    CHECK(std::any_of(snap.begin(), snap.end(), [](const auto& entry) {
            return entry.speaker_id == "spk_a";
          }),
          "incomplete primary coverage preserves the direct attribution");
  }

  // ---- 20d. A medium complete phrase may challenge a dynamic slot identity
  // when the initial slot mapping, complete activity, and both galleries agree.
  // The regular absolute score remains below its gate. ----
  {
    BusinessSpeakerPipeline::Config config;
    config.voiceprint_fusion_enabled = true;
    config.align_snap_pause_sec = 0.0;
    TestBusinessSpeakerPipeline tl(config);
    const std::string source = "甲乙。";
    tl.UpsertText(0, 0.0, 1.8, source);
    tl.ReplaceSpeakers({{-2.0, -1.0, "slot_0", 0.9f, "spk_b"},
                        {0.0, 1.8, "slot_0", 0.9f, "spk_a"}});
    tl.UpsertAlign(0, 0.0, 1.8,
                   {{0.0, 0.8, "甲"}, {0.8, 1.8, "乙"}});
    ComprehensiveTimeline::SpeakerVoiceprintEvidence direct;
    direct.evidence_id = "business_interval:0:0";
    direct.kind = "business_interval";
    direct.text_id = 0;
    direct.source_start = 0;
    direct.source_end = 3;
    direct.start = 0.0;
    direct.end = 1.8;
    direct.embedding_available = true;
    direct.session_scores = {{"spk_a", 0.75f}, {"spk_b", 0.60f}};
    ComprehensiveTimeline::SpeakerVoiceprintEvidence phrase = direct;
    phrase.evidence_id = "punctuation_phrase:0:0";
    phrase.kind = "punctuation_phrase";
    phrase.robust_gallery_complete = true;
    phrase.session_scores = {{"spk_a", 0.40f}, {"spk_b", 0.49f}};
    phrase.robust_scores = {{"spk_a", 0.39f}, {"spk_b", 0.48f}};
    tl.ReplaceVoiceprint({direct, phrase});
    const auto snap = tl.Snapshot();
    CHECK(!snap.empty() &&
              std::all_of(snap.begin(), snap.end(), [](const auto& entry) {
                return entry.speaker_id == "spk_b";
              }),
          "initial-slot and dual-gallery agreement challenges the dynamic id");
    CHECK(!snap.empty() &&
              std::all_of(snap.begin(), snap.end(), [](const auto& entry) {
                return entry.speaker_decision.reason ==
                       "voiceprint_phrase_initial_slot_dual_gallery_override";
              }),
          "initial-slot phrase challenge exposes its typed audit reason");
  }

  // ---- 20e. Partial activity coverage cannot support the initial-slot
  // challenge even when both phrase galleries agree. ----
  {
    BusinessSpeakerPipeline::Config config;
    config.voiceprint_fusion_enabled = true;
    config.align_snap_pause_sec = 0.0;
    TestBusinessSpeakerPipeline tl(config);
    tl.UpsertText(0, 0.0, 1.8, "甲乙。");
    tl.ReplaceSpeakers({{-2.0, -1.0, "slot_0", 0.9f, "spk_b"},
                        {0.0, 1.6, "slot_0", 0.9f, "spk_a"}});
    tl.UpsertAlign(0, 0.0, 1.8,
                   {{0.0, 0.8, "甲"}, {0.8, 1.8, "乙"}});
    ComprehensiveTimeline::SpeakerVoiceprintEvidence direct;
    direct.evidence_id = "business_interval:0:0";
    direct.kind = "business_interval";
    direct.text_id = 0;
    direct.source_start = 0;
    direct.source_end = 3;
    direct.start = 0.0;
    direct.end = 1.8;
    direct.embedding_available = true;
    direct.session_scores = {{"spk_a", 0.75f}, {"spk_b", 0.60f}};
    ComprehensiveTimeline::SpeakerVoiceprintEvidence phrase = direct;
    phrase.evidence_id = "punctuation_phrase:0:0";
    phrase.kind = "punctuation_phrase";
    phrase.robust_gallery_complete = true;
    phrase.session_scores = {{"spk_a", 0.40f}, {"spk_b", 0.49f}};
    phrase.robust_scores = {{"spk_a", 0.39f}, {"spk_b", 0.48f}};
    tl.ReplaceVoiceprint({direct, phrase});
    const auto snap = tl.Snapshot();
    CHECK(!snap.empty() &&
              std::all_of(snap.begin(), snap.end(), [](const auto& entry) {
                return entry.speaker_id == "spk_a";
              }),
          "partial activity coverage preserves the dynamic identity");
  }

  // ---- 20f. One margin-only abstention may be resolved when the other three
  // phrase/VAD views pass unchanged gates and all four top-rank one identity.
  // ----
  {
    BusinessSpeakerPipeline::Config config;
    config.voiceprint_fusion_enabled = true;
    config.align_snap_pause_sec = 0.0;
    TestBusinessSpeakerPipeline tl(config);
    tl.UpsertText(0, 0.0, 0.8, "甲乙。");
    tl.ReplaceSpeakers({{0.0, 1.0, "slot_0", 0.9f, "spk_a"}});
    tl.ReplacePrimarySpeakers(
        {{0.02, 0.82, "slot_0", 0.9f, "spk_a"}});
    tl.UpsertAlign(0, 0.0, 0.8,
                   {{0.0, 0.4, "甲"}, {0.4, 0.8, "乙"}});
    ComprehensiveTimeline::SpeakerVoiceprintEvidence direct;
    direct.evidence_id = "business_interval:0:0";
    direct.kind = "business_interval";
    direct.text_id = 0;
    direct.source_start = 0;
    direct.source_end = 3;
    direct.start = 0.0;
    direct.end = 0.8;
    direct.embedding_available = true;
    direct.session_scores = {{"spk_a", 0.75f}, {"spk_b", 0.60f}};
    ComprehensiveTimeline::SpeakerVoiceprintEvidence phrase = direct;
    phrase.evidence_id = "punctuation_phrase:0:0";
    phrase.kind = "punctuation_phrase";
    phrase.robust_gallery_complete = true;
    phrase.session_scores = {{"spk_a", 0.24f}, {"spk_b", 0.27f}};
    phrase.robust_scores = {{"spk_a", 0.25f}, {"spk_b", 0.35f}};
    ComprehensiveTimeline::SpeakerVoiceprintEvidence vad;
    vad.evidence_id = "vad:0";
    vad.kind = "vad";
    vad.text_id = -1;
    vad.start = 0.0;
    vad.end = 1.0;
    vad.embedding_available = true;
    vad.robust_gallery_complete = true;
    vad.session_scores = {{"spk_a", 0.30f}, {"spk_b", 0.45f}};
    vad.robust_scores = {{"spk_a", 0.30f}, {"spk_b", 0.46f}};
    tl.ReplaceVoiceprint({direct, phrase, vad});
    const auto snap = tl.Snapshot();
    CHECK(!snap.empty() &&
              std::all_of(snap.begin(), snap.end(), [](const auto& entry) {
                return entry.speaker_id == "spk_b";
              }),
          "four agreeing views resolve one margin-only abstention");
    CHECK(!snap.empty() &&
              std::all_of(snap.begin(), snap.end(), [](const auto& entry) {
                return entry.speaker_decision.reason ==
                       "voiceprint_phrase_vad_four_view_margin_override";
              }),
          "four-view challenge exposes its typed audit reason");
  }

  // ---- 20g. Two margin-only abstentions preserve the current attribution.
  // ----
  {
    BusinessSpeakerPipeline::Config config;
    config.voiceprint_fusion_enabled = true;
    config.align_snap_pause_sec = 0.0;
    TestBusinessSpeakerPipeline tl(config);
    tl.UpsertText(0, 0.0, 0.8, "甲乙。");
    tl.ReplaceSpeakers({{0.0, 1.0, "slot_0", 0.9f, "spk_a"}});
    tl.ReplacePrimarySpeakers(
        {{0.02, 0.82, "slot_0", 0.9f, "spk_a"}});
    tl.UpsertAlign(0, 0.0, 0.8,
                   {{0.0, 0.4, "甲"}, {0.4, 0.8, "乙"}});
    ComprehensiveTimeline::SpeakerVoiceprintEvidence direct;
    direct.evidence_id = "business_interval:0:0";
    direct.kind = "business_interval";
    direct.text_id = 0;
    direct.source_start = 0;
    direct.source_end = 3;
    direct.start = 0.0;
    direct.end = 0.8;
    direct.embedding_available = true;
    direct.session_scores = {{"spk_a", 0.75f}, {"spk_b", 0.60f}};
    ComprehensiveTimeline::SpeakerVoiceprintEvidence phrase = direct;
    phrase.evidence_id = "punctuation_phrase:0:0";
    phrase.kind = "punctuation_phrase";
    phrase.robust_gallery_complete = true;
    phrase.session_scores = {{"spk_a", 0.24f}, {"spk_b", 0.27f}};
    phrase.robust_scores = {{"spk_a", 0.25f}, {"spk_b", 0.35f}};
    ComprehensiveTimeline::SpeakerVoiceprintEvidence vad;
    vad.evidence_id = "vad:0";
    vad.kind = "vad";
    vad.text_id = -1;
    vad.start = 0.0;
    vad.end = 1.0;
    vad.embedding_available = true;
    vad.robust_gallery_complete = true;
    vad.session_scores = {{"spk_a", 0.30f}, {"spk_b", 0.45f}};
    vad.robust_scores = {{"spk_a", 0.30f}, {"spk_b", 0.33f}};
    tl.ReplaceVoiceprint({direct, phrase, vad});
    const auto snap = tl.Snapshot();
    CHECK(!snap.empty() &&
              std::all_of(snap.begin(), snap.end(), [](const auto& entry) {
                return entry.speaker_id == "spk_a";
              }),
          "two margin-only abstentions preserve the current identity");
  }

  // ---- 20h. Conflicting aligned-unit voiceprint cannot overwrite exact
  // primary arbitration inside an activity overlap. ----
  {
    using Pipeline = orator::pipeline::BusinessSpeakerPipeline;
    Pipeline::Config config;
    config.voiceprint_fusion_enabled = true;
    config.speaker_overlap_tie_policy =
        Pipeline::SpeakerOverlapTiePolicy::kPrimarySpeaker;
    TestBusinessSpeakerPipeline tl(config);
    tl.UpsertText(0, 0.0, 1.0, "甲");
    tl.ReplaceSpeakers({{0.0, 1.0, "speaker_0", 0.9f, "spk_a"},
                        {0.0, 1.0, "speaker_1", 0.9f, "spk_b"}});
    tl.ReplacePrimarySpeakers(
        {{0.0, 1.0, "speaker_0", 0.9f, "spk_a"}});
    tl.UpsertAlign(0, 0.0, 1.0, {{0.0, 1.0, "甲"}});
    ComprehensiveTimeline::SpeakerVoiceprintEvidence unit;
    unit.evidence_id = "aligned_unit:0:0";
    unit.kind = "aligned_unit";
    unit.text_id = 0;
    unit.source_start = 0;
    unit.source_end = 1;
    unit.start = 0.0;
    unit.end = 1.0;
    unit.embedding_available = true;
    unit.robust_gallery_complete = true;
    unit.session_scores = {{"spk_a", 0.55f}, {"spk_b", 0.75f}};
    unit.robust_scores = {{"spk_a", 0.54f}, {"spk_b", 0.73f}};
    tl.ReplaceVoiceprint({unit});
    const auto snap = tl.Snapshot();
    CHECK(snap.size() == 1 && snap[0].speaker_id == "spk_a" &&
              snap[0].speaker_decision.reason ==
                  "primary_speaker_tie_break",
          "conflicting aligned voiceprint preserves primary arbitration");
  }

  // ---- 20g4. FR16ABA: an exact short phrase may recover the sole local slot's
  // initial identity from two margin-only phrase galleries when the current
  // label came from a coarser direct write. ----
  {
    using Entry = ComprehensiveTimeline::Entry;
    auto run_case = [](bool include_epoch, bool include_direct,
                       bool competing_activity, bool passing_margin,
                       bool gallery_agreement,
                       bool enough_aligned_units) -> std::vector<Entry> {
      BusinessSpeakerPipeline::Config config;
      config.voiceprint_fusion_enabled = true;
      config.align_snap_pause_sec = 0.0;
      TestBusinessSpeakerPipeline tl(config);
      std::vector<TestBusinessSpeakerPipeline::SpeakerInput> activity;
      if (include_epoch) {
        activity.push_back({-2.0, -1.0, "slot_0", 0.9f, "spk_b"});
      }
      activity.push_back({0.0, 1.0, "slot_0", 0.9f, "spk_a"});
      if (competing_activity) {
        activity.push_back({0.2, 0.8, "slot_1", 0.9f, "spk_c"});
      }
      tl.ReplaceSpeakers(activity);
      tl.ReplacePrimarySpeakers({{0.0, 1.0, "slot_0", 0.9f, "spk_a"}});
      tl.UpsertText(0, 0.0, 0.8, "前，甲乙。");
      if (enough_aligned_units) {
        tl.UpsertAlign(0, 0.0, 0.8,
                       {{0.0, 0.2, "前"},
                        {0.2, 0.5, "甲"},
                        {0.5, 0.8, "乙"}});
      } else {
        tl.UpsertAlign(0, 0.0, 0.8,
                       {{0.0, 0.2, "前"}, {0.2, 0.8, "甲乙"}});
      }

      std::vector<ComprehensiveTimeline::SpeakerVoiceprintEvidence> evidence;
      if (include_direct) {
        ComprehensiveTimeline::SpeakerVoiceprintEvidence direct;
        direct.evidence_id = "business_interval:0:0";
        direct.kind = "business_interval";
        direct.text_id = 0;
        direct.source_start = 0;
        direct.source_end = 5;
        direct.start = 0.0;
        direct.end = 0.8;
        direct.embedding_available = true;
        direct.session_scores = {{"spk_a", 0.60f}, {"spk_b", 0.20f}};
        evidence.push_back(direct);
      }
      ComprehensiveTimeline::SpeakerVoiceprintEvidence phrase;
      phrase.evidence_id = "punctuation_phrase:0:0";
      phrase.kind = "punctuation_phrase";
      phrase.text_id = 0;
      phrase.source_start = 2;
      phrase.source_end = 5;
      phrase.start = 0.2;
      phrase.end = 0.8;
      phrase.embedding_available = true;
      phrase.robust_gallery_complete = true;
      phrase.session_scores = passing_margin
                                  ? std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                        {"spk_a", 0.40f}, {"spk_b", 0.48f}}
                                  : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                        {"spk_a", 0.43f}, {"spk_b", 0.45f}};
      phrase.robust_scores = gallery_agreement
                                 ? std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                       {"spk_a", 0.42f}, {"spk_b", 0.44f}}
                                 : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                       {"spk_a", 0.45f}, {"spk_b", 0.43f}};
      evidence.push_back(phrase);
      tl.ReplaceVoiceprint(evidence);
      return tl.Snapshot();
    };

    auto has_phrase = [](const std::vector<Entry>& entries,
                         const std::string& speaker_id,
                         const std::string& reason = "") {
      return std::any_of(entries.begin(), entries.end(), [&](const auto& entry) {
        return entry.text.find("甲") != std::string::npos &&
               entry.speaker_id == speaker_id &&
               (reason.empty() || entry.speaker_decision.reason == reason);
      });
    };
    CHECK(has_phrase(run_case(true, true, false, false, true, true), "spk_b",
                     "voiceprint_phrase_short_initial_slot_direct_override"),
          "short exact phrase recovers the initial slot identity");
    CHECK(has_phrase(run_case(false, true, false, false, true, true), "spk_a"),
          "missing identity epoch preserves coarse direct attribution");
    CHECK(has_phrase(run_case(true, false, false, false, true, true), "spk_a"),
          "non-direct current label preserves current attribution");
    CHECK(has_phrase(run_case(true, true, true, false, true, true), "spk_a"),
          "competing activity slot preserves coarse direct attribution");
    CHECK(has_phrase(run_case(true, true, false, true, true, true), "spk_a"),
          "passing exact-phrase margin preserves coarse direct attribution");
    CHECK(has_phrase(run_case(true, true, false, false, false, true), "spk_a"),
          "phrase gallery disagreement preserves coarse direct attribution");
    CHECK(has_phrase(run_case(true, true, false, false, true, false), "spk_a"),
          "insufficient exact alignment preserves coarse direct attribution");
  }

  // ---- 20g5. FR16ABB/FR35: an isolated subminimum aligned unit without its
  // own embedding may recover one initial slot identity only from uncontested
  // native tracks and a passing dual-gallery containing VAD. Alignment
  // boundary jitter may consume only the configured split tolerance. ----
  {
    using Entry = ComprehensiveTimeline::Entry;
    auto run_case = [](bool include_epoch, bool isolated, bool strong_vad,
                       bool gallery_agreement, bool primary_current,
                       bool unit_embedding_available,
                       double following_unit_start = 1.5,
                       double boundary_tolerance = 0.08)
        -> std::vector<Entry> {
      BusinessSpeakerPipeline::Config config;
      config.voiceprint_fusion_enabled = true;
      config.align_boundary_split_tolerance_sec = boundary_tolerance;
      config.speaker_overlap_tie_policy =
          BusinessSpeakerPipeline::SpeakerOverlapTiePolicy::kPrimarySpeaker;
      TestBusinessSpeakerPipeline tl(config);
      std::vector<TestBusinessSpeakerPipeline::SpeakerInput> activity;
      if (include_epoch) {
        activity.push_back({-2.0, -1.0, "slot_0", 0.9f, "spk_b"});
      }
      activity.push_back({0.0, 2.0, "slot_0", 0.9f, "spk_a"});
      tl.ReplaceSpeakers(activity);
      tl.ReplacePrimarySpeakers(
          {{0.0, 2.0, "slot_0", 0.9f,
            primary_current ? "spk_a" : "spk_c"}});
      tl.UpsertText(0, 0.0, 2.0, "前嗯后");
      tl.UpsertAlign(0, 0.0, 2.0,
                     {{0.0, isolated ? 0.5 : 0.9, "前"},
                      {1.0, 1.1, "嗯"},
                      {following_unit_start, 2.0, "后"}});

      ComprehensiveTimeline::SpeakerVoiceprintEvidence unit;
      unit.evidence_id = "aligned_unit:0:1";
      unit.kind = "aligned_unit";
      unit.text_id = 0;
      unit.source_start = 1;
      unit.source_end = 2;
      unit.start = 1.0;
      unit.end = 1.1;
      unit.embedding_available = unit_embedding_available;

      ComprehensiveTimeline::SpeakerVoiceprintEvidence vad;
      vad.evidence_id = "vad:0";
      vad.kind = "vad";
      vad.text_id = -1;
      vad.start = 0.8;
      vad.end = 1.3;
      vad.embedding_available = true;
      vad.robust_gallery_complete = true;
      vad.session_scores = strong_vad
                               ? std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                     {"spk_a", 0.40f}, {"spk_b", 0.60f}}
                               : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                     {"spk_a", 0.48f}, {"spk_b", 0.50f}};
      vad.robust_scores = gallery_agreement
                              ? std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                    {"spk_a", 0.39f}, {"spk_b", 0.61f}}
                              : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                    {"spk_a", 0.61f}, {"spk_b", 0.39f}};
      tl.ReplaceVoiceprint({unit, vad});
      return tl.Snapshot();
    };

    auto has_unit = [](const std::vector<Entry>& entries,
                       const std::string& speaker_id,
                       const std::string& reason = "") {
      return std::any_of(entries.begin(), entries.end(), [&](const auto& entry) {
        return entry.text.find("嗯") != std::string::npos &&
               entry.speaker_id == speaker_id &&
               (reason.empty() || entry.speaker_decision.reason == reason);
      });
    };
    CHECK(has_unit(run_case(true, true, true, true, true, false), "spk_b",
                   "voiceprint_aligned_unit_isolated_initial_slot_vad_"
                   "override"),
          "isolated no-embedding unit recovers the initial slot identity");
    CHECK(has_unit(run_case(true, true, true, true, true, false, 1.34, 0.08),
                   "spk_b",
                   "voiceprint_aligned_unit_isolated_initial_slot_vad_"
                   "override"),
          "configured boundary tolerance preserves aligned isolation");
    CHECK(has_unit(run_case(true, true, true, true, true, false, 1.25, 0.08),
                   "spk_a"),
          "gap outside boundary tolerance preserves the native identity");
    CHECK(has_unit(run_case(true, true, true, true, true, false, 1.34, 0.0),
                   "spk_a"),
          "zero boundary tolerance preserves the exact pause contract");
    CHECK(has_unit(run_case(false, true, true, true, true, false), "spk_a"),
          "missing initial epoch preserves the native identity");
    CHECK(has_unit(run_case(true, false, true, true, true, false), "spk_a"),
          "insufficient aligned pause preserves the native identity");
    CHECK(has_unit(run_case(true, true, false, true, true, false), "spk_a"),
          "weak containing VAD preserves the native identity");
    CHECK(has_unit(run_case(true, true, true, false, true, false), "spk_a"),
          "disagreeing VAD galleries preserve the native identity");
    CHECK(has_unit(run_case(true, true, true, true, false, false), "spk_a"),
          "primary mismatch preserves the native identity");
    CHECK(has_unit(run_case(true, true, true, true, true, true), "spk_a"),
          "an available unit embedding stays on the ordinary evidence path");
  }

  // ---- 20g5b. FR36: a regular native phrase may recover one initial slot
  // identity only when exact phrase evidence and both outer acoustic scales
  // expose the complete partition-invariant reversal. ----
  {
    using Entry = ComprehensiveTimeline::Entry;
    struct CaseOptions {
      bool include_initial_identity = true;
      bool primary_matches_activity = true;
      bool competing_activity = false;
      bool enough_alignment = true;
      bool phrase_second_is_initial = true;
      bool exactly_one_phrase_margin = true;
      int vad_count = 1;
      bool vad_initial_first = true;
      bool vad_outer_abstention = true;
      int complete_source_count = 1;
      bool complete_source_initial_first = true;
      bool complete_source_outer_abstention = true;
      bool protected_labels = false;
    };
    auto run_case = [](const CaseOptions& options) -> std::vector<Entry> {
      BusinessSpeakerPipeline::Config config;
      config.voiceprint_fusion_enabled = true;
      config.speaker_overlap_tie_policy =
          BusinessSpeakerPipeline::SpeakerOverlapTiePolicy::kPrimarySpeaker;
      TestBusinessSpeakerPipeline tl(config);

      std::vector<TestBusinessSpeakerPipeline::SpeakerInput> activity;
      if (options.include_initial_identity) {
        activity.push_back({-2.0, -1.0, "slot_0", 0.9f, "spk_b"});
      }
      activity.push_back({0.0, 2.0, "slot_0", 0.9f, "spk_a"});
      if (options.competing_activity) {
        activity.push_back({0.0, 1.8, "slot_1", 0.8f, "spk_c"});
      }
      tl.ReplaceSpeakers(activity);
      tl.ReplacePrimarySpeakers(
          {{0.0, 2.0, "slot_0", 0.9f,
            options.primary_matches_activity ? "spk_a" : "spk_c"}});
      tl.UpsertText(0, 0.0, 2.0, "甲乙丙。");
      tl.UpsertAlign(
          0, 0.0, 2.0,
          options.enough_alignment
              ? std::vector<TestBusinessSpeakerPipeline::AlignUnitSeg>{
                    {0.0, 0.6, "甲"}, {0.6, 1.2, "乙"}, {1.2, 1.8, "丙"}}
              : std::vector<TestBusinessSpeakerPipeline::AlignUnitSeg>{
                    {0.0, 1.8, "甲乙丙"}});

      ComprehensiveTimeline::SpeakerVoiceprintEvidence phrase;
      phrase.evidence_id = "punctuation_phrase:0:0";
      phrase.kind = "punctuation_phrase";
      phrase.text_id = 0;
      phrase.source_start = 0;
      phrase.source_end = 4;
      phrase.start = 0.0;
      phrase.end = 1.8;
      phrase.embedding_available = true;
      phrase.robust_gallery_complete = true;
      phrase.session_scores = options.phrase_second_is_initial
                                  ? std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                        {"spk_a", 0.50f}, {"spk_b", 0.44f},
                                        {"spk_c", 0.20f}}
                                  : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                        {"spk_a", 0.50f}, {"spk_c", 0.44f},
                                        {"spk_b", 0.20f}};
      phrase.robust_scores = options.exactly_one_phrase_margin
                                 ? std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                       {"spk_a", 0.49f}, {"spk_b", 0.47f},
                                       {"spk_c", 0.20f}}
                                 : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                       {"spk_a", 0.50f}, {"spk_b", 0.44f},
                                       {"spk_c", 0.20f}};

      auto outer = [](const std::string& id, const std::string& kind,
                      double start, double end, bool initial_first,
                      bool abstains) {
        ComprehensiveTimeline::SpeakerVoiceprintEvidence evidence;
        evidence.evidence_id = id;
        evidence.kind = kind;
        evidence.text_id = kind == "complete_source" ? 0 : -1;
        evidence.source_start = 0;
        evidence.source_end = kind == "complete_source" ? 4 : 0;
        evidence.start = start;
        evidence.end = end;
        evidence.embedding_available = true;
        evidence.robust_gallery_complete = true;
        const std::string first = initial_first ? "spk_b" : "spk_a";
        const std::string second = initial_first ? "spk_a" : "spk_b";
        evidence.session_scores =
            abstains
                ? std::vector<ComprehensiveTimeline::VoiceprintScore>{
                      {first, 0.50f}, {second, 0.48f}, {"spk_c", 0.20f}}
                : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                      {first, 0.70f}, {second, 0.50f}, {"spk_c", 0.20f}};
        evidence.robust_scores =
            abstains
                ? std::vector<ComprehensiveTimeline::VoiceprintScore>{
                      {first, 0.49f}, {second, 0.48f}, {"spk_c", 0.20f}}
                : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                      {first, 0.69f}, {second, 0.49f}, {"spk_c", 0.20f}};
        return evidence;
      };

      std::vector<ComprehensiveTimeline::SpeakerVoiceprintEvidence> evidence = {
          phrase};
      for (int index = 0; index < options.vad_count; ++index) {
        evidence.push_back(outer("vad:" + std::to_string(index), "vad", -0.1,
                                 1.9, options.vad_initial_first,
                                 options.vad_outer_abstention));
      }
      for (int index = 0; index < options.complete_source_count; ++index) {
        evidence.push_back(outer("complete_source:" + std::to_string(index),
                                 "complete_source", -0.2, 2.0,
                                 options.complete_source_initial_first,
                                 options.complete_source_outer_abstention));
      }
      if (options.protected_labels) {
        ComprehensiveTimeline::SpeakerVoiceprintEvidence direct = phrase;
        direct.evidence_id = "business_interval:0:0";
        direct.kind = "business_interval";
        direct.session_scores = {{"spk_a", 0.75f}, {"spk_b", 0.50f}};
        direct.robust_scores = {{"spk_a", 0.74f}, {"spk_b", 0.49f}};
        evidence.push_back(direct);
      }
      tl.ReplaceVoiceprint(evidence);
      return tl.Snapshot();
    };

    const std::string reason =
        "voiceprint_phrase_partition_invariant_regular_initial_slot_override";
    auto selected = [&](const std::vector<Entry>& entries) {
      return !entries.empty() &&
             std::all_of(entries.begin(), entries.end(), [&](const auto& entry) {
               return entry.speaker_id == "spk_b" &&
                      entry.speaker_decision.reason == reason;
             });
    };
    auto abstained = [&](const std::vector<Entry>& entries) {
      return std::none_of(entries.begin(), entries.end(), [&](const auto& entry) {
        return entry.speaker_decision.reason == reason;
      });
    };

    const auto positive = run_case({});
    CHECK(selected(positive),
          "six-view regular reversal recovers the initial slot identity");
    CHECK(!positive.empty() &&
              positive.front().speaker_decision.speaker_source ==
                  "sortformer_initial_slot+activity+primary_top1+"
                  "titanet_phrase+titanet_vad+titanet_complete_source+"
                  "robust_gallery+forced_alignment",
          "regular initial-slot reversal exposes every contributing track");

    CaseOptions missing_initial;
    missing_initial.include_initial_identity = false;
    CHECK(abstained(run_case(missing_initial)),
          "missing initial identity preserves ordinary attribution");
    CaseOptions primary_mismatch;
    primary_mismatch.primary_matches_activity = false;
    CHECK(abstained(run_case(primary_mismatch)),
          "activity and primary slot disagreement blocks the reversal");
    CaseOptions competing;
    competing.competing_activity = true;
    CHECK(abstained(run_case(competing)),
          "competing activity blocks the regular initial-slot reversal");
    CaseOptions insufficient_alignment;
    insufficient_alignment.enough_alignment = false;
    CHECK(abstained(run_case(insufficient_alignment)),
          "insufficient phrase alignment blocks the reversal");
    CaseOptions phrase_rank;
    phrase_rank.phrase_second_is_initial = false;
    CHECK(abstained(run_case(phrase_rank)),
          "a different exact-phrase runner-up blocks the reversal");
    CaseOptions phrase_gate;
    phrase_gate.exactly_one_phrase_margin = false;
    CHECK(abstained(run_case(phrase_gate)),
          "a different exact-phrase margin pattern blocks the reversal");
    CaseOptions missing_vad;
    missing_vad.vad_count = 0;
    CHECK(abstained(run_case(missing_vad)),
          "missing containing VAD blocks the reversal");
    CaseOptions duplicate_vad;
    duplicate_vad.vad_count = 2;
    CHECK(abstained(run_case(duplicate_vad)),
          "duplicate containing VAD blocks the reversal");
    CaseOptions eligible_vad;
    eligible_vad.vad_outer_abstention = false;
    CHECK(abstained(run_case(eligible_vad)),
          "an independently eligible VAD stays on the ordinary path");
    CaseOptions reversed_vad;
    reversed_vad.vad_initial_first = false;
    CHECK(abstained(run_case(reversed_vad)),
          "a current-first VAD blocks the initial-slot reversal");
    CaseOptions missing_complete;
    missing_complete.complete_source_count = 0;
    CHECK(abstained(run_case(missing_complete)),
          "missing complete-source evidence blocks the reversal");
    CaseOptions duplicate_complete;
    duplicate_complete.complete_source_count = 2;
    CHECK(abstained(run_case(duplicate_complete)),
          "duplicate complete-source evidence blocks the reversal");
    CaseOptions eligible_complete;
    eligible_complete.complete_source_outer_abstention = false;
    CHECK(abstained(run_case(eligible_complete)),
          "an independently eligible complete source stays ordinary");
    CaseOptions reversed_complete;
    reversed_complete.complete_source_initial_first = false;
    CHECK(abstained(run_case(reversed_complete)),
          "a current-first complete source blocks the reversal");
    CaseOptions protected_labels;
    protected_labels.protected_labels = true;
    CHECK(abstained(run_case(protected_labels)),
          "an existing voiceprint write protects the current phrase");
  }

  // ---- 20g6. FR16ABC: a different primary micro-run may own one complete
  // no-embedding aligned unit only when the current primary identity brackets
  // it on both sides and the sole activity track remains uncontested. ----
  {
    using Entry = ComprehensiveTimeline::Entry;
    auto run_case = [](bool full_primary_coverage, bool same_brackets,
                       bool short_primary_run, bool candidate_activity,
                       bool include_direct, bool neighbors_outside_run,
                       bool gapless_brackets)
        -> std::vector<Entry> {
      BusinessSpeakerPipeline::Config config;
      config.voiceprint_fusion_enabled = true;
      TestBusinessSpeakerPipeline tl(config);
      std::vector<TestBusinessSpeakerPipeline::SpeakerInput> activity = {
          {0.0, 2.0, "slot_0", 0.9f, "spk_a"}};
      if (candidate_activity) {
        activity.push_back({1.0, 1.1, "slot_1", 0.9f, "spk_b"});
      }
      tl.ReplaceSpeakers(activity);

      const double primary_start = short_primary_run ? 0.9 : 0.7;
      const double primary_end = full_primary_coverage
                                     ? (short_primary_run ? 1.2 : 1.3)
                                     : 1.05;
      tl.ReplacePrimarySpeakers(
          {{0.0, gapless_brackets ? primary_start : primary_start - 0.1,
            "slot_0", 0.9f,
            same_brackets ? "spk_a" : "spk_c"},
           {primary_start, primary_end, "slot_1", 0.9f, "spk_b"},
           {primary_end, 2.0, "slot_0", 0.9f, "spk_a"}});
      tl.UpsertText(0, 0.0, 2.0, "前嗯后");
      tl.UpsertAlign(0, 0.0, 2.0,
                     {{0.0, neighbors_outside_run ? 0.5 : 0.95, "前"},
                      {1.0, 1.1, "嗯"},
                      {1.5, 2.0, "后"}});

      std::vector<ComprehensiveTimeline::SpeakerVoiceprintEvidence> evidence;
      if (include_direct) {
        ComprehensiveTimeline::SpeakerVoiceprintEvidence direct;
        direct.evidence_id = "business_interval:0:0";
        direct.kind = "business_interval";
        direct.text_id = 0;
        direct.source_start = 0;
        direct.source_end = 3;
        direct.start = 0.0;
        direct.end = 2.0;
        direct.embedding_available = true;
        direct.session_scores = {{"spk_a", 0.70f}, {"spk_b", 0.20f}};
        evidence.push_back(direct);
      }
      ComprehensiveTimeline::SpeakerVoiceprintEvidence unit;
      unit.evidence_id = "aligned_unit:0:1";
      unit.kind = "aligned_unit";
      unit.text_id = 0;
      unit.source_start = 1;
      unit.source_end = 2;
      unit.start = 1.0;
      unit.end = 1.1;
      unit.embedding_available = false;
      evidence.push_back(unit);
      tl.ReplaceVoiceprint(evidence);
      return tl.Snapshot();
    };

    auto has_unit = [](const std::vector<Entry>& entries,
                       const std::string& speaker_id,
                       const std::string& reason = "") {
      return std::any_of(entries.begin(), entries.end(), [&](const auto& entry) {
        return entry.text.find("嗯") != std::string::npos &&
               entry.speaker_id == speaker_id &&
               (reason.empty() || entry.speaker_decision.reason == reason);
      });
    };
    CHECK(has_unit(run_case(true, true, true, false, true, true, true), "spk_b",
                   "primary_speaker_bracketed_aligned_unit_override"),
          "bracketed primary micro-run owns its complete aligned unit");
    CHECK(has_unit(run_case(false, true, true, false, true, true, true), "spk_a"),
          "partial primary coverage preserves the direct identity");
    CHECK(has_unit(run_case(true, false, true, false, true, true, true), "spk_a"),
          "different bracketing identities preserve the direct identity");
    CHECK(has_unit(run_case(true, true, false, false, true, true, true), "spk_a"),
          "regular-duration primary run preserves the direct identity");
    CHECK(has_unit(run_case(true, true, true, true, true, true, true), "spk_a"),
          "candidate activity support preserves the direct identity");
    CHECK(has_unit(run_case(true, true, true, false, false, true, true), "spk_a"),
          "non-direct current attribution stays outside the challenge");
    CHECK(has_unit(run_case(true, true, true, false, true, false, true), "spk_a"),
          "aligned neighbor inside the micro-run preserves direct identity");
    CHECK(has_unit(run_case(true, true, true, false, true, true, false), "spk_a"),
          "gapped primary brackets preserve the direct identity");
  }

  // ---- 20g7. FR16ABD: an isolated robust VAD may own only the contiguous
  // complete aligned island inside it when native tracks agree on a different
  // coarse direct identity. ----
  {
    using Entry = ComprehensiveTimeline::Entry;
    auto run_case = [](bool isolated, bool gallery_agreement,
                       bool enough_aligned_units, bool direct_labels,
                       bool candidate_activity, bool primary_current,
                       bool contiguous_source) -> std::vector<Entry> {
      BusinessSpeakerPipeline::Config config;
      config.voiceprint_fusion_enabled = true;
      config.align_snap_pause_sec = 0.25;
      TestBusinessSpeakerPipeline tl(config);
      std::vector<TestBusinessSpeakerPipeline::SpeakerInput> activity = {
          {0.8, 1.8, "slot_0", 0.9f, "spk_a"}};
      if (candidate_activity) {
        activity.push_back({1.0, 1.4, "slot_1", 0.9f, "spk_b"});
      }
      tl.ReplaceSpeakers(activity);
      tl.ReplacePrimarySpeakers(
          {{0.8, 1.8, "slot_0", 0.9f,
            primary_current ? "spk_a" : "spk_c"}});

      const std::string source = contiguous_source ? "甲乙" : "甲，乙";
      const int source_end = contiguous_source ? 2 : 3;
      const int second_start = contiguous_source ? 1 : 2;
      tl.UpsertText(0, 0.8, 1.8, source);
      tl.UpsertAlign(0, 0.8, 1.8,
                     {{1.0, 1.2, "甲"}, {1.2, 1.4, "乙"}});

      std::vector<ComprehensiveTimeline::SpeakerVoiceprintEvidence> evidence;
      if (direct_labels) {
        ComprehensiveTimeline::SpeakerVoiceprintEvidence direct;
        direct.evidence_id = "business_interval:0:0";
        direct.kind = "business_interval";
        direct.text_id = 0;
        direct.source_start = 0;
        direct.source_end = source_end;
        direct.start = 1.0;
        direct.end = 1.4;
        direct.embedding_available = true;
        direct.session_scores = {{"spk_a", 0.70f}, {"spk_b", 0.20f}};
        evidence.push_back(direct);
      }

      ComprehensiveTimeline::SpeakerVoiceprintEvidence first_unit;
      first_unit.evidence_id = "aligned_unit:0:0";
      first_unit.kind = "aligned_unit";
      first_unit.text_id = 0;
      first_unit.source_start = 0;
      first_unit.source_end = 1;
      first_unit.start = 1.0;
      first_unit.end = 1.2;
      evidence.push_back(first_unit);
      if (enough_aligned_units) {
        ComprehensiveTimeline::SpeakerVoiceprintEvidence second_unit;
        second_unit.evidence_id = "aligned_unit:0:1";
        second_unit.kind = "aligned_unit";
        second_unit.text_id = 0;
        second_unit.source_start = second_start;
        second_unit.source_end = source_end;
        second_unit.start = 1.2;
        second_unit.end = 1.4;
        evidence.push_back(second_unit);
      }

      ComprehensiveTimeline::SpeakerVoiceprintEvidence previous_vad;
      previous_vad.evidence_id = "vad:0";
      previous_vad.kind = "vad";
      previous_vad.text_id = -1;
      previous_vad.start = 0.0;
      previous_vad.end = isolated ? 0.5 : 0.8;
      ComprehensiveTimeline::SpeakerVoiceprintEvidence target_vad;
      target_vad.evidence_id = "vad:1";
      target_vad.kind = "vad";
      target_vad.text_id = -1;
      target_vad.start = 0.9;
      target_vad.end = 1.8;
      target_vad.embedding_available = true;
      target_vad.robust_gallery_complete = true;
      target_vad.session_scores = {{"spk_a", 0.30f}, {"spk_b", 0.65f},
                                   {"spk_c", 0.20f}};
      target_vad.robust_scores =
          gallery_agreement
              ? std::vector<ComprehensiveTimeline::VoiceprintScore>{
                    {"spk_a", 0.31f}, {"spk_b", 0.64f}, {"spk_c", 0.20f}}
              : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                    {"spk_a", 0.31f}, {"spk_b", 0.20f}, {"spk_c", 0.64f}};
      ComprehensiveTimeline::SpeakerVoiceprintEvidence next_vad;
      next_vad.evidence_id = "vad:2";
      next_vad.kind = "vad";
      next_vad.text_id = -1;
      next_vad.start = 2.2;
      next_vad.end = 2.6;
      evidence.push_back(previous_vad);
      evidence.push_back(target_vad);
      evidence.push_back(next_vad);
      tl.ReplaceVoiceprint(evidence);
      return tl.Snapshot();
    };

    auto has_source = [](const std::vector<Entry>& entries,
                         const std::string& speaker_id,
                         const std::string& reason = "") {
      return std::any_of(entries.begin(), entries.end(), [&](const auto& entry) {
        return entry.text.find("甲") != std::string::npos &&
               entry.speaker_id == speaker_id &&
               (reason.empty() || entry.speaker_decision.reason == reason);
      });
    };
    CHECK(has_source(run_case(true, true, true, true, false, true, true),
                     "spk_b",
                     "voiceprint_vad_isolated_aligned_island_override"),
          "isolated dual-gallery VAD owns its contained aligned island");
    CHECK(has_source(run_case(false, true, true, true, false, true, true),
                     "spk_a"),
          "non-isolated VAD preserves the direct identity");
    CHECK(has_source(run_case(true, false, true, true, false, true, true),
                     "spk_a"),
          "VAD gallery disagreement preserves the direct identity");
    CHECK(has_source(run_case(true, true, false, true, false, true, true),
                     "spk_a"),
          "insufficient aligned units preserve the direct identity");
    CHECK(has_source(run_case(true, true, true, false, false, true, true),
                     "spk_a"),
          "non-direct labels stay outside the VAD challenge");
    CHECK(has_source(run_case(true, true, true, true, true, true, true),
                     "spk_a"),
          "candidate activity support preserves the direct identity");
    CHECK(has_source(run_case(true, true, true, true, false, false, true),
                     "spk_a"),
          "primary mismatch preserves the direct identity");
    CHECK(has_source(run_case(true, true, true, true, false, true, false),
                     "spk_a"),
          "discontinuous aligned source preserves the direct identity");
  }

  // ---- 20g8. FR16ABE: a short primary/activity island at a separately
  // bounded VAD onset may own only its complete aligned source before the
  // current identity resumes gaplessly. ----
  {
    using Entry = ComprehensiveTimeline::Entry;
    auto run_case = [](bool eligible_duration, bool primary_pause,
                       bool gapless_recovery, bool vad_continuation,
                       bool candidate_activity, bool third_activity,
                       bool vad_current_ranking) -> std::vector<Entry> {
      BusinessSpeakerPipeline::Config config;
      config.voiceprint_fusion_enabled = true;
      config.align_snap_pause_sec = 0.25;
      TestBusinessSpeakerPipeline tl(config);
      const double candidate_start = eligible_duration ? 0.8 : 1.1;
      const double candidate_end = 1.4;
      const double unit_start = candidate_start + 0.1;
      const double unit_mid = unit_start + 0.15;
      const double unit_end = unit_mid + 0.15;

      std::vector<TestBusinessSpeakerPipeline::SpeakerInput> activity = {
          {0.0, 2.2, "slot_0", 0.9f, "spk_a"}};
      if (candidate_activity) {
        activity.push_back(
            {candidate_start, candidate_end, "slot_1", 0.9f, "spk_b"});
      }
      if (third_activity) {
        activity.push_back(
            {unit_start, unit_end, "slot_2", 0.9f, "spk_c"});
      }
      tl.ReplaceSpeakers(activity);
      tl.ReplacePrimarySpeakers(
          {{0.0, primary_pause ? 0.5 : candidate_start - 0.1,
            "slot_0", 0.9f, "spk_a"},
           {candidate_start, candidate_end, "slot_1", 0.9f, "spk_b"},
           {gapless_recovery ? candidate_end : candidate_end + 0.1, 2.2,
            "slot_0", 0.9f, "spk_a"}});
      tl.UpsertText(0, candidate_start, 2.0, "甲，乙");
      tl.UpsertAlign(0, candidate_start, 2.0,
                     {{unit_start, unit_mid, "甲"},
                      {unit_mid, unit_end, "乙"}});

      std::vector<ComprehensiveTimeline::SpeakerVoiceprintEvidence> evidence;
      ComprehensiveTimeline::SpeakerVoiceprintEvidence phrase;
      phrase.evidence_id = "punctuation_phrase:0:0";
      phrase.kind = "punctuation_phrase";
      phrase.text_id = 0;
      phrase.source_start = 0;
      phrase.source_end = 3;
      phrase.start = unit_start;
      phrase.end = unit_end;
      phrase.embedding_available = true;
      phrase.robust_gallery_complete = true;
      phrase.session_scores = {{"spk_a", 0.70f}, {"spk_b", 0.20f}};
      phrase.robust_scores = {{"spk_a", 0.69f}, {"spk_b", 0.21f}};
      evidence.push_back(phrase);
      for (int index = 0; index < 2; ++index) {
        ComprehensiveTimeline::SpeakerVoiceprintEvidence unit;
        unit.evidence_id = "aligned_unit:0:" + std::to_string(index);
        unit.kind = "aligned_unit";
        unit.text_id = 0;
        unit.source_start = index == 0 ? 0 : 2;
        unit.source_end = index == 0 ? 1 : 3;
        unit.start = index == 0 ? unit_start : unit_mid;
        unit.end = index == 0 ? unit_mid : unit_end;
        evidence.push_back(unit);
      }

      ComprehensiveTimeline::SpeakerVoiceprintEvidence previous_vad;
      previous_vad.evidence_id = "vad:0";
      previous_vad.kind = "vad";
      previous_vad.text_id = -1;
      previous_vad.start = 0.0;
      previous_vad.end = 0.4;
      ComprehensiveTimeline::SpeakerVoiceprintEvidence target_vad;
      target_vad.evidence_id = "vad:1";
      target_vad.kind = "vad";
      target_vad.text_id = -1;
      target_vad.start = candidate_start + 0.02;
      target_vad.end = vad_continuation ? 2.0 : candidate_end + 0.2;
      target_vad.embedding_available = true;
      target_vad.robust_gallery_complete = true;
      target_vad.session_scores =
          vad_current_ranking
              ? std::vector<ComprehensiveTimeline::VoiceprintScore>{
                    {"spk_a", 0.50f}, {"spk_b", 0.40f}}
              : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                    {"spk_a", 0.40f}, {"spk_b", 0.50f}};
      target_vad.robust_scores = target_vad.session_scores;
      evidence.push_back(previous_vad);
      evidence.push_back(target_vad);
      tl.ReplaceVoiceprint(evidence);
      return tl.Snapshot();
    };

    auto has_source = [](const std::vector<Entry>& entries,
                         const std::string& speaker_id,
                         const std::string& reason = "") {
      return std::any_of(entries.begin(), entries.end(), [&](const auto& entry) {
        return entry.text.find("乙") != std::string::npos &&
               entry.speaker_id == speaker_id &&
               (reason.empty() || entry.speaker_decision.reason == reason);
      });
    };
    CHECK(has_source(run_case(true, true, true, true, true, false, true),
                     "spk_b",
                     "primary_speaker_pause_onset_aligned_island_override"),
          "pause-onset primary/activity island owns its aligned source");
    CHECK(has_source(run_case(false, true, true, true, true, false, true),
                     "spk_a"),
          "subminimum primary run preserves the voiceprint identity");
    CHECK(has_source(run_case(true, false, true, true, true, false, true),
                     "spk_a"),
          "missing primary pause preserves the voiceprint identity");
    CHECK(has_source(run_case(true, true, false, true, true, false, true),
                     "spk_a"),
          "gapped current recovery preserves the voiceprint identity");
    CHECK(has_source(run_case(true, true, true, false, true, false, true),
                     "spk_a"),
          "short VAD continuation preserves the voiceprint identity");
    CHECK(has_source(run_case(true, true, true, true, false, false, true),
                     "spk_a"),
          "missing candidate activity preserves the voiceprint identity");
    CHECK(has_source(run_case(true, true, true, true, true, true, true),
                     "spk_a"),
          "third activity identity preserves the voiceprint identity");
    CHECK(has_source(run_case(true, true, true, true, true, false, false),
                     "spk_a"),
          "VAD ranking disagreement preserves the voiceprint identity");
  }

  // ---- 20g9. FR16ABF: a no-embedding subminimum business interval may
  // recover its local slot's initial identity only under independent VAD and
  // aligned-unit isolation. ----
  {
    using Entry = ComprehensiveTimeline::Entry;
    auto run_case = [](bool include_epoch, bool embedding_available,
                       bool vad_isolated, bool alignment_isolated,
                       bool enough_aligned_units, bool primary_current,
                       bool competing_activity) -> std::vector<Entry> {
      BusinessSpeakerPipeline::Config config;
      config.voiceprint_fusion_enabled = true;
      config.align_snap_pause_sec = 0.25;
      TestBusinessSpeakerPipeline tl(config);
      std::vector<TestBusinessSpeakerPipeline::SpeakerInput> activity;
      if (include_epoch) {
        activity.push_back({-2.0, -1.0, "slot_0", 0.9f, "spk_b"});
      }
      activity.push_back({0.8, 1.5, "slot_0", 0.9f, "spk_a"});
      if (competing_activity) {
        activity.push_back({1.0, 1.3, "slot_1", 0.9f, "spk_b"});
      }
      tl.ReplaceSpeakers(activity);
      tl.ReplacePrimarySpeakers(
          {{0.8, 1.5, "slot_0", 0.9f,
            primary_current ? "spk_a" : "spk_c"}});
      tl.UpsertText(0, 0.8, 1.5, "甲乙");
      tl.UpsertAlign(0, 0.8, 1.5,
                     {{1.05, 1.12, "甲"}, {1.18, 1.25, "乙"}});

      std::vector<ComprehensiveTimeline::SpeakerVoiceprintEvidence> evidence;
      ComprehensiveTimeline::SpeakerVoiceprintEvidence interval;
      interval.evidence_id = "business_interval:0:0";
      interval.kind = "business_interval";
      interval.text_id = 0;
      interval.source_start = 0;
      interval.source_end = 2;
      interval.start = 1.0;
      interval.end = 1.3;
      interval.embedding_available = embedding_available;
      if (embedding_available) {
        interval.session_scores = {{"spk_a", 0.70f}, {"spk_b", 0.20f}};
      }
      evidence.push_back(interval);
      const int inside_count = enough_aligned_units ? 2 : 1;
      for (int index = 0; index < inside_count; ++index) {
        ComprehensiveTimeline::SpeakerVoiceprintEvidence unit;
        unit.evidence_id = "aligned_unit:0:" + std::to_string(index);
        unit.kind = "aligned_unit";
        unit.text_id = 0;
        unit.source_start = index;
        unit.source_end = index + 1;
        unit.start = index == 0 ? 1.05 : 1.18;
        unit.end = index == 0 ? 1.12 : 1.25;
        evidence.push_back(unit);
      }
      ComprehensiveTimeline::SpeakerVoiceprintEvidence previous_unit;
      previous_unit.evidence_id = "aligned_unit:1:0";
      previous_unit.kind = "aligned_unit";
      previous_unit.text_id = 1;
      previous_unit.source_start = 0;
      previous_unit.source_end = 1;
      previous_unit.start = alignment_isolated ? 0.4 : 0.9;
      previous_unit.end = alignment_isolated ? 0.5 : 0.95;
      ComprehensiveTimeline::SpeakerVoiceprintEvidence next_unit;
      next_unit.evidence_id = "aligned_unit:2:0";
      next_unit.kind = "aligned_unit";
      next_unit.text_id = 2;
      next_unit.source_start = 0;
      next_unit.source_end = 1;
      next_unit.start = 1.8;
      next_unit.end = 1.9;
      evidence.push_back(previous_unit);
      evidence.push_back(next_unit);

      ComprehensiveTimeline::SpeakerVoiceprintEvidence previous_vad;
      previous_vad.evidence_id = "vad:0";
      previous_vad.kind = "vad";
      previous_vad.text_id = -1;
      previous_vad.start = 0.0;
      previous_vad.end = vad_isolated ? 0.5 : 1.1;
      ComprehensiveTimeline::SpeakerVoiceprintEvidence next_vad;
      next_vad.evidence_id = "vad:1";
      next_vad.kind = "vad";
      next_vad.text_id = -1;
      next_vad.start = 1.8;
      next_vad.end = 2.1;
      evidence.push_back(previous_vad);
      evidence.push_back(next_vad);
      tl.ReplaceVoiceprint(evidence);
      return tl.Snapshot();
    };

    auto has_source = [](const std::vector<Entry>& entries,
                         const std::string& speaker_id,
                         const std::string& reason = "") {
      return std::any_of(entries.begin(), entries.end(), [&](const auto& entry) {
        return entry.text.find("甲") != std::string::npos &&
               entry.speaker_id == speaker_id &&
               (reason.empty() || entry.speaker_decision.reason == reason);
      });
    };
    CHECK(has_source(run_case(true, false, true, true, true, true, false),
                     "spk_b",
                     "sortformer_initial_slot_isolated_no_vad_interval_"
                     "override"),
          "dual-isolated no-VAD interval recovers initial slot identity");
    CHECK(has_source(run_case(false, false, true, true, true, true, false),
                     "spk_a"),
          "missing initial epoch preserves the current identity");
    CHECK(has_source(run_case(true, true, true, true, true, true, false),
                     "spk_a"),
          "available interval embedding stays on the voiceprint path");
    CHECK(has_source(run_case(true, false, false, true, true, true, false),
                     "spk_a"),
          "VAD overlap preserves the current identity");
    CHECK(has_source(run_case(true, false, true, false, true, true, false),
                     "spk_a"),
          "insufficient alignment isolation preserves current identity");
    CHECK(has_source(run_case(true, false, true, true, false, true, false),
                     "spk_a"),
          "insufficient aligned units preserve the current identity");
    CHECK(has_source(run_case(true, false, true, true, true, false, false),
                     "spk_a"),
          "primary mismatch preserves the current identity");
    const auto competing =
        run_case(true, false, true, true, true, true, true);
    CHECK(std::none_of(competing.begin(), competing.end(), [](const auto& entry) {
            return entry.speaker_decision.reason ==
                   "sortformer_initial_slot_isolated_no_vad_interval_"
                   "override";
          }),
          "competing activity blocks isolated initial-slot recovery");
  }

  // ---- 20g10. FR16ABG: one consistent four-view near tie may recover the
  // immutable initial identity when the current dynamic identity is absent
  // from every top-two pair. ----
  {
    using Entry = ComprehensiveTimeline::Entry;
    struct CaseOptions {
      bool include_epoch = true;
      int candidate_top_count = 1;
      bool consistent_competitor = true;
      bool passing_margin = false;
      bool current_in_pair = false;
      bool primary_current = true;
      bool competing_activity = false;
      bool include_vad = true;
      bool vad_robust_complete = true;
      bool enough_aligned_units = true;
    };
    auto run_case = [](const CaseOptions& options) -> std::vector<Entry> {
      BusinessSpeakerPipeline::Config config;
      config.voiceprint_fusion_enabled = true;
      TestBusinessSpeakerPipeline tl(config);

      std::vector<TestBusinessSpeakerPipeline::SpeakerInput> activity;
      if (options.include_epoch) {
        activity.push_back({-2.0, -1.0, "slot_0", 0.9f, "spk_b"});
      }
      activity.push_back({0.0, 1.0, "slot_0", 0.9f, "spk_a"});
      if (options.competing_activity) {
        activity.push_back({0.2, 0.8, "slot_1", 0.9f, "spk_c"});
      }
      tl.ReplaceSpeakers(activity);
      tl.ReplacePrimarySpeakers(
          {{0.0, 1.0, "slot_0", 0.9f,
            options.primary_current ? "spk_a" : "spk_c"}});
      tl.UpsertText(0, 0.0, 1.0, "甲乙。");
      if (options.enough_aligned_units) {
        tl.UpsertAlign(0, 0.0, 1.0,
                       {{0.1, 0.4, "甲"}, {0.4, 0.9, "乙。"}});
      } else {
        tl.UpsertAlign(0, 0.0, 1.0, {{0.1, 0.9, "甲乙。"}});
      }

      auto scores_for_view = [&](int view) {
        const bool candidate_first = view < options.candidate_top_count;
        std::string competitor = "spk_c";
        if (!options.consistent_competitor && view == 3) {
          competitor = "spk_d";
        }
        std::vector<ComprehensiveTimeline::VoiceprintScore> scores =
            candidate_first
                ? std::vector<ComprehensiveTimeline::VoiceprintScore>{
                      {"spk_b", 0.36f}, {competitor, 0.34f}, {"spk_a", 0.20f}}
                : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                      {competitor, 0.36f}, {"spk_b", 0.34f}, {"spk_a", 0.20f}};
        if (options.passing_margin && view == 2) {
          scores[0].score = 0.40f;
          scores[1].score = 0.34f;
        }
        if (options.current_in_pair && view == 3) {
          scores = {{competitor, 0.36f}, {"spk_a", 0.34f}, {"spk_b", 0.20f}};
        }
        return scores;
      };

      ComprehensiveTimeline::SpeakerVoiceprintEvidence phrase;
      phrase.evidence_id = "punctuation_phrase:0:0";
      phrase.kind = "punctuation_phrase";
      phrase.text_id = 0;
      phrase.source_start = 0;
      phrase.source_end = 3;
      phrase.start = 0.1;
      phrase.end = 0.9;
      phrase.embedding_available = true;
      phrase.robust_gallery_complete = true;
      phrase.session_scores = scores_for_view(0);
      phrase.robust_scores = scores_for_view(1);
      std::vector<ComprehensiveTimeline::SpeakerVoiceprintEvidence> evidence = {
          phrase};
      if (options.include_vad) {
        ComprehensiveTimeline::SpeakerVoiceprintEvidence vad;
        vad.evidence_id = "vad:0";
        vad.kind = "vad";
        vad.text_id = -1;
        vad.start = 0.05;
        vad.end = 0.95;
        vad.embedding_available = true;
        vad.robust_gallery_complete = options.vad_robust_complete;
        vad.session_scores = scores_for_view(2);
        vad.robust_scores = scores_for_view(3);
        evidence.push_back(vad);
      }
      tl.ReplaceVoiceprint(evidence);
      return tl.Snapshot();
    };

    const std::string reason =
        "voiceprint_phrase_initial_slot_four_view_near_tie_override";
    auto has_reason = [&](const std::vector<Entry>& entries) {
      return std::any_of(entries.begin(), entries.end(), [&](const auto& entry) {
        return entry.speaker_id == "spk_b" &&
               entry.speaker_decision.reason == reason &&
               entry.speaker_decision.speaker_source ==
                   "sortformer_initial_slot+titanet_phrase+vad_session+"
                   "robust_gallery+forced_alignment";
      });
    };
    auto preserves_current = [&](const CaseOptions& options) {
      const auto entries = run_case(options);
      return !entries.empty() &&
             std::all_of(entries.begin(), entries.end(),
                         [&](const auto& entry) {
                           return entry.speaker_id == "spk_a" &&
                                  entry.speaker_decision.reason != reason;
                         });
    };

    CHECK(has_reason(run_case({})),
          "consistent four-view near tie recovers initial slot identity");
    CaseOptions missing_epoch;
    missing_epoch.include_epoch = false;
    CHECK(preserves_current(missing_epoch),
          "missing initial epoch preserves the current identity");
    CaseOptions no_candidate_top;
    no_candidate_top.candidate_top_count = 0;
    CHECK(preserves_current(no_candidate_top),
          "zero candidate top ranks preserve the current identity");
    CaseOptions two_candidate_tops;
    two_candidate_tops.candidate_top_count = 2;
    CHECK(preserves_current(two_candidate_tops),
          "two candidate top ranks preserve the current identity");
    CaseOptions inconsistent_competitor;
    inconsistent_competitor.consistent_competitor = false;
    CHECK(preserves_current(inconsistent_competitor),
          "changing top-two competitor preserves the current identity");
    CaseOptions passing_margin;
    passing_margin.passing_margin = true;
    CHECK(preserves_current(passing_margin),
          "one passing margin preserves the current identity");
    CaseOptions current_in_pair;
    current_in_pair.current_in_pair = true;
    CHECK(preserves_current(current_in_pair),
          "current identity in a top-two pair blocks recovery");
    CaseOptions primary_mismatch;
    primary_mismatch.primary_current = false;
    CHECK(preserves_current(primary_mismatch),
          "primary mismatch preserves the current identity");
    CaseOptions competing_activity;
    competing_activity.competing_activity = true;
    CHECK(!has_reason(run_case(competing_activity)),
          "competing activity blocks four-view recovery");
    CaseOptions missing_vad;
    missing_vad.include_vad = false;
    CHECK(preserves_current(missing_vad),
          "missing containing VAD preserves the current identity");
    CaseOptions incomplete_vad;
    incomplete_vad.vad_robust_complete = false;
    CHECK(preserves_current(incomplete_vad),
          "incomplete VAD gallery preserves the current identity");
    CaseOptions insufficient_alignment;
    insufficient_alignment.enough_aligned_units = false;
    CHECK(preserves_current(insufficient_alignment),
          "insufficient aligned units preserve the current identity");
  }

  // ---- 20g11. FR16ABH: symmetric phrase/VAD near ties may resolve one
  // direct interval attribution only when both scales expose the same pair.
  // ----
  {
    using Entry = ComprehensiveTimeline::Entry;
    struct CaseOptions {
      bool phrase_robust_candidate = true;
      bool vad_robust_current = true;
      bool phrase_margin_abstains = true;
      bool vad_score_abstains = true;
      bool consistent_pair = true;
      bool direct_baseline = true;
      bool primary_current = true;
      bool competing_activity = false;
      int containing_vad_count = 1;
      bool vad_robust_complete = true;
      bool enough_aligned_units = true;
    };
    auto run_case = [](const CaseOptions& options) -> std::vector<Entry> {
      BusinessSpeakerPipeline::Config config;
      config.voiceprint_fusion_enabled = true;
      TestBusinessSpeakerPipeline tl(config);
      std::vector<TestBusinessSpeakerPipeline::SpeakerInput> activity = {
          {-2.0, -1.0, "slot_1", 0.9f, "spk_b"},
          {0.0, 2.0, "slot_0", 0.9f, "spk_a"}};
      if (options.competing_activity) {
        activity.push_back({0.5, 1.3, "slot_1", 0.9f, "spk_b"});
      }
      tl.ReplaceSpeakers(activity);
      tl.ReplacePrimarySpeakers(
          {{0.0, 2.0, "slot_0", 0.9f,
            options.primary_current ? "spk_a" : "spk_b"}});
      tl.UpsertText(0, 0.5, 1.3, "甲乙。");
      if (options.enough_aligned_units) {
        tl.UpsertAlign(0, 0.5, 1.3,
                       {{0.5, 0.8, "甲"}, {0.8, 1.3, "乙。"}});
      } else {
        tl.UpsertAlign(0, 0.5, 1.3, {{0.5, 1.3, "甲乙。"}});
      }

      std::vector<ComprehensiveTimeline::SpeakerVoiceprintEvidence> evidence;
      if (options.direct_baseline) {
        ComprehensiveTimeline::SpeakerVoiceprintEvidence direct;
        direct.evidence_id = "business_interval:0:0";
        direct.kind = "business_interval";
        direct.text_id = 0;
        direct.source_start = 0;
        direct.source_end = 3;
        direct.start = 0.5;
        direct.end = 1.3;
        direct.embedding_available = true;
        direct.session_scores = { {"spk_a", 0.60f}, {"spk_b", 0.20f} };
        evidence.push_back(direct);
      }

      ComprehensiveTimeline::SpeakerVoiceprintEvidence phrase;
      phrase.evidence_id = "punctuation_phrase:0:0";
      phrase.kind = "punctuation_phrase";
      phrase.text_id = 0;
      phrase.source_start = 0;
      phrase.source_end = 3;
      phrase.start = 0.5;
      phrase.end = 1.3;
      phrase.embedding_available = true;
      phrase.robust_gallery_complete = true;
      const float phrase_top = options.phrase_margin_abstains ? 0.36f : 0.40f;
      phrase.session_scores = {
          {"spk_b", phrase_top}, {"spk_a", 0.34f}, {"spk_c", 0.10f}};
      phrase.robust_scores = options.phrase_robust_candidate
                                 ? std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                       {"spk_b", 0.36f},
                                       {options.consistent_pair ? "spk_a"
                                                                : "spk_d",
                                        0.34f},
                                       {"spk_c", 0.10f}}
                                 : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                       {"spk_a", 0.36f},
                                       {"spk_b", 0.34f},
                                       {"spk_c", 0.10f}};
      evidence.push_back(phrase);

      for (int index = 0; index < options.containing_vad_count; ++index) {
        ComprehensiveTimeline::SpeakerVoiceprintEvidence vad;
        vad.evidence_id = "vad:" + std::to_string(index);
        vad.kind = "vad";
        vad.text_id = -1;
        vad.start = 0.0 - index * 0.01;
        vad.end = 2.0 + index * 0.01;
        vad.embedding_available = true;
        vad.robust_gallery_complete = options.vad_robust_complete;
        const float vad_top = options.vad_score_abstains ? 0.54f : 0.56f;
        vad.session_scores = {
            {"spk_a", vad_top}, {"spk_b", 0.52f}, {"spk_c", 0.10f}};
        vad.robust_scores = options.vad_robust_current
                                ? std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                      {"spk_a", 0.54f},
                                      {options.consistent_pair ? "spk_b"
                                                               : "spk_d",
                                       0.52f},
                                      {"spk_c", 0.10f}}
                                : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                      {"spk_b", 0.54f},
                                      {"spk_a", 0.52f},
                                      {"spk_c", 0.10f}};
        evidence.push_back(vad);
      }
      tl.ReplaceVoiceprint(evidence);
      return tl.Snapshot();
    };

    const std::string reason =
        "voiceprint_phrase_cross_scale_symmetric_near_tie_override";
    auto has_reason = [&](const std::vector<Entry>& entries) {
      return std::any_of(entries.begin(), entries.end(), [&](const auto& entry) {
        return entry.speaker_id == "spk_b" &&
               entry.speaker_decision.reason == reason &&
               entry.speaker_decision.speaker_source ==
                   "sortformer_activity+primary_top1+titanet_phrase+"
                   "vad_session+robust_gallery+forced_alignment";
      });
    };
    auto preserves_current = [&](const CaseOptions& options) {
      const auto entries = run_case(options);
      return !entries.empty() &&
             std::all_of(entries.begin(), entries.end(),
                         [&](const auto& entry) {
                           return entry.speaker_id == "spk_a" &&
                                  entry.speaker_decision.reason != reason;
                         });
    };

    CHECK(has_reason(run_case({})),
          "symmetric cross-scale near tie resolves direct attribution");
    CaseOptions phrase_disagreement;
    phrase_disagreement.phrase_robust_candidate = false;
    CHECK(preserves_current(phrase_disagreement),
          "phrase top-rank disagreement preserves current identity");
    CaseOptions vad_disagreement;
    vad_disagreement.vad_robust_current = false;
    CHECK(preserves_current(vad_disagreement),
          "VAD top-rank disagreement preserves current identity");
    CaseOptions phrase_eligible;
    phrase_eligible.phrase_margin_abstains = false;
    CHECK(preserves_current(phrase_eligible),
          "eligible phrase view preserves current identity");
    CaseOptions vad_score_eligible;
    vad_score_eligible.vad_score_abstains = false;
    CHECK(preserves_current(vad_score_eligible),
          "outer score eligibility preserves current identity");
    CaseOptions inconsistent_pair;
    inconsistent_pair.consistent_pair = false;
    CHECK(preserves_current(inconsistent_pair),
          "third top-two identity preserves current attribution");
    CaseOptions native_baseline;
    native_baseline.direct_baseline = false;
    CHECK(preserves_current(native_baseline),
          "native baseline provenance preserves current identity");
    CaseOptions primary_mismatch;
    primary_mismatch.primary_current = false;
    CHECK(preserves_current(primary_mismatch),
          "primary mismatch preserves current identity");
    CaseOptions competing_activity;
    competing_activity.competing_activity = true;
    CHECK(!has_reason(run_case(competing_activity)),
          "competing candidate activity blocks cross-scale recovery");
    CaseOptions multiple_vad;
    multiple_vad.containing_vad_count = 2;
    CHECK(preserves_current(multiple_vad),
          "multiple containing VAD intervals preserve current identity");
    CaseOptions incomplete_vad;
    incomplete_vad.vad_robust_complete = false;
    CHECK(preserves_current(incomplete_vad),
          "incomplete containing VAD preserves current identity");
    CaseOptions insufficient_alignment;
    insufficient_alignment.enough_aligned_units = false;
    CHECK(preserves_current(insufficient_alignment),
          "insufficient aligned units preserve current identity");
  }

  // ---- 20g12. FR16ABI: eligible exact-interval galleries may override one
  // primary overlap choice only when the containing VAD independently
  // excludes that identity and abstains on the opposite top-two pair. ----
  {
    using Entry = ComprehensiveTimeline::Entry;
    struct CaseOptions {
      bool interval_robust_candidate = true;
      bool interval_margin_passes = true;
      bool vad_abstains = true;
      bool vad_opposite_order = true;
      bool consistent_vad_pair = true;
      bool current_in_vad_pair = false;
      bool include_competitor_activity = true;
      bool include_extra_activity = false;
      bool include_candidate_activity = false;
      bool primary_current = true;
      bool primary_covers = true;
      int containing_vad_count = 1;
      bool vad_robust_complete = true;
      bool enough_aligned_units = true;
    };
    auto run_case = [](const CaseOptions& options) -> std::vector<Entry> {
      BusinessSpeakerPipeline::Config config;
      config.voiceprint_fusion_enabled = true;
      config.speaker_overlap_tie_policy =
          BusinessSpeakerPipeline::SpeakerOverlapTiePolicy::kPrimarySpeaker;
      TestBusinessSpeakerPipeline tl(config);

      std::vector<TestBusinessSpeakerPipeline::SpeakerInput> activity = {
          {0.0, 1.0, "slot_0", 0.9f, "spk_a"}};
      if (options.include_competitor_activity) {
        activity.push_back({0.0, 1.0, "slot_1", 0.9f, "spk_c"});
      }
      if (options.include_extra_activity) {
        activity.push_back({0.0, 1.0, "slot_3", 0.9f, "spk_d"});
      }
      if (options.include_candidate_activity) {
        activity.push_back({0.0, 1.0, "slot_2", 0.9f, "spk_b"});
      }
      tl.ReplaceSpeakers(activity);
      tl.ReplacePrimarySpeakers(
          {{0.0, options.primary_covers ? 1.0 : 0.5, "slot_0", 0.9f,
            options.primary_current ? "spk_a" : "spk_c"}});
      tl.UpsertText(0, 0.1, 0.9, "甲乙。");
      if (options.enough_aligned_units) {
        tl.UpsertAlign(0, 0.1, 0.9,
                       {{0.1, 0.4, "甲"}, {0.4, 0.9, "乙。"}});
      } else {
        tl.UpsertAlign(0, 0.1, 0.9, {{0.1, 0.9, "甲乙。"}});
      }

      ComprehensiveTimeline::SpeakerVoiceprintEvidence interval;
      interval.evidence_id = "business_interval:0:0";
      interval.kind = "business_interval";
      interval.text_id = 0;
      interval.source_start = 0;
      interval.source_end = 3;
      interval.start = 0.1;
      interval.end = 0.9;
      interval.embedding_available = true;
      interval.robust_gallery_complete = true;
      const float interval_top =
          options.interval_margin_passes ? 0.60f : 0.36f;
      interval.session_scores = {
          {"spk_b", interval_top}, {"spk_c", 0.34f}, {"spk_a", 0.10f}};
      interval.robust_scores = options.interval_robust_candidate
                                   ? std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                         {"spk_b", 0.58f},
                                         {"spk_c", 0.30f},
                                         {"spk_a", 0.10f}}
                                   : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                         {"spk_c", 0.58f},
                                         {"spk_b", 0.30f},
                                         {"spk_a", 0.10f}};
      std::vector<ComprehensiveTimeline::SpeakerVoiceprintEvidence> evidence = {
          interval};

      for (int index = 0; index < options.containing_vad_count; ++index) {
        ComprehensiveTimeline::SpeakerVoiceprintEvidence vad;
        vad.evidence_id = "vad:" + std::to_string(index);
        vad.kind = "vad";
        vad.text_id = -1;
        vad.start = -0.6 - index * 0.01;
        vad.end = 1.6 + index * 0.01;
        vad.embedding_available = true;
        vad.robust_gallery_complete = options.vad_robust_complete;
        const float vad_top = options.vad_abstains ? 0.52f : 0.58f;
        vad.session_scores = options.current_in_vad_pair
                                 ? std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                       {"spk_b", vad_top},
                                       {"spk_a", 0.51f},
                                       {"spk_c", 0.10f}}
                                 : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                       {"spk_b", vad_top},
                                       {"spk_c", 0.51f},
                                       {"spk_a", 0.10f}};
        if (!options.consistent_vad_pair) {
          vad.robust_scores = {
              {"spk_d", 0.52f}, {"spk_b", 0.51f}, {"spk_c", 0.10f}};
        } else if (options.vad_opposite_order) {
          vad.robust_scores = {
              {"spk_c", 0.52f}, {"spk_b", 0.51f}, {"spk_a", 0.10f}};
        } else {
          vad.robust_scores = {
              {"spk_b", 0.52f}, {"spk_c", 0.51f}, {"spk_a", 0.10f}};
        }
        evidence.push_back(vad);
      }
      tl.ReplaceVoiceprint(evidence);
      return tl.Snapshot();
    };

    const std::string reason =
        "voiceprint_interval_primary_conflict_vad_abstention_override";
    auto has_reason = [&](const std::vector<Entry>& entries) {
      return std::any_of(entries.begin(), entries.end(), [&](const auto& entry) {
        return entry.speaker_id == "spk_b" &&
               entry.speaker_decision.reason == reason &&
               entry.speaker_decision.speaker_source ==
                   "sortformer_activity+primary_top1+titanet_interval+"
                   "vad_session+robust_gallery+forced_alignment";
      });
    };
    auto preserves_primary = [&](const CaseOptions& options) {
      const auto entries = run_case(options);
      return !entries.empty() &&
             std::all_of(entries.begin(), entries.end(),
                         [&](const auto& entry) {
                           return entry.speaker_id == "spk_a" &&
                                  entry.speaker_decision.reason != reason;
                         });
    };

    CHECK(has_reason(run_case({})),
          "eligible interval evidence resolves an excluded primary identity");
    CaseOptions interval_disagreement;
    interval_disagreement.interval_robust_candidate = false;
    CHECK(preserves_primary(interval_disagreement),
          "interval gallery disagreement preserves the primary identity");
    CaseOptions interval_abstention;
    interval_abstention.interval_margin_passes = false;
    CHECK(preserves_primary(interval_abstention),
          "interval margin abstention preserves the primary identity");
    CaseOptions eligible_vad;
    eligible_vad.vad_abstains = false;
    CHECK(preserves_primary(eligible_vad),
          "eligible containing VAD preserves the primary identity");
    CaseOptions same_vad_order;
    same_vad_order.vad_opposite_order = false;
    CHECK(preserves_primary(same_vad_order),
          "repeated VAD top rank preserves the primary identity");
    CaseOptions inconsistent_vad_pair;
    inconsistent_vad_pair.consistent_vad_pair = false;
    CHECK(preserves_primary(inconsistent_vad_pair),
          "a changed VAD top-two identity preserves the primary identity");
    CaseOptions current_in_vad_pair;
    current_in_vad_pair.current_in_vad_pair = true;
    CHECK(preserves_primary(current_in_vad_pair),
          "current identity in a VAD top-two pair blocks recovery");
    CaseOptions missing_competitor;
    missing_competitor.include_competitor_activity = false;
    CHECK(!has_reason(run_case(missing_competitor)),
          "missing competing activity blocks recovery");
    CaseOptions extra_activity;
    extra_activity.include_extra_activity = true;
    CHECK(!has_reason(run_case(extra_activity)),
          "an additional activity identity blocks recovery");
    CaseOptions candidate_activity;
    candidate_activity.include_candidate_activity = true;
    CHECK(!has_reason(run_case(candidate_activity)),
          "candidate activity blocks recovery");
    CaseOptions primary_mismatch;
    primary_mismatch.primary_current = false;
    CHECK(!has_reason(run_case(primary_mismatch)),
          "primary identity mismatch blocks recovery");
    CaseOptions incomplete_primary;
    incomplete_primary.primary_covers = false;
    CHECK(!has_reason(run_case(incomplete_primary)),
          "incomplete primary coverage blocks recovery");
    CaseOptions multiple_vad;
    multiple_vad.containing_vad_count = 2;
    CHECK(preserves_primary(multiple_vad),
          "multiple containing VAD intervals preserve the primary identity");
    CaseOptions incomplete_vad;
    incomplete_vad.vad_robust_complete = false;
    CHECK(preserves_primary(incomplete_vad),
          "incomplete VAD evidence preserves the primary identity");
    CaseOptions insufficient_alignment;
    insufficient_alignment.enough_aligned_units = false;
    CHECK(preserves_primary(insufficient_alignment),
          "insufficient alignment preserves the primary identity");
  }

  // ---- 20g12b. FR37: a primary-owned short interval may recover one initial
  // slot identity only under exact A/C and A/B interval ranks, a gaplessly
  // bracketed primary island, and agreeing adjacent-phrase plus VAD evidence.
  // ----
  {
    using Entry = ComprehensiveTimeline::Entry;
    struct CaseOptions {
      bool include_initial_identity = true;
      bool include_competitor_activity = true;
      bool include_extra_activity = false;
      bool include_candidate_activity = false;
      bool primary_current = true;
      bool same_primary_brackets = true;
      bool gapless_primary_brackets = true;
      bool long_enough_primary_brackets = true;
      bool current_primary_is_short = true;
      bool enough_alignment = true;
      bool interval_session_competitor_second = true;
      bool interval_robust_initial_second = true;
      bool interval_eligible = true;
      int adjacent_phrase_count = 1;
      bool adjacent_boundary_matches = true;
      bool adjacent_initial_first = true;
      bool adjacent_exactly_one_margin = true;
      int containing_vad_count = 1;
      bool vad_initial_first = true;
      bool vad_eligible = true;
    };
    auto run_case = [](const CaseOptions& options) -> std::vector<Entry> {
      BusinessSpeakerPipeline::Config config;
      config.voiceprint_fusion_enabled = true;
      config.speaker_overlap_tie_policy =
          BusinessSpeakerPipeline::SpeakerOverlapTiePolicy::kPrimarySpeaker;
      TestBusinessSpeakerPipeline tl(config);

      std::vector<TestBusinessSpeakerPipeline::SpeakerInput> activity;
      if (options.include_initial_identity) {
        activity.push_back({-2.0, -1.0, "slot_a", 0.9f, "spk_b"});
      }
      activity.push_back({0.8, 1.8, "slot_a", 0.9f, "spk_a"});
      if (options.include_competitor_activity) {
        activity.push_back({0.8, 1.8, "slot_c", 0.8f, "spk_c"});
      }
      if (options.include_extra_activity) {
        activity.push_back({1.0, 1.4, "slot_d", 0.7f, "spk_d"});
      }
      if (options.include_candidate_activity) {
        activity.push_back({1.0, 1.4, "slot_b", 0.7f, "spk_b"});
      }
      tl.ReplaceSpeakers(activity);

      const double current_end = options.current_primary_is_short ? 1.8 : 2.6;
      const double previous_start =
          options.long_enough_primary_brackets ? 0.4 : 0.8;
      const double following_start =
          current_end + (options.gapless_primary_brackets ? 0.0 : 0.2);
      const double following_end =
          following_start +
          (options.long_enough_primary_brackets ? 0.6 : 0.2);
      tl.ReplacePrimarySpeakers(
          {{previous_start, 1.0, "slot_c", 0.9f, "spk_c"},
           {1.0, current_end, "slot_a", 0.9f,
            options.primary_current ? "spk_a" : "spk_c"},
           {following_start, following_end,
            options.same_primary_brackets ? "slot_c" : "slot_d", 0.9f,
            options.same_primary_brackets ? "spk_c" : "spk_d"}});

      tl.UpsertText(0, 0.2, 2.0, "甲乙，丙丁。");
      tl.UpsertAlign(
          0, 0.2, 2.0,
          options.enough_alignment
              ? std::vector<TestBusinessSpeakerPipeline::AlignUnitSeg>{
                    {0.2, 0.5, "甲"}, {0.5, 0.8, "乙"},
                    {1.0, 1.2, "丙"}, {1.2, 1.4, "丁"}}
              : std::vector<TestBusinessSpeakerPipeline::AlignUnitSeg>{
                    {0.2, 0.5, "甲"}, {0.5, 0.8, "乙"},
                    {1.0, 1.4, "丙丁"}});

      ComprehensiveTimeline::SpeakerVoiceprintEvidence interval;
      interval.evidence_id = "business_interval:0:1";
      interval.kind = "business_interval";
      interval.text_id = 0;
      interval.source_start = 3;
      interval.source_end = 5;
      interval.start = 1.0;
      interval.end = 1.4;
      interval.embedding_available = true;
      interval.robust_gallery_complete = true;
      const float interval_top = options.interval_eligible ? 0.70f : 0.52f;
      const float interval_second = options.interval_eligible ? 0.50f : 0.50f;
      interval.session_scores =
          options.interval_session_competitor_second
              ? std::vector<ComprehensiveTimeline::VoiceprintScore>{
                    {"spk_a", interval_top}, {"spk_c", interval_second},
                    {"spk_b", 0.30f}, {"spk_d", 0.20f}}
              : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                    {"spk_a", interval_top}, {"spk_d", interval_second},
                    {"spk_c", 0.30f}, {"spk_b", 0.20f}};
      interval.robust_scores =
          options.interval_robust_initial_second
              ? std::vector<ComprehensiveTimeline::VoiceprintScore>{
                    {"spk_a", interval_top}, {"spk_b", interval_second},
                    {"spk_c", 0.30f}, {"spk_d", 0.20f}}
              : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                    {"spk_a", interval_top}, {"spk_c", interval_second},
                    {"spk_b", 0.30f}, {"spk_d", 0.20f}};

      std::vector<ComprehensiveTimeline::SpeakerVoiceprintEvidence> evidence = {
          interval};
      for (int index = 0; index < options.adjacent_phrase_count; ++index) {
        ComprehensiveTimeline::SpeakerVoiceprintEvidence phrase;
        phrase.evidence_id =
            "punctuation_phrase:0:" + std::to_string(index);
        phrase.kind = "punctuation_phrase";
        phrase.text_id = 0;
        phrase.source_start = 0;
        phrase.source_end = 3;
        phrase.start = 0.2 - index * 0.01;
        phrase.end = options.adjacent_boundary_matches ? 1.0 : 0.8;
        phrase.embedding_available = true;
        phrase.robust_gallery_complete = true;
        const std::string first =
            options.adjacent_initial_first ? "spk_b" : "spk_a";
        const std::string second =
            options.adjacent_initial_first ? "spk_a" : "spk_b";
        phrase.session_scores = {
            {first, 0.50f}, {second, 0.48f}, {"spk_c", 0.20f}};
        phrase.robust_scores = options.adjacent_exactly_one_margin
                                   ? std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                         {first, 0.62f},
                                         {second, 0.50f},
                                         {"spk_c", 0.20f}}
                                   : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                         {first, 0.51f},
                                         {second, 0.49f},
                                         {"spk_c", 0.20f}};
        evidence.push_back(phrase);
      }
      for (int index = 0; index < options.containing_vad_count; ++index) {
        ComprehensiveTimeline::SpeakerVoiceprintEvidence vad;
        vad.evidence_id = "vad:" + std::to_string(index);
        vad.kind = "vad";
        vad.text_id = -1;
        vad.start = 0.0 - index * 0.01;
        vad.end = 1.6 + index * 0.01;
        vad.embedding_available = true;
        vad.robust_gallery_complete = true;
        const std::string first = options.vad_initial_first ? "spk_b" : "spk_a";
        const std::string second = options.vad_initial_first ? "spk_a" : "spk_b";
        const float vad_top = options.vad_eligible ? 0.70f : 0.52f;
        const float vad_second = options.vad_eligible ? 0.50f : 0.50f;
        vad.session_scores = {
            {first, vad_top}, {second, vad_second}, {"spk_c", 0.20f}};
        vad.robust_scores = {
            {first, vad_top - 0.01f}, {second, vad_second - 0.01f},
            {"spk_c", 0.20f}};
        evidence.push_back(vad);
      }
      tl.ReplaceVoiceprint(evidence);
      return tl.Snapshot();
    };

    const std::string reason =
        "voiceprint_interval_bracketed_primary_adjacent_vad_reconstruction";
    auto selected = [&](const std::vector<Entry>& entries) {
      return std::any_of(entries.begin(), entries.end(), [&](const auto& entry) {
        return entry.text.find("丙") != std::string::npos &&
               entry.speaker_id == "spk_b" &&
               entry.speaker_decision.reason == reason &&
               entry.speaker_decision.speaker_source ==
                   "sortformer_initial_slot+activity+bracketed_primary+"
                   "titanet_interval+titanet_adjacent_phrase+titanet_vad+"
                   "robust_gallery+forced_alignment";
      });
    };
    auto abstained = [&](const std::vector<Entry>& entries) {
      return std::none_of(entries.begin(), entries.end(), [&](const auto& entry) {
        return entry.speaker_decision.reason == reason;
      });
    };

    CHECK(selected(run_case({})),
          "bracketed primary plus adjacent phrase and VAD recovers initial id");
    CaseOptions missing_initial;
    missing_initial.include_initial_identity = false;
    CHECK(abstained(run_case(missing_initial)),
          "missing initial identity blocks interval reconstruction");
    CaseOptions missing_competitor;
    missing_competitor.include_competitor_activity = false;
    CHECK(abstained(run_case(missing_competitor)),
          "missing competitor activity blocks interval reconstruction");
    CaseOptions extra_activity;
    extra_activity.include_extra_activity = true;
    CHECK(abstained(run_case(extra_activity)),
          "additional activity blocks interval reconstruction");
    CaseOptions candidate_activity;
    candidate_activity.include_candidate_activity = true;
    CHECK(abstained(run_case(candidate_activity)),
          "current candidate activity blocks historical reconstruction");
    CaseOptions primary_mismatch;
    primary_mismatch.primary_current = false;
    CHECK(abstained(run_case(primary_mismatch)),
          "primary identity mismatch blocks interval reconstruction");
    CaseOptions changed_bracket;
    changed_bracket.same_primary_brackets = false;
    CHECK(abstained(run_case(changed_bracket)),
          "different primary brackets block interval reconstruction");
    CaseOptions primary_gap;
    primary_gap.gapless_primary_brackets = false;
    CHECK(abstained(run_case(primary_gap)),
          "a primary bracket gap blocks interval reconstruction");
    CaseOptions short_bracket;
    short_bracket.long_enough_primary_brackets = false;
    CHECK(abstained(run_case(short_bracket)),
          "subminimum primary brackets block interval reconstruction");
    CaseOptions long_current;
    long_current.current_primary_is_short = false;
    CHECK(abstained(run_case(long_current)),
          "a sustained current-primary run blocks historical reconstruction");
    CaseOptions insufficient_alignment;
    insufficient_alignment.enough_alignment = false;
    CHECK(abstained(run_case(insufficient_alignment)),
          "insufficient aligned units block interval reconstruction");
    CaseOptions session_order;
    session_order.interval_session_competitor_second = false;
    CHECK(abstained(run_case(session_order)),
          "a different interval session runner-up blocks reconstruction");
    CaseOptions robust_order;
    robust_order.interval_robust_initial_second = false;
    CHECK(abstained(run_case(robust_order)),
          "a different interval robust runner-up blocks reconstruction");
    CaseOptions weak_interval;
    weak_interval.interval_eligible = false;
    CHECK(abstained(run_case(weak_interval)),
          "an abstaining interval stays on the ordinary evidence path");
    CaseOptions missing_phrase;
    missing_phrase.adjacent_phrase_count = 0;
    CHECK(abstained(run_case(missing_phrase)),
          "missing adjacent phrase blocks interval reconstruction");
    CaseOptions duplicate_phrase;
    duplicate_phrase.adjacent_phrase_count = 2;
    CHECK(abstained(run_case(duplicate_phrase)),
          "duplicate adjacent phrases block interval reconstruction");
    CaseOptions phrase_boundary;
    phrase_boundary.adjacent_boundary_matches = false;
    CHECK(abstained(run_case(phrase_boundary)),
          "a nonadjacent phrase boundary blocks interval reconstruction");
    CaseOptions phrase_rank;
    phrase_rank.adjacent_initial_first = false;
    CHECK(abstained(run_case(phrase_rank)),
          "a current-first adjacent phrase blocks interval reconstruction");
    CaseOptions phrase_gate;
    phrase_gate.adjacent_exactly_one_margin = false;
    CHECK(abstained(run_case(phrase_gate)),
          "a different adjacent margin pattern blocks reconstruction");
    CaseOptions missing_vad;
    missing_vad.containing_vad_count = 0;
    CHECK(abstained(run_case(missing_vad)),
          "missing containing VAD blocks interval reconstruction");
    CaseOptions duplicate_vad;
    duplicate_vad.containing_vad_count = 2;
    CHECK(abstained(run_case(duplicate_vad)),
          "duplicate containing VAD blocks interval reconstruction");
    CaseOptions vad_rank;
    vad_rank.vad_initial_first = false;
    CHECK(abstained(run_case(vad_rank)),
          "a current-first VAD blocks interval reconstruction");
    CaseOptions weak_vad;
    weak_vad.vad_eligible = false;
    CHECK(abstained(run_case(weak_vad)),
          "an abstaining VAD blocks interval reconstruction");
  }

  // ---- 20g12c. FR38: a source-leading punctuation phrase may recover only
  // one cross-VAD, no-embedding tail character while preserving the following
  // native clause.
  // ----
  {
    using Entry = ComprehensiveTimeline::Entry;
    struct CaseOptions {
      bool phrase_starts_at_source_zero = true;
      bool valid_tail_source_shape = true;
      bool tail_embedding = false;
      bool include_following = true;
      bool following_embedding = true;
      bool following_same_identity = true;
      bool native_primary_slots_match = true;
      bool include_competing_activity = false;
      bool leading_interval_eligible = true;
      bool leading_robust_order = true;
      bool phrase_rank_pattern = true;
      bool phrase_score_abstains = true;
      bool phrase_exactly_one_margin = true;
      bool include_tail_unit = true;
      bool duplicate_tail_unit = false;
      int leading_vad_count = 1;
      bool leading_vad_rank_pattern = true;
      bool leading_vad_exactly_one_margin = true;
      int tail_vad_count = 1;
      bool tail_vad_rank_pattern = true;
      bool tail_vad_eligible = true;
      bool tail_vad_starts_near = true;
      bool tail_vad_extends_following = true;
      bool vad_gap_isolated = true;
      bool following_rank_native = true;
    };
    auto run_case = [](const CaseOptions& options) -> std::vector<Entry> {
      BusinessSpeakerPipeline::Config config;
      config.voiceprint_fusion_enabled = true;
      config.speaker_overlap_tie_policy =
          BusinessSpeakerPipeline::SpeakerOverlapTiePolicy::kPrimarySpeaker;
      TestBusinessSpeakerPipeline tl(config);

      const std::string following_identity =
          options.following_same_identity ? "spk_c" : "spk_d";
      std::vector<TestBusinessSpeakerPipeline::SpeakerInput> activity = {
          {0.1, 1.0, "slot_a", 0.9f, "spk_a"},
          {1.5, options.following_same_identity ? 4.0 : 1.75, "slot_c",
           0.9f, "spk_c"}};
      if (!options.following_same_identity) {
        activity.push_back({1.75, 4.0, "slot_d", 0.9f, "spk_d"});
      }
      if (options.include_competing_activity) {
        activity.push_back({1.6, 1.72, "slot_x", 0.7f, "spk_x"});
      }
      tl.ReplaceSpeakers(activity);

      const std::string primary_c_slot =
          options.native_primary_slots_match ? "slot_c" : "slot_x";
      std::vector<TestBusinessSpeakerPipeline::SpeakerInput> primary = {
          {0.1, 1.0, "slot_a", 0.9f, "spk_a"},
          {1.5, options.following_same_identity ? 4.0 : 1.75,
           primary_c_slot, 0.9f, "spk_c"}};
      if (!options.following_same_identity) {
        primary.push_back({1.75, 4.0, "slot_d", 0.9f, "spk_d"});
      }
      tl.ReplacePrimarySpeakers(primary);

      tl.UpsertText(0, 0.0, 4.0, "甲乙丙丁戊，己庚辛。");
      tl.UpsertAlign(0, 0.0, 4.0,
                     {{0.2, 0.3, "甲"}, {0.3, 0.4, "乙"},
                      {0.4, 0.5, "丙"}, {0.5, 0.8, "丁"},
                      {1.6, 1.72, "戊"}, {1.8, 2.0, "己"},
                      {2.0, 2.2, "庚"}, {2.2, 2.4, "辛"}});

      ComprehensiveTimeline::SpeakerVoiceprintEvidence leading;
      leading.evidence_id = "business_interval:0:0";
      leading.kind = "business_interval";
      leading.text_id = 0;
      leading.source_start = 0;
      leading.source_end = 4;
      leading.start = 0.2;
      leading.end = 0.8;
      leading.embedding_available = true;
      leading.robust_gallery_complete = true;
      const float leading_top =
          options.leading_interval_eligible ? 0.70f : 0.52f;
      const float leading_second =
          options.leading_interval_eligible ? 0.50f : 0.50f;
      leading.session_scores = {{"spk_b", leading_top},
                                {"spk_a", leading_second},
                                {"spk_c", 0.20f}};
      leading.robust_scores = options.leading_robust_order
                                  ? std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                        {"spk_b", leading_top - 0.01f},
                                        {"spk_a", leading_second - 0.01f},
                                        {"spk_c", 0.20f}}
                                  : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                        {"spk_a", leading_top - 0.01f},
                                        {"spk_b", leading_second - 0.01f},
                                        {"spk_c", 0.20f}};

      const int phrase_source_end =
          options.valid_tail_source_shape ? 6 : 7;
      ComprehensiveTimeline::SpeakerVoiceprintEvidence tail;
      tail.evidence_id = "business_interval:0:1";
      tail.kind = "business_interval";
      tail.text_id = 0;
      tail.source_start = 4;
      tail.source_end = phrase_source_end;
      tail.start = 1.6;
      tail.end = 1.72;
      tail.embedding_available = options.tail_embedding;

      ComprehensiveTimeline::SpeakerVoiceprintEvidence following;
      following.evidence_id = "business_interval:0:2";
      following.kind = "business_interval";
      following.text_id = 0;
      following.source_start = phrase_source_end;
      following.source_end = 10;
      following.start = 1.8;
      following.end = 3.8;
      following.embedding_available = options.following_embedding;
      following.robust_gallery_complete = true;
      const std::string following_top =
          options.following_rank_native ? following_identity : "spk_b";
      const std::string following_second =
          options.following_rank_native ? "spk_a" : following_identity;
      following.session_scores = {{following_top, 0.53f},
                                  {following_second, 0.31f},
                                  {"spk_b", 0.20f}};
      following.robust_scores = {{following_top, 0.52f},
                                 {following_second, 0.30f},
                                 {"spk_b", 0.20f}};

      ComprehensiveTimeline::SpeakerVoiceprintEvidence phrase;
      phrase.evidence_id = "punctuation_phrase:0:0";
      phrase.kind = "punctuation_phrase";
      phrase.text_id = 0;
      phrase.source_start = options.phrase_starts_at_source_zero ? 0 : 1;
      phrase.source_end = phrase_source_end;
      phrase.start = 0.2;
      phrase.end = 1.72;
      phrase.embedding_available = true;
      phrase.robust_gallery_complete = true;
      const float phrase_session_top =
          options.phrase_score_abstains ? 0.50f : 0.62f;
      phrase.session_scores = options.phrase_rank_pattern
                                  ? std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                        {"spk_a", phrase_session_top},
                                        {"spk_b", 0.45f},
                                        {"spk_c", 0.20f}}
                                  : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                        {"spk_b", phrase_session_top},
                                        {"spk_a", 0.45f},
                                        {"spk_c", 0.20f}};
      phrase.robust_scores =
          options.phrase_exactly_one_margin
              ? std::vector<ComprehensiveTimeline::VoiceprintScore>{
                    {"spk_b", 0.50f}, {"spk_a", 0.49f},
                    {"spk_c", 0.20f}}
              : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                    {"spk_b", 0.52f}, {"spk_a", 0.47f},
                    {"spk_c", 0.20f}};

      std::vector<ComprehensiveTimeline::SpeakerVoiceprintEvidence> evidence =
          {leading, tail, phrase};
      if (options.include_following) evidence.push_back(following);

      if (options.include_tail_unit) {
        ComprehensiveTimeline::SpeakerVoiceprintEvidence unit;
        unit.evidence_id = "aligned_unit:0:4";
        unit.kind = "aligned_unit";
        unit.text_id = 0;
        unit.source_start = 4;
        unit.source_end = 5;
        unit.start = 1.6;
        unit.end = 1.72;
        evidence.push_back(unit);
        if (options.duplicate_tail_unit) {
          unit.evidence_id = "aligned_unit:0:4-duplicate";
          evidence.push_back(unit);
        }
      }

      for (int index = 0; index < options.leading_vad_count; ++index) {
        ComprehensiveTimeline::SpeakerVoiceprintEvidence vad;
        vad.evidence_id = "vad:leading:" + std::to_string(index);
        vad.kind = "vad";
        vad.text_id = -1;
        vad.start = 0.1 - index * 0.01;
        vad.end = (options.vad_gap_isolated ? 0.9 : 1.4) + index * 0.01;
        vad.embedding_available = true;
        vad.robust_gallery_complete = true;
        const std::string first =
            options.leading_vad_rank_pattern ? "spk_b" : "spk_a";
        const std::string second =
            options.leading_vad_rank_pattern ? "spk_a" : "spk_b";
        vad.session_scores = {
            {first, 0.55f}, {second, 0.53f}, {"spk_c", 0.20f}};
        vad.robust_scores =
            options.leading_vad_exactly_one_margin
                ? std::vector<ComprehensiveTimeline::VoiceprintScore>{
                      {first, 0.62f}, {second, 0.54f}, {"spk_c", 0.20f}}
                : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                      {first, 0.55f}, {second, 0.53f}, {"spk_c", 0.20f}};
        evidence.push_back(vad);
      }

      for (int index = 0; index < options.tail_vad_count; ++index) {
        ComprehensiveTimeline::SpeakerVoiceprintEvidence vad;
        vad.evidence_id = "vad:tail:" + std::to_string(index);
        vad.kind = "vad";
        vad.text_id = -1;
        vad.start =
            (options.tail_vad_starts_near ? 1.55 : 1.40) - index * 0.01;
        vad.end =
            (options.tail_vad_extends_following ? 2.4 : 1.75) + index * 0.01;
        vad.embedding_available = true;
        vad.robust_gallery_complete = true;
        const std::string first =
            options.tail_vad_rank_pattern ? "spk_c" : "spk_b";
        const std::string second =
            options.tail_vad_rank_pattern ? "spk_b" : "spk_c";
        const float top = options.tail_vad_eligible ? 0.70f : 0.52f;
        const float runner_up = options.tail_vad_eligible ? 0.50f : 0.50f;
        vad.session_scores = {
            {first, top}, {second, runner_up}, {"spk_a", 0.20f}};
        vad.robust_scores = {{first, top - 0.01f},
                             {second, runner_up - 0.01f},
                             {"spk_a", 0.20f}};
        evidence.push_back(vad);
      }
      tl.ReplaceVoiceprint(evidence);
      return tl.Snapshot();
    };

    const std::string reason =
        "voiceprint_cross_vad_phrase_tail_reconstruction";
    auto selected = [&](const std::vector<Entry>& entries) {
      bool tail_selected = false;
      bool following_preserved = false;
      for (const auto& entry : entries) {
        if (entry.text == "戊，" && entry.speaker_id == "spk_b" &&
            entry.speaker_decision.reason == reason &&
            entry.speaker_decision.speaker_source ==
                "sortformer_activity+primary_top1+titanet_interval+"
                "titanet_phrase+titanet_vad+robust_gallery+"
                "forced_alignment") {
          tail_selected = true;
        }
        if (entry.text.find("己庚辛") != std::string::npos &&
            entry.speaker_id == "spk_c" &&
            entry.speaker_decision.reason != reason) {
          following_preserved = true;
        }
      }
      return tail_selected && following_preserved;
    };
    auto abstained = [&](const std::vector<Entry>& entries) {
      return std::none_of(entries.begin(), entries.end(), [&](const auto& entry) {
        return entry.speaker_decision.reason == reason;
      });
    };

    CHECK(selected(run_case({})),
          "cross-VAD phrase tail is restored without repainting next clause");
    CaseOptions nonleading_phrase;
    nonleading_phrase.phrase_starts_at_source_zero = false;
    CHECK(abstained(run_case(nonleading_phrase)),
          "a nonleading phrase blocks tail reconstruction");
    CaseOptions invalid_tail_shape;
    invalid_tail_shape.valid_tail_source_shape = false;
    CHECK(abstained(run_case(invalid_tail_shape)),
          "multiple visible tail characters block reconstruction");
    CaseOptions embedded_tail;
    embedded_tail.tail_embedding = true;
    CHECK(abstained(run_case(embedded_tail)),
          "an independently embedded tail keeps ordinary behavior");
    CaseOptions missing_following;
    missing_following.include_following = false;
    CHECK(abstained(run_case(missing_following)),
          "missing following interval blocks reconstruction");
    CaseOptions unembedded_following;
    unembedded_following.following_embedding = false;
    CHECK(abstained(run_case(unembedded_following)),
          "an unembedded following interval blocks reconstruction");
    CaseOptions changed_following_identity;
    changed_following_identity.following_same_identity = false;
    CHECK(abstained(run_case(changed_following_identity)),
          "a different following native identity blocks reconstruction");
    CaseOptions primary_slot_mismatch;
    primary_slot_mismatch.native_primary_slots_match = false;
    CHECK(abstained(run_case(primary_slot_mismatch)),
          "activity and primary local-slot mismatch blocks reconstruction");
    CaseOptions competing_activity;
    competing_activity.include_competing_activity = true;
    CHECK(abstained(run_case(competing_activity)),
          "competing tail activity blocks reconstruction");
    CaseOptions weak_leading;
    weak_leading.leading_interval_eligible = false;
    CHECK(abstained(run_case(weak_leading)),
          "an abstaining leading interval blocks reconstruction");
    CaseOptions leading_order;
    leading_order.leading_robust_order = false;
    CHECK(abstained(run_case(leading_order)),
          "a changed leading robust order blocks reconstruction");
    CaseOptions phrase_order;
    phrase_order.phrase_rank_pattern = false;
    CHECK(abstained(run_case(phrase_order)),
          "a changed phrase top-two order blocks reconstruction");
    CaseOptions eligible_phrase;
    eligible_phrase.phrase_score_abstains = false;
    CHECK(abstained(run_case(eligible_phrase)),
          "an independently eligible phrase stays on its ordinary path");
    CaseOptions phrase_margins;
    phrase_margins.phrase_exactly_one_margin = false;
    CHECK(abstained(run_case(phrase_margins)),
          "a changed phrase margin pattern blocks reconstruction");
    CaseOptions missing_unit;
    missing_unit.include_tail_unit = false;
    CHECK(abstained(run_case(missing_unit)),
          "missing tail alignment blocks reconstruction");
    CaseOptions duplicate_unit;
    duplicate_unit.duplicate_tail_unit = true;
    CHECK(abstained(run_case(duplicate_unit)),
          "duplicate tail alignment blocks reconstruction");
    CaseOptions missing_leading_vad;
    missing_leading_vad.leading_vad_count = 0;
    CHECK(abstained(run_case(missing_leading_vad)),
          "missing leading VAD blocks reconstruction");
    CaseOptions duplicate_leading_vad;
    duplicate_leading_vad.leading_vad_count = 2;
    CHECK(abstained(run_case(duplicate_leading_vad)),
          "duplicate leading VAD blocks reconstruction");
    CaseOptions leading_vad_order;
    leading_vad_order.leading_vad_rank_pattern = false;
    CHECK(abstained(run_case(leading_vad_order)),
          "a changed leading VAD order blocks reconstruction");
    CaseOptions leading_vad_margins;
    leading_vad_margins.leading_vad_exactly_one_margin = false;
    CHECK(abstained(run_case(leading_vad_margins)),
          "a changed leading VAD margin pattern blocks reconstruction");
    CaseOptions missing_tail_vad;
    missing_tail_vad.tail_vad_count = 0;
    CHECK(abstained(run_case(missing_tail_vad)),
          "missing tail VAD blocks reconstruction");
    CaseOptions duplicate_tail_vad;
    duplicate_tail_vad.tail_vad_count = 2;
    CHECK(abstained(run_case(duplicate_tail_vad)),
          "duplicate tail VAD blocks reconstruction");
    CaseOptions tail_vad_order;
    tail_vad_order.tail_vad_rank_pattern = false;
    CHECK(abstained(run_case(tail_vad_order)),
          "a changed tail VAD order blocks reconstruction");
    CaseOptions weak_tail_vad;
    weak_tail_vad.tail_vad_eligible = false;
    CHECK(abstained(run_case(weak_tail_vad)),
          "an abstaining tail VAD blocks reconstruction");
    CaseOptions distant_tail_vad;
    distant_tail_vad.tail_vad_starts_near = false;
    CHECK(abstained(run_case(distant_tail_vad)),
          "a tail VAD outside alignment tolerance blocks reconstruction");
    CaseOptions short_tail_vad;
    short_tail_vad.tail_vad_extends_following = false;
    CHECK(abstained(run_case(short_tail_vad)),
          "a tail VAD that excludes the following clause blocks reconstruction");
    CaseOptions short_vad_gap;
    short_vad_gap.vad_gap_isolated = false;
    CHECK(abstained(run_case(short_vad_gap)),
          "a sub-pause VAD gap blocks cross-VAD reconstruction");
    CaseOptions following_order;
    following_order.following_rank_native = false;
    CHECK(abstained(run_case(following_order)),
          "a changed following interval rank blocks reconstruction");
  }

  // ---- 20g12d. FR39: isolate a source-leading exact phrase from one
  // independently aligned tail response under the full cross-scale topology.
  // ----
  {
    using Entry = ComprehensiveTimeline::Entry;
    using VoiceprintScore = ComprehensiveTimeline::VoiceprintScore;
    struct CaseOptions {
      bool phrase_starts_at_zero = true;
      bool phrase_embedding = true;
      bool phrase_robust_complete = true;
      bool phrase_candidate_first = true;
      bool phrase_score_passes = true;
      int phrase_margin_pass_count = 0;
      bool interval_full_source = true;
      bool interval_embedding = true;
      bool interval_current_first = true;
      bool interval_margins_abstain = true;
      bool valid_tail_shape = true;
      int tail_unit_count = 1;
      bool tail_pause_passes = true;
      bool tail_end_matches = true;
      bool initial_identity_differs = true;
      bool competing_activity = false;
      bool phrase_primary_slot_matches = true;
      bool duplicate_phrase_primary = false;
      int tail_primary_count = 1;
      bool tail_primary_distinct = true;
      int containing_vad_count = 1;
      bool vad_contains_interval = true;
      bool vad_robust_complete = true;
      bool vad_cross_order = true;
      bool vad_gates_abstain = true;
      int complete_source_count = 1;
      bool complete_contains_interval = true;
      bool complete_robust_complete = true;
      bool complete_expected_pair = true;
      bool complete_candidate_top = true;
      bool complete_gates_abstain = true;
    };
    auto run_case = [](const CaseOptions& options) -> std::vector<Entry> {
      BusinessSpeakerPipeline::Config config;
      config.voiceprint_fusion_enabled = true;
      config.speaker_overlap_tie_policy =
          BusinessSpeakerPipeline::SpeakerOverlapTiePolicy::kPrimarySpeaker;
      TestBusinessSpeakerPipeline tl(config);

      const std::string source = options.valid_tail_shape
                                     ? "甲乙丙丁戊己庚。辛。"
                                     : "甲乙丙丁戊己庚。辛壬。";
      const int source_size = options.valid_tail_shape ? 10 : 11;
      const std::string initial_identity =
          options.initial_identity_differs ? "spk_a" : "spk_b";
      std::vector<TestBusinessSpeakerPipeline::SpeakerInput> activity = {
          {-2.0, -1.0, "slot_current", 0.9f, initial_identity},
          {0.0, 2.2, "slot_current", 0.9f, "spk_b"}};
      if (options.competing_activity) {
        activity.push_back({0.2, 1.5, "slot_x", 0.8f, "spk_x"});
      }
      tl.ReplaceSpeakers(activity);

      const std::string phrase_primary_slot =
          options.phrase_primary_slot_matches ? "slot_current" : "slot_x";
      std::vector<TestBusinessSpeakerPipeline::SpeakerInput> primary = {
          {0.1, 1.1, phrase_primary_slot, 0.9f, "spk_b"}};
      if (options.duplicate_phrase_primary) {
        primary.push_back({0.2, 1.0, "slot_x", 0.8f, "spk_b"});
      }
      for (int index = 0; index < options.tail_primary_count; ++index) {
        primary.push_back(
            {1.3, 1.6, "slot_tail_" + std::to_string(index), 0.9f,
             options.tail_primary_distinct ? "spk_c" : "spk_b"});
      }
      tl.ReplacePrimarySpeakers(primary);

      tl.UpsertText(0, 0.0, 2.0, source);
      std::vector<ComprehensiveTimeline::AlignUnitSeg> alignment = {
          {0.2, 0.3, "甲"}, {0.3, 0.4, "乙"}, {0.4, 0.5, "丙"},
          {0.5, 0.6, "丁"}, {0.6, 0.7, "戊"}, {0.7, 0.8, "己"},
          {0.8, 1.0, "庚"},
          {options.tail_pause_passes ? 1.4 : 1.2,
           options.tail_end_matches ? 1.5 : 1.48, "辛"}};
      if (!options.valid_tail_shape) {
        alignment.push_back({1.5, 1.58, "壬"});
      }
      tl.UpsertAlign(0, 0.0, 2.0, alignment);

      ComprehensiveTimeline::SpeakerVoiceprintEvidence interval;
      interval.evidence_id = "business_interval:0:0";
      interval.kind = "business_interval";
      interval.text_id = 0;
      interval.source_start = 0;
      interval.source_end =
          options.interval_full_source ? source_size : source_size - 1;
      interval.start = 0.2;
      interval.end = 1.5;
      interval.embedding_available = options.interval_embedding;
      interval.robust_gallery_complete = true;
      interval.session_scores = options.interval_current_first
                                    ? std::vector<VoiceprintScore>{
                                          {"spk_b", 0.43f},
                                          {"spk_a", 0.40f},
                                          {"spk_c", 0.20f}}
                                    : std::vector<VoiceprintScore>{
                                          {"spk_a", 0.43f},
                                          {"spk_b", 0.40f},
                                          {"spk_c", 0.20f}};
      const float interval_robust_top =
          options.interval_margins_abstain ? 0.42f : 0.50f;
      interval.robust_scores = options.interval_current_first
                                   ? std::vector<VoiceprintScore>{
                                         {"spk_b", interval_robust_top},
                                         {"spk_a", 0.40f},
                                         {"spk_c", 0.20f}}
                                   : std::vector<VoiceprintScore>{
                                         {"spk_a", interval_robust_top},
                                         {"spk_b", 0.40f},
                                         {"spk_c", 0.20f}};
      if (!options.interval_margins_abstain) {
        interval.session_scores[0].score = 0.50f;
      }

      ComprehensiveTimeline::SpeakerVoiceprintEvidence phrase;
      phrase.evidence_id = "punctuation_phrase:0:0";
      phrase.kind = "punctuation_phrase";
      phrase.text_id = 0;
      phrase.source_start = options.phrase_starts_at_zero ? 0 : 1;
      phrase.source_end = 8;
      phrase.start = 0.2;
      phrase.end = 1.0;
      phrase.embedding_available = options.phrase_embedding;
      phrase.robust_gallery_complete = options.phrase_robust_complete;
      const float phrase_top = options.phrase_score_passes ? 0.42f : -0.01f;
      const float phrase_second = options.phrase_score_passes ? 0.40f : -0.02f;
      phrase.session_scores = options.phrase_candidate_first
                                  ? std::vector<VoiceprintScore>{
                                        {"spk_a", phrase_top},
                                        {"spk_b", phrase_second},
                                        {"spk_c", -0.20f}}
                                  : std::vector<VoiceprintScore>{
                                        {"spk_b", phrase_top},
                                        {"spk_a", phrase_second},
                                        {"spk_c", -0.20f}};
      phrase.robust_scores = phrase.session_scores;
      if (options.phrase_margin_pass_count >= 1) {
        phrase.session_scores[0].score = 0.48f;
      }
      if (options.phrase_margin_pass_count >= 2) {
        phrase.robust_scores[0].score = 0.48f;
      }

      ComprehensiveTimeline::SpeakerVoiceprintEvidence tail_unit;
      tail_unit.evidence_id = "aligned_unit:0:8";
      tail_unit.kind = "aligned_unit";
      tail_unit.text_id = 0;
      tail_unit.source_start = 8;
      tail_unit.source_end = 9;
      tail_unit.start = options.tail_pause_passes ? 1.4 : 1.2;
      tail_unit.end = options.tail_end_matches ? 1.5 : 1.48;

      std::vector<ComprehensiveTimeline::SpeakerVoiceprintEvidence> evidence =
          {interval, phrase};
      for (int index = 0; index < options.tail_unit_count; ++index) {
        tail_unit.evidence_id = "aligned_unit:0:8:" + std::to_string(index);
        evidence.push_back(tail_unit);
      }

      for (int index = 0; index < options.containing_vad_count; ++index) {
        ComprehensiveTimeline::SpeakerVoiceprintEvidence vad;
        vad.evidence_id = "vad:tail-isolation:" + std::to_string(index);
        vad.kind = "vad";
        vad.text_id = -1;
        vad.start = options.vad_contains_interval ? 0.1 - index * 0.01 : 0.4;
        vad.end = 1.7 + index * 0.01;
        vad.embedding_available = true;
        vad.robust_gallery_complete = options.vad_robust_complete;
        vad.session_scores = options.vad_cross_order
                                 ? std::vector<VoiceprintScore>{
                                       {"spk_a", 0.39f},
                                       {"spk_b", 0.38f},
                                       {"spk_c", 0.20f}}
                                 : std::vector<VoiceprintScore>{
                                       {"spk_b", 0.39f},
                                       {"spk_a", 0.38f},
                                       {"spk_c", 0.20f}};
        vad.robust_scores = {{"spk_b", 0.40f},
                             {"spk_a", 0.39f},
                             {"spk_c", 0.20f}};
        if (!options.vad_gates_abstain) vad.session_scores[0].score = 0.60f;
        evidence.push_back(vad);
      }

      for (int index = 0; index < options.complete_source_count; ++index) {
        ComprehensiveTimeline::SpeakerVoiceprintEvidence complete;
        complete.evidence_id =
            "complete_source:0:" + std::to_string(index);
        complete.kind = "complete_source";
        complete.text_id = 0;
        complete.source_start = 0;
        complete.source_end = source_size;
        complete.start = options.complete_contains_interval ? 0.0 : 0.4;
        complete.end = 2.0;
        complete.embedding_available = true;
        complete.robust_gallery_complete =
            options.complete_robust_complete;
        complete.session_scores = options.complete_expected_pair
                                      ? std::vector<VoiceprintScore>{
                                            {"spk_a", 0.50f},
                                            {"spk_b", 0.48f},
                                            {"spk_c", 0.20f}}
                                      : std::vector<VoiceprintScore>{
                                            {"spk_a", 0.50f},
                                            {"spk_c", 0.48f},
                                            {"spk_b", 0.20f}};
        complete.robust_scores = options.complete_candidate_top
                                     ? complete.session_scores
                                     : std::vector<VoiceprintScore>{
                                           {"spk_b", 0.50f},
                                           {"spk_a", 0.48f},
                                           {"spk_c", 0.20f}};
        if (!options.complete_candidate_top) {
          complete.session_scores = complete.robust_scores;
        }
        if (!options.complete_gates_abstain) {
          complete.session_scores[0].score = 0.60f;
        }
        evidence.push_back(complete);
      }
      tl.ReplaceVoiceprint(evidence);
      return tl.Snapshot();
    };

    const std::string reason =
        "voiceprint_phrase_source_leading_tail_isolation_override";
    auto selected = [&](const std::vector<Entry>& entries) {
      bool phrase_restored = false;
      bool tail_preserved = false;
      for (const auto& entry : entries) {
        if (entry.text == "甲乙丙丁戊己庚。" &&
            entry.speaker_id == "spk_a" &&
            entry.speaker_decision.reason == reason &&
            entry.speaker_decision.speaker_source ==
                "sortformer_activity+primary_top1+initial_slot+"
                "titanet_interval+titanet_phrase+titanet_vad+"
                "titanet_complete_source+robust_gallery+forced_alignment") {
          phrase_restored = true;
        }
        if (entry.text == "辛。" && entry.speaker_id == "spk_b" &&
            entry.speaker_decision.reason != reason) {
          tail_preserved = true;
        }
      }
      return phrase_restored && tail_preserved;
    };
    auto abstained = [&](const std::vector<Entry>& entries) {
      return std::none_of(entries.begin(), entries.end(), [&](const auto& entry) {
        return entry.speaker_decision.reason == reason;
      });
    };

    CHECK(selected(run_case({})),
          "source-leading phrase is isolated from its aligned tail");
    CaseOptions one_phrase_margin;
    one_phrase_margin.phrase_margin_pass_count = 1;
    CHECK(selected(run_case(one_phrase_margin)),
          "one eligible phrase margin preserves the partition-stable case");
    CaseOptions nonleading;
    nonleading.phrase_starts_at_zero = false;
    CHECK(abstained(run_case(nonleading)),
          "a nonleading phrase blocks tail isolation");
    CaseOptions unembedded_phrase;
    unembedded_phrase.phrase_embedding = false;
    CHECK(abstained(run_case(unembedded_phrase)),
          "missing exact phrase embedding blocks tail isolation");
    CaseOptions incomplete_phrase_gallery;
    incomplete_phrase_gallery.phrase_robust_complete = false;
    CHECK(abstained(run_case(incomplete_phrase_gallery)),
          "an incomplete phrase gallery blocks tail isolation");
    CaseOptions changed_phrase_order;
    changed_phrase_order.phrase_candidate_first = false;
    CHECK(abstained(run_case(changed_phrase_order)),
          "a changed phrase top-two order blocks tail isolation");
    CaseOptions weak_phrase;
    weak_phrase.phrase_score_passes = false;
    CHECK(abstained(run_case(weak_phrase)),
          "a phrase failing the short score gate blocks tail isolation");
    CaseOptions ordinary_phrase;
    ordinary_phrase.phrase_margin_pass_count = 2;
    CHECK(abstained(run_case(ordinary_phrase)),
          "a normally eligible phrase stays on its ordinary path");
    CaseOptions partial_interval;
    partial_interval.interval_full_source = false;
    CHECK(abstained(run_case(partial_interval)),
          "a partial containing interval blocks tail isolation");
    CaseOptions unembedded_interval;
    unembedded_interval.interval_embedding = false;
    CHECK(abstained(run_case(unembedded_interval)),
          "missing interval embedding blocks tail isolation");
    CaseOptions changed_interval_order;
    changed_interval_order.interval_current_first = false;
    CHECK(abstained(run_case(changed_interval_order)),
          "a changed interval top-two order blocks tail isolation");
    CaseOptions eligible_interval;
    eligible_interval.interval_margins_abstain = false;
    CHECK(abstained(run_case(eligible_interval)),
          "an eligible interval preserves ordinary direct provenance");
    CaseOptions invalid_tail;
    invalid_tail.valid_tail_shape = false;
    CHECK(abstained(run_case(invalid_tail)),
          "multiple visible tail characters block isolation");
    CaseOptions missing_tail_unit;
    missing_tail_unit.tail_unit_count = 0;
    CHECK(abstained(run_case(missing_tail_unit)),
          "missing tail alignment blocks isolation");
    CaseOptions duplicate_tail_unit;
    duplicate_tail_unit.tail_unit_count = 2;
    CHECK(abstained(run_case(duplicate_tail_unit)),
          "duplicate tail alignment blocks isolation");
    CaseOptions short_tail_pause;
    short_tail_pause.tail_pause_passes = false;
    CHECK(abstained(run_case(short_tail_pause)),
          "a tail inside the configured pause blocks isolation");
    CaseOptions mismatched_tail_end;
    mismatched_tail_end.tail_end_matches = false;
    CHECK(abstained(run_case(mismatched_tail_end)),
          "a tail not closing the interval blocks isolation");
    CaseOptions unchanged_initial;
    unchanged_initial.initial_identity_differs = false;
    CHECK(abstained(run_case(unchanged_initial)),
          "a slot without an initial identity change blocks isolation");
    CaseOptions competing_activity;
    competing_activity.competing_activity = true;
    CHECK(abstained(run_case(competing_activity)),
          "competing activity blocks tail isolation");
    CaseOptions changed_phrase_slot;
    changed_phrase_slot.phrase_primary_slot_matches = false;
    CHECK(abstained(run_case(changed_phrase_slot)),
          "activity and phrase-primary slot mismatch blocks isolation");
    CaseOptions duplicate_phrase_primary;
    duplicate_phrase_primary.duplicate_phrase_primary = true;
    CHECK(abstained(run_case(duplicate_phrase_primary)),
          "competing phrase primary blocks isolation");
    CaseOptions missing_tail_primary;
    missing_tail_primary.tail_primary_count = 0;
    CHECK(abstained(run_case(missing_tail_primary)),
          "missing tail primary blocks isolation");
    CaseOptions duplicate_tail_primary;
    duplicate_tail_primary.tail_primary_count = 2;
    CHECK(abstained(run_case(duplicate_tail_primary)),
          "competing tail primary blocks isolation");
    CaseOptions same_tail_identity;
    same_tail_identity.tail_primary_distinct = false;
    CHECK(abstained(run_case(same_tail_identity)),
          "a non-distinct tail identity blocks isolation");
    CaseOptions missing_vad;
    missing_vad.containing_vad_count = 0;
    CHECK(abstained(run_case(missing_vad)),
          "missing containing VAD blocks isolation");
    CaseOptions duplicate_vad;
    duplicate_vad.containing_vad_count = 2;
    CHECK(abstained(run_case(duplicate_vad)),
          "duplicate containing VAD blocks isolation");
    CaseOptions partial_vad;
    partial_vad.vad_contains_interval = false;
    CHECK(abstained(run_case(partial_vad)),
          "a partial VAD blocks isolation");
    CaseOptions incomplete_vad;
    incomplete_vad.vad_robust_complete = false;
    CHECK(abstained(run_case(incomplete_vad)),
          "an incomplete VAD gallery blocks isolation");
    CaseOptions changed_vad_order;
    changed_vad_order.vad_cross_order = false;
    CHECK(abstained(run_case(changed_vad_order)),
          "a changed VAD cross-order blocks isolation");
    CaseOptions eligible_vad;
    eligible_vad.vad_gates_abstain = false;
    CHECK(abstained(run_case(eligible_vad)),
          "an eligible VAD blocks the abstention topology");
    CaseOptions missing_complete;
    missing_complete.complete_source_count = 0;
    CHECK(abstained(run_case(missing_complete)),
          "missing complete-source evidence blocks isolation");
    CaseOptions duplicate_complete;
    duplicate_complete.complete_source_count = 2;
    CHECK(abstained(run_case(duplicate_complete)),
          "duplicate complete-source evidence blocks isolation");
    CaseOptions partial_complete;
    partial_complete.complete_contains_interval = false;
    CHECK(abstained(run_case(partial_complete)),
          "a partial complete-source view blocks isolation");
    CaseOptions incomplete_complete;
    incomplete_complete.complete_robust_complete = false;
    CHECK(abstained(run_case(incomplete_complete)),
          "an incomplete complete-source gallery blocks isolation");
    CaseOptions changed_complete_pair;
    changed_complete_pair.complete_expected_pair = false;
    CHECK(abstained(run_case(changed_complete_pair)),
          "a third complete-source identity blocks isolation");
    CaseOptions current_complete;
    current_complete.complete_candidate_top = false;
    CHECK(abstained(run_case(current_complete)),
          "complete-source views without the candidate on top block isolation");
    CaseOptions eligible_complete;
    eligible_complete.complete_gates_abstain = false;
    CHECK(abstained(run_case(eligible_complete)),
          "an eligible complete-source view blocks the abstention topology");
  }

  // ---- 20g12e. FR40: reconstruct a two-unit primary handoff identically
  // across merged and adjacent ASR source partitions.
  // ----
  {
    using Entry = ComprehensiveTimeline::Entry;
    using Evidence = ComprehensiveTimeline::SpeakerVoiceprintEvidence;
    using VoiceprintScore = ComprehensiveTimeline::VoiceprintScore;
    struct CaseOptions {
      bool split_sources = false;
      bool direct_label = true;
      bool candidate_embedded = false;
      bool valid_unit_width = true;
      bool left_punctuation = true;
      bool right_punctuation = true;
      int containing_vad_count = 1;
      bool vad_contains_pair = true;
      bool vad_robust_complete = true;
      bool short_vad = true;
      bool extra_unit_inside_vad = false;
      double unit_gap = 0.32;
      double boundary_tolerance = 0.08;
      bool left_primary = true;
      bool right_primary = true;
      bool duplicate_right_primary = false;
      bool distinct_primary = true;
      bool overlapping_primary = false;
      bool activity_covers_pair = true;
      bool competing_activity = false;
      bool vad_right_first = true;
      bool vad_expected_pair = true;
      bool vad_score_passes = true;
      bool vad_margin_passes = true;
      bool adjacent_texts = true;
      bool current_identity_in_pair = true;
    };
    auto run_case = [](const CaseOptions& options) -> std::vector<Entry> {
      BusinessSpeakerPipeline::Config config;
      config.voiceprint_fusion_enabled = true;
      config.align_boundary_split_tolerance_sec =
          options.boundary_tolerance;
      config.speaker_overlap_tie_policy =
          BusinessSpeakerPipeline::SpeakerOverlapTiePolicy::kPrimarySpeaker;
      TestBusinessSpeakerPipeline tl(config);

      constexpr double kLeftStart = 0.20;
      constexpr double kLeftEnd = 0.28;
      const double right_start = kLeftEnd + options.unit_gap;
      const double right_end = right_start + 0.08;
      const double later_start = right_end + 0.52;
      const double interval_end = options.split_sources
                                      ? right_start + 1.60
                                      : right_end;

      std::vector<TestBusinessSpeakerPipeline::SpeakerInput> activity = {
          {0.0,
           options.activity_covers_pair ? interval_end + 0.2
                                        : kLeftEnd + 0.01,
           "slot_a", 0.9f, "spk_a"}};
      if (options.competing_activity) {
        activity.push_back(
            {kLeftStart, right_end, "slot_c", 0.8f, "spk_c"});
      }
      tl.ReplaceSpeakers(activity);

      std::vector<TestBusinessSpeakerPipeline::SpeakerInput> primary;
      if (options.left_primary) {
        primary.push_back(
            {0.12,
             options.overlapping_primary ? right_start + 0.04 : 0.40,
             "slot_a", 0.9f, "spk_a"});
      }
      if (options.right_primary) {
        primary.push_back(
            {right_start - 0.02, right_end + 0.16, "slot_b", 0.9f,
             options.distinct_primary ? "spk_b" : "spk_a"});
      }
      if (options.duplicate_right_primary) {
        primary.push_back({right_start, right_end + 0.10, "slot_c", 0.8f,
                           "spk_b"});
      }
      if (options.split_sources) {
        primary.push_back({later_start - 0.02, interval_end, "slot_a", 0.9f,
                           "spk_a"});
      }
      tl.ReplacePrimarySpeakers(primary);

      const std::string left_text =
          options.left_punctuation ? "甲？" : "甲";
      const std::string right_clause = options.right_punctuation
                                           ? (options.split_sources ? "乙，"
                                                                    : "乙。")
                                           : "乙";
      const int left_character_count = options.left_punctuation ? 2 : 1;
      const int right_source_start = options.split_sources
                                         ? 0
                                         : left_character_count;
      const int right_source_end = right_source_start + 1;
      const int right_clause_end =
          right_source_end + (options.right_punctuation ? 1 : 0);

      if (options.split_sources) {
        const double right_text_start = options.adjacent_texts ? 0.50 : 0.51;
        tl.UpsertText(0, 0.0, 0.50, left_text);
        tl.UpsertText(1, right_text_start, interval_end + 0.1,
                      right_clause + "丙？");
        tl.UpsertAlign(0, 0.0, 0.50,
                       {{kLeftStart, kLeftEnd, "甲"}});
        tl.UpsertAlign(1, right_text_start, interval_end + 0.1,
                       {{right_start, right_end, "乙"},
                        {later_start, later_start + 0.08, "丙"}});
      } else {
        const std::string source = left_text + right_clause;
        tl.UpsertText(0, 0.0, 1.0, source);
        tl.UpsertAlign(0, 0.0, 1.0,
                       {{kLeftStart, kLeftEnd, "甲"},
                        {right_start, right_end, "乙"}});
      }

      Evidence left_unit;
      left_unit.evidence_id = "aligned_unit:left";
      left_unit.kind = "aligned_unit";
      left_unit.text_id = 0;
      left_unit.source_start = 0;
      left_unit.source_end =
          options.valid_unit_width ? 1 : left_character_count;
      left_unit.start = kLeftStart;
      left_unit.end = kLeftEnd;
      left_unit.embedding_available = options.candidate_embedded;

      Evidence right_unit;
      right_unit.evidence_id = "aligned_unit:right";
      right_unit.kind = "aligned_unit";
      right_unit.text_id = options.split_sources ? 1 : 0;
      right_unit.source_start = right_source_start;
      right_unit.source_end =
          options.valid_unit_width ? right_source_end : right_clause_end;
      right_unit.start = right_start;
      right_unit.end = right_end;
      right_unit.embedding_available = options.candidate_embedded;

      Evidence interval;
      interval.evidence_id = "business_interval:coarse";
      interval.kind = "business_interval";
      interval.text_id = options.split_sources ? 1 : 0;
      interval.source_start = 0;
      interval.source_end = options.split_sources
                                ? right_clause_end + 2
                                : right_clause_end;
      interval.start = options.split_sources ? right_start : kLeftStart;
      interval.end = interval_end;
      interval.embedding_available = options.direct_label;
      interval.robust_gallery_complete = true;
      const std::string direct_identity =
          options.current_identity_in_pair
              ? (options.split_sources ? "spk_a" : "spk_b")
              : "spk_c";
      if (direct_identity == "spk_a") {
        interval.session_scores = {
            {"spk_a", 0.70f}, {"spk_b", 0.30f}, {"spk_c", 0.10f}};
      } else if (direct_identity == "spk_b") {
        interval.session_scores = {
            {"spk_b", 0.70f}, {"spk_a", 0.30f}, {"spk_c", 0.10f}};
      } else {
        interval.session_scores = {
            {"spk_c", 0.70f}, {"spk_a", 0.30f}, {"spk_b", 0.20f}};
      }
      interval.robust_scores = interval.session_scores;

      std::vector<Evidence> evidence = {interval, left_unit, right_unit};
      if (options.extra_unit_inside_vad) {
        Evidence extra = right_unit;
        extra.evidence_id = "aligned_unit:extra";
        extra.start = right_end + 0.02;
        extra.end = right_end + 0.06;
        evidence.push_back(extra);
      }
      if (options.split_sources) {
        Evidence later_unit;
        later_unit.evidence_id = "aligned_unit:later";
        later_unit.kind = "aligned_unit";
        later_unit.text_id = 1;
        later_unit.source_start = right_clause_end;
        later_unit.source_end = right_clause_end + 1;
        later_unit.start = later_start;
        later_unit.end = later_start + 0.08;
        evidence.push_back(later_unit);
      }

      for (int index = 0; index < options.containing_vad_count; ++index) {
        Evidence vad;
        vad.evidence_id = "vad:two-unit:" + std::to_string(index);
        vad.kind = "vad";
        vad.text_id = -1;
        vad.start = options.vad_contains_pair
                        ? (options.short_vad ? 0.10 - index * 0.01 : -1.0)
                        : 0.40;
        vad.end = right_end + 0.10 + index * 0.01;
        vad.embedding_available = true;
        vad.robust_gallery_complete = options.vad_robust_complete;
        const float top_score = options.vad_score_passes
                                    ? (options.vad_margin_passes ? 0.50f
                                                               : 0.42f)
                                    : -0.05f;
        const float second_score = options.vad_score_passes
                                       ? 0.40f
                                       : -0.15f;
        if (options.vad_expected_pair) {
          vad.session_scores = options.vad_right_first
                                   ? std::vector<VoiceprintScore>{
                                         {"spk_b", top_score},
                                         {"spk_a", second_score},
                                         {"spk_c", 0.10f}}
                                   : std::vector<VoiceprintScore>{
                                         {"spk_a", top_score},
                                         {"spk_b", second_score},
                                         {"spk_c", 0.10f}};
        } else {
          vad.session_scores = {{"spk_b", top_score},
                                {"spk_c", second_score},
                                {"spk_a", 0.10f}};
        }
        vad.robust_scores = vad.session_scores;
        evidence.push_back(vad);
      }
      tl.ReplaceVoiceprint(evidence);
      return tl.Snapshot();
    };

    const std::string reason =
        "voiceprint_aligned_unit_two_unit_primary_handoff_override";
    auto merged_selected = [&](const std::vector<Entry>& entries) {
      bool left_restored = false;
      bool right_preserved = false;
      for (const auto& entry : entries) {
        if (entry.text == "甲？" && entry.speaker_id == "spk_a" &&
            entry.speaker_decision.reason == reason &&
            entry.speaker_decision.speaker_source ==
                "sortformer_activity+primary_top1+titanet_vad+"
                "robust_gallery+forced_alignment") {
          left_restored = true;
        }
        if (entry.text == "乙。" && entry.speaker_id == "spk_b" &&
            entry.speaker_decision.reason != reason) {
          right_preserved = true;
        }
      }
      return left_restored && right_preserved;
    };
    auto split_selected = [&](const std::vector<Entry>& entries) {
      bool right_restored = false;
      bool later_preserved = false;
      for (const auto& entry : entries) {
        if (entry.text == "乙，" && entry.speaker_id == "spk_b" &&
            entry.speaker_decision.reason == reason) {
          right_restored = true;
        }
        if (entry.text == "丙？" && entry.speaker_id == "spk_a" &&
            entry.speaker_decision.reason != reason) {
          later_preserved = true;
        }
      }
      return right_restored && later_preserved;
    };
    auto abstained = [&](const std::vector<Entry>& entries) {
      return std::none_of(entries.begin(), entries.end(), [&](const auto& entry) {
        return entry.speaker_decision.reason == reason;
      });
    };

    CHECK(merged_selected(run_case({})),
          "merged source reconstructs the exact two-unit primary handoff");
    CaseOptions split;
    split.split_sources = true;
    split.unit_gap = 0.404;
    CHECK(split_selected(run_case(split)),
          "adjacent sources reconstruct the same two-unit primary handoff");
    CaseOptions split_without_tolerance = split;
    split_without_tolerance.boundary_tolerance = 0.0;
    CHECK(abstained(run_case(split_without_tolerance)),
          "the split-source boundary requires configured alignment tolerance");
    CaseOptions native_label;
    native_label.direct_label = false;
    CHECK(abstained(run_case(native_label)),
          "native provenance blocks the final direct-write challenge");
    CaseOptions embedded_unit;
    embedded_unit.candidate_embedded = true;
    CHECK(abstained(run_case(embedded_unit)),
          "an independently embedded aligned unit blocks reconstruction");
    CaseOptions wide_unit;
    wide_unit.valid_unit_width = false;
    CHECK(abstained(run_case(wide_unit)),
          "a multi-character aligned unit blocks reconstruction");
    CaseOptions missing_left_punctuation;
    missing_left_punctuation.left_punctuation = false;
    CHECK(abstained(run_case(missing_left_punctuation)),
          "missing left punctuation blocks the two-clause handoff");
    CaseOptions missing_right_punctuation;
    missing_right_punctuation.right_punctuation = false;
    CHECK(abstained(run_case(missing_right_punctuation)),
          "missing right punctuation blocks the two-clause handoff");
    CaseOptions missing_vad;
    missing_vad.containing_vad_count = 0;
    CHECK(abstained(run_case(missing_vad)),
          "missing containing VAD blocks reconstruction");
    CaseOptions duplicate_vad;
    duplicate_vad.containing_vad_count = 2;
    CHECK(abstained(run_case(duplicate_vad)),
          "duplicate containing VAD blocks reconstruction");
    CaseOptions partial_vad;
    partial_vad.vad_contains_pair = false;
    CHECK(abstained(run_case(partial_vad)),
          "a VAD that does not contain both units blocks reconstruction");
    CaseOptions incomplete_vad;
    incomplete_vad.vad_robust_complete = false;
    CHECK(abstained(run_case(incomplete_vad)),
          "an incomplete VAD gallery blocks reconstruction");
    CaseOptions long_vad;
    long_vad.short_vad = false;
    CHECK(abstained(run_case(long_vad)),
          "a regular-duration VAD blocks the short handoff contract");
    CaseOptions third_unit;
    third_unit.extra_unit_inside_vad = true;
    CHECK(abstained(run_case(third_unit)),
          "a third aligned unit inside the VAD blocks reconstruction");
    CaseOptions short_gap;
    short_gap.unit_gap = 0.20;
    CHECK(abstained(run_case(short_gap)),
          "a sub-pause unit gap blocks reconstruction");
    CaseOptions long_gap;
    long_gap.unit_gap = 0.50;
    CHECK(abstained(run_case(long_gap)),
          "a regular-duration unit gap blocks reconstruction");
    CaseOptions missing_left_primary;
    missing_left_primary.left_primary = false;
    CHECK(abstained(run_case(missing_left_primary)),
          "missing left primary evidence blocks reconstruction");
    CaseOptions missing_right_primary;
    missing_right_primary.right_primary = false;
    CHECK(abstained(run_case(missing_right_primary)),
          "missing right primary evidence blocks reconstruction");
    CaseOptions duplicate_right_primary;
    duplicate_right_primary.duplicate_right_primary = true;
    CHECK(abstained(run_case(duplicate_right_primary)),
          "competing right primary evidence blocks reconstruction");
    CaseOptions same_primary;
    same_primary.distinct_primary = false;
    CHECK(abstained(run_case(same_primary)),
          "one primary identity across both units blocks reconstruction");
    CaseOptions overlapping_primary;
    overlapping_primary.overlapping_primary = true;
    CHECK(abstained(run_case(overlapping_primary)),
          "overlapping primary runs block reconstruction");
    CaseOptions short_activity;
    short_activity.activity_covers_pair = false;
    CHECK(abstained(run_case(short_activity)),
          "activity that does not cover both units blocks reconstruction");
    CaseOptions competing_activity;
    competing_activity.competing_activity = true;
    CHECK(abstained(run_case(competing_activity)),
          "competing activity blocks reconstruction");
    CaseOptions changed_vad_order;
    changed_vad_order.vad_right_first = false;
    CHECK(abstained(run_case(changed_vad_order)),
          "a changed VAD order blocks reconstruction");
    CaseOptions changed_vad_pair;
    changed_vad_pair.vad_expected_pair = false;
    CHECK(abstained(run_case(changed_vad_pair)),
          "a third VAD top-two identity blocks reconstruction");
    CaseOptions weak_vad;
    weak_vad.vad_score_passes = false;
    CHECK(abstained(run_case(weak_vad)),
          "a VAD failing the short score gate blocks reconstruction");
    CaseOptions tied_vad;
    tied_vad.vad_margin_passes = false;
    CHECK(abstained(run_case(tied_vad)),
          "a VAD failing the short margin gate blocks reconstruction");
    CaseOptions nonadjacent_sources;
    nonadjacent_sources.split_sources = true;
    nonadjacent_sources.adjacent_texts = false;
    CHECK(abstained(run_case(nonadjacent_sources)),
          "nonadjacent ASR source clocks block reconstruction");
    CaseOptions outsider;
    outsider.current_identity_in_pair = false;
    CHECK(abstained(run_case(outsider)),
          "a coarse identity outside the corroborated pair is preserved");
  }

  // ---- 20g13. FR16ABJ: a subminimum interval may restore uncontested native
  // evidence only under the exact phrase/VAD cross-scale abstention pattern.
  // ----
  {
    using Entry = ComprehensiveTimeline::Entry;
    struct CaseOptions {
      bool interval_embedding = false;
      bool phrase_session_margin_passes = true;
      bool phrase_robust_margin_abstains = true;
      bool phrase_robust_current = true;
      bool vad_native = true;
      bool vad_current_runner_up = true;
      bool vad_margin_abstains = true;
      int containing_vad_count = 1;
      bool vad_robust_complete = true;
      bool include_competing_activity = true;
      bool additional_activity = false;
      bool candidate_activity = false;
      bool primary_native = true;
      bool two_positive_units = false;
      bool straddling_unit = false;
      bool foreign_interval = false;
    };
    auto run_case = [](const CaseOptions& options) -> std::vector<Entry> {
      BusinessSpeakerPipeline::Config config;
      config.voiceprint_fusion_enabled = true;
      config.speaker_overlap_tie_policy =
          BusinessSpeakerPipeline::SpeakerOverlapTiePolicy::kPrimarySpeaker;
      TestBusinessSpeakerPipeline tl(config);
      std::vector<TestBusinessSpeakerPipeline::SpeakerInput> activity = {
          {0.0, 1.0, "slot_0", 0.9f, "spk_a"}};
      if (options.include_competing_activity) {
        activity.push_back({0.2, 0.28, "slot_1", 0.9f, "spk_c"});
      }
      if (options.additional_activity) {
        activity.push_back({0.2, 0.28, "slot_3", 0.9f, "spk_d"});
      }
      if (options.candidate_activity) {
        activity.push_back({0.2, 0.28, "slot_2", 0.9f, "spk_b"});
      }
      tl.ReplaceSpeakers(activity);
      tl.ReplacePrimarySpeakers(
          {{0.0, 1.0, "slot_0", 0.9f,
            options.primary_native ? "spk_a" : "spk_c"}});
      tl.UpsertText(0, 0.2, 0.8, "甲乙。");
      if (options.two_positive_units) {
        tl.UpsertAlign(0, 0.2, 0.8,
                       {{0.2, 0.24, "甲"}, {0.24, 0.28, "乙"},
                        {0.5, 0.8, "。"}});
      } else if (options.straddling_unit) {
        tl.UpsertAlign(0, 0.2, 0.8,
                       {{0.18, 0.28, "甲乙"}, {0.5, 0.8, "。"}});
      } else {
        tl.UpsertAlign(0, 0.2, 0.8,
                       {{0.2, 0.28, "甲乙"}, {0.5, 0.8, "。"}});
      }

      ComprehensiveTimeline::SpeakerVoiceprintEvidence interval;
      interval.evidence_id = "business_interval:0:0";
      interval.kind = "business_interval";
      interval.text_id = options.foreign_interval ? 1 : 0;
      interval.source_start = 0;
      interval.source_end = 2;
      interval.start = 0.2;
      interval.end = 0.28;
      interval.embedding_available = options.interval_embedding;

      ComprehensiveTimeline::SpeakerVoiceprintEvidence phrase;
      phrase.evidence_id = "punctuation_phrase:0:0";
      phrase.kind = "punctuation_phrase";
      phrase.text_id = 0;
      phrase.source_start = 0;
      phrase.source_end = 3;
      phrase.start = 0.2;
      phrase.end = 0.8;
      phrase.embedding_available = true;
      phrase.robust_gallery_complete = true;
      phrase.session_scores = {
          {"spk_b", options.phrase_session_margin_passes ? 0.40f : 0.36f},
          {"spk_c", 0.34f}, {"spk_a", 0.20f}};
      if (options.phrase_robust_current) {
        phrase.robust_scores = {
            {"spk_b",
             options.phrase_robust_margin_abstains ? 0.36f : 0.40f},
            {"spk_c", 0.34f}, {"spk_a", 0.20f}};
      } else {
        phrase.robust_scores = {
            {"spk_c", 0.36f}, {"spk_b", 0.34f}, {"spk_a", 0.20f}};
      }
      std::vector<ComprehensiveTimeline::SpeakerVoiceprintEvidence> evidence = {
          interval, phrase};
      for (int index = 0; index < options.containing_vad_count; ++index) {
        ComprehensiveTimeline::SpeakerVoiceprintEvidence vad;
        vad.evidence_id = "vad:" + std::to_string(index);
        vad.kind = "vad";
        vad.text_id = -1;
        vad.start = 0.1 - index * 0.01;
        vad.end = 0.9 + index * 0.01;
        vad.embedding_available = true;
        vad.robust_gallery_complete = options.vad_robust_complete;
        const float vad_top = options.vad_margin_abstains ? 0.36f : 0.40f;
        const std::string top = options.vad_native ? "spk_a" : "spk_c";
        const std::string second =
            options.vad_current_runner_up ? "spk_b" : "spk_c";
        vad.session_scores = {
            {top, vad_top}, {second, 0.34f}, {"spk_d", 0.10f}};
        vad.robust_scores = vad.session_scores;
        evidence.push_back(vad);
      }
      tl.ReplaceVoiceprint(evidence);
      return tl.Snapshot();
    };

    const std::string reason =
        "voiceprint_subminimum_native_cross_scale_restore";
    auto has_reason = [&](const std::vector<Entry>& entries) {
      return std::any_of(entries.begin(), entries.end(), [&](const auto& entry) {
        return entry.speaker_id == "spk_a" &&
               entry.speaker_decision.reason == reason &&
               entry.speaker_decision.speaker_source ==
                   "sortformer_activity+primary_top1+titanet_phrase+"
                   "vad_session+robust_gallery+forced_alignment";
      });
    };

    CHECK(has_reason(run_case({})),
          "cross-scale native evidence restores the exact subminimum interval");
    CaseOptions embedded_interval;
    embedded_interval.interval_embedding = true;
    CHECK(!has_reason(run_case(embedded_interval)),
          "an available interval embedding blocks native restoration");
    CaseOptions weak_phrase_session;
    weak_phrase_session.phrase_session_margin_passes = false;
    CHECK(!has_reason(run_case(weak_phrase_session)),
          "phrase-session abstention blocks native restoration");
    CaseOptions eligible_phrase_robust;
    eligible_phrase_robust.phrase_robust_margin_abstains = false;
    CHECK(!has_reason(run_case(eligible_phrase_robust)),
          "eligible robust phrase evidence blocks native restoration");
    CaseOptions phrase_disagreement;
    phrase_disagreement.phrase_robust_current = false;
    CHECK(!has_reason(run_case(phrase_disagreement)),
          "phrase gallery disagreement blocks native restoration");
    CaseOptions wrong_vad_identity;
    wrong_vad_identity.vad_native = false;
    CHECK(!has_reason(run_case(wrong_vad_identity)),
          "changed VAD top identity blocks native restoration");
    CaseOptions wrong_vad_runner_up;
    wrong_vad_runner_up.vad_current_runner_up = false;
    CHECK(!has_reason(run_case(wrong_vad_runner_up)),
          "changed VAD runner-up blocks native restoration");
    CaseOptions eligible_vad;
    eligible_vad.vad_margin_abstains = false;
    CHECK(!has_reason(run_case(eligible_vad)),
          "eligible VAD evidence blocks native restoration");
    CaseOptions multiple_vad;
    multiple_vad.containing_vad_count = 2;
    CHECK(!has_reason(run_case(multiple_vad)),
          "multiple containing VAD intervals block native restoration");
    CaseOptions incomplete_vad;
    incomplete_vad.vad_robust_complete = false;
    CHECK(!has_reason(run_case(incomplete_vad)),
          "incomplete VAD evidence blocks native restoration");
    CaseOptions missing_competitor;
    missing_competitor.include_competing_activity = false;
    CHECK(!has_reason(run_case(missing_competitor)),
          "missing competing activity blocks native restoration");
    CaseOptions additional_activity;
    additional_activity.additional_activity = true;
    CHECK(!has_reason(run_case(additional_activity)),
          "an additional activity identity blocks native restoration");
    CaseOptions candidate_activity;
    candidate_activity.candidate_activity = true;
    CHECK(!has_reason(run_case(candidate_activity)),
          "current candidate activity blocks native restoration");
    CaseOptions primary_mismatch;
    primary_mismatch.primary_native = false;
    CHECK(!has_reason(run_case(primary_mismatch)),
          "primary identity mismatch blocks native restoration");
    CaseOptions two_units;
    two_units.two_positive_units = true;
    CHECK(!has_reason(run_case(two_units)),
          "two positive aligned units block atomic native restoration");
    CaseOptions straddling_unit;
    straddling_unit.straddling_unit = true;
    CHECK(!has_reason(run_case(straddling_unit)),
          "an edge-straddling unit blocks native restoration");
    CaseOptions foreign_interval;
    foreign_interval.foreign_interval = true;
    CHECK(!has_reason(run_case(foreign_interval)),
          "an interval from another text cannot access current labels");
  }

  // ---- 20g14. FR16ABK: a padded complete source may use short gates only
  // when typed intervals, alignment, VAD, phrase, and native views close. ----
  {
    using Entry = ComprehensiveTimeline::Entry;
    struct CaseOptions {
      bool partition_gap = false;
      bool unanchored_interval = false;
      bool complete_regular_pass = false;
      bool complete_robust_candidate = true;
      bool phrase_margin_abstains = true;
      bool phrase_candidate_top_two = true;
      int phrase_count = 1;
      bool vad_candidate = true;
      int containing_vad_count = 1;
      bool vad_robust_complete = true;
      bool additional_activity = false;
      bool candidate_activity_covers = false;
      bool primary_gap = false;
      bool additional_primary = false;
      bool candidate_primary_edge = true;
      bool aligned_span_is_short = true;
    };
    auto run_case = [](const CaseOptions& options) -> std::vector<Entry> {
      BusinessSpeakerPipeline::Config config;
      config.voiceprint_fusion_enabled = true;
      config.speaker_overlap_tie_policy =
          BusinessSpeakerPipeline::SpeakerOverlapTiePolicy::kPrimarySpeaker;
      if (!options.aligned_span_is_short) {
        config.voiceprint_short_max_sec = 0.75;
      }
      TestBusinessSpeakerPipeline tl(config);

      std::vector<TestBusinessSpeakerPipeline::SpeakerInput> activity = {
          {0.0, 1.0, "slot_0", 0.9f, "spk_a"},
          {options.candidate_activity_covers ? 0.0 : 0.6, 1.0, "slot_1",
           0.9f, "spk_b"}};
      if (options.additional_activity) {
        activity.push_back({0.4, 0.5, "slot_2", 0.9f, "spk_d"});
      }
      tl.ReplaceSpeakers(activity);

      std::vector<TestBusinessSpeakerPipeline::SpeakerInput> primary = {
          {0.0, options.primary_gap ? 0.7 : 0.8, "slot_0", 0.9f,
           "spk_a"}};
      if (options.candidate_primary_edge) {
        primary.push_back({0.8, 1.0, "slot_1", 0.9f, "spk_b"});
      }
      if (options.additional_primary) {
        primary.push_back({0.4, 0.5, "slot_2", 0.9f, "spk_d"});
      }
      tl.ReplacePrimarySpeakers(primary);
      tl.UpsertText(0, 0.0, 2.0, "甲乙，丙丁。");
      tl.UpsertAlign(0, 0.0, 2.0,
                     {{0.1, 0.2, "甲"},
                      {0.2, 0.3, "乙"},
                      {0.6, 0.7, "丙"},
                      {0.82, 0.9, "丁"}});

      std::vector<ComprehensiveTimeline::SpeakerVoiceprintEvidence> evidence;
      auto add_interval = [&](int ordinal, int source_start, int source_end,
                              double start, double end) {
        ComprehensiveTimeline::SpeakerVoiceprintEvidence interval;
        interval.evidence_id =
            "business_interval:0:" + std::to_string(ordinal);
        interval.kind = "business_interval";
        interval.text_id = 0;
        interval.source_start = source_start;
        interval.source_end = source_end;
        interval.start = start;
        interval.end = end;
        evidence.push_back(std::move(interval));
      };
      add_interval(0, 0, 3, 0.1, 0.3);
      add_interval(1, 3, 4, 0.6, 0.7);
      add_interval(2, options.partition_gap ? 5 : 4, 6,
                   options.unanchored_interval ? 0.91 : 0.82,
                   options.unanchored_interval ? 0.99 : 0.9);

      int unit_ordinal = 0;
      auto add_unit = [&](int source_start, int source_end, double start,
                          double end) {
        ComprehensiveTimeline::SpeakerVoiceprintEvidence unit;
        unit.evidence_id =
            "aligned_unit:0:" + std::to_string(unit_ordinal++);
        unit.kind = "aligned_unit";
        unit.text_id = 0;
        unit.source_start = source_start;
        unit.source_end = source_end;
        unit.start = start;
        unit.end = end;
        evidence.push_back(std::move(unit));
      };
      add_unit(0, 1, 0.1, 0.2);
      add_unit(1, 2, 0.2, 0.3);
      add_unit(3, 4, 0.6, 0.7);
      add_unit(4, 5, 0.82, 0.9);

      for (int index = 0; index < options.phrase_count; ++index) {
        ComprehensiveTimeline::SpeakerVoiceprintEvidence phrase;
        phrase.evidence_id =
            "punctuation_phrase:0:" + std::to_string(index);
        phrase.kind = "punctuation_phrase";
        phrase.text_id = 0;
        phrase.source_start = index == 0 ? 0 : 3;
        phrase.source_end = index == 0 ? 3 : 6;
        phrase.start = index == 0 ? 0.1 : 0.6;
        phrase.end = index == 0 ? 0.3 : 0.9;
        phrase.embedding_available = true;
        phrase.robust_gallery_complete = true;
        const float phrase_top =
            options.phrase_margin_abstains ? 0.36f : 0.42f;
        phrase.session_scores = options.phrase_candidate_top_two
                                    ? std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                          {"spk_c", phrase_top},
                                          {"spk_b", 0.35f},
                                          {"spk_a", 0.10f}}
                                    : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                          {"spk_c", phrase_top},
                                          {"spk_d", 0.35f},
                                          {"spk_b", 0.10f}};
        phrase.robust_scores = phrase.session_scores;
        evidence.push_back(std::move(phrase));
      }

      ComprehensiveTimeline::SpeakerVoiceprintEvidence complete;
      complete.evidence_id = "complete_source:0";
      complete.kind = "complete_source";
      complete.text_id = 0;
      complete.source_start = 0;
      complete.source_end = 6;
      complete.start = 0.0;
      complete.end = 2.0;
      complete.embedding_available = true;
      complete.robust_gallery_complete = true;
      const float complete_top =
          options.complete_regular_pass ? 0.60f : 0.40f;
      complete.session_scores = {
          {"spk_b", complete_top}, {"spk_c", 0.20f}, {"spk_a", 0.10f}};
      complete.robust_scores = options.complete_robust_candidate
                                   ? complete.session_scores
                                   : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                         {"spk_c", complete_top},
                                         {"spk_b", 0.20f},
                                         {"spk_a", 0.10f}};
      evidence.push_back(std::move(complete));

      for (int index = 0; index < options.containing_vad_count; ++index) {
        ComprehensiveTimeline::SpeakerVoiceprintEvidence vad;
        vad.evidence_id = "vad:" + std::to_string(index);
        vad.kind = "vad";
        vad.text_id = -1;
        vad.start = 0.05 - index * 0.01;
        vad.end = 0.95 + index * 0.01;
        vad.embedding_available = true;
        vad.robust_gallery_complete = options.vad_robust_complete;
        const std::string top = options.vad_candidate ? "spk_b" : "spk_c";
        vad.session_scores = {
            {top, 0.45f}, {"spk_a", 0.20f}, {"spk_d", 0.10f}};
        vad.robust_scores = vad.session_scores;
        evidence.push_back(std::move(vad));
      }
      tl.ReplaceVoiceprint(evidence);
      return tl.Snapshot();
    };

    const std::string reason =
        "voiceprint_complete_source_aligned_vad_closure";
    auto has_reason = [&](const std::vector<Entry>& entries) {
      return entries.size() == 1 && entries[0].speaker_id == "spk_b" &&
             entries[0].text == "甲乙，丙丁。" &&
             entries[0].speaker_decision.reason == reason &&
             entries[0].speaker_decision.speaker_source ==
                 "sortformer_activity+primary_top1+titanet_complete_source+"
                 "titanet_phrase+vad_session+robust_gallery+forced_alignment";
    };

    const auto positive_entries = run_case({});
    CHECK(has_reason(positive_entries),
          "aligned complete-source evidence closes one padded short source");
    CaseOptions partition_gap;
    partition_gap.partition_gap = true;
    CHECK(!has_reason(run_case(partition_gap)),
          "a source-partition gap blocks complete-source closure");
    CaseOptions unanchored_interval;
    unanchored_interval.unanchored_interval = true;
    CHECK(!has_reason(run_case(unanchored_interval)),
          "an unanchored typed interval blocks complete-source closure");
    CaseOptions regular_aligned_span;
    regular_aligned_span.aligned_span_is_short = false;
    CHECK(!has_reason(run_case(regular_aligned_span)),
          "a regular aligned envelope preserves the current attribution");
    CaseOptions regular_complete;
    regular_complete.complete_regular_pass = true;
    CHECK(!has_reason(run_case(regular_complete)),
          "regular-score eligibility preserves the current attribution");
    CaseOptions complete_disagreement;
    complete_disagreement.complete_robust_candidate = false;
    CHECK(!has_reason(run_case(complete_disagreement)),
          "complete-source gallery disagreement blocks closure");
    CaseOptions eligible_phrase;
    eligible_phrase.phrase_margin_abstains = false;
    CHECK(!has_reason(run_case(eligible_phrase)),
          "an eligible phrase view blocks complete-source closure");
    CaseOptions changed_phrase_pair;
    changed_phrase_pair.phrase_candidate_top_two = false;
    CHECK(!has_reason(run_case(changed_phrase_pair)),
          "a phrase top-two change blocks complete-source closure");
    CaseOptions multiple_phrases;
    multiple_phrases.phrase_count = 2;
    CHECK(!has_reason(run_case(multiple_phrases)),
          "multiple phrase views block complete-source closure");
    CaseOptions wrong_vad;
    wrong_vad.vad_candidate = false;
    CHECK(!has_reason(run_case(wrong_vad)),
          "a changed VAD identity blocks complete-source closure");
    CaseOptions multiple_vad;
    multiple_vad.containing_vad_count = 2;
    CHECK(!has_reason(run_case(multiple_vad)),
          "multiple containing VAD intervals block complete-source closure");
    CaseOptions incomplete_vad;
    incomplete_vad.vad_robust_complete = false;
    CHECK(!has_reason(run_case(incomplete_vad)),
          "incomplete robust VAD evidence blocks complete-source closure");
    CaseOptions additional_activity;
    additional_activity.additional_activity = true;
    CHECK(!has_reason(run_case(additional_activity)),
          "a third activity identity blocks complete-source closure");
    CaseOptions full_candidate_activity;
    full_candidate_activity.candidate_activity_covers = true;
    CHECK(!has_reason(run_case(full_candidate_activity)),
          "full candidate activity preserves the native attribution path");
    CaseOptions primary_gap;
    primary_gap.primary_gap = true;
    CHECK(!has_reason(run_case(primary_gap)),
          "a primary coverage gap blocks complete-source closure");
    CaseOptions additional_primary;
    additional_primary.additional_primary = true;
    CHECK(!has_reason(run_case(additional_primary)),
          "a third primary identity blocks complete-source closure");
    CaseOptions missing_candidate_edge;
    missing_candidate_edge.candidate_primary_edge = false;
    CHECK(!has_reason(run_case(missing_candidate_edge)),
          "missing candidate edge provenance blocks complete-source closure");
  }

  // ---- 20g3. FR16AAZ: a strong exact phrase may extend into one immediately
  // adjacent margin-only business interval when the anchor is runner-up in both
  // target galleries and activity continues for the configured minimum. ----
  {
    using Entry = ComprehensiveTimeline::Entry;
    auto run_case = [](bool strong_anchor, bool anchor_is_runner_up,
                       bool long_activity_continuation, bool include_primary,
                       bool enough_aligned_units) -> std::vector<Entry> {
      BusinessSpeakerPipeline::Config config;
      config.voiceprint_fusion_enabled = true;
      config.align_snap_pause_sec = 0.0;
      config.speaker_overlap_tie_policy =
          BusinessSpeakerPipeline::SpeakerOverlapTiePolicy::kPrimarySpeaker;
      TestBusinessSpeakerPipeline tl(config);
      const double anchor_end = long_activity_continuation ? 1.25 : 1.10;
      tl.ReplaceSpeakers({{0.0, anchor_end, "slot_0", 0.9f, "spk_b"},
                          {0.8, 2.0, "slot_1", 0.9f, "spk_a"}});
      if (include_primary) {
        tl.ReplacePrimarySpeakers(
            {{0.8, 1.6, "slot_1", 0.9f, "spk_a"}});
      }
      tl.UpsertText(0, 0.0, 1.6, "甲乙，丙丁。");
      if (enough_aligned_units) {
        tl.UpsertAlign(0, 0.0, 1.6,
                       {{0.0, 0.4, "甲"},
                        {0.4, 0.8, "乙"},
                        {0.8, 1.2, "丙"},
                        {1.2, 1.6, "丁"}});
      } else {
        tl.UpsertAlign(0, 0.0, 1.6,
                       {{0.0, 0.4, "甲"},
                        {0.4, 0.8, "乙"},
                        {0.8, 1.6, "丙丁"}});
      }

      ComprehensiveTimeline::SpeakerVoiceprintEvidence target;
      target.evidence_id = "business_interval:0:1";
      target.kind = "business_interval";
      target.text_id = 0;
      target.source_start = 3;
      target.source_end = 6;
      target.start = 0.8;
      target.end = 1.6;
      target.embedding_available = true;
      target.robust_gallery_complete = true;
      target.session_scores = anchor_is_runner_up
                                  ? std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                        {"spk_a", 0.45f}, {"spk_b", 0.43f},
                                        {"spk_c", 0.20f}}
                                  : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                        {"spk_a", 0.45f}, {"spk_b", 0.20f},
                                        {"spk_c", 0.43f}};
      target.robust_scores = anchor_is_runner_up
                                 ? std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                       {"spk_a", 0.46f}, {"spk_b", 0.44f},
                                       {"spk_c", 0.20f}}
                                 : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                       {"spk_a", 0.46f}, {"spk_b", 0.20f},
                                       {"spk_c", 0.44f}};
      ComprehensiveTimeline::SpeakerVoiceprintEvidence anchor;
      anchor.evidence_id = "punctuation_phrase:0:0";
      anchor.kind = "punctuation_phrase";
      anchor.text_id = 0;
      anchor.source_start = 0;
      anchor.source_end = 3;
      anchor.start = 0.0;
      anchor.end = 0.8;
      anchor.embedding_available = true;
      anchor.robust_gallery_complete = true;
      anchor.session_scores = strong_anchor
                                  ? std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                        {"spk_a", 0.20f}, {"spk_b", 0.50f}}
                                  : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                        {"spk_a", 0.43f}, {"spk_b", 0.45f}};
      anchor.robust_scores = strong_anchor
                                 ? std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                       {"spk_a", 0.20f}, {"spk_b", 0.49f}}
                                 : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                       {"spk_a", 0.42f}, {"spk_b", 0.44f}};
      tl.ReplaceVoiceprint({target, anchor});
      return tl.Snapshot();
    };

    auto has_target = [](const std::vector<Entry>& entries,
                         const std::string& speaker_id,
                         const std::string& reason = "") {
      return std::any_of(entries.begin(), entries.end(), [&](const auto& entry) {
        return entry.text.find("丙") != std::string::npos &&
               entry.speaker_id == speaker_id &&
               (reason.empty() || entry.speaker_decision.reason == reason);
      });
    };
    const auto anchored = run_case(true, true, true, true, true);
    CHECK(has_target(anchored, "spk_b",
                     "voiceprint_direct_adjacent_phrase_anchor"),
          "strong adjacent phrase anchors the margin-only continuation");
    CHECK(has_target(run_case(false, true, true, true, true), "spk_a"),
          "weak preceding phrase preserves current target identity");
    CHECK(has_target(run_case(true, false, true, true, true), "spk_a"),
          "non-runner-up anchor preserves current target identity");
    CHECK(has_target(run_case(true, true, false, true, true), "spk_a"),
          "short activity continuation preserves current target identity");
    CHECK(has_target(run_case(true, true, true, false, true), "spk_a"),
          "missing covering primary preserves current target identity");
    CHECK(has_target(run_case(true, true, true, true, false), "spk_a"),
          "insufficient target alignment preserves current identity");
  }

  // ---- 20g2. FR16AAY: a short primary-selected phrase may recover the local
  // slot's initial identity only under strong dual-gallery VAD support and the
  // exact opposing margin-only phrase topology. ----
  {
    using Entry = ComprehensiveTimeline::Entry;
    auto run_case = [](bool include_initial_epoch, bool include_overlap,
                       bool strong_vad, bool phrase_session_margin_only,
                       bool enough_aligned_units) -> std::vector<Entry> {
      BusinessSpeakerPipeline::Config config;
      config.voiceprint_fusion_enabled = true;
      config.align_snap_pause_sec = 0.0;
      config.speaker_overlap_tie_policy =
          BusinessSpeakerPipeline::SpeakerOverlapTiePolicy::kPrimarySpeaker;
      TestBusinessSpeakerPipeline tl(config);
      std::vector<TestBusinessSpeakerPipeline::SpeakerInput> activity;
      if (include_initial_epoch) {
        activity.push_back({-2.0, -1.0, "slot_0", 0.9f, "spk_b"});
      }
      activity.push_back({0.0, 1.0, "slot_0", 0.9f, "spk_a"});
      if (include_overlap) {
        activity.push_back({0.0, 1.0, "slot_1", 0.9f, "spk_c"});
      }
      tl.ReplaceSpeakers(activity);
      tl.ReplacePrimarySpeakers({{0.0, 1.0, "slot_0", 0.9f, "spk_a"}});
      tl.UpsertText(0, 0.0, 0.8, "甲乙。");
      if (enough_aligned_units) {
        tl.UpsertAlign(0, 0.0, 0.8,
                       {{0.0, 0.4, "甲"}, {0.4, 0.8, "乙"}});
      } else {
        tl.UpsertAlign(0, 0.0, 0.8, {{0.0, 0.8, "甲乙"}});
      }

      ComprehensiveTimeline::SpeakerVoiceprintEvidence phrase;
      phrase.evidence_id = "punctuation_phrase:0:0";
      phrase.kind = "punctuation_phrase";
      phrase.text_id = 0;
      phrase.source_start = 0;
      phrase.source_end = 3;
      phrase.start = 0.0;
      phrase.end = 0.8;
      phrase.embedding_available = true;
      phrase.robust_gallery_complete = true;
      phrase.session_scores = phrase_session_margin_only
                                  ? std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                        {"spk_a", 0.45f}, {"spk_b", 0.43f}}
                                  : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                        {"spk_a", 0.50f}, {"spk_b", 0.43f}};
      phrase.robust_scores = {{"spk_a", 0.43f}, {"spk_b", 0.45f}};
      ComprehensiveTimeline::SpeakerVoiceprintEvidence vad;
      vad.evidence_id = "vad:0";
      vad.kind = "vad";
      vad.text_id = -1;
      vad.start = -1.0;
      vad.end = 2.0;
      vad.embedding_available = true;
      vad.robust_gallery_complete = true;
      vad.session_scores = {{"spk_a", 0.60f}, {"spk_b", 0.70f}};
      vad.robust_scores = strong_vad
                              ? std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                    {"spk_a", 0.58f}, {"spk_b", 0.68f}}
                              : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                    {"spk_a", 0.58f}, {"spk_b", 0.60f}};
      tl.ReplaceVoiceprint({phrase, vad});
      return tl.Snapshot();
    };

    const auto recovered = run_case(true, true, true, true, true);
    CHECK(!recovered.empty() &&
              std::all_of(recovered.begin(), recovered.end(),
                          [](const auto& entry) {
                            return entry.speaker_id == "spk_b" &&
                                   entry.speaker_decision.reason ==
                                       "voiceprint_phrase_short_initial_slot_"
                                       "vad_override";
                          }),
          "short strong-VAD topology recovers the initial slot identity");

    const auto no_epoch = run_case(false, true, true, true, true);
    const auto no_primary = run_case(true, false, true, true, true);
    const auto weak_vad = run_case(true, true, false, true, true);
    const auto passing_phrase = run_case(true, true, true, false, true);
    const auto one_unit = run_case(true, true, true, true, false);
    auto preserves_current = [](const std::vector<Entry>& entries) {
      return !entries.empty() &&
             std::all_of(entries.begin(), entries.end(), [](const auto& entry) {
               return entry.speaker_id == "spk_a";
             });
    };
    CHECK(preserves_current(no_epoch),
          "missing initial slot epoch preserves current identity");
    CHECK(preserves_current(no_primary),
          "non-primary phrase preserves current identity");
    CHECK(preserves_current(weak_vad),
          "margin-abstaining VAD preserves current identity");
    CHECK(preserves_current(passing_phrase),
          "passing phrase margin preserves current identity");
    CHECK(preserves_current(one_unit),
          "insufficient aligned structure preserves current identity");
  }

  // ---- 20i. Aligned-unit voiceprint that agrees with primary arbitration
  // remains eligible and records its evidence source. ----
  {
    using Pipeline = orator::pipeline::BusinessSpeakerPipeline;
    Pipeline::Config config;
    config.voiceprint_fusion_enabled = true;
    config.speaker_overlap_tie_policy =
        Pipeline::SpeakerOverlapTiePolicy::kPrimarySpeaker;
    TestBusinessSpeakerPipeline tl(config);
    tl.UpsertText(0, 0.0, 1.0, "甲");
    tl.ReplaceSpeakers({{0.0, 1.0, "speaker_0", 0.9f, "spk_a"},
                        {0.0, 1.0, "speaker_1", 0.9f, "spk_b"}});
    tl.ReplacePrimarySpeakers(
        {{0.0, 1.0, "speaker_0", 0.9f, "spk_a"}});
    tl.UpsertAlign(0, 0.0, 1.0, {{0.0, 1.0, "甲"}});
    ComprehensiveTimeline::SpeakerVoiceprintEvidence unit;
    unit.evidence_id = "aligned_unit:0:0";
    unit.kind = "aligned_unit";
    unit.text_id = 0;
    unit.source_start = 0;
    unit.source_end = 1;
    unit.start = 0.0;
    unit.end = 1.0;
    unit.embedding_available = true;
    unit.robust_gallery_complete = true;
    unit.session_scores = {{"spk_a", 0.75f}, {"spk_b", 0.55f}};
    unit.robust_scores = {{"spk_a", 0.73f}, {"spk_b", 0.54f}};
    tl.ReplaceVoiceprint({unit});
    const auto snap = tl.Snapshot();
    CHECK(snap.size() == 1 && snap[0].speaker_id == "spk_a" &&
              snap[0].speaker_decision.reason ==
                  "voiceprint_aligned_unit_dual_gallery",
          "agreeing aligned voiceprint remains eligible");
  }

  // ---- 20g15. FR16ABL: an eligible adjacent pair may restore only its
  // source-initial subminimum prefix while preserving the following handoff.
  // ----
  {
    using Entry = ComprehensiveTimeline::Entry;
    struct CaseOptions {
      bool pair_source_initial = true;
      bool pair_robust_candidate = true;
      bool pair_margin_passes = true;
      bool leading_embedding = false;
      bool following_robust_complete = true;
      bool extra_component = false;
      bool primary_starts_in_prefix = true;
      bool candidate_activity_starts_in_prefix = true;
      bool incumbent_activity_covers = true;
      bool include_lower_activity = true;
      bool extra_activity = false;
      bool enough_aligned_units = true;
      bool include_next_interval = true;
      bool next_primary_incumbent = true;
      bool next_starts_after_pair = true;
    };
    auto run_case = [](const CaseOptions& options) -> std::vector<Entry> {
      BusinessSpeakerPipeline::Config config;
      config.voiceprint_fusion_enabled = true;
      config.speaker_overlap_tie_policy =
          BusinessSpeakerPipeline::SpeakerOverlapTiePolicy::kPrimarySpeaker;
      TestBusinessSpeakerPipeline tl(config);

      std::vector<TestBusinessSpeakerPipeline::SpeakerInput> activity = {
          {options.incumbent_activity_covers ? 0.0 : 0.10, 1.4, "slot_0",
           0.9f, "spk_a"},
          {options.candidate_activity_starts_in_prefix ? 0.12 : 0.26, 1.4,
           "slot_1", 0.9f, "spk_b"}};
      if (options.include_lower_activity) {
        activity.push_back({0.0, 1.4, "slot_2", 0.4f, "spk_c"});
      }
      if (options.extra_activity) {
        activity.push_back({0.0, 1.4, "slot_3", 0.95f, "spk_d"});
      }
      tl.ReplaceSpeakers(activity);
      tl.ReplacePrimarySpeakers(
          {{options.primary_starts_in_prefix ? 0.12 : 0.26, 0.8, "slot_1",
            0.9f, "spk_b"},
           {1.0, 1.4, "slot_0", 0.9f,
            options.next_primary_incumbent ? "spk_a" : "spk_b"}});
      tl.UpsertText(0, 0.0, 1.4, "甲乙丙丁戊");
      tl.UpsertAlign(0, 0.0, 1.4,
                     {{0.0, 0.16, "甲"},
                      {0.16, 0.50, "乙"},
                      {0.50, 0.80, "丙"},
                      {1.0, 1.20, "丁"},
                      {1.20, 1.40, "戊"}});

      auto interval = [](const std::string& id, int source_start,
                         int source_end, double start, double end) {
        ComprehensiveTimeline::SpeakerVoiceprintEvidence value;
        value.evidence_id = id;
        value.kind = "business_interval";
        value.text_id = 0;
        value.source_start = source_start;
        value.source_end = source_end;
        value.start = start;
        value.end = end;
        return value;
      };
      auto leading = interval("business_interval:0:0", 0, 1, 0.0, 0.16);
      leading.embedding_available = options.leading_embedding;
      if (leading.embedding_available) {
        leading.session_scores = {{"spk_a", 0.20f}, {"spk_b", 0.50f}};
      }
      auto following =
          interval("business_interval:0:1", 1, 3, 0.16, 0.80);
      following.embedding_available = true;
      following.robust_gallery_complete = options.following_robust_complete;
      following.session_scores = {{"spk_a", 0.40f}, {"spk_b", 0.41f}};
      following.robust_scores = {{"spk_a", 0.39f}, {"spk_b", 0.40f}};

      ComprehensiveTimeline::SpeakerVoiceprintEvidence pair;
      pair.evidence_id = "adjacent_business_pair:0:0";
      pair.kind = "adjacent_business_pair";
      pair.text_id = 0;
      pair.source_start = options.pair_source_initial ? 0 : 1;
      pair.source_end = 3;
      pair.start = 0.0;
      pair.end = 0.80;
      pair.embedding_available = true;
      pair.robust_gallery_complete = true;
      const float pair_top = options.pair_margin_passes ? 0.50f : 0.32f;
      pair.session_scores = {{"spk_a", 0.30f}, {"spk_b", pair_top}};
      pair.robust_scores = options.pair_robust_candidate
                               ? std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                     {"spk_a", 0.30f}, {"spk_b", 0.48f}}
                               : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                     {"spk_a", 0.48f}, {"spk_b", 0.30f}};

      std::vector<ComprehensiveTimeline::SpeakerVoiceprintEvidence> evidence = {
          leading, following, pair};
      if (options.extra_component) {
        evidence.push_back(
            interval("business_interval:0:extra", 1, 2, 0.16, 0.50));
      }
      auto unit = [](int ordinal, int source_start, int source_end,
                     double start, double end) {
        ComprehensiveTimeline::SpeakerVoiceprintEvidence value;
        value.evidence_id =
            "aligned_unit:0:" + std::to_string(ordinal);
        value.kind = "aligned_unit";
        value.text_id = 0;
        value.source_start = source_start;
        value.source_end = source_end;
        value.start = start;
        value.end = end;
        return value;
      };
      evidence.push_back(unit(0, 0, 1, 0.0, 0.16));
      if (options.enough_aligned_units) {
        evidence.push_back(unit(1, 1, 2, 0.16, 0.50));
        evidence.push_back(unit(2, 2, 3, 0.50, 0.80));
      }
      if (options.include_next_interval) {
        evidence.push_back(interval(
            "business_interval:0:2", 3, 5,
            options.next_starts_after_pair ? 1.0 : 0.70, 1.40));
      }
      tl.ReplaceVoiceprint(evidence);
      return tl.Snapshot();
    };

    const std::string reason =
        "voiceprint_adjacent_primary_supported_prefix_restore";
    auto has_exact_restore = [&](const std::vector<Entry>& entries) {
      bool restored_prefix = false;
      bool following_candidate = false;
      bool next_incumbent = false;
      for (const auto& entry : entries) {
        if (entry.text == "甲" && entry.speaker_id == "spk_b" &&
            entry.speaker_decision.reason == reason &&
            entry.speaker_decision.speaker_source ==
                "sortformer_activity+primary_top1+titanet_adjacent_pair+"
                "robust_gallery+forced_alignment") {
          restored_prefix = true;
        }
        if (entry.text.find("乙") != std::string::npos &&
            entry.speaker_id == "spk_b") {
          following_candidate = true;
        }
        if (entry.text.find("丁") != std::string::npos &&
            entry.speaker_id == "spk_a" &&
            entry.speaker_decision.reason != reason) {
          next_incumbent = true;
        }
      }
      return restored_prefix && following_candidate && next_incumbent;
    };
    auto preserves_prefix = [&](const CaseOptions& options) {
      const auto entries = run_case(options);
      return std::none_of(entries.begin(), entries.end(), [&](const auto& entry) {
        return entry.speaker_decision.reason == reason;
      });
    };

    CHECK(has_exact_restore(run_case({})),
          "an eligible pair with weaker background activity restores only "
          "the initial prefix");
    CaseOptions noninitial;
    noninitial.pair_source_initial = false;
    CHECK(preserves_prefix(noninitial),
          "a noninitial adjacent pair preserves the current prefix");
    CaseOptions disagreement;
    disagreement.pair_robust_candidate = false;
    CHECK(preserves_prefix(disagreement),
          "adjacent-pair gallery disagreement preserves the current prefix");
    CaseOptions abstention;
    abstention.pair_margin_passes = false;
    CHECK(preserves_prefix(abstention),
          "adjacent-pair margin abstention preserves the current prefix");
    CaseOptions embedded_leading;
    embedded_leading.leading_embedding = true;
    CHECK(preserves_prefix(embedded_leading),
          "an embedded leading interval preserves the current prefix");
    CaseOptions incomplete_following;
    incomplete_following.following_robust_complete = false;
    CHECK(preserves_prefix(incomplete_following),
          "an incomplete following interval preserves the current prefix");
    CaseOptions extra_component;
    extra_component.extra_component = true;
    CHECK(preserves_prefix(extra_component),
          "an extra typed component preserves the current prefix");
    CaseOptions late_primary;
    late_primary.primary_starts_in_prefix = false;
    CHECK(preserves_prefix(late_primary),
          "a primary run starting after the prefix blocks restoration");
    CaseOptions late_activity;
    late_activity.candidate_activity_starts_in_prefix = false;
    CHECK(preserves_prefix(late_activity),
          "candidate activity starting after the prefix blocks restoration");
    CaseOptions activity_gap;
    activity_gap.incumbent_activity_covers = false;
    CHECK(preserves_prefix(activity_gap),
          "insufficient incumbent activity coverage blocks restoration");
    CaseOptions extra_activity;
    extra_activity.extra_activity = true;
    CHECK(preserves_prefix(extra_activity),
          "a fourth or stronger activity identity blocks restoration");
    CaseOptions insufficient_alignment;
    insufficient_alignment.enough_aligned_units = false;
    CHECK(preserves_prefix(insufficient_alignment),
          "insufficient following alignment blocks restoration");
    CaseOptions missing_next;
    missing_next.include_next_interval = false;
    CHECK(preserves_prefix(missing_next),
          "a missing following handoff blocks restoration");
    CaseOptions changed_next;
    changed_next.next_primary_incumbent = false;
    CHECK(preserves_prefix(changed_next),
          "a changed following identity blocks restoration");
    CaseOptions overlapping_next;
    overlapping_next.next_starts_after_pair = false;
    CHECK(preserves_prefix(overlapping_next),
          "an overlapping following interval blocks restoration");
  }

  // ---- 20d. FR16ABO may use a later stable identity epoch only when native,
  // alignment, and both gallery views corroborate the exact phrase. ----
  {
    struct CaseOptions {
      double lookahead_sec = 4.0;
      double future_start = 2.0;
      bool future_primary = true;
      bool robust_margin = true;
      bool session_candidate_top_two = true;
      bool enough_alignment = true;
      bool competing_activity = false;
    };
    auto run_case = [](const CaseOptions& options) {
      BusinessSpeakerPipeline::Config config;
      config.voiceprint_fusion_enabled = true;
      config.voiceprint_future_epoch_lookahead_sec = options.lookahead_sec;
      config.voiceprint_four_view_min_aligned_units = 2;
      TestBusinessSpeakerPipeline tl(config);
      tl.UpsertText(0, 0.0, 0.8, "甲乙。");
      std::vector<TestBusinessSpeakerPipeline::SpeakerInput> activity = {
          {0.0, 0.8, "slot_0", 0.9f, "spk_old"},
          {options.future_start, options.future_start + 1.0, "slot_0", 0.9f,
           "spk_new"}};
      if (options.competing_activity) {
        activity.push_back({0.2, 0.6, "slot_1", 0.9f, "spk_other"});
      }
      tl.ReplaceSpeakers(activity);
      std::vector<TestBusinessSpeakerPipeline::SpeakerInput> primary = {
          {0.0, 0.8, "slot_0", 0.9f, "spk_old"}};
      if (options.future_primary) {
        primary.push_back({options.future_start, options.future_start + 1.0,
                           "slot_0", 0.9f, "spk_new"});
      }
      tl.ReplacePrimarySpeakers(primary);
      tl.UpsertAlign(0, 0.0, 0.8,
                     options.enough_alignment
                         ? std::vector<TestBusinessSpeakerPipeline::AlignUnitSeg>{
                               {0.0, 0.3, "甲"}, {0.3, 0.7, "乙"}}
                         : std::vector<TestBusinessSpeakerPipeline::AlignUnitSeg>{
                               {0.0, 0.7, "甲乙"}});
      ComprehensiveTimeline::SpeakerVoiceprintEvidence phrase;
      phrase.evidence_id = "punctuation_phrase:0:0";
      phrase.kind = "punctuation_phrase";
      phrase.text_id = 0;
      phrase.source_start = 0;
      phrase.source_end = 3;
      phrase.start = 0.0;
      phrase.end = 0.7;
      phrase.embedding_available = true;
      phrase.robust_gallery_complete = true;
      phrase.session_scores = options.session_candidate_top_two
                                  ? std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                        {"spk_new", 0.52f},
                                        {"spk_old", 0.50f},
                                        {"spk_other", 0.30f}}
                                  : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                        {"spk_other", 0.52f},
                                        {"spk_third", 0.50f},
                                        {"spk_new", 0.30f},
                                        {"spk_old", 0.20f}};
      phrase.robust_scores = options.robust_margin
                                 ? std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                       {"spk_new", 0.70f},
                                       {"spk_old", 0.50f},
                                       {"spk_other", 0.30f}}
                                 : std::vector<ComprehensiveTimeline::VoiceprintScore>{
                                       {"spk_new", 0.52f},
                                       {"spk_old", 0.50f},
                                       {"spk_other", 0.30f}};
      tl.ReplaceVoiceprint({phrase});
      return tl.Snapshot();
    };
    auto future_epoch_selected = [](const auto& entries) {
      return entries.size() == 1 && entries[0].speaker_id == "spk_new" &&
             entries[0].speaker_decision.reason ==
                 "voiceprint_phrase_future_epoch_robust_override";
    };
    auto future_epoch_abstained = [](const auto& entries) {
      for (const auto& entry : entries) {
        if (entry.speaker_decision.reason ==
            "voiceprint_phrase_future_epoch_robust_override") {
          return false;
        }
      }
      return true;
    };

    CHECK(future_epoch_selected(run_case({})),
          "future epoch plus native and gallery evidence selects exact phrase");
    CaseOptions disabled;
    disabled.lookahead_sec = 0.0;
    CHECK(future_epoch_abstained(run_case(disabled)),
          "zero lookahead disables future-epoch corroboration");
    CaseOptions too_far;
    too_far.lookahead_sec = 1.0;
    CHECK(future_epoch_abstained(run_case(too_far)),
          "future identity outside the TOML horizon abstains");
    CaseOptions no_future_primary;
    no_future_primary.future_primary = false;
    CHECK(future_epoch_abstained(run_case(no_future_primary)),
          "future identity without primary support abstains");
    CaseOptions weak_robust;
    weak_robust.robust_margin = false;
    CHECK(future_epoch_abstained(run_case(weak_robust)),
          "future identity without robust margin abstains");
    CaseOptions absent_session;
    absent_session.session_candidate_top_two = false;
    CHECK(future_epoch_abstained(run_case(absent_session)),
          "future identity outside session top two abstains");
    CaseOptions insufficient_alignment;
    insufficient_alignment.enough_alignment = false;
    CHECK(future_epoch_abstained(run_case(insufficient_alignment)),
          "future identity with insufficient alignment abstains");
    CaseOptions competing;
    competing.competing_activity = true;
    CHECK(future_epoch_abstained(run_case(competing)),
          "overlapping local activity blocks future-epoch corroboration");
  }

  // ---- 21. An identity change surrounded by unaligned punctuation remains
  // source ordered after terminal serialization sorts entries by time. ----
  {
    BusinessSpeakerPipeline::Config config;
    config.voiceprint_fusion_enabled = true;
    TestBusinessSpeakerPipeline tl(config);
    const std::string source = "呢，嗯，就";
    tl.UpsertText(0, 0.0, 1.2, source);
    tl.ReplaceSpeakers({{0.0, 1.2, "speaker_0", 0.9f, "spk_a"},
                        {1.3, 2.0, "speaker_1", 0.9f, "spk_b"}});
    tl.UpsertAlign(0, 0.0, 1.2,
                   {{0.0, 0.2, "呢"},
                    {0.4, 0.6, "嗯"},
                    {0.8, 1.0, "就"}});
    ComprehensiveTimeline::SpeakerVoiceprintEvidence unit;
    unit.evidence_id = "aligned_unit:0:1";
    unit.kind = "aligned_unit";
    unit.text_id = 0;
    unit.source_start = 2;
    unit.source_end = 3;
    unit.start = 0.4;
    unit.end = 0.6;
    unit.embedding_available = true;
    unit.robust_gallery_complete = true;
    unit.session_scores = {{"spk_a", 0.55f}, {"spk_b", 0.75f}};
    unit.robust_scores = {{"spk_a", 0.54f}, {"spk_b", 0.73f}};
    tl.ReplaceVoiceprint({unit});
    const auto snap = tl.Snapshot();
    std::string rebuilt;
    double previous_end = -1.0;
    bool monotonic = true;
    for (const auto& entry : snap) {
      rebuilt += entry.text;
      if (entry.start + 1e-9 < previous_end) monotonic = false;
      previous_end = entry.end;
    }
    CHECK(rebuilt == source,
          "unaligned punctuation remains in source order after fusion");
    CHECK(monotonic,
          "voiceprint-projected source ranges have monotonic time spans");
  }

  // ---- 22. Slightly reversed adjacent alignment times cannot reorder source
  // text after voiceprint evidence creates an identity boundary. ----
  {
    BusinessSpeakerPipeline::Config config;
    config.voiceprint_fusion_enabled = true;
    TestBusinessSpeakerPipeline tl(config);
    const std::string source = "甲乙";
    tl.UpsertText(0, 0.0, 1.0, source);
    tl.ReplaceSpeakers({{0.0, 1.0, "speaker_0", 0.9f, "spk_a"},
                        {1.1, 2.0, "speaker_1", 0.9f, "spk_b"}});
    tl.UpsertAlign(0, 0.0, 1.0,
                   {{0.4, 0.6, "甲"}, {0.3, 0.5, "乙"}});
    ComprehensiveTimeline::SpeakerVoiceprintEvidence unit;
    unit.evidence_id = "aligned_unit:0:1";
    unit.kind = "aligned_unit";
    unit.text_id = 0;
    unit.source_start = 1;
    unit.source_end = 2;
    unit.start = 0.3;
    unit.end = 0.5;
    unit.embedding_available = true;
    unit.robust_gallery_complete = true;
    unit.session_scores = {{"spk_a", 0.55f}, {"spk_b", 0.75f}};
    unit.robust_scores = {{"spk_a", 0.54f}, {"spk_b", 0.73f}};
    tl.ReplaceVoiceprint({unit});
    const auto snap = tl.Snapshot();
    std::string rebuilt;
    double previous_end = -1.0;
    bool monotonic = true;
    for (const auto& entry : snap) {
      rebuilt += entry.text;
      if (entry.start + 1e-9 < previous_end) monotonic = false;
      previous_end = entry.end;
    }
    CHECK(snap.size() == 2 && snap[0].speaker_id == "spk_a" &&
              snap[1].speaker_id == "spk_b",
          "reversed alignment times preserve the source identity boundary");
    CHECK(rebuilt == source,
          "reversed alignment times preserve source byte order");
    CHECK(monotonic,
          "reversed alignment times are normalized before publication");
  }

  // ---- 23. FR29 splits only an anomalously long alignment unit crossing a
  // handoff corroborated by activity and primary views. A collapsed
  // zero-duration unit that closes the same punctuation clause stays on the
  // preceding source run. ----
  {
    struct CaseOptions {
      bool include_following_primary = true;
      std::string primary_following_id = "spk_b";
      double following_end = 2.0;
      double crossing_end = 1.4;
      double activity_preceding_start = 0.2;
      double primary_preceding_start = 0.2;
      double preceding_end = 1.1;
      double primary_preceding_end = 0.85;
      bool voiceprint = true;
      std::string source = "甲乙丁。丙";
      int source_end = 5;
    };
    auto run_case = [](const CaseOptions& options) {
      BusinessSpeakerPipeline::Config config;
      config.align_snap_pause_sec = 0.25;
      config.align_boundary_split_tolerance_sec = 0.08;
      config.voiceprint_primary_consensus_min_sec = 0.4;
      config.voiceprint_fusion_enabled = options.voiceprint;
      config.speaker_overlap_tie_policy =
          BusinessSpeakerPipeline::SpeakerOverlapTiePolicy::kPrimarySpeaker;
      TestBusinessSpeakerPipeline tl(config);
      tl.UpsertText(0, 0.2, 2.0, options.source);
      tl.ReplaceSpeakers(
          {{options.activity_preceding_start, options.preceding_end, "slot_0",
            0.9f, "spk_a"},
           {1.12, options.following_end, "slot_1", 0.9f, "spk_b"}});
      std::vector<TestBusinessSpeakerPipeline::SpeakerInput> primary = {
          {options.primary_preceding_start, options.primary_preceding_end,
           "slot_0", 0.9f, "spk_a"}};
      if (options.include_following_primary) {
        primary.push_back({1.12, options.following_end, "slot_1", 0.9f,
                           options.primary_following_id});
      }
      tl.ReplacePrimarySpeakers(primary);
      tl.UpsertAlign(0, 0.2, 2.0,
                     {{0.3, 0.4, "甲"},
                      {0.4, options.crossing_end, "乙"},
                      {options.crossing_end, options.crossing_end, "丁"},
                      {1.6, 1.8, "丙"}});
      if (options.voiceprint) {
        ComprehensiveTimeline::SpeakerVoiceprintEvidence interval;
        interval.evidence_id = "business_interval:0:0";
        interval.kind = "business_interval";
        interval.text_id = 0;
        interval.source_start = 0;
        interval.source_end = options.source_end;
        interval.start = 0.3;
        interval.end = 1.8;
        interval.embedding_available = true;
        interval.robust_gallery_complete = true;
        interval.session_scores = {{"spk_a", 0.40f}, {"spk_b", 0.80f}};
        interval.robust_scores = {{"spk_a", 0.42f}, {"spk_b", 0.78f}};
        tl.ReplaceVoiceprint({interval});
      }
      return tl.Snapshot();
    };
    auto retains_corroborated_runs = [](const auto& entries) {
      return entries.size() == 2 && entries[0].speaker_id == "spk_a" &&
             entries[0].text == "甲乙丁。" &&
             entries[1].speaker_id == "spk_b" && entries[1].text == "丙";
    };
    auto all_following = [](const auto& entries) {
      return !entries.empty() &&
             std::all_of(entries.begin(), entries.end(), [](const auto& entry) {
               return entry.speaker_id == "spk_b";
             });
    };

    CHECK(retains_corroborated_runs(run_case({})),
          "corroborated straddled handoff preserves both source runs");

    CaseOptions base_only;
    base_only.voiceprint = false;
    CHECK(retains_corroborated_runs(run_case(base_only)),
          "base projection applies the corroborated alignment split");

    CaseOptions one_view;
    one_view.include_following_primary = false;
    CHECK(all_following(run_case(one_view)),
          "one-view handoff cannot protect a mixed business interval");

    CaseOptions disagreeing_view;
    disagreeing_view.primary_following_id = "spk_c";
    CHECK(all_following(run_case(disagreeing_view)),
          "disagreeing following identities cannot protect a handoff");

    CaseOptions short_following;
    short_following.following_end = 1.4;
    CHECK(all_following(run_case(short_following)),
          "subminimum following runs cannot protect a handoff");

    CaseOptions normal_unit;
    normal_unit.crossing_end = 1.0;
    CHECK(all_following(run_case(normal_unit)),
          "normally timed alignment units preserve existing fusion behavior");

    CaseOptions insufficient_preceding_coverage;
    insufficient_preceding_coverage.activity_preceding_start = 0.9;
    insufficient_preceding_coverage.primary_preceding_start = 0.7;
    CHECK(all_following(run_case(insufficient_preceding_coverage)),
          "insufficient preceding native coverage cannot block voiceprint");

    CaseOptions unaligned_gap_content;
    unaligned_gap_content.source = "甲乙丁。未丙";
    unaligned_gap_content.source_end = 6;
    const auto unaligned_gap_entries = run_case(unaligned_gap_content);
    CHECK(unaligned_gap_entries.size() == 2 &&
              unaligned_gap_entries[0].speaker_id == "spk_a" &&
              unaligned_gap_entries[0].text == "甲乙" &&
              unaligned_gap_entries[1].speaker_id == "spk_b" &&
              unaligned_gap_entries[1].text == "丁。未丙",
          "unaligned source content blocks punctuation-edge inheritance");
  }

  if (g_fail == 0) {
    std::printf("BusinessSpeakerPipeline test PASSED\n");
    return 0;
  }
  std::printf("BusinessSpeakerPipeline test FAILED (%d checks)\n", g_fail);
  return 1;
}
