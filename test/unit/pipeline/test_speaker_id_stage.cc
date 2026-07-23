// Deterministic unit test for SpeakerIdentityStage (Spec 010 Phase B): the
// clean-segment gate, auto-enroll, cross-local re-identification, and the
// local->global assignment. Uses a stub embedder that returns a controlled
// one-hot embedding keyed on the audio content, so the stage's decision logic
// is exercised without GPU weights.

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "core/stages.h"
#include "core/time_base.h"
#include "core/types.h"
#include "model/speaker_database.h"
#include "pipeline/speaker_identity_stage.h"

using namespace orator;

namespace {

constexpr int kDim = 192;

// Returns a controlled 192-d embedding selected by the first audio sample:
// samples[0] <= 0.5 -> "speaker 0" (dim 0), <= 1.5 -> "speaker 1" (dim 1),
// <= 2.5 -> "speaker 2" (dim 2), <= 3.5 -> a weak candidate toward speaker 1,
// else a weak candidate toward speaker 2. Candidate embeddings have cosine
// 0.65. One-hot vectors are orthogonal, giving clean match/no-match behaviour
// while allowing deterministic competing-drift confirmation tests.
class StubEmbedder : public core::ISpeakerEmbedder {
 public:
  void LoadWeights(const std::string&) override {}
  int dim() const override { return kDim; }
  std::string name() const override { return "stub"; }
  std::vector<float> Embed(const core::AudioChunk& c) override {
    ++embed_calls_;
    std::vector<float> e(kDim, 0.0f);
    if (c.num_samples > 0 && c.samples[0] > 2.5f) {
      const int weak_speaker = c.samples[0] <= 3.5f ? 1 : 2;
      e[weak_speaker] = 0.65f;
      e[3] = static_cast<float>(std::sqrt(1.0 - 0.65 * 0.65));
      return e;
    }
    int which = 0;
    if (c.num_samples > 0 && c.samples[0] > 1.5f) {
      which = 2;
    } else if (c.num_samples > 0 && c.samples[0] > 0.5f) {
      which = 1;
    }
    e[which] = 1.0f;
    return e;
  }

  int embed_calls() const { return embed_calls_; }

 private:
  int embed_calls_ = 0;
};

core::DiarSegment Seg(int local, double s, double e, float conf) {
  core::DiarSegment d;
  d.local_speaker = local;
  d.start_sec = s;
  d.end_sec = e;
  d.confidence = conf;
  return d;
}

int fails = 0;
void Check(bool ok, const char* msg) {
  if (!ok) {
    std::printf("FAIL: %s\n", msg);
    ++fails;
  }
}

}  // namespace

int main() {
  const int sr = 16000;
  core::TimeBase tb(sr, 0);
  model::SpeakerDatabase db(/*max_speakers=*/16, kDim);
  StubEmbedder embedder;

  pipeline::SpeakerIdConfig cfg;
  cfg.embedding_dim = kDim;
  cfg.min_embed_sec = 1.0;
  cfg.match_threshold = 0.5f;
  cfg.min_confidence = 0.5f;
  cfg.retain_sec = 180.0;
  cfg.enroll_min_refs = 1;  // enroll on first clean span for this unit test
  pipeline::SpeakerIdentityStage stage(&embedder, &db, tb, cfg);

  // Acoustic precomputation caches only the embedding. Final gallery scoring
  // must be byte-for-byte identical to an uncached path, and session reset must
  // discard the prior session's cache.
  {
    StubEmbedder cached_embedder;
    StubEmbedder uncached_embedder;
    model::SpeakerDatabase cached_db(/*max_speakers=*/16, kDim);
    model::SpeakerDatabase uncached_db(/*max_speakers=*/16, kDim);
    pipeline::SpeakerIdentityStage cached_stage(&cached_embedder, &cached_db,
                                                tb, cfg);
    pipeline::SpeakerIdentityStage uncached_stage(&uncached_embedder,
                                                  &uncached_db, tb, cfg);
    std::vector<float> cache_audio(8 * sr, 0.0f);
    for (int i = 4 * sr; i < 8 * sr; ++i) cache_audio[i] = 1.0f;
    cached_stage.AppendAudio(cache_audio.data(),
                             static_cast<int>(cache_audio.size()));
    uncached_stage.AppendAudio(cache_audio.data(),
                               static_cast<int>(cache_audio.size()));
    std::vector<core::DiarSegment> cached_segments = {Seg(0, 0.5, 3.5, 0.9f),
                                                      Seg(1, 4.5, 7.5, 0.9f)};
    std::vector<core::DiarSegment> uncached_segments = cached_segments;
    cached_stage.Process(cached_segments);
    uncached_stage.Process(uncached_segments);

    const int before_precompute = cached_embedder.embed_calls();
    Check(cached_stage.PrecomputeSpan(3.0, 4.0, 0.4, 0.0, 3.0),
          "available fusion span should precompute");
    Check(cached_embedder.embed_calls() == before_precompute + 1,
          "first precompute should invoke the embedder once");
    const auto cached_evidence =
        cached_stage.EvaluateSpan(3.0, 4.0, {"spk_0", "spk_1"}, 0.4, 0.0, 3.0);
    Check(cached_embedder.embed_calls() == before_precompute + 1,
          "final scoring should reuse the precomputed embedding");
    const auto uncached_evidence = uncached_stage.EvaluateSpan(
        3.0, 4.0, {"spk_0", "spk_1"}, 0.4, 0.0, 3.0);
    Check(cached_evidence.embedding_available ==
                  uncached_evidence.embedding_available &&
              cached_evidence.session_gallery_complete ==
                  uncached_evidence.session_gallery_complete &&
              cached_evidence.robust_gallery_complete ==
                  uncached_evidence.robust_gallery_complete &&
              cached_evidence.session_scores.size() ==
                  uncached_evidence.session_scores.size() &&
              cached_evidence.robust_scores.size() ==
                  uncached_evidence.robust_scores.size(),
          "cached and uncached evidence structure must match exactly");
    for (std::size_t i = 0; i < cached_evidence.session_scores.size(); ++i) {
      Check(cached_evidence.session_scores[i].speaker_id ==
                    uncached_evidence.session_scores[i].speaker_id &&
                cached_evidence.session_scores[i].score ==
                    uncached_evidence.session_scores[i].score,
            "cached and uncached session-gallery scores must match exactly");
    }
    for (std::size_t i = 0; i < cached_evidence.robust_scores.size(); ++i) {
      Check(cached_evidence.robust_scores[i].speaker_id ==
                    uncached_evidence.robust_scores[i].speaker_id &&
                cached_evidence.robust_scores[i].score ==
                    uncached_evidence.robust_scores[i].score,
            "cached and uncached robust-gallery scores must match exactly");
    }

    cached_stage.Reset();
    cached_stage.AppendAudio(cache_audio.data(),
                             static_cast<int>(cache_audio.size()));
    const int before_reset_query = cached_embedder.embed_calls();
    Check(cached_stage.PrecomputeSpan(3.0, 4.0, 0.4, 0.0, 3.0),
          "reset session should precompute retained replacement audio");
    Check(cached_embedder.embed_calls() == before_reset_query + 1,
          "reset must clear the prior session embedding cache");
  }

  // 16 s of audio: [0,8) = speaker 0 (value 0), [8,16) = speaker 1 (value 1).
  std::vector<float> audio(16 * sr, 0.0f);
  for (int i = 8 * sr; i < 16 * sr; ++i) audio[i] = 1.0f;
  stage.AppendAudio(audio.data(), static_cast<int>(audio.size()));

  // Delivery 1: a clean span for each speaker (different local slots) plus a
  // too-short span that must be gated out.
  {
    std::vector<core::DiarSegment> segs = {
        Seg(0, 1.0, 4.0, 0.9f),    // speaker 0 audio -> enroll spk_0
        Seg(1, 9.0, 12.0, 0.9f),   // speaker 1 audio -> enroll spk_1
        Seg(2, 14.0, 14.4, 0.9f),  // 0.4 s < min_embed_sec -> gated, no id
    };
    stage.Process(segs);
    Check(segs[0].speaker_id == "spk_0", "seg0 should enroll spk_0");
    Check(segs[1].speaker_id == "spk_1", "seg1 should enroll spk_1");
    Check(segs[2].speaker_id.empty(), "short seg must stay unidentified");
    Check(db.Size() == 2, "two speakers enrolled");
  }

  // Final phrase queries reuse the mature session and robust galleries but do
  // not make an identity decision inside the identity stage.
  {
    const auto references = stage.RetainedReferences();
    Check(references.size() == 2,
          "retained reference provenance should expose both gallery rows");
    Check(references[0].evidence_id == "identity_ref:0:0:0" &&
              references[0].source_start_sec == 1.0 &&
              references[0].source_end_sec == 4.0 &&
              references[0].embedding_start_sample == 20800 &&
              references[0].embedding_end_sample == 59200 &&
              references[0].speaker_id == "spk_0",
          "retained reference provenance must preserve exact source bounds");

    const auto evidence = stage.EvaluateSpan(
        1.0, 2.0, {"spk_0", "spk_1"}, 0.4, 0.0, 3.0);
    Check(evidence.embedding_available,
          "retained phrase audio should produce an embedding");
    Check(evidence.session_gallery_complete &&
              evidence.robust_gallery_complete,
          "both voiceprint gallery views should be complete");
    Check(evidence.session_scores.size() == 2 &&
              evidence.robust_scores.size() == 2,
          "phrase evidence exposes every active identity score");
    Check(stage.IdentityAt(0, 2.0) == "spk_0",
          "common-clock local identity lookup resolves the active epoch");

    const auto excluded = stage.EvaluateSpanWithoutOverlappingReferences(
        1.0, 2.0, {"spk_0", "spk_1"}, 0.4, 0.0, 3.0);
    Check(excluded.query_embedding_start_sample == 16000 &&
              excluded.query_embedding_end_sample == 32000,
          "diagnostic query must preserve exact embedded sample bounds");
    Check(excluded.intersecting_reference_ids ==
              std::vector<std::string>{"identity_ref:0:0:0"},
          "diagnostic query must list only positive sample intersections");
    Check(excluded.evidence.embedding_available &&
              !excluded.evidence.session_gallery_complete &&
              !excluded.evidence.robust_gallery_complete &&
              excluded.evidence.session_scores.size() == 1 &&
              excluded.evidence.session_scores[0].speaker_id == "spk_1" &&
              excluded.evidence.robust_scores.size() == 1 &&
              excluded.evidence.robust_scores[0].speaker_id == "spk_1",
          "overlap exclusion must retain raw scores only for remaining refs");

    const auto independent = stage.EvaluateSpanWithoutOverlappingReferences(
        6.0, 7.0, {"spk_0", "spk_1"}, 0.4, 0.0, 3.0);
    Check(independent.intersecting_reference_ids.empty() &&
              independent.evidence.session_gallery_complete &&
              independent.evidence.robust_gallery_complete &&
              independent.evidence.session_scores.size() == 2 &&
              independent.evidence.robust_scores.size() == 2,
          "non-overlapping query must retain both complete gallery views");
  }

  // Delivery 2: a NEW SESSION's local slot (4 = session 1, speakers_per_session
  // = 4) whose audio is speaker 0 must re-identify as spk_0 by CROSS-SESSION
  // voiceprint match. (Same-session slots never merge, but a later session's
  // slot is stitched to an earlier global by matching.)
  {
    std::vector<core::DiarSegment> segs = {
        Seg(4, 5.0, 7.5, 0.9f),    // speaker 0 audio -> match spk_0
    };
    stage.Process(segs);
    Check(segs[0].speaker_id == "spk_0", "cross-session slot must re-id as spk_0");
    Check(db.Size() == 2, "no new enrollment on re-identification");
  }

  // Overlapping different-speaker spans are not clean -> not embedded and not
  // matched; a brand-new overlapped local slot gets no identity.
  {
    std::vector<core::DiarSegment> segs = {
        Seg(8, 2.0, 5.0, 0.9f),    // overlaps local 9 below (different speaker)
        Seg(9, 3.0, 6.0, 0.9f),
    };
    stage.Process(segs);
    Check(segs[0].speaker_id.empty() && segs[1].speaker_id.empty(),
          "overlapped (non-single-speaker) spans must stay unidentified");
    Check(db.Size() == 2, "overlap must not enroll");
  }

  // Long turns are embedded from their centre window. If a different local
  // speaker overlaps only the discarded edge, the centre voiceprint is still
  // clean evidence and should be used.
  {
    model::SpeakerDatabase db_edge(/*max_speakers=*/16, kDim);
    pipeline::SpeakerIdConfig cfg_edge = cfg;
    cfg_edge.max_embed_window_sec = 4.0;
    pipeline::SpeakerIdentityStage stage_edge(&embedder, &db_edge, tb,
                                              cfg_edge);
    std::vector<float> audio_edge(14 * sr, 0.0f);
    stage_edge.AppendAudio(audio_edge.data(),
                           static_cast<int>(audio_edge.size()));

    std::vector<core::DiarSegment> segs = {
        Seg(0, 0.0, 12.0, 0.9f),  // centre window [4,8] is single-speaker
        Seg(1, 0.5, 2.0, 0.9f),   // boundary overlap only
    };
    stage_edge.Process(segs);
    Check(segs[0].speaker_id == "spk_0",
          "edge-overlapped long span should still enroll from centre window");
    Check(segs[1].speaker_id.empty(),
          "short overlapped edge span must stay unidentified");
    Check(db_edge.Size() == 1,
          "edge overlap should enroll only the clean centre speaker");
  }

  // Low-confidence spans are gated out.
  {
    std::vector<core::DiarSegment> segs = {Seg(12, 1.0, 4.0, 0.2f)};
    stage.Process(segs);
    Check(segs[0].speaker_id.empty(), "low-confidence span must be gated");
  }

  // A conservative cross-session policy can require more than one clean
  // reference before a reset-session slot re-identifies. The first clean span is
  // intentionally left local-only; the second span gives a stable centroid and
  // matches the existing global id.
  {
    model::SpeakerDatabase db2(/*max_speakers=*/16, kDim);
    pipeline::SpeakerIdConfig cfg2 = cfg;
    cfg2.cross_session_match_min_refs = 2;
    pipeline::SpeakerIdentityStage stage2(&embedder, &db2, tb, cfg2);
    std::vector<float> audio2(12 * sr, 0.0f);
    stage2.AppendAudio(audio2.data(), static_cast<int>(audio2.size()));

    std::vector<core::DiarSegment> first_session = {Seg(0, 1.0, 3.0, 0.9f)};
    stage2.Process(first_session);
    Check(first_session[0].speaker_id == "spk_0",
          "control speaker should enroll before reset");

    std::vector<core::DiarSegment> first_ref = {Seg(4, 4.0, 6.0, 0.9f)};
    stage2.Process(first_ref);
    Check(first_ref[0].speaker_id.empty(),
          "cross-session slot waits for enough references");
    Check(db2.Size() == 1, "deferred re-id must not enroll a duplicate");

    std::vector<core::DiarSegment> second_ref = {Seg(4, 7.0, 9.0, 0.9f)};
    stage2.Process(second_ref);
    Check(second_ref[0].speaker_id == "spk_0",
          "cross-session slot re-identifies after enough references");
    Check(db2.Size() == 1, "re-identification must not grow registry");
  }

  // With defer_unmatched_cross_session enabled, a later-session slot that does
  // not match any known global remains local-only instead of immediately
  // registering a new global id. This is the guard against uncertain reset
  // windows becoming stable wrong speaker identities.
  {
    model::SpeakerDatabase db3(/*max_speakers=*/16, kDim);
    pipeline::SpeakerIdConfig cfg3 = cfg;
    cfg3.defer_unmatched_cross_session = true;
    pipeline::SpeakerIdentityStage stage3(&embedder, &db3, tb, cfg3);
    std::vector<float> audio3(10 * sr, 0.0f);
    for (int i = 4 * sr; i < 10 * sr; ++i) audio3[i] = 2.0f;
    stage3.AppendAudio(audio3.data(), static_cast<int>(audio3.size()));

    std::vector<core::DiarSegment> known = {Seg(0, 1.0, 3.0, 0.9f)};
    stage3.Process(known);
    Check(known[0].speaker_id == "spk_0",
          "known speaker should enroll in session zero");

    std::vector<core::DiarSegment> unknown = {Seg(4, 5.0, 7.0, 0.9f)};
    stage3.Process(unknown);
    Check(unknown[0].speaker_id.empty(),
          "unmatched cross-session slot stays local-only");
    Check(db3.Size() == 1, "unmatched cross-session slot must not enroll");
  }

  // A long-session diarizer-local slot can drift to another real speaker. With
  // local drift epochs enabled, the later epoch can match an existing
  // same-session global speaker while the earlier epoch keeps its original id.
  {
    model::SpeakerDatabase db4(/*max_speakers=*/16, kDim);
    pipeline::SpeakerIdConfig cfg4 = cfg;
    cfg4.local_drift_threshold = 0.25f;
    cfg4.local_drift_min_span_sec = 5.0;
    cfg4.local_drift_min_epoch_sec = 0.0;
    cfg4.local_drift_allow_same_session_match = true;
    pipeline::SpeakerIdentityStage stage4(&embedder, &db4, tb, cfg4);

    std::vector<float> audio4(24 * sr, 0.0f);
    for (int i = 8 * sr; i < 24 * sr; ++i) audio4[i] = 1.0f;
    stage4.AppendAudio(audio4.data(), static_cast<int>(audio4.size()));

    std::vector<core::DiarSegment> initial = {
        Seg(0, 1.0, 4.0, 0.9f),   // local 0 -> speaker 0 -> spk_0
        Seg(1, 9.0, 12.0, 0.9f),  // local 1 -> speaker 1 -> spk_1
    };
    stage4.Process(initial);
    Check(initial[0].speaker_id == "spk_0", "drift setup local0 -> spk_0");
    Check(initial[1].speaker_id == "spk_1", "drift setup local1 -> spk_1");

    std::vector<core::DiarSegment> drift = {
        Seg(0, 1.0, 4.0, 0.9f),
        Seg(1, 9.0, 12.0, 0.9f),
        Seg(0, 17.0, 23.0, 0.9f),  // local 0 now carries speaker 1 audio
    };
    stage4.Process(drift);
    Check(drift[0].speaker_id == "spk_0",
          "early local0 epoch must keep spk_0");
    Check(drift[1].speaker_id == "spk_1",
          "local1 epoch must keep spk_1");
    Check(drift[2].speaker_id == "spk_1",
          "drifted local0 epoch should match existing spk_1");
    Check(db4.Size() == 2, "drift match must not enroll a duplicate speaker");
  }

  // Similar voices may still exceed the simple "unlike current epoch" drift
  // threshold. A cleaner signal is a competing known global id that scores
  // better than the active epoch; in that case the local slot starts a new epoch
  // bound to the competing identity.
  {
    model::SpeakerDatabase db5(/*max_speakers=*/16, kDim);
    pipeline::SpeakerIdConfig cfg5 = cfg;
    cfg5.local_drift_threshold = 0.0f;  // disable dissimilarity split
    cfg5.local_drift_min_span_sec = 5.0;
    cfg5.local_drift_min_epoch_sec = 0.0;
    cfg5.local_drift_allow_same_session_match = true;
    cfg5.local_drift_competing_threshold = 0.8f;
    cfg5.local_drift_competing_margin = 0.1f;
    pipeline::SpeakerIdentityStage stage5(&embedder, &db5, tb, cfg5);

    std::vector<float> audio5(24 * sr, 0.0f);
    for (int i = 8 * sr; i < 24 * sr; ++i) audio5[i] = 1.0f;
    stage5.AppendAudio(audio5.data(), static_cast<int>(audio5.size()));

    std::vector<core::DiarSegment> initial = {
        Seg(0, 1.0, 4.0, 0.9f),   // local 0 -> speaker 0 -> spk_0
        Seg(1, 9.0, 12.0, 0.9f),  // local 1 -> speaker 1 -> spk_1
    };
    stage5.Process(initial);
    Check(initial[0].speaker_id == "spk_0",
          "competing setup local0 -> spk_0");
    Check(initial[1].speaker_id == "spk_1",
          "competing setup local1 -> spk_1");

    std::vector<core::DiarSegment> competing = {
        Seg(0, 1.0, 4.0, 0.9f),
        Seg(1, 9.0, 12.0, 0.9f),
        Seg(0, 15.0, 15.8, 0.9f),
        Seg(0, 17.0, 23.0, 0.9f),  // local 0 now matches spk_1
    };
    stage5.Process(competing);
    Check(competing[0].speaker_id == "spk_0",
          "competing split keeps early epoch spk_0");
    Check(competing[2].speaker_id == "spk_1",
          "competing split should backfill short same-local lead-in");
    Check(competing[3].speaker_id == "spk_1",
          "competing split should bind later local0 to spk_1");
    Check(db5.Size() == 2, "competing split must reuse existing speaker id");
  }

  // A weak competing span should not immediately rewrite attribution. If a
  // later strong span confirms the same competing global id, the new epoch is
  // backfilled to the start of the short same-local run that introduced the
  // weak evidence.
  {
    model::SpeakerDatabase db6(/*max_speakers=*/16, kDim);
    pipeline::SpeakerIdConfig cfg6 = cfg;
    cfg6.min_embed_sec = 2.0;
    cfg6.local_drift_threshold = 0.0f;
    cfg6.local_drift_min_epoch_sec = 0.0;
    cfg6.local_drift_allow_same_session_match = true;
    cfg6.local_drift_competing_min_span_sec = 2.0;
    cfg6.local_drift_competing_threshold = 0.8f;
    cfg6.local_drift_competing_margin = 0.1f;
    cfg6.local_drift_competing_candidate_threshold = 0.6f;
    cfg6.local_drift_competing_candidate_margin = 0.05f;
    cfg6.local_drift_competing_backfill_sec = 20.0;
    cfg6.local_drift_competing_backfill_gap_sec = 4.0;
    pipeline::SpeakerIdentityStage stage6(&embedder, &db6, tb, cfg6);

    std::vector<float> audio6(32 * sr, 0.0f);
    for (int i = 8 * sr; i < 13 * sr; ++i) audio6[i] = 1.0f;
    for (int i = 16 * sr; i < 21 * sr; ++i) audio6[i] = 3.0f;
    for (int i = 24 * sr; i < 32 * sr; ++i) audio6[i] = 1.0f;
    stage6.AppendAudio(audio6.data(), static_cast<int>(audio6.size()));

    std::vector<core::DiarSegment> initial = {
        Seg(0, 1.0, 4.0, 0.9f),
        Seg(1, 9.0, 12.0, 0.9f),
    };
    stage6.Process(initial);
    Check(initial[0].speaker_id == "spk_0",
          "backfill setup local0 -> spk_0");
    Check(initial[1].speaker_id == "spk_1",
          "backfill setup local1 -> spk_1");

    std::vector<core::DiarSegment> weak_only = {
        Seg(0, 1.0, 4.0, 0.9f),
        Seg(1, 9.0, 12.0, 0.9f),
        Seg(0, 15.0, 15.8, 0.9f),
        Seg(0, 17.0, 20.0, 0.9f),
    };
    stage6.Process(weak_only);
    Check(weak_only[2].speaker_id == "spk_0",
          "weak competing evidence alone must not rewrite the short run");
    Check(weak_only[3].speaker_id == "spk_0",
          "weak competing evidence alone must stay on the active epoch");

    std::vector<core::DiarSegment> confirmed = {
        Seg(0, 1.0, 4.0, 0.9f),
        Seg(1, 9.0, 12.0, 0.9f),
        Seg(0, 15.0, 15.8, 0.9f),
        Seg(0, 17.0, 20.0, 0.9f),
        Seg(0, 24.0, 30.0, 0.9f),
    };
    stage6.Process(confirmed);
    Check(confirmed[0].speaker_id == "spk_0",
          "backfilled split must preserve early local0 epoch");
    Check(confirmed[2].speaker_id == "spk_1",
          "confirmed competing split backfills the short same-local run");
    Check(confirmed[3].speaker_id == "spk_1",
          "confirmed competing split backfills the weak clean span");
    Check(confirmed[4].speaker_id == "spk_1",
          "confirmed competing split binds the strong span");
    Check(db6.Size() == 2, "backfilled competing split must reuse speaker id");
  }

  // Repeated candidate-strength spans can confirm an epoch without lowering
  // the strong gate. A different candidate replaces the pending state, and a
  // strong active-epoch span clears it. A later strong return starts another
  // epoch instead of rewriting the confirmed interval.
  {
    model::SpeakerDatabase db7(/*max_speakers=*/16, kDim);
    pipeline::SpeakerIdConfig cfg7 = cfg;
    cfg7.min_embed_sec = 2.0;
    cfg7.local_drift_threshold = 0.0f;
    cfg7.local_drift_min_epoch_sec = 0.0;
    cfg7.local_drift_allow_same_session_match = true;
    cfg7.local_drift_competing_min_span_sec = 2.0;
    cfg7.local_drift_competing_threshold = 0.8f;
    cfg7.local_drift_competing_margin = 0.1f;
    cfg7.local_drift_competing_candidate_threshold = 0.6f;
    cfg7.local_drift_competing_candidate_margin = 0.05f;
    cfg7.local_drift_competing_candidate_min_confirmations = 2;
    cfg7.local_drift_competing_backfill_sec = 20.0;
    cfg7.local_drift_competing_backfill_gap_sec = 4.0;
    pipeline::SpeakerIdentityStage stage7(&embedder, &db7, tb, cfg7);

    std::vector<float> audio7(64 * sr, 0.0f);
    for (int i = 8 * sr; i < 13 * sr; ++i) audio7[i] = 1.0f;
    for (int i = 14 * sr; i < 18 * sr; ++i) audio7[i] = 2.0f;
    for (int i = 21 * sr; i < 25 * sr; ++i) audio7[i] = 3.0f;
    for (int i = 27 * sr; i < 31 * sr; ++i) audio7[i] = 4.0f;
    for (int i = 33 * sr; i < 37 * sr; ++i) audio7[i] = 3.0f;
    for (int i = 45 * sr; i < 49 * sr; ++i) audio7[i] = 3.0f;
    for (int i = 51 * sr; i < 55 * sr; ++i) audio7[i] = 3.0f;
    stage7.AppendAudio(audio7.data(), static_cast<int>(audio7.size()));

    std::vector<core::DiarSegment> setup = {
        Seg(0, 1.0, 4.0, 0.9f), Seg(1, 9.0, 12.0, 0.9f),
        Seg(2, 14.0, 17.0, 0.9f)};
    stage7.Process(setup);
    Check(setup[0].speaker_id == "spk_0", "repeat setup local0 -> spk_0");
    Check(setup[1].speaker_id == "spk_1", "repeat setup local1 -> spk_1");
    Check(setup[2].speaker_id == "spk_2", "repeat setup local2 -> spk_2");

    auto first_candidate = setup;
    first_candidate.push_back(Seg(0, 21.0, 24.0, 0.9f));
    stage7.Process(first_candidate);
    Check(first_candidate.back().speaker_id == "spk_0",
          "one candidate must preserve the active epoch");

    auto conflicting_candidate = first_candidate;
    conflicting_candidate.push_back(Seg(0, 27.0, 30.0, 0.9f));
    stage7.Process(conflicting_candidate);
    Check(conflicting_candidate.back().speaker_id == "spk_0",
          "a different candidate must replace rather than confirm");

    auto replaced_candidate = conflicting_candidate;
    replaced_candidate.push_back(Seg(0, 33.0, 36.0, 0.9f));
    stage7.Process(replaced_candidate);
    Check(replaced_candidate.back().speaker_id == "spk_0",
          "one span after candidate replacement must remain provisional");

    auto active_confirmation = replaced_candidate;
    active_confirmation.push_back(Seg(0, 39.0, 42.0, 0.9f));
    stage7.Process(active_confirmation);
    Check(active_confirmation.back().speaker_id == "spk_0",
          "strong active evidence must clear the pending candidate");

    auto repeated_first = active_confirmation;
    repeated_first.push_back(Seg(0, 45.0, 48.0, 0.9f));
    stage7.Process(repeated_first);
    Check(repeated_first.back().speaker_id == "spk_0",
          "first repeated candidate must remain provisional");

    auto repeated_second = repeated_first;
    repeated_second.push_back(Seg(0, 51.0, 54.0, 0.9f));
    stage7.Process(repeated_second);
    Check(repeated_second[repeated_second.size() - 2].speaker_id == "spk_1",
          "second matching candidate must backfill the confirmed epoch");
    Check(repeated_second.back().speaker_id == "spk_1",
          "second matching candidate must bind to the competing identity");
    const auto repeated_references = stage7.RetainedReferences();
    bool found_first_candidate_reference = false;
    bool found_confirming_reference = false;
    bool found_extended_reference = false;
    for (const auto& reference : repeated_references) {
      if (reference.local_speaker != 0) continue;
      found_first_candidate_reference |=
          reference.source_start_sec == 45.0 &&
          reference.source_end_sec == 48.0 &&
          reference.embedding_start_sample == 724800 &&
          reference.embedding_end_sample == 763200;
      found_confirming_reference |= reference.source_start_sec == 51.0 &&
                                    reference.source_end_sec == 54.0;
      found_extended_reference |= reference.source_start_sec == 45.0 &&
                                  reference.source_end_sec == 54.0;
    }
    Check(found_first_candidate_reference && found_confirming_reference &&
              !found_extended_reference,
          "repeated confirmation provenance must preserve each embedded span");

    auto strong_return = repeated_second;
    strong_return.push_back(Seg(0, 57.0, 63.0, 0.9f));
    stage7.Process(strong_return);
    Check(strong_return[strong_return.size() - 2].speaker_id == "spk_1",
          "strong return must preserve the confirmed competing epoch");
    Check(strong_return.back().speaker_id == "spk_0",
          "strong return must start a new active-identity epoch");
    Check(db7.Size() == 3,
          "repeated confirmation must reuse enrolled speaker ids");
  }

  if (fails) {
    std::printf("SpeakerIdentityStage test FAILED (%d)\n", fails);
    return 1;
  }
  std::printf("SpeakerIdentityStage test PASSED\n");
  return 0;
}
