// P4 gate: the Qwen3 Forced Aligner language model (28 causal layers, single
// non-autoregressive forward) + score head. Feed the oracle's input_ids and
// projected audio features and compare (a) the final hidden states and (b) the
// argmax timestamp labels against the PyTorch reference
// (tools/reference/aligner_oracle.py). The labels are the authoritative gate.

#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "io/sharded_safetensor.h"
#include "model/qwen3_aligner_lm.h"

using namespace orator;

static std::vector<float> ReadF32(const std::string& p, bool* ok) {
  std::ifstream f(p, std::ios::binary | std::ios::ate);
  if (!f) { *ok = false; return {}; }
  std::streamsize n = f.tellg();
  f.seekg(0);
  std::vector<float> v(n / sizeof(float));
  f.read(reinterpret_cast<char*>(v.data()), n);
  *ok = true;
  return v;
}
static std::vector<int> ReadI32(const std::string& p, bool* ok) {
  std::ifstream f(p, std::ios::binary | std::ios::ate);
  if (!f) { *ok = false; return {}; }
  std::streamsize n = f.tellg();
  f.seekg(0);
  std::vector<int> v(n / sizeof(int));
  f.read(reinterpret_cast<char*>(v.data()), n);
  *ok = true;
  return v;
}

int main() {
  std::printf("Testing Qwen3 Forced Aligner LM + score vs oracle...\n");
  const std::string model = "models/ForcedAligner";
  const std::string dump = "tools/reference/aligner_dump/";
  bool a, b, c, d;
  auto input_ids = ReadI32(dump + "input_ids.i32", &a);
  auto ref_labels = ReadI32(dump + "ts_labels.i32", &b);
  auto audio_feats = ReadF32(dump + "audio_feats.f32", &c);  // [78,1024]
  auto ref_hidden = ReadF32(dump + "lm_hidden.f32", &d);     // [128,1024]
  if (!(a && b && c && d) || !std::ifstream(model + "/model.safetensors").good()) {
    std::printf("[skip] need weights + oracle dump (aligner_oracle.py)\n");
    return 0;
  }

  const int kAudioPad = 151676, kTs = 151705, Hh = 1024, NL = 5000;
  const int n_audio = static_cast<int>(audio_feats.size()) / Hh;
  const int T = static_cast<int>(input_ids.size());
  std::printf("  T=%d n_audio=%d ts_labels=%zu\n", T, n_audio, ref_labels.size());

  io::ShardedSafeTensors w(model);
  model::AlignerLm lm{};
  lm.LoadWeights(w);

  std::vector<float> hidden;
  auto logits =
      lm.Forward(input_ids, audio_feats.data(), n_audio, kAudioPad, &hidden);

  // (a) hidden-state agreement (bf16-limited; informational + sanity).
  double max_abs = 0, sum_abs = 0, max_ref = 0;
  for (size_t i = 0; i < hidden.size(); ++i) {
    const double e = std::abs(static_cast<double>(hidden[i]) - ref_hidden[i]);
    max_abs = e > max_abs ? e : max_abs;
    sum_abs += e;
    const double m = std::abs(static_cast<double>(ref_hidden[i]));
    max_ref = m > max_ref ? m : max_ref;
  }
  std::printf("  lm_hidden: max abs=%.3e mean abs=%.3e rel=%.3e\n", max_abs,
              sum_abs / hidden.size(), max_abs / (max_ref + 1e-9));

  // (b) AUTHORITATIVE gate: argmax timestamp labels must match the oracle.
  int li = 0, mism = 0;
  for (int t = 0; t < T; ++t) {
    if (input_ids[t] != kTs) continue;
    const float* row = logits.data() + static_cast<size_t>(t) * NL;
    int best = 0;
    float bestv = row[0];
    for (int j = 1; j < NL; ++j)
      if (row[j] > bestv) { bestv = row[j]; best = j; }
    if (li < static_cast<int>(ref_labels.size()) && best != ref_labels[li]) {
      if (mism < 8)
        std::printf("  label[%d] mismatch: ours=%d oracle=%d\n", li, best,
                    ref_labels[li]);
      ++mism;
    }
    ++li;
  }
  std::printf("  timestamp positions=%d, label mismatches=%d\n", li, mism);

  if (li != static_cast<int>(ref_labels.size())) {
    std::printf("FAIL: timestamp position count mismatch\n");
    return 1;
  }
  if (mism != 0) {
    std::printf("FAIL: %d argmax label mismatches vs oracle\n", mism);
    return 1;
  }
  std::printf("Aligner LM test PASSED\n");
  return 0;
}
