// End-to-end gate for the Qwen3 Forced Aligner: PCM + transcript -> per-word
// timestamps. Uses the exact 6 s PCM and decoded word times dumped by
// tools/reference/aligner_oracle.py. This exercises the whole pipeline,
// including the WhisperMel front-end (the only stage not isolated elsewhere).

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "model/qwen3_forced_aligner.h"

using namespace orator;

static std::vector<float> ReadF32(const std::string& p, bool* ok) {
  std::ifstream f(p, std::ios::binary | std::ios::ate);
  if (!f) {
    *ok = false;
    return {};
  }
  std::streamsize n = f.tellg();
  f.seekg(0);
  std::vector<float> v(n / sizeof(float));
  f.read(reinterpret_cast<char*>(v.data()), n);
  *ok = true;
  return v;
}
static std::vector<int> ReadI32(const std::string& p, bool* ok) {
  std::ifstream f(p, std::ios::binary | std::ios::ate);
  if (!f) {
    *ok = false;
    return {};
  }
  std::streamsize n = f.tellg();
  f.seekg(0);
  std::vector<int> v(n / sizeof(int));
  f.read(reinterpret_cast<char*>(v.data()), n);
  *ok = true;
  return v;
}

int main() {
  std::printf("Testing Qwen3 Forced Aligner end-to-end vs oracle...\n");
  const std::string model = "models/ForcedAligner";
  const std::string dump = "tools/reference/aligner_dump/";
  bool a, b;
  auto audio = ReadF32(dump + "audio.f32", &a);         // 6 s @ 16 kHz mono
  auto word_ms = ReadI32(dump + "word_times.i32", &b);  // [2K] start,end ms
  if (!(a && b) || !std::ifstream(model + "/model.safetensors").good()) {
    std::printf("[skip] need weights + oracle dump (aligner_oracle.py)\n");
    return 0;
  }
  // Must match aligner_oracle.py DEFAULT_TRANSCRIPT / DEFAULT_LANGUAGE.
  const std::string transcript = "比较理想化的一个人吧其实是这样的";
  const std::string language = "Chinese";

  model::Qwen3ForcedAligner aligner;
  aligner.LoadWeights(model);
  auto units = aligner.Align(audio.data(), static_cast<int>(audio.size()),
                             transcript, language);

  std::printf("  words: oracle=%zu ours=%zu\n", word_ms.size() / 2,
              units.size());
  if (units.size() != word_ms.size() / 2) {
    std::printf("FAIL: word count mismatch\n");
    return 1;
  }

  // Compare start/end (ms). Words the reference itself could not place (a
  // degenerate zero-duration [t,t], e.g. a leading char over the intro silence)
  // are a numerical near-tie the bf16/mel ~1e-3 noise can flip, so exclude
  // them; the model forward is proven exact against the oracle in
  // test_aligner_lm. Every word the reference DID place must agree within one
  // 80 ms bucket.
  int worst = 0, off = 0, checked = 0, skipped = 0;
  for (size_t k = 0; k < units.size(); ++k) {
    const int rs = word_ms[2 * k], re = word_ms[2 * k + 1];
    if (re <= rs) {
      ++skipped;
      continue;
    }  // degenerate reference word
    const int os = static_cast<int>(std::lround(units[k].start_sec * 1000.0));
    const int oe = static_cast<int>(std::lround(units[k].end_sec * 1000.0));
    const int ds = std::abs(os - rs), de = std::abs(oe - re);
    worst = std::max(worst, std::max(ds, de));
    ++checked;
    if (ds > 80 || de > 80) {
      if (off < 10)
        std::printf("  word[%zu] ours=[%d,%d] oracle=[%d,%d]\n", k, os, oe, rs,
                    re);
      ++off;
    }
  }
  std::printf(
      "  checked=%d skipped(degenerate)=%d worst diff=%d ms off>80ms=%d\n",
      checked, skipped, worst, off);
  if (off != 0) {
    std::printf("FAIL: %d words exceed tolerance\n", off);
    return 1;
  }
  std::printf("Aligner end-to-end test PASSED\n");
  return 0;
}
