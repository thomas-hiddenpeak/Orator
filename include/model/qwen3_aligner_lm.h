#pragma once

// Qwen3 Forced Aligner language model + token-classification head.
//
// Non-autoregressive: a single causal forward over [audio | text] returns the
// per-position hidden states, and the `score` head classifies each <timestamp>
// position into one of num_labels time buckets. Unlike the ASR decoder this
// path has no KV cache and no generation loop -- one prefill, all positions.
//
// It reuses the verified Qwen3 building blocks (asr_ops RmsNorm / RopeHalf /
// GqaAttention / SwiGLU, asr_gemm Linear) but keeps its own orchestration so
// the autoregressive ASR decoder is untouched. Weights load from the aligner
// checkpoint under model.language_model.* and score.weight.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <cuda_runtime.h>

#include "gpu/device_scratch.h"
#include "gpu/memory.h"
#include "io/sharded_safetensor.h"

namespace orator {
namespace model {

struct AlignerLmConfig {
  int hidden_size = 1024;
  int num_layers = 28;
  int num_q_heads = 16;
  int num_kv_heads = 8;
  int head_dim = 128;
  int intermediate_size = 3072;
  int vocab_size = 152064;
  int num_labels = 5000;
  float rms_norm_eps = 1e-6f;
  float rope_theta = 1000000.0f;
};

class AlignerLm {
 public:
  explicit AlignerLm(const AlignerLmConfig& config = {});

  // Loads embed_tokens, the 28 decoder layers, the final norm and the score
  // head from `model.language_model.*` and `score.weight`.
  void LoadWeights(const io::ShardedSafeTensors& weights);

  // Single causal forward + score head.
  //   input_ids: [T] token ids (audio placeholders included).
  //   audio_feats: [n_audio, hidden] projected audio features (host), injected
  //     in order at the positions where input_ids == audio_pad_id.
  // Returns the classification logits [T, num_labels] (host, FP32). When
  // `hidden_out` is non-null it also receives the [T, hidden] final hidden
  // states (for stage validation).
  std::vector<float> Forward(const std::vector<int>& input_ids,
                             const float* audio_feats, int n_audio,
                             int audio_pad_id,
                             std::vector<float>* hidden_out = nullptr) const;

  const AlignerLmConfig& config() const { return config_; }

 private:
  struct F32Buf {
    std::shared_ptr<gpu::UnifiedBuffer> buf;
    float* p = nullptr;
  };
  struct BfBuf {
    std::shared_ptr<gpu::UnifiedBuffer> buf;
    uint16_t* p = nullptr;
  };
  F32Buf LoadF32(const io::ShardedSafeTensors& w, const std::string& name);
  BfBuf LoadBf16(const io::ShardedSafeTensors& w, const std::string& name);

  struct Layer {
    F32Buf in_ln, q_norm, k_norm, post_ln;
    BfBuf q_w, k_w, v_w, o_w, gate_w, up_w, down_w;
  };

  AlignerLmConfig config_;
  BfBuf embed_;                       // [vocab, hidden] bf16
  std::vector<uint16_t> embed_host_;  // host shadow for embedding lookup
  F32Buf final_norm_;                 // [hidden]
  BfBuf score_;                       // [num_labels, hidden] bf16
  std::vector<Layer> layers_;
  // Per-instance device scratch for Forward's working buffers (one aligner
  // worker -> single-thread-of-control per instance).
  mutable gpu::DeviceScratch scratch_;
};

}  // namespace model
}  // namespace orator
