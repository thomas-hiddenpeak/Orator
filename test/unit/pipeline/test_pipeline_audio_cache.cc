// Verifies that a private pipeline audio cache preserves the canonical session
// time base supplied by the ingest owner while maintaining its own read cursor.

#include <cstdio>
#include <vector>

#include "core/time_base.h"
#include "pipeline/pipeline_audio_cache.h"

using orator::core::TimeBase;
using orator::pipeline::PipelineAudioCache;

int main() {
  int failures = 0;
#define CHECK(condition, message)         \
  do {                                    \
    if (!(condition)) {                   \
      std::printf("FAIL: %s\n", message); \
      ++failures;                         \
    }                                     \
  } while (0)

  const TimeBase session_time_base(/*sample_rate=*/100, /*origin=*/17);
  PipelineAudioCache cache(session_time_base);
  CHECK(cache.time_base().sample_rate() == 100,
        "cache preserves canonical sample rate");
  CHECK(cache.time_base().origin_sample() == 17,
        "cache preserves canonical origin");

  const std::vector<float> input = {1.0f, 2.0f, 3.0f};
  cache.Append(input.data(), static_cast<int>(input.size()));
  cache.Close();
  std::vector<float> output;
  long start_sample = -1;
  CHECK(cache.WaitAndRead(&output, &start_sample),
        "cache returns appended audio");
  CHECK(start_sample == 0, "first cache span starts at absolute sample zero");
  CHECK(output == input, "cache preserves audio samples");
  CHECK(!cache.WaitAndRead(&output, &start_sample),
        "closed cache reports fully drained state");

  if (failures == 0) {
    std::printf("test_pipeline_audio_cache PASSED\n");
    return 0;
  }
  std::printf("test_pipeline_audio_cache FAILED (%d checks)\n", failures);
  return 1;
}
