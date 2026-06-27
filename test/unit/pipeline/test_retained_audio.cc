// Unit test for RetainedAudioBuffer: bounded sliding-window audio addressed by
// absolute sample index, with on-demand span reads for forced alignment.

#include <cstdio>
#include <vector>

#include "pipeline/retained_audio_buffer.h"

using orator::pipeline::RetainedAudioBuffer;

static int g_fail = 0;

#define CHECK(cond, msg)              \
  do {                                \
    if (!(cond)) {                    \
      std::printf("FAIL: %s\n", msg); \
      ++g_fail;                       \
    }                                 \
  } while (0)

static void test_basic_span() {
  RetainedAudioBuffer buf(/*sample_rate=*/100,
                          /*retain_sec=*/10.0);  // 1000 smp
  std::vector<float> a(500);
  for (int i = 0; i < 500; ++i) a[i] = static_cast<float>(i);
  buf.Append(a.data(), 500);
  CHECK(buf.total_samples() == 500, "total after append");
  CHECK(buf.base_sample() == 0, "base unchanged within window");

  auto span = buf.ReadSpan(100, 200);
  CHECK(span.size() == 100, "span length");
  CHECK(span.front() == 100.0f && span.back() == 199.0f, "span values");
}

static void test_window_trim() {
  RetainedAudioBuffer buf(/*sample_rate=*/100, /*retain_sec=*/1.0);  // 100 smp
  std::vector<float> a(250);
  for (int i = 0; i < 250; ++i) a[i] = static_cast<float>(i);
  buf.Append(a.data(), 250);
  CHECK(buf.total_samples() == 250, "total after big append");
  CHECK(buf.base_sample() == 150, "base advanced to keep last 100");

  // Trimmed region is unavailable.
  CHECK(buf.ReadSpan(0, 50).empty(), "trimmed span unavailable");
  // Tail span available.
  auto span = buf.ReadSpan(200, 250);
  CHECK(span.size() == 50, "tail span length");
  CHECK(span.front() == 200.0f && span.back() == 249.0f, "tail span values");
}

static void test_out_of_range() {
  RetainedAudioBuffer buf(100, 10.0);
  std::vector<float> a(100, 1.0f);
  buf.Append(a.data(), 100);
  CHECK(buf.ReadSpan(50, 200).empty(), "span past head unavailable");
  CHECK(buf.ReadSpan(80, 80).empty(), "empty span");
  CHECK(buf.ReadSpan(90, 80).empty(), "inverted span");
}

static void test_incremental_append() {
  RetainedAudioBuffer buf(100, 10.0);
  for (int blk = 0; blk < 5; ++blk) {
    std::vector<float> a(100);
    for (int i = 0; i < 100; ++i) a[i] = static_cast<float>(blk * 100 + i);
    buf.Append(a.data(), 100);
  }
  CHECK(buf.total_samples() == 500, "total after incremental");
  auto span = buf.ReadSpan(250, 350);
  CHECK(span.size() == 100, "cross-block span length");
  CHECK(span.front() == 250.0f && span.back() == 349.0f, "cross-block values");
}

int main() {
  test_basic_span();
  test_window_trim();
  test_out_of_range();
  test_incremental_append();
  if (g_fail == 0) {
    std::printf("test_retained_audio: ALL PASS\n");
    return 0;
  }
  std::printf("test_retained_audio: %d FAIL\n", g_fail);
  return 1;
}
