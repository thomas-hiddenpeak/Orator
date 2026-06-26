// ASR preprocessor unit tests — validate AsrPreprocessor against known inputs.
// CPU-only test (no GPU required).

#include "pipeline/asr_preprocessor.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

using orator::pipeline::AsrPreprocessor;

// ---------------------------------------------------------------------------
// Test helpers
// ---------------------------------------------------------------------------
static int g_failures = 0;
static int g_tests = 0;

#define TEST(name) do { ++g_tests; printf("  RUN  %s\n", name); } while (0)
#define ASSERT_NEAR(a, b, tol) do {                                           \
  float va_ = (a), vb_ = (b);                                                 \
  if (std::abs(va_ - vb_) > tol) {                                           \
    printf("  FAIL %s:%d: expected %.6f ≈ %.6f (tol=%.2e)\n",                \
           __FILE__, __LINE__, double(va_), double(vb_), double(tol));        \
    ++g_failures;                                                             \
  }                                                                           \
} while (0)
#define ASSERT_TRUE(cond) do {                                                \
  if (!(cond)) {                                                              \
    printf("  FAIL %s:%d: expected true\n", __FILE__, __LINE__);              \
    ++g_failures;                                                             \
  }                                                                           \
} while (0)

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static void TestModeNonePassthrough() {
  TEST("TestModeNonePassthrough: mode=none returns identical samples");
  AsrPreprocessor preproc({.mode = "none"});

  const int N = 160;  // 10 ms @ 16 kHz
  std::vector<float> in(N);
  for (int i = 0; i < N; ++i)
    in[i] = std::sin(2.0f * 3.14159f * 440.0f * static_cast<float>(i) / 16000.0f);

  std::vector<float> out;
  preproc.Process(in.data(), N, &out);

  ASSERT_TRUE(static_cast<int>(out.size()) == N);
  for (int i = 0; i < N; ++i)
    ASSERT_NEAR(out[i], in[i], 1e-6f);
}

static void TestModeNoneEmptyInput() {
  TEST("TestModeNoneEmptyInput: empty input produces empty output");
  AsrPreprocessor preproc({.mode = "none"});

  std::vector<float> out;
  preproc.Process(nullptr, 0, &out);
  ASSERT_TRUE(out.empty());

  // Null samples
  preproc.Process(nullptr, 100, &out);
  ASSERT_TRUE(out.empty());

  // Zero length
  std::vector<float> in = {1.0f, 2.0f};
  preproc.Process(in.data(), 0, &out);
  ASSERT_TRUE(out.empty());
}

static void TestModeNoneNullOutput() {
  TEST("TestModeNoneNullOutput: null output pointer should not crash");
  AsrPreprocessor preproc({.mode = "none"});
  std::vector<float> in = {1.0f, 2.0f, 3.0f};
  preproc.Process(in.data(), 3, nullptr);
  // No crash = pass
}

static void TestClassicalOutputShape() {
  TEST("TestClassicalOutputShape: mode=classical preserves length");
  AsrPreprocessor preproc({.mode = "classical"});

  const int N = 1600;  // 100 ms @ 16 kHz
  std::vector<float> in(N);
  for (int i = 0; i < N; ++i)
    in[i] = std::sin(2.0f * 3.14159f * 440.0f * static_cast<float>(i) / 16000.0f);

  std::vector<float> out;
  preproc.Process(in.data(), N, &out);

  ASSERT_TRUE(static_cast<int>(out.size()) == N);
  // Output should be different from input (denoising applied)
  bool differs = false;
  for (int i = 0; i < N; ++i) {
    if (std::abs(out[i] - in[i]) > 1e-6f) {
      differs = true;
      break;
    }
  }
  ASSERT_TRUE(differs);
}

static void TestClassicalSilence() {
  TEST("TestClassicalSilence: mode=classical with silence stays silent");
  AsrPreprocessor preproc({.mode = "classical"});

  const int N = 1600;
  std::vector<float> in(N, 0.0f);

  std::vector<float> out;
  preproc.Process(in.data(), N, &out);

  ASSERT_TRUE(static_cast<int>(out.size()) == N);
  for (int i = 0; i < N; ++i)
    ASSERT_NEAR(out[i], 0.0f, 1e-6f);
}

static void TestClassicalSingleSample() {
  TEST("TestClassicalSingleSample: single sample does not crash");
  AsrPreprocessor preproc({.mode = "classical"});

  std::vector<float> in = {0.5f};
  std::vector<float> out;
  preproc.Process(in.data(), 1, &out);

  ASSERT_TRUE(static_cast<int>(out.size()) == 1);
  // Single sample: n <= 1 returns early after copy
  ASSERT_NEAR(out[0], in[0], 1e-6f);
}

static void TestClassicalPeakPreservation() {
  TEST("TestClassicalPeakPreservation: output peak does not exceed 2x input peak");
  AsrPreprocessor preproc({.mode = "classical"});

  const int N = 1600;
  std::vector<float> in(N);
  float in_peak = 0.0f;
  for (int i = 0; i < N; ++i) {
    in[i] = std::sin(2.0f * 3.14159f * 440.0f * static_cast<float>(i) / 16000.0f);
    in_peak = std::max(in_peak, std::abs(in[i]));
  }

  std::vector<float> out;
  preproc.Process(in.data(), N, &out);

  float out_peak = 0.0f;
  for (int i = 0; i < N; ++i)
    out_peak = std::max(out_peak, std::abs(out[i]));

  // Peak preservation caps gain at 2x
  ASSERT_TRUE(out_peak <= 2.0f * in_peak + 1e-4f);
}

static void TestUnknownMode() {
  TEST("TestUnknownMode: unknown mode falls back to none (passthrough)");
  AsrPreprocessor preproc({.mode = "bogus_mode"});

  const int N = 100;
  std::vector<float> in(N);
  for (int i = 0; i < N; ++i) in[i] = static_cast<float>(i) * 0.01f;

  std::vector<float> out;
  preproc.Process(in.data(), N, &out);

  ASSERT_TRUE(static_cast<int>(out.size()) == N);
  for (int i = 0; i < N; ++i)
    ASSERT_NEAR(out[i], in[i], 1e-6f);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main() {
  printf("[test_preprocessor] ASR preprocessor unit tests\n");

  TestModeNonePassthrough();
  TestModeNoneEmptyInput();
  TestModeNoneNullOutput();
  TestClassicalOutputShape();
  TestClassicalSilence();
  TestClassicalSingleSample();
  TestClassicalPeakPreservation();
  TestUnknownMode();

  printf("\n");
  printf("Results: %d / %d passed", g_tests - g_failures, g_tests);
  if (g_failures > 0) printf(", %d FAILED", g_failures);
  printf("\n");

  return g_failures > 0 ? 1 : 0;
}
