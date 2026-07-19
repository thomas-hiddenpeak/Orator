#include <algorithm>
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
    evidence_config.boundary_tolerance_sec = 0.08;
    evidence_config.minimum_gallery_size = 2;
    evidence_config.source_leading_primary_prefix_enabled = true;
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

    query_snapshot.primary_speaker = {
        {.start = 0.0,
         .end = 0.3,
         .speaker = "speaker_0",
         .speaker_id = "spk_0"},
        {.start = 0.3,
         .end = 1.0,
         .speaker = "speaker_1",
         .speaker_id = "spk_1"}};
    const auto primary_queries =
        TestSpeakerEvidenceStage::BuildVoiceprintQueries(stage, query_snapshot);
    std::vector<VoiceprintEvidence> primary_run_queries;
    for (const auto& query : primary_queries) {
      if (query.kind == "primary_run") primary_run_queries.push_back(query);
    }
    CHECK(primary_run_queries.size() == 2 &&
              primary_run_queries[0].evidence_id == "primary_run:0" &&
              primary_run_queries[0].text_id == -1 &&
              primary_run_queries[0].source_start == 0 &&
              primary_run_queries[0].source_end == 0 &&
              primary_run_queries[0].start == 0.0 &&
              primary_run_queries[0].end == 0.3 &&
              primary_run_queries[1].evidence_id == "primary_run:1" &&
              primary_run_queries[1].start == 0.3 &&
              primary_run_queries[1].end == 1.0,
          "enabled primary-run evidence preserves every sorted primary span");

    auto disabled_config = evidence_config;
    disabled_config.source_leading_primary_prefix_enabled = false;
    orator::pipeline::SpeakerEvidenceStage disabled_stage(&identity,
                                                           disabled_config);
    const auto disabled_queries =
        TestSpeakerEvidenceStage::BuildVoiceprintQueries(disabled_stage,
                                                         query_snapshot);
    CHECK(std::none_of(disabled_queries.begin(), disabled_queries.end(),
                       [](const auto& query) {
                         return query.kind == "primary_run";
                       }),
          "disabled primary-prefix policy emits no primary-run evidence");

    leading.end = 4.0;
    leading.text = "甲乙丙丁";
    query_snapshot.business_speaker = {leading};
    const auto joined_queries =
        TestSpeakerEvidenceStage::BuildVoiceprintQueries(stage, query_snapshot);
    CHECK(business_ranges(joined_queries) == std::vector<Range>({{0, 4}}),
          "joined business projection emits one derived query");

    auto alignment_gap_snapshot = [] {
      orator::pipeline::ComprehensiveTimeline::SpeakerEvidenceSnapshot value;
      value.asr.push_back({9, 0.0, 2.2, "甲乙丙。乙丙，丁。"});
      value.align.push_back(
          {9,
           0.0,
           2.2,
           {{0.0, 0.2, "甲"},
            {0.2, 0.4, "乙"},
            {1.2, 1.4, "丙"},
            {1.4, 1.5, "。"},
            {1.5, 1.7, "乙"},
            {1.7, 1.9, "丙"},
            {1.9, 2.0, "，"},
            {2.0, 2.1, "丁"},
            {2.1, 2.2, "。"}}});
      value.primary_speaker = {
          {.start = 0.0,
           .end = 0.52,
           .speaker = "speaker_0",
           .speaker_id = "spk_a"},
          {.start = 0.6,
           .end = 1.0,
           .speaker = "speaker_1",
           .speaker_id = "spk_b"},
          {.start = 1.0,
           .end = 1.6,
           .speaker = "speaker_0",
           .speaker_id = "spk_a"}};
      return value;
    };
    auto alignment_gap_queries = [&](const auto& value) {
      std::vector<VoiceprintEvidence> result;
      for (const auto& query :
           TestSpeakerEvidenceStage::BuildVoiceprintQueries(stage, value)) {
        if (query.kind == "primary_alignment_gap_echo") {
          result.push_back(query);
        }
      }
      return result;
    };
    auto gap_snapshot = alignment_gap_snapshot();
    const auto gap_queries = alignment_gap_queries(gap_snapshot);
    CHECK(gap_queries.size() == 1 && gap_queries[0].text_id == 9 &&
              gap_queries[0].source_start == 4 &&
              gap_queries[0].source_end == 7 &&
              gap_queries[0].start == 0.6 && gap_queries[0].end == 1.0,
          "a unique bracketed primary island emits one typed echo query");

    auto no_suffix = alignment_gap_snapshot();
    no_suffix.asr[0].text = "甲乙丙。甲丙，丁。";
    no_suffix.align[0].units[4].text = "甲";
    CHECK(alignment_gap_queries(no_suffix).empty(),
          "a non-suffix following phrase emits no gap query");

    auto changed_outer = alignment_gap_snapshot();
    changed_outer.primary_speaker[2].speaker_id = "spk_c";
    CHECK(alignment_gap_queries(changed_outer).empty(),
          "different outer primary identities emit no gap query");

    auto wide_primary_gap = alignment_gap_snapshot();
    wide_primary_gap.primary_speaker[0].end = 0.50;
    CHECK(alignment_gap_queries(wide_primary_gap).empty(),
          "a primary boundary beyond configured tolerance emits no gap query");

    auto short_middle = alignment_gap_snapshot();
    short_middle.primary_speaker[0].end = 0.7;
    short_middle.primary_speaker[1].start = 0.7;
    short_middle.primary_speaker[1].end = 1.0;
    CHECK(alignment_gap_queries(short_middle).empty(),
          "a subminimum middle primary run emits no gap query");

    auto outside_gap = alignment_gap_snapshot();
    outside_gap.primary_speaker[0].end = 0.3;
    outside_gap.primary_speaker[1].start = 0.3;
    outside_gap.primary_speaker[1].end = 0.7;
    outside_gap.primary_speaker[2].start = 0.7;
    CHECK(alignment_gap_queries(outside_gap).empty(),
          "a primary run crossing positive alignment emits no gap query");

    auto missing_following_alignment = alignment_gap_snapshot();
    missing_following_alignment.align[0].units.erase(
        missing_following_alignment.align[0].units.begin() + 4,
        missing_following_alignment.align[0].units.begin() + 7);
    CHECK(alignment_gap_queries(missing_following_alignment).empty(),
          "a following source phrase without positive alignment emits no query");

    auto ambiguous_islands = alignment_gap_snapshot();
    ambiguous_islands.align[0].units[2].start = 2.4;
    ambiguous_islands.align[0].units[2].end = 2.6;
    ambiguous_islands.primary_speaker = {
        {.start = 0.0,
         .end = 0.52,
         .speaker = "speaker_0",
         .speaker_id = "spk_a"},
        {.start = 0.6,
         .end = 1.0,
         .speaker = "speaker_1",
         .speaker_id = "spk_b"},
        {.start = 1.0,
         .end = 1.4,
         .speaker = "speaker_0",
         .speaker_id = "spk_a"},
        {.start = 1.4,
         .end = 1.8,
         .speaker = "speaker_1",
         .speaker_id = "spk_b"},
        {.start = 1.8,
         .end = 2.4,
         .speaker = "speaker_0",
         .speaker_id = "spk_a"}};
    CHECK(alignment_gap_queries(ambiguous_islands).empty(),
          "multiple primary islands inside one alignment gap emit no query");

    auto multiple_phrase_pairs = alignment_gap_snapshot();
    multiple_phrase_pairs.asr[0] =
        {10, 0.0, 5.0, "甲乙丙。乙丙，甲乙丙。乙丙，"};
    multiple_phrase_pairs.align[0] =
        {10,
         0.0,
         5.0,
         {{0.0, 0.2, "甲"},
          {0.2, 0.4, "乙"},
          {1.2, 1.4, "丙"},
          {1.4, 1.5, "。"},
          {1.5, 1.7, "乙"},
          {1.7, 1.9, "丙"},
          {1.9, 2.0, "，"},
          {3.0, 3.2, "甲"},
          {3.2, 3.4, "乙"},
          {4.2, 4.4, "丙"},
          {4.4, 4.5, "。"},
          {4.5, 4.7, "乙"},
          {4.7, 4.9, "丙"},
          {4.9, 5.0, "，"}}};
    multiple_phrase_pairs.primary_speaker.insert(
        multiple_phrase_pairs.primary_speaker.end(),
        {{.start = 3.0,
          .end = 3.52,
          .speaker = "speaker_2",
          .speaker_id = "spk_c"},
         {.start = 3.6,
          .end = 4.0,
          .speaker = "speaker_3",
          .speaker_id = "spk_d"},
         {.start = 4.0,
          .end = 4.6,
          .speaker = "speaker_2",
          .speaker_id = "spk_c"}});
    CHECK(alignment_gap_queries(multiple_phrase_pairs).empty(),
          "multiple matching phrase mappings emit no gap query");

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
