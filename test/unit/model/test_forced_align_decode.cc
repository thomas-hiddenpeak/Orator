// Unit test for the forced-alignment CPU decode/tokenize helpers.
//
// Ground-truth values are produced by tools/reference/aligner_oracle.py, which
// calls the real transformers `Qwen3ASRProcessor` functions (torch-free). Keep
// these in sync with tools/reference/aligner_dump/oracle.json.

#include <cstdio>
#include <string>
#include <vector>

#include "model/forced_align_decode.h"

using orator::core::AlignUnit;
using orator::model::AudioTokenLength;
using orator::model::BuildAlignerInputIds;
using orator::model::FixTimestamps;
using orator::model::PairWordTimestamps;
using orator::model::SplitWordsForAlignment;

static int g_fail = 0;

#define CHECK(cond, msg)              \
  do {                                \
    if (!(cond)) {                    \
      std::printf("FAIL: %s\n", msg); \
      ++g_fail;                       \
    }                                 \
  } while (0)

static bool WordsEq(const std::vector<std::string>& a,
                    const std::vector<std::string>& b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i)
    if (a[i] != b[i]) return false;
  return true;
}

static void test_split_words() {
  CHECK(WordsEq(SplitWordsForAlignment("你好，世界123 abc。", "Chinese"),
                {"你", "好", "世", "界", "123", "abc"}),
        "split zh mixed");
  CHECK(WordsEq(SplitWordsForAlignment(
                    "Mr. Quilter's the apostle of the middle classes.", "English"),
                {"Mr", "Quilter's", "the", "apostle", "of", "the", "middle",
                 "classes"}),
        "split en punctuation+apostrophe");
  CHECK(WordsEq(SplitWordsForAlignment("我觉得十五是差不多的。", "Chinese"),
                {"我", "觉", "得", "十", "五", "是", "差", "不", "多", "的"}),
        "split zh pure cjk");
}

static bool LongEq(const std::vector<long>& a, const std::vector<long>& b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i)
    if (a[i] != b[i]) return false;
  return true;
}

static void test_fix_timestamps() {
  CHECK(LongEq(FixTimestamps({0, 80, 160, 240, 320}), {0, 80, 160, 240, 320}),
        "fix already-monotonic");
  CHECK(LongEq(FixTimestamps({0, 80, 60, 240, 320}), {0, 80, 80, 240, 320}),
        "fix single dip (snap)");
  CHECK(LongEq(FixTimestamps({0, 500, 80, 160, 240, 600}),
               {0, 0, 80, 160, 240, 600}),
        "fix outlier spike (snap)");
  CHECK(LongEq(FixTimestamps({100, 90, 80, 200, 210, 50, 300}),
               {100, 100, 200, 200, 210, 210, 300}),
        "fix multiple anomalies");
  CHECK(LongEq(FixTimestamps({0, 0, 80, 80, 160, 160}),
               {0, 0, 80, 80, 160, 160}),
        "fix non-decreasing repeats");
}

static void test_pair() {
  // 3 words, 6 timestamps (start,end pairs) in ms.
  std::vector<std::string> words = {"a", "b", "c"};
  std::vector<long> ms = {0, 80, 160, 320, 400, 560};
  auto units = PairWordTimestamps(words, ms);
  CHECK(units.size() == 3, "pair count");
  CHECK(units[0].start_sec == 0.0 && units[0].end_sec == 0.08, "pair w0");
  CHECK(units[1].start_sec == 0.16 && units[1].end_sec == 0.32, "pair w1");
  CHECK(units[2].start_sec == 0.40 && units[2].end_sec == 0.56, "pair w2");
}

static void test_audio_token_length() {
  // Oracle anchor: 6 s -> 600 mel frames -> 78 audio tokens.
  CHECK(AudioTokenLength(600) == 78, "atl 600");
  CHECK(AudioTokenLength(100) == 13, "atl 100 (one full chunk)");
  CHECK(AudioTokenLength(150) == 20, "atl 150");
  CHECK(AudioTokenLength(250) == 33, "atl 250");
  CHECK(AudioTokenLength(99) == 13, "atl 99 (partial chunk)");
  CHECK(AudioTokenLength(101) == 14, "atl 101");
}

static void test_build_input_ids() {
  // Ground truth from tools/reference/aligner_oracle.py (6 s of test.mp3,
  // transcript "比较理想化的一个人吧其实是这样的", 16 single-token CJK words,
  // N_audio=78, seq=128). We assert the assembly against the oracle's structural
  // facts rather than a 128-int literal.
  const int kAudioStart = 151669, kAudioPad = 151676, kAudioEnd = 151670,
            kTs = 151705;
  const int kN = 78;
  const std::vector<int> wt = {56006, 99260, 21887, 99172, 32108,  9370,
                               14777, 18947, 17340, 100003, 41146, 39973,
                               20412, 43288, 90885, 9370};
  std::vector<std::vector<int>> words;
  for (int t : wt) words.push_back({t});

  auto ids = BuildAlignerInputIds(words, kN, kAudioStart, kAudioPad, kAudioEnd,
                                  kTs);
  CHECK(ids.size() == 128, "input_ids length 128 (oracle)");
  CHECK(!ids.empty() && ids.front() == kAudioStart, "first = audio_start");
  int pads = 0;
  for (int i = 1; i <= kN && i < static_cast<int>(ids.size()); ++i)
    pads += (ids[i] == kAudioPad);
  CHECK(pads == kN, "78 audio_pad tokens");
  CHECK(ids.size() > 79 && ids[1 + kN] == kAudioEnd, "audio_end after pads");
  bool interleave_ok = true;
  for (int k = 0; k < 16; ++k) {
    const int base = 1 + kN + 1 + 3 * k;  // word k: token, ts, ts
    if (ids[base] != wt[k] || ids[base + 1] != kTs || ids[base + 2] != kTs)
      interleave_ok = false;
  }
  CHECK(interleave_ok, "words interleaved with <timestamp><timestamp>");
  int ts_count = 0;
  for (int x : ids) ts_count += (x == kTs);
  CHECK(ts_count == 32, "32 timestamp tokens (16 words x 2)");
}

int main() {
  test_split_words();
  test_fix_timestamps();
  test_pair();
  test_audio_token_length();
  test_build_input_ids();
  if (g_fail == 0) std::printf("forced_align_decode test PASSED\n");
  return g_fail == 0 ? 0 : 1;
}
