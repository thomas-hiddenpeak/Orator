// P2 gate: the Qwen3 Forced Aligner audio path = Qwen3-ASR audio tower (reused,
// output_dim=1024) + the model's multi_modal_projector. Feed the oracle's mel
// (tools/reference/aligner_oracle.py) and compare the projected audio features
// against the oracle, isolating the tower+projector from the mel front-end.

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
  std::printf("Testing Qwen3 Forced Aligner audio tower vs oracle...\n");
  const std::string model = "models/ForcedAligner";
  const std::string dump = "tools/reference/aligner_dump/";
  bool a, b;
  auto mel = ReadF32(dump + "mel.f32", &a);          // [128, T]
  auto ref = ReadF32(dump + "audio_feats.f32", &b);  // [N, 1024]
  if (!(a && b) || !std::ifstream(model + "/model.safetensors").good()) {
    std::printf("[skip] need weights + oracle dump (aligner_oracle.py)\n");
    return 0;
  }

  const int n_mels = 128;
  const int n_frames = static_cast<int>(mel.size()) / n_mels;
  const int out_dim = 1024;
  const int ref_tokens = static_cast<int>(ref.size()) / out_dim;
  std::printf("  mel=[%d,%d] ref audio_feats=[%d,%d]\n", n_mels, n_frames,
              ref_tokens, out_dim);

  io::ShardedSafeTensors w(model);
  model::AsrAudioConfig cfg;          // defaults already match (d_model=1024 ...)
  cfg.output_dim = 1024;              // aligner projects to 1024, not ASR's 2048
  model::AsrAudioTower tower{cfg};
  model::AsrAudioTower::WeightNames names;
  names.prefix = "model.audio_tower.";
  names.proj1 = "model.multi_modal_projector.linear_1";
  names.proj2 = "model.multi_modal_projector.linear_2";
  tower.LoadWeights(w, names);

  int tokens = 0;
  auto out = tower.Forward(mel.data(), n_frames, &tokens);
  std::printf("  ours audio_feats=[%d,%d]\n", tokens, out_dim);
  if (tokens != ref_tokens) {
    std::printf("FAIL: token count mismatch (%d vs %d)\n", tokens, ref_tokens);
    return 1;
  }

  double max_abs = 0.0, sum_abs = 0.0, max_ref = 0.0;
  const size_t n = out.size();
  for (size_t i = 0; i < n; ++i) {
    const double e = std::abs(static_cast<double>(out[i]) - ref[i]);
    max_abs = e > max_abs ? e : max_abs;
    sum_abs += e;
    const double m = std::abs(static_cast<double>(ref[i]));
    max_ref = m > max_ref ? m : max_ref;
  }
  std::printf("  max abs err = %.3e, mean abs err = %.3e, max|ref| = %.3e, "
              "rel = %.3e\n",
              max_abs, sum_abs / n, max_ref, max_abs / (max_ref + 1e-9));

  // bf16 weights -> fp32 compute. The mean error is the correctness signal
  // (~1e-3); the max is bf16 rounding on the largest-magnitude features
  // (relative ~4e-3). The authoritative end-to-end gate is the timestamp argmax
  // labels (P4), which are robust to this small feature noise.
  if (sum_abs / n > 3e-3 || max_abs > 2.5e-2) {
    std::printf("FAIL: audio features exceed tolerance\n");
    return 1;
  }
  std::printf("Aligner audio tower test PASSED\n");
  return 0;
}
