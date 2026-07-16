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

  // ---- 20g5. FR16ABB: an isolated subminimum aligned unit without its own
  // embedding may recover one initial slot identity only from uncontested
  // native tracks and a passing dual-gallery containing VAD. ----
  {
    using Entry = ComprehensiveTimeline::Entry;
    auto run_case = [](bool include_epoch, bool isolated, bool strong_vad,
                       bool gallery_agreement, bool primary_current,
                       bool unit_embedding_available) -> std::vector<Entry> {
      BusinessSpeakerPipeline::Config config;
      config.voiceprint_fusion_enabled = true;
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
                      {1.5, 2.0, "后"}});

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

  if (g_fail == 0) {
    std::printf("BusinessSpeakerPipeline test PASSED\n");
    return 0;
  }
  std::printf("BusinessSpeakerPipeline test FAILED (%d checks)\n", g_fail);
  return 1;
}
