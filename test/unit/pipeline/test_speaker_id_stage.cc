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

// Returns a one-hot 192-d embedding selected by the first audio sample:
// samples[0] <= 0.5 -> "speaker 0" (dim 0), else "speaker 1" (dim 1). One-hot
// vectors are orthogonal, so cosine is 1 within a speaker and 0 across, giving
// a clean match/no-match around any threshold in (0, 1).
class StubEmbedder : public core::ISpeakerEmbedder {
 public:
  void LoadWeights(const std::string&) override {}
  int dim() const override { return kDim; }
  std::string name() const override { return "stub"; }
  std::vector<float> Embed(const core::AudioChunk& c) override {
    std::vector<float> e(kDim, 0.0f);
    int which = (c.num_samples > 0 && c.samples[0] > 0.5f) ? 1 : 0;
    e[which] = 1.0f;
    return e;
  }
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

  // Low-confidence spans are gated out.
  {
    std::vector<core::DiarSegment> segs = {Seg(12, 1.0, 4.0, 0.2f)};
    stage.Process(segs);
    Check(segs[0].speaker_id.empty(), "low-confidence span must be gated");
  }

  if (fails) {
    std::printf("SpeakerIdentityStage test FAILED (%d)\n", fails);
    return 1;
  }
  std::printf("SpeakerIdentityStage test PASSED\n");
  return 0;
}
