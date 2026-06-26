#include <cstdio>
#include <fstream>
#include <string>

#include "io/sharded_safetensor.h"

using namespace orator;

static bool Exists(const std::string& p) { return std::ifstream(p).good(); }

int main() {
  std::printf("Testing multi-shard SafeTensors loader...\n");

  const char* kCandidates[] = {
      "models/asr/Qwen/Qwen3-ASR-1.7B",
      "../models/asr/Qwen/Qwen3-ASR-1.7B",
      "../../models/asr/Qwen/Qwen3-ASR-1.7B"};
  std::string dir;
  for (const auto& c : kCandidates)
    if (Exists(std::string(c) + "/model.safetensors.index.json")) {
      dir = c;
      break;
    }
  if (dir.empty()) {
    std::printf("[skip] Qwen3-ASR weights not found; loader test is a no-op\n");
    return 0;
  }

  io::ShardedSafeTensors w(dir);
  std::printf("  shards=%zu tensors=%zu\n", w.NumShards(), w.NumTensors());

  // Qwen3-ASR-1.7B has 708 tensors across 2 shards.
  if (w.NumShards() != 2) { std::printf("FAIL: expected 2 shards\n"); return 1; }
  if (w.NumTensors() != 708) {
    std::printf("FAIL: expected 708 tensors, got %zu\n", w.NumTensors());
    return 1;
  }

  // Spot-check representative tensors: name -> {shape, dtype}.
  struct Check {
    const char* name;
    std::vector<int64_t> shape;
  };
  const Check checks[] = {
      {"thinker.audio_tower.conv2d1.weight", {480, 1, 3, 3}},
      {"thinker.audio_tower.conv2d1.bias", {480}},
      {"thinker.model.embed_tokens.weight", {151936, 2048}},
      {"thinker.model.layers.0.self_attn.q_proj.weight", {2048, 2048}},
      {"thinker.model.layers.0.self_attn.k_proj.weight", {1024, 2048}},
      {"thinker.model.layers.0.self_attn.q_norm.weight", {128}},
      {"thinker.model.layers.27.mlp.gate_proj.weight", {6144, 2048}},
      {"thinker.audio_tower.layers.23.self_attn.q_proj.weight", {1024, 1024}},
      {"thinker.model.norm.weight", {2048}},
  };
  for (const auto& c : checks) {
    if (!w.Has(c.name)) {
      std::printf("FAIL: missing %s\n", c.name);
      return 1;
    }
    core::Tensor t = w.GetTensorView(c.name);
    if (t.dtype() != core::DType::BF16) {
      std::printf("FAIL: %s dtype != BF16\n", c.name);
      return 1;
    }
    if (t.shape() != c.shape) {
      std::printf("FAIL: %s shape mismatch (got [", c.name);
      for (auto d : t.shape()) std::printf("%lld ", (long long)d);
      std::printf("])\n");
      return 1;
    }
    if (t.data() == nullptr) {
      std::printf("FAIL: %s null data view\n", c.name);
      return 1;
    }
  }
  std::printf("  spot-checked %zu tensors: shapes + BF16 dtype + non-null views OK\n",
              sizeof(checks) / sizeof(checks[0]));

  std::printf("Multi-shard loader test PASSED\n");
  return 0;
}
