#include <cstdio>
#include <utility>
#include <vector>

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

void CheckReconstruction(const std::vector<Range>& ranges, int start,
                         int end) {
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

  if (failures == 0) {
    std::printf("SpeakerEvidenceStage test PASSED\n");
    return 0;
  }
  std::printf("SpeakerEvidenceStage test FAILED (%d checks)\n", failures);
  return 1;
}
