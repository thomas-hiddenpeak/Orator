#include "model/qwen3_aligner_lm.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

#include "core/tensor.h"
#include "model/asr_gemm.h"
#include "model/asr_ops.cuh"

namespace orator {
namespace model {

using gpu::DeviceBuffer;
using gpu::UnifiedBuffer;
using gpu::CheckCudaError;

namespace {
constexpr int kThreads = 256;
inline int Blocks(long n) { return static_cast<int>((n + kThreads - 1) / kThreads); }

__global__ void AddInPlaceKernel(float* x, const float* y, long n) {
  const long i = static_cast<long>(blockIdx.x) * blockDim.x + threadIdx.x;
  if (i < n) x[i] += y[i];
}

inline float Bf16ToF32(uint16_t v) {
  return __builtin_bit_cast(float, static_cast<uint32_t>(v) << 16);
}
}  // namespace

AlignerLm::AlignerLm(const AlignerLmConfig& config) : config_(config) {}

AlignerLm::F32Buf AlignerLm::LoadF32(const io::ShardedSafeTensors& w,
                                     const std::string& name) {
  core::Tensor t = w.GetTensorView(name);
  const int64_t n = t.numel();
  F32Buf out;
  out.buf = std::make_shared<UnifiedBuffer>(sizeof(float) * n);
  out.p = static_cast<float*>(out.buf->data());
  if (t.dtype() == core::DType::BF16) {
    const uint16_t* src = static_cast<const uint16_t*>(t.data());
    for (int64_t i = 0; i < n; ++i) out.p[i] = Bf16ToF32(src[i]);
  } else if (t.dtype() == core::DType::F32) {
    std::memcpy(out.p, t.data(), sizeof(float) * n);
  } else {
    throw std::runtime_error("unsupported dtype for " + name);
  }
  return out;
}

AlignerLm::BfBuf AlignerLm::LoadBf16(const io::ShardedSafeTensors& w,
                                     const std::string& name) {
  core::Tensor t = w.GetTensorView(name);
  if (t.dtype() != core::DType::BF16)
    throw std::runtime_error("expected BF16 for " + name);
  const int64_t n = t.numel();
  BfBuf out;
  out.buf = std::make_shared<UnifiedBuffer>(sizeof(uint16_t) * n);
  out.p = static_cast<uint16_t*>(out.buf->data());
  std::memcpy(out.p, t.data(), sizeof(uint16_t) * n);
  return out;
}

void AlignerLm::LoadWeights(const io::ShardedSafeTensors& w) {
  const std::string M = "model.language_model.";
  embed_ = LoadBf16(w, M + "embed_tokens.weight");
  final_norm_ = LoadF32(w, M + "norm.weight");
  score_ = LoadBf16(w, "score.weight");

  const size_t embed_elems =
      static_cast<size_t>(config_.vocab_size) * config_.hidden_size;
  embed_host_.assign(embed_.p, embed_.p + embed_elems);

  layers_.resize(config_.num_layers);
  for (int i = 0; i < config_.num_layers; ++i) {
    const std::string p = M + "layers." + std::to_string(i) + ".";
    Layer& L = layers_[i];
    L.in_ln = LoadF32(w, p + "input_layernorm.weight");
    L.q_norm = LoadF32(w, p + "self_attn.q_norm.weight");
    L.k_norm = LoadF32(w, p + "self_attn.k_norm.weight");
    L.post_ln = LoadF32(w, p + "post_attention_layernorm.weight");
    L.q_w = LoadBf16(w, p + "self_attn.q_proj.weight");
    L.k_w = LoadBf16(w, p + "self_attn.k_proj.weight");
    L.v_w = LoadBf16(w, p + "self_attn.v_proj.weight");
    L.o_w = LoadBf16(w, p + "self_attn.o_proj.weight");
    L.gate_w = LoadBf16(w, p + "mlp.gate_proj.weight");
    L.up_w = LoadBf16(w, p + "mlp.up_proj.weight");
    L.down_w = LoadBf16(w, p + "mlp.down_proj.weight");
  }
}

std::vector<float> AlignerLm::Forward(const std::vector<int>& input_ids,
                                      const float* audio_feats, int n_audio,
                                      int audio_pad_id,
                                      std::vector<float>* hidden_out) const {
  const int T = static_cast<int>(input_ids.size());
  const int Hh = config_.hidden_size;
  const int Hq = config_.num_q_heads;
  const int Hkv = config_.num_kv_heads;
  const int Dh = config_.head_dim;
  const int Qd = Hq * Dh;
  const int KVd = Hkv * Dh;
  const int I = config_.intermediate_size;
  const int L_ = config_.num_labels;
  const float eps = config_.rms_norm_eps;
  const float scale = 1.0f / std::sqrt(static_cast<float>(Dh));
  cudaStream_t s = 0;  // asr_ops::GqaAttention runs on the default stream

  // ---- Build input embeddings on host: embed table lookup, audio injected ----
  std::vector<float> h_embed(static_cast<size_t>(T) * Hh);
  int audio_idx = 0;
  for (int t = 0; t < T; ++t) {
    float* dst = h_embed.data() + static_cast<size_t>(t) * Hh;
    if (input_ids[t] == audio_pad_id && audio_idx < n_audio) {
      std::memcpy(dst, audio_feats + static_cast<size_t>(audio_idx) * Hh,
                  sizeof(float) * Hh);
      ++audio_idx;
    } else {
      const uint16_t* row =
          embed_host_.data() + static_cast<size_t>(input_ids[t]) * Hh;
      for (int d = 0; d < Hh; ++d) dst[d] = Bf16ToF32(row[d]);
    }
  }

  std::vector<int> h_pos(T);
  for (int t = 0; t < T; ++t) h_pos[t] = t;

  // ---- Device scratch (device memory: avoids Tegra managed-migration hazard) ----
  auto dbuf = [](size_t bytes) { return std::make_shared<DeviceBuffer>(bytes); };
  auto d_x = dbuf(sizeof(float) * T * Hh);
  auto d_pos = dbuf(sizeof(int) * T);
  auto d_norm = dbuf(sizeof(float) * T * Hh);
  auto d_q = dbuf(sizeof(float) * T * Qd);
  auto d_k = dbuf(sizeof(float) * T * KVd);
  auto d_v = dbuf(sizeof(float) * T * KVd);
  auto d_attn = dbuf(sizeof(float) * T * Qd);
  auto d_proj = dbuf(sizeof(float) * T * Hh);
  auto d_gate = dbuf(sizeof(float) * T * I);
  auto d_up = dbuf(sizeof(float) * T * I);
  auto d_logits = dbuf(sizeof(float) * static_cast<size_t>(T) * L_);

  float* x = static_cast<float*>(d_x->data());
  float* nrm = static_cast<float*>(d_norm->data());
  float* q = static_cast<float*>(d_q->data());
  float* k = static_cast<float*>(d_k->data());
  float* v = static_cast<float*>(d_v->data());
  float* attn = static_cast<float*>(d_attn->data());
  float* proj = static_cast<float*>(d_proj->data());
  float* gate = static_cast<float*>(d_gate->data());
  float* up = static_cast<float*>(d_up->data());
  int* pos = static_cast<int*>(d_pos->data());

  CheckCudaError(cudaMemcpyAsync(x, h_embed.data(), sizeof(float) * T * Hh,
                                 cudaMemcpyHostToDevice, s),
                 __FILE__, __LINE__);
  CheckCudaError(cudaMemcpyAsync(pos, h_pos.data(), sizeof(int) * T,
                                 cudaMemcpyHostToDevice, s),
                 __FILE__, __LINE__);

  for (int li = 0; li < config_.num_layers; ++li) {
    const Layer& L = layers_[li];
    // Attention block.
    asr_ops::RmsNorm(x, L.in_ln.p, nrm, T, Hh, eps, s);
    asr_gemm::Linear(nrm, L.q_w.p, nullptr, q, T, Hh, Qd, 0, s);
    asr_gemm::Linear(nrm, L.k_w.p, nullptr, k, T, Hh, KVd, 0, s);
    asr_gemm::Linear(nrm, L.v_w.p, nullptr, v, T, Hh, KVd, 0, s);
    asr_ops::RmsNorm(q, L.q_norm.p, q, T * Hq, Dh, eps, s);
    asr_ops::RmsNorm(k, L.k_norm.p, k, T * Hkv, Dh, eps, s);
    asr_ops::RopeHalf(q, pos, T, Hq, Dh, config_.rope_theta, s);
    asr_ops::RopeHalf(k, pos, T, Hkv, Dh, config_.rope_theta, s);
    asr_ops::GqaAttention(q, k, v, attn, T, Hq, Hkv, Dh, scale, /*causal=*/true);
    asr_gemm::Linear(attn, L.o_w.p, nullptr, proj, T, Qd, Hh, 0, s);
    AddInPlaceKernel<<<Blocks(static_cast<long>(T) * Hh), kThreads, 0, s>>>(
        x, proj, static_cast<long>(T) * Hh);
    // MLP block.
    asr_ops::RmsNorm(x, L.post_ln.p, nrm, T, Hh, eps, s);
    asr_gemm::Linear(nrm, L.gate_w.p, nullptr, gate, T, Hh, I, 0, s);
    asr_gemm::Linear(nrm, L.up_w.p, nullptr, up, T, Hh, I, 0, s);
    asr_ops::SwiGLU(gate, up, gate, T * I, s);
    asr_gemm::Linear(gate, L.down_w.p, nullptr, proj, T, I, Hh, 0, s);
    AddInPlaceKernel<<<Blocks(static_cast<long>(T) * Hh), kThreads, 0, s>>>(
        x, proj, static_cast<long>(T) * Hh);
    CheckCudaError(cudaGetLastError(), __FILE__, __LINE__);
  }

  // Final RMSNorm over all positions, then the score head.
  asr_ops::RmsNorm(x, final_norm_.p, nrm, T, Hh, eps, s);
  if (hidden_out) {
    hidden_out->resize(static_cast<size_t>(T) * Hh);
    CheckCudaError(cudaMemcpyAsync(hidden_out->data(), nrm,
                                   sizeof(float) * T * Hh, cudaMemcpyDeviceToHost,
                                   s),
                   __FILE__, __LINE__);
  }
  asr_gemm::Linear(nrm, score_.p, nullptr, static_cast<float*>(d_logits->data()),
                   T, Hh, L_, 0, s);

  std::vector<float> logits(static_cast<size_t>(T) * L_);
  CheckCudaError(cudaMemcpyAsync(logits.data(), d_logits->data(),
                                 sizeof(float) * T * L_, cudaMemcpyDeviceToHost,
                                 s),
                 __FILE__, __LINE__);
  CheckCudaError(cudaStreamSynchronize(s), __FILE__, __LINE__);
  return logits;
}

}  // namespace model
}  // namespace orator
