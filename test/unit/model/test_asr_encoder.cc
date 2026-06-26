#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "io/sharded_safetensor.h"
#include "model/asr_audio_tower.h"

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
  std::printf("Testing ASR audio tower vs FP32 PyTorch encoder reference...\n");
  const std::string dir = "models/reference/asr/";
  const std::string model = "models/asr/Qwen/Qwen3-ASR-1.7B";
  bool a, b;
  auto mel = ReadF32(dir + "input_features.f32", &a);  // [1,128,T]
  auto ref = ReadF32(dir + "audio_features_fp32.f32", &b);  // [N,2048]
  if (!(a && b) || !std::ifstream(model + "/model.safetensors.index.json").good()) {
    std::printf("[skip] need weights + fp32 dump (asr_oracle.py --dump --dtype fp32)\n");
    return 0;
  }

  const int n_mels = 128;
  const int n_frames = static_cast<int>(mel.size()) / n_mels;
  const int out_dim = 2048;
  const int ref_tokens = static_cast<int>(ref.size()) / out_dim;
  std::printf("  mel=[%d,%d] ref encoder=[%d,%d]\n", n_mels, n_frames, ref_tokens, out_dim);

  io::ShardedSafeTensors w(model);
  model::AsrAudioTower tower{};
  tower.LoadWeights(w);

  int tokens = 0;
  auto out = tower.Forward(mel.data(), n_frames, &tokens);
  std::printf("  ours encoder=[%d,%d]\n", tokens, out_dim);
  if (tokens != ref_tokens) {
    std::printf("FAIL: token count mismatch (%d vs %d)\n", tokens, ref_tokens);
    return 1;
  }

  double max_abs = 0.0, sum_abs = 0.0;
  const size_t n = out.size();
  for (size_t i = 0; i < n; ++i) {
    const double e = std::abs(static_cast<double>(out[i]) - ref[i]);
    max_abs = e > max_abs ? e : max_abs;
    sum_abs += e;
  }
  std::printf("  max abs err = %.3e, mean abs err = %.3e\n", max_abs, sum_abs / n);
  // bf16 tensor-core GEMM vs fp32 oracle: looser tolerance than a pure-fp32 path,
  // but the encoder output feeds a 0.5-scaled cross-attention so abs error stays small.
  if (max_abs > 0.2) {
    std::printf("FAIL: encoder output exceeds tolerance\n");
    return 1;
  }
  std::printf("ASR audio tower test PASSED\n");
  return 0;
}
