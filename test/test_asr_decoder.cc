#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "io/sharded_safetensor.h"
#include "model/asr_text_decoder.h"

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

int main() {
  std::printf("Testing ASR text decoder prefill vs FP32 PyTorch logits...\n");
  const std::string dir = "models/reference/asr/";
  const std::string model = "models/asr/Qwen/Qwen3-ASR-1.7B";
  bool a, b;
  auto embeds = ReadF32(dir + "text_embeds_fp32.f32", &a);  // [T, 2048]
  auto ref = ReadF32(dir + "prefill_last_logits_fp32.f32", &b);  // [vocab]
  if (!(a && b) || !std::ifstream(model + "/model.safetensors.index.json").good()) {
    std::printf("[skip] need weights + fp32 dump (asr_oracle.py --dump --dtype fp32)\n");
    return 0;
  }

  const int hidden = 2048;
  const int T = static_cast<int>(embeds.size()) / hidden;
  const int vocab = static_cast<int>(ref.size());
  std::printf("  prompt T=%d hidden=%d vocab=%d\n", T, hidden, vocab);

  io::ShardedSafeTensors w(model);
  model::AsrTextDecoder dec{};
  dec.LoadWeights(w);
  dec.ResetCache();
  dec.Prefill(embeds.data(), T);
  auto logits = dec.CopyLogits();

  // Argmax must match; logits should be numerically close (bf16 GEMM path).
  int our_am = dec.Argmax();
  int ref_am = 0;
  for (int i = 1; i < vocab; ++i) {
    if (ref[i] > ref[ref_am]) ref_am = i;
  }
  double max_abs = 0.0;
  for (int i = 0; i < vocab; ++i)
    max_abs = std::max(max_abs, std::abs((double)logits[i] - ref[i]));
  std::printf("  argmax ours=%d ref=%d ; logits max abs err = %.3e\n",
              our_am, ref_am, max_abs);
  if (our_am != ref_am) {
    std::printf("FAIL: argmax mismatch\n");
    return 1;
  }
  // bf16 tensor-core GEMM vs fp32 oracle: looser tolerance than the old fp32 path.
  if (max_abs > 2.0) {
    std::printf("FAIL: logits exceed tolerance\n");
    return 1;
  }
  std::printf("ASR text decoder test PASSED\n");
  return 0;
}
