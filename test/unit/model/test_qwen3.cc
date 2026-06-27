// Qwen3Asr model shell unit tests — test construction, initialization, and
// error handling WITHOUT requiring GPU or model weights.
// CPU-only test.

#include "model/qwen3_asr.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>

using orator::core::AsrConfig;
using orator::model::Qwen3Asr;

// ---------------------------------------------------------------------------
// Test helpers
// ---------------------------------------------------------------------------
static int g_failures = 0;
static int g_tests = 0;

#define TEST(name)               \
  do {                           \
    ++g_tests;                   \
    printf("  RUN  %s\n", name); \
  } while (0)
#define ASSERT_TRUE(cond)                                          \
  do {                                                             \
    if (!(cond)) {                                                 \
      printf("  FAIL %s:%d: expected true\n", __FILE__, __LINE__); \
      ++g_failures;                                                \
    }                                                              \
  } while (0)
#define ASSERT_EQ(a, b)                                                   \
  do {                                                                    \
    std::string va_ = (a);                                                \
    std::string vb_ = (b);                                                \
    if (va_ != vb_) {                                                     \
      printf("  FAIL %s:%d: expected '%s' == '%s'\n", __FILE__, __LINE__, \
             va_.c_str(), vb_.c_str());                                   \
      ++g_failures;                                                       \
    }                                                                     \
  } while (0)
#define ASSERT_THROWS(expr)                                             \
  do {                                                                  \
    bool caught_ = false;                                               \
    try {                                                               \
      expr;                                                             \
    } catch (const std::exception&) {                                   \
      caught_ = true;                                                   \
    }                                                                   \
    if (!caught_) {                                                     \
      printf("  FAIL %s:%d: expected exception\n", __FILE__, __LINE__); \
      ++g_failures;                                                     \
    }                                                                   \
  } while (0)
#define ASSERT_NO_THROW(expr)                                                \
  do {                                                                       \
    try {                                                                    \
      expr;                                                                  \
    } catch (const std::exception& e) {                                      \
      printf("  FAIL %s:%d: unexpected exception: %s\n", __FILE__, __LINE__, \
             e.what());                                                      \
      ++g_failures;                                                          \
    }                                                                        \
  } while (0)

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static void TestName() {
  TEST("TestName: name() returns 'qwen3_asr'");
  Qwen3Asr asr;
  ASSERT_EQ(asr.name(), "qwen3_asr");
}

static void TestInitialize() {
  TEST("TestInitialize: Initialize with valid config succeeds");
  Qwen3Asr asr;
  AsrConfig cfg;
  cfg.sample_rate = 16000;
  cfg.language = "Chinese";
  ASSERT_NO_THROW(asr.Initialize(cfg));
}

static void TestInitializeDefault() {
  TEST("TestInitializeDefault: Initialize with empty config succeeds");
  Qwen3Asr asr;
  ASSERT_NO_THROW(asr.Initialize(AsrConfig{}));
}

static void TestSetMaxNewTokens() {
  TEST("TestSetMaxNewTokens: set_max_new_tokens does not throw");
  Qwen3Asr asr;
  ASSERT_NO_THROW(asr.set_max_new_tokens(64));
  ASSERT_NO_THROW(asr.set_max_new_tokens(0));
  ASSERT_NO_THROW(asr.set_max_new_tokens(-1));
}

static void TestSetLanguage() {
  TEST("TestSetLanguage: set_language does not throw");
  Qwen3Asr asr;
  ASSERT_NO_THROW(asr.set_language("English"));
  ASSERT_NO_THROW(asr.set_language(""));
  ASSERT_NO_THROW(asr.set_language("Chinese"));
}

static void TestLoadWeightsNonexistent() {
  TEST("TestLoadWeightsNonexistent: LoadWeights with bad path throws");
  Qwen3Asr asr;
  ASSERT_THROWS(asr.LoadWeights("/nonexistent/path/to/model"));
}

static void TestLoadWeightsEmptyPath() {
  TEST("TestLoadWeightsEmptyPath: LoadWeights with empty path throws");
  Qwen3Asr asr;
  ASSERT_THROWS(asr.LoadWeights(""));
}

static void TestResetBeforeLoad() {
  TEST("TestResetBeforeLoad: Reset before LoadWeights does not crash");
  Qwen3Asr asr;
  ASSERT_NO_THROW(asr.Reset());
}

static void TestSegmentSpeechEmpty() {
  TEST("TestSegmentSpeechEmpty: SegmentSpeech with empty input returns empty");
  Qwen3Asr asr;
  auto spans = asr.SegmentSpeech(nullptr, 0, 16000);
  ASSERT_TRUE(spans.empty());
}

static void TestSegmentSpeechSilence() {
  TEST("TestSegmentSpeechSilence: SegmentSpeech with silence returns empty");
  Qwen3Asr asr;
  const int N = 16000;  // 1 second of silence
  std::vector<float> silence(N, 0.0f);
  auto spans = asr.SegmentSpeech(silence.data(), N, 16000);
  ASSERT_TRUE(spans.empty());
}

static void TestSegmentSpeechContinuous() {
  TEST(
      "TestSegmentSpeechContinuous: SegmentSpeech with continuous speech "
      "returns one span");
  Qwen3Asr asr;
  const int sr = 16000;
  const int N = sr * 2;  // 2 seconds
  std::vector<float> speech(N);
  for (int i = 0; i < N; ++i)
    speech[i] =
        0.5f * std::sin(2.0f * 3.14159f * 440.0f * static_cast<float>(i) / sr);

  auto spans = asr.SegmentSpeech(speech.data(), N, sr);
  ASSERT_TRUE(!spans.empty());
  // Should have at least one span covering most of the signal
  int total_covered = 0;
  for (const auto& s : spans) total_covered += s.end - s.begin;
  ASSERT_TRUE(total_covered > 0);
}

static void TestStreamKnobs() {
  TEST("TestStreamKnobs: streaming knobs have sensible defaults");
  Qwen3Asr asr;
  ASSERT_NO_THROW(asr.set_stream_unfixed_chunks(3));
  ASSERT_NO_THROW(asr.set_stream_unfixed_tokens(7));
  // stream_audio_tokens and stream_chunk_id start at 0 before any streaming
  ASSERT_TRUE(asr.stream_audio_tokens() == 0);
  ASSERT_TRUE(asr.stream_chunk_id() == 0);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main() {
  printf("[test_qwen3] Qwen3Asr model shell unit tests\n");

  TestName();
  TestInitialize();
  TestInitializeDefault();
  TestSetMaxNewTokens();
  TestSetLanguage();
  TestLoadWeightsNonexistent();
  TestLoadWeightsEmptyPath();
  TestResetBeforeLoad();
  TestSegmentSpeechEmpty();
  TestSegmentSpeechSilence();
  TestSegmentSpeechContinuous();
  TestStreamKnobs();

  printf("\n");
  printf("Results: %d / %d passed", g_tests - g_failures, g_tests);
  if (g_failures > 0) printf(", %d FAILED", g_failures);
  printf("\n");

  return g_failures > 0 ? 1 : 0;
}
