#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "core/stages.h"
#include "core/time_base.h"
#include "model/speaker_database.h"
#include "pipeline/speaker_identity_stage.h"
#include "test_speaker_evidence_stage_access.h"

namespace {

int failures = 0;

#define CHECK(condition, message)    \
  do {                               \
    if (!(condition)) {              \
      std::printf("FAIL: %s\n", message); \
      ++failures;                    \
    }                                \
  } while (0)

using Range = std::pair<int, int>;
using orator::pipeline::TestSpeakerEvidenceStage;
using VoiceprintEvidence =
    orator::pipeline::ComprehensiveTimeline::SpeakerVoiceprintEvidence;

class CountingEmbedder : public orator::core::ISpeakerEmbedder {
 public:
  void LoadWeights(const std::string&) override {}
  int dim() const override { return 4; }
  std::string name() const override { return "counting"; }
  std::vector<float> Embed(const orator::core::AudioChunk&) override {
    ++calls_;
    return {1.0f, 0.0f, 0.0f, 0.0f};
  }

  int calls() const { return calls_; }

 private:
  int calls_ = 0;
};

void CheckReconstruction(const std::vector<Range>& ranges, int start, int end) {
  CHECK(!ranges.empty(), "split emits at least one range");
  if (ranges.empty()) return;
  CHECK(ranges.front().first == start, "split preserves source start");
  CHECK(ranges.back().second == end, "split preserves source end");
  for (std::size_t index = 1; index < ranges.size(); ++index) {
    CHECK(ranges[index - 1].second == ranges[index].first,
          "split ranges reconstruct without a gap or overlap");
  }
}

}  // namespace

int main() {
  const std::vector<Range> phrases = {{0, 5}, {5, 10}, {10, 15}};

  const auto both_edges =
      TestSpeakerEvidenceStage::SplitPartialPhraseEdges(2, 12, phrases);
  CHECK(both_edges == std::vector<Range>({{2, 5}, {5, 10}, {10, 12}}),
        "leading and trailing partial phrases are isolated");
  CheckReconstruction(both_edges, 2, 12);

  const auto trailing =
      TestSpeakerEvidenceStage::SplitPartialPhraseEdges(0, 12, phrases);
  CHECK(trailing == std::vector<Range>({{0, 10}, {10, 12}}),
        "complete interior phrases retain long contextual evidence");
  CheckReconstruction(trailing, 0, 12);

  const auto contained =
      TestSpeakerEvidenceStage::SplitPartialPhraseEdges(2, 4, phrases);
  CHECK(contained == std::vector<Range>({{2, 4}}),
        "an interval wholly inside one phrase remains intact");

  const auto aligned =
      TestSpeakerEvidenceStage::SplitPartialPhraseEdges(5, 10, phrases);
  CHECK(aligned == std::vector<Range>({{5, 10}}),
        "phrase-aligned interval remains intact");

  auto interval = [](int source_start, int source_end, double start,
                     double end) {
    VoiceprintEvidence value;
    value.kind = "business_interval";
    value.text_id = 7;
    value.source_start = source_start;
    value.source_end = source_end;
    value.start = start;
    value.end = end;
    return value;
  };
  const auto adjacent = TestSpeakerEvidenceStage::BuildAdjacentBusinessPairs(
      {interval(0, 1, 2.0, 2.16), interval(1, 4, 2.16, 2.80)}, 0.4,
      1.5);
  CHECK(adjacent.size() == 1,
        "a subminimum interval and regular adjacent interval form one query");
  if (adjacent.size() == 1) {
    CHECK(adjacent[0].kind == "adjacent_business_pair" &&
              adjacent[0].text_id == 7 && adjacent[0].source_start == 0 &&
              adjacent[0].source_end == 4 && adjacent[0].start == 2.0 &&
              adjacent[0].end == 2.80,
          "adjacent query preserves typed source and common-clock bounds");
  }
  CHECK(TestSpeakerEvidenceStage::BuildAdjacentBusinessPairs(
            {interval(0, 1, 2.0, 2.16), interval(2, 4, 2.16, 2.80)},
            0.4, 1.5)
            .empty(),
        "a source gap does not form an adjacent query");
  CHECK(TestSpeakerEvidenceStage::BuildAdjacentBusinessPairs(
            {interval(0, 1, 2.0, 2.16), interval(1, 4, 2.20, 2.80)},
            0.4, 1.5)
            .empty(),
        "a common-clock gap does not form an adjacent query");
  CHECK(TestSpeakerEvidenceStage::BuildAdjacentBusinessPairs(
            {interval(0, 2, 2.0, 2.40), interval(2, 4, 2.40, 2.80)},
            0.4, 1.5)
            .empty(),
        "a regular leading interval does not form an adjacent query");
  CHECK(TestSpeakerEvidenceStage::BuildAdjacentBusinessPairs(
            {interval(0, 1, 2.0, 2.16), interval(1, 2, 2.16, 2.40)},
            0.4, 1.5)
            .empty(),
        "a subminimum following interval does not form an adjacent query");
  CHECK(TestSpeakerEvidenceStage::BuildAdjacentBusinessPairs(
            {interval(0, 1, 2.0, 2.16), interval(1, 4, 2.16, 3.60)},
            0.4, 1.5)
            .empty(),
        "a pair at the short-span ceiling does not form an adjacent query");

  {
    constexpr int kSampleRate = 10;
    CountingEmbedder embedder;
    orator::model::SpeakerDatabase database(/*max_speakers=*/4, embedder.dim());
    orator::pipeline::SpeakerIdConfig identity_config;
    identity_config.embedding_dim = embedder.dim();
    identity_config.retain_sec = 10.0;
    orator::pipeline::SpeakerIdentityStage identity(
        &embedder, &database, orator::core::TimeBase(kSampleRate, 0),
        identity_config);
    std::vector<float> audio(4 * kSampleRate, 0.25f);
    identity.AppendAudio(audio.data(), static_cast<int>(audio.size()));

    orator::pipeline::SpeakerEvidenceStage::Config evidence_config;
    evidence_config.enabled = true;
    evidence_config.min_embed_sec = 0.4;
    evidence_config.minimum_gallery_size = 2;
    orator::pipeline::SpeakerEvidenceStage stage(&identity, evidence_config);

    orator::pipeline::ComprehensiveTimeline::SpeakerEvidenceSnapshot
        query_snapshot;
    query_snapshot.asr.push_back({7, 0.0, 4.0, "甲乙丙丁"});
    query_snapshot.align.push_back(
        {7,
         0.0,
         4.0,
         {{0.0, 1.0, "甲"},
          {1.0, 2.0, "乙"},
          {2.0, 3.0, "丙"},
          {3.0, 4.0, "丁"}}});
    orator::pipeline::ComprehensiveTimeline::Entry leading;
    leading.start = 0.0;
    leading.end = 2.0;
    leading.speaker_id = "spk_0";
    leading.text = "甲乙";
    leading.text_id = 7;
    orator::pipeline::ComprehensiveTimeline::Entry following;
    following.start = 2.0;
    following.end = 4.0;
    following.speaker_id = "spk_1";
    following.text = "丙丁";
    following.text_id = 7;
    query_snapshot.business_speaker = {leading, following};

    auto business_ranges = [](const auto& queries) {
      std::vector<Range> ranges;
      for (const auto& query : queries) {
        if (query.kind == "business_interval") {
          ranges.push_back({query.source_start, query.source_end});
        }
      }
      return ranges;
    };
    const auto split_queries =
        TestSpeakerEvidenceStage::BuildVoiceprintQueries(stage, query_snapshot);
    CHECK(business_ranges(split_queries) ==
              std::vector<Range>({{0, 2}, {2, 4}}),
          "business projection partitions derived voiceprint queries");

    leading.end = 4.0;
    leading.text = "甲乙丙丁";
    query_snapshot.business_speaker = {leading};
    const auto joined_queries =
        TestSpeakerEvidenceStage::BuildVoiceprintQueries(stage, query_snapshot);
    CHECK(business_ranges(joined_queries) == std::vector<Range>({{0, 4}}),
          "joined business projection emits one derived query");

    orator::pipeline::ComprehensiveTimeline::SpeakerEvidenceSnapshot snapshot;
    snapshot.diarization.push_back(
        {.start = 0.0, .end = 1.0, .speaker_id = "spk_0"});
    snapshot.vad = {{.start = 0.0, .end = 1.0},
                    {.start = 1.0, .end = 2.0},
                    {.start = 2.0, .end = 3.0}};
    TestSpeakerEvidenceStage::Precompute(&stage, snapshot, 1);
    CHECK(embedder.calls() == 0 && stage.precomputed_span_count() == 0,
          "precompute waits for the minimum active gallery");

    snapshot.diarization.push_back(
        {.start = 1.0, .end = 2.0, .speaker_id = "spk_1"});
    TestSpeakerEvidenceStage::Precompute(&stage, snapshot, 1);
    CHECK(embedder.calls() == 1 && stage.precomputed_span_count() == 1,
          "one live cycle caches at most one available span");
    TestSpeakerEvidenceStage::Precompute(&stage, snapshot, 1);
    CHECK(embedder.calls() == 2 && stage.precomputed_span_count() == 2,
          "a later live cycle advances to the next span");
    TestSpeakerEvidenceStage::Precompute(&stage, snapshot, 0);
    CHECK(embedder.calls() == 3 && stage.precomputed_span_count() == 3,
          "unlimited final drain caches every remaining span");
  }

  if (failures == 0) {
    std::printf("SpeakerEvidenceStage test PASSED\n");
    return 0;
  }
  std::printf("SpeakerEvidenceStage test FAILED (%d checks)\n", failures);
  return 1;
}
