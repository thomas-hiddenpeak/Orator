// Validates the reused C++ BpeTokenizer against the Qwen3 Forced Aligner's
// tokenizer (vocab.json + merges.txt extracted from tokenizer.json by
// tools/convert/aligner_extract_tokenizer.py). Expected ids are from the HF
// tokenizer via tools/reference/aligner_oracle.py. This is the P1 gate: the
// model assembles input_ids by encoding each alignment word, so per-word
// encoding must match the reference exactly.

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "io/bpe_tokenizer.h"

using namespace orator;

int main() {
  const std::string model = "models/ForcedAligner";
  if (!std::ifstream(model + "/vocab.json").good()) {
    std::printf("[skip] aligner tokenizer files not found\n");
    return 0;
  }
  io::BpeTokenizer tk;
  if (!tk.Load(model)) {
    std::printf("FAIL: load\n");
    return 1;
  }

  int fails = 0;
  auto chk = [&](const std::string& s, const std::vector<int>& exp) {
    auto got = tk.Encode(s);
    if (got != exp) {
      std::printf("MISMATCH encode(\"%s\") ->", s.c_str());
      for (int t : got) std::printf(" %d", t);
      std::printf(" | expected");
      for (int t : exp) std::printf(" %d", t);
      std::printf("\n");
      ++fails;
    }
  };

  // Ground truth: tools/reference/aligner_dump/oracle.json word_cases + the
  // forward sample's per-word tokens.
  chk("你", {56568});
  chk("好", {52801});
  chk("世", {99244});
  chk("界", {97120});
  chk("123", {16, 17, 18});
  chk("abc", {13683});
  chk("Mr", {12275});
  chk("Quilter's", {2183, 2044, 594});
  chk("apostle", {391, 535, 273});
  chk("middle", {19656});
  chk("classes", {8855});
  chk("我", {35946});
  chk("的", {9370});
  chk("比", {56006});
  chk("较", {99260});

  if (fails) {
    std::printf("FAIL: %d mismatches\n", fails);
    return 1;
  }
  std::printf("aligner tokenizer test PASSED\n");
  return 0;
}
