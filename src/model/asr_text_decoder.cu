#include "model/asr_text_decoder.h"

#include <cuda_runtime.h>
#include <cuda_bf16.h>

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

#include "model/asr_gemm.h"
#include "model/asr_ops.cuh"

namespace orator {
namespace model {

using ::orator::gpu::CheckCudaError;
using ::orator::gpu::UnifiedBuffer;
using ::orator::gpu::PinnedBuffer;

namespace {

constexpr int kThreads = 256;
inline int Blocks(long n) { return static_cast<int>((n + kThreads - 1) / kThreads); }

__global__ void AddResidualKernel(float* __restrict__ a, const float* __restrict__ b, long n) {
  const long i = static_cast<long>(blockIdx.x) * blockDim.x + threadIdx.x;
  if (i < n) a[i] += b[i];
}

// Gather embedding for the token id in d_token[0] from the bf16 embed table
// directly into the f32 residual stream. No host round-trip.
__global__ void EmbedGatherKernel(const uint16_t* __restrict__ table,
                                  const int* __restrict__ d_token,
                                  float* __restrict__ out, int hidden) {
  const int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i >= hidden) return;
  const uint16_t bf = table[static_cast<size_t>(*d_token) * hidden + i];
  out[i] = __uint_as_float(static_cast<uint32_t>(bf) << 16);
}

// Records the current token id into the output ring and advances the counter.
// Single thread; runs at the START of each decode step (the recorded token is
// the one about to be embedded -- i.e. the emitted token for this step).
__global__ void RecordTokenKernel(int* __restrict__ out, int* __restrict__ count,
                                  const int* __restrict__ tok) {
  out[*count] = *tok;
  *count = *count + 1;
}

// Advances the absolute decode position by one (runs at the END of each step).
__global__ void IncPosKernel(int* __restrict__ pos) { *pos = *pos + 1; }

// Write k/v into the cache at [pos0 + t]. cache: [max_seq, Hkv, Dh] BF16.
// pos0 is read from a device scalar so a captured graph works at any position.
__global__ void WriteKvCacheKernel(const float* __restrict__ k, const float* __restrict__ v,
                                  uint16_t* __restrict__ kc, uint16_t* __restrict__ vc,
                                  int Tq, int Hkv, int Dh, const int* __restrict__ pos0p) {
  const long idx = static_cast<long>(blockIdx.x) * blockDim.x + threadIdx.x;
  const long total = static_cast<long>(Tq) * Hkv * Dh;
  if (idx >= total) return;
  const int pos0 = *pos0p;
  const int d = idx % Dh;
  const int h = (idx / Dh) % Hkv;
  const int t = idx / (static_cast<long>(Dh) * Hkv);
  const long dst = ((static_cast<long>(pos0 + t) * Hkv + h) * Dh) + d;
  kc[dst] = __bfloat16_as_ushort(__float2bfloat16(k[idx]));
  vc[dst] = __bfloat16_as_ushort(__float2bfloat16(v[idx]));
}

__device__ __forceinline__ float Bf16(uint16_t x) {
  return __bfloat162float(__ushort_as_bfloat16(x));
}

// Causal GQA against a BF16 KV cache. q: [Tq, Hq, Dh]; kc/vc: [S, Hkv, Dh].
// Query row t (absolute pos0+t) attends keys [0, pos0+t]. Warp per (token, head).
// Used for the PREFILL path (Tq>1) where Tq*Hq warps give good occupancy.
constexpr int kWarp = 32;
__global__ void GqaCacheAttnKernel(const float* __restrict__ q,
                                  const uint16_t* __restrict__ kc, const uint16_t* __restrict__ vc,
                                  float* __restrict__ out, int Tq, int Hq, int Hkv,
                                  int Dh, const int* __restrict__ pos0p, float scale) {
  const int pos0 = *pos0p;
  const int lane = threadIdx.x % kWarp;
  const int warp = (blockIdx.x * blockDim.x + threadIdx.x) / kWarp;
  const int t = warp / Hq;
  const int h = warp % Hq;
  if (t >= Tq) return;
  const int ELEMS = (Dh + kWarp - 1) / kWarp;  // 4
  const int group = Hq / Hkv;
  const int kvh = h / group;

  const float* qrow = q + (static_cast<size_t>(t) * Hq + h) * Dh;
  float qreg[8];
  for (int e = 0; e < ELEMS; ++e) {
    const int d = lane + e * kWarp;
    qreg[e] = (d < Dh) ? qrow[d] : 0.0f;
  }
  float m = -1e30f, l = 0.0f, acc[8];
  for (int e = 0; e < ELEMS; ++e) acc[e] = 0.0f;

  const int jmax = pos0 + t;
  for (int j = 0; j <= jmax; ++j) {
    const uint16_t* krow = kc + (static_cast<size_t>(j) * Hkv + kvh) * Dh;
    const uint16_t* vrow = vc + (static_cast<size_t>(j) * Hkv + kvh) * Dh;
    float partial = 0.0f;
    for (int e = 0; e < ELEMS; ++e) {
      const int d = lane + e * kWarp;
      if (d < Dh) partial += qreg[e] * Bf16(krow[d]);
    }
    for (int mask = kWarp / 2; mask > 0; mask >>= 1)
      partial += __shfl_xor_sync(0xffffffffu, partial, mask);
    const float score = partial * scale;
    const float nm = fmaxf(m, score);
    const float corr = expf(m - nm);
    const float p = expf(score - nm);
    l = l * corr + p;
    for (int e = 0; e < ELEMS; ++e) {
      const int d = lane + e * kWarp;
      const float vv = (d < Dh) ? Bf16(vrow[d]) : 0.0f;
      acc[e] = acc[e] * corr + p * vv;
    }
    m = nm;
  }
  const float invl = (l > 0.0f) ? 1.0f / l : 0.0f;
  float* orow = out + (static_cast<size_t>(t) * Hq + h) * Dh;
  for (int e = 0; e < ELEMS; ++e) {
    const int d = lane + e * kWarp;
    if (d < Dh) orow[d] = acc[e] * invl;
  }
}

// Decode attention (Tq==1): ONE BLOCK PER QUERY HEAD (Hq blocks), blockDim=Dh.
// Two passes with the whole block parallelizing over cache keys, so all Dh
// threads stay busy instead of the previous 1-warp-per-head (16 warps total).
// Scores live in shared memory (cap kMaxCtx). bf16 KV halves the read.
constexpr int kMaxCtx = 2048;
__global__ void GqaDecodeAttnKernel(const float* __restrict__ q,
                                   const uint16_t* __restrict__ kc, const uint16_t* __restrict__ vc,
                                   float* __restrict__ out, int Hq, int Hkv, int Dh,
                                   const int* __restrict__ pos0p, float scale) {
  const int h = blockIdx.x;        // query head
  const int d = threadIdx.x;       // 0..Dh-1
  const int group = Hq / Hkv;
  const int kvh = h / group;
  const int S = *pos0p + 1;        // cache length (causal: attend [0, pos0])

  extern __shared__ float sh[];
  float* qs = sh;                  // [Dh]
  float* sc = sh + Dh;             // [S] scores
  float* red = sc + kMaxCtx;       // [blockDim] reduction scratch

  qs[d] = q[(static_cast<size_t>(h)) * Dh + d];
  __syncthreads();

  // Pass 1: scores[j] = scale * dot(q, k_j). Each thread strides over keys.
  for (int j = d; j < S; j += Dh) {
    const uint16_t* krow = kc + (static_cast<size_t>(j) * Hkv + kvh) * Dh;
    float dot = 0.0f;
    for (int e = 0; e < Dh; ++e) dot += qs[e] * Bf16(krow[e]);
    sc[j] = dot * scale;
  }
  __syncthreads();

  // Block max over scores.
  float local = -1e30f;
  for (int j = d; j < S; j += Dh) local = fmaxf(local, sc[j]);
  red[d] = local;
  __syncthreads();
  for (int s = Dh / 2; s > 0; s >>= 1) {
    if (d < s) red[d] = fmaxf(red[d], red[d + s]);
    __syncthreads();
  }
  const float m = red[0];
  __syncthreads();

  // exp + sum.
  float lsum = 0.0f;
  for (int j = d; j < S; j += Dh) {
    const float e = expf(sc[j] - m);
    sc[j] = e;
    lsum += e;
  }
  red[d] = lsum;
  __syncthreads();
  for (int s = Dh / 2; s > 0; s >>= 1) {
    if (d < s) red[d] += red[d + s];
    __syncthreads();
  }
  const float inv = red[0] > 0.0f ? 1.0f / red[0] : 0.0f;
  __syncthreads();

  // Pass 2: out[d] = sum_j p_j * v_j[d].  Thread d owns output dim d.
  float acc = 0.0f;
  for (int j = 0; j < S; ++j) {
    const uint16_t* vrow = vc + (static_cast<size_t>(j) * Hkv + kvh) * Dh;
    acc += sc[j] * Bf16(vrow[d]);
  }
  out[static_cast<size_t>(h) * Dh + d] = acc * inv;
}

}  // namespace

AsrTextDecoder::AsrTextDecoder(const AsrTextConfig& config) : config_(config) {}

AsrTextDecoder::F32Buf AsrTextDecoder::LoadF32(const io::ShardedSafeTensors& w,
                                               const std::string& name) {
  core::Tensor t = w.GetTensorView(name);
  const int64_t n = t.numel();
  F32Buf out;
  out.buf = std::make_shared<UnifiedBuffer>(sizeof(float) * n);
  out.p = static_cast<float*>(out.buf->data());
  if (t.dtype() == core::DType::BF16) {
    const uint16_t* src = static_cast<const uint16_t*>(t.data());
    for (int64_t i = 0; i < n; ++i)
      out.p[i] = __builtin_bit_cast(float, static_cast<uint32_t>(src[i]) << 16);
  } else if (t.dtype() == core::DType::F32) {
    std::memcpy(out.p, t.data(), sizeof(float) * n);
  } else {
    throw std::runtime_error("unsupported dtype for " + name);
  }
  return out;
}

AsrTextDecoder::BfBuf AsrTextDecoder::LoadBf16(const io::ShardedSafeTensors& w,
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

void AsrTextDecoder::LoadWeights(const io::ShardedSafeTensors& w) {
  const std::string M = "thinker.model.";
  embed_ = LoadBf16(w, M + "embed_tokens.weight");
  final_norm_ = LoadF32(w, M + "norm.weight");
  lm_head_ = w.Has("thinker.lm_head.weight") ? LoadBf16(w, "thinker.lm_head.weight")
                                             : embed_;  // tied

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

  k_cache_.resize(config_.num_layers);
  v_cache_.resize(config_.num_layers);
  const size_t cache_elems =
      static_cast<size_t>(config_.max_seq_len) * config_.num_kv_heads * config_.head_dim;
  for (int i = 0; i < config_.num_layers; ++i) {
    k_cache_[i] = std::make_shared<UnifiedBuffer>(sizeof(uint16_t) * cache_elems);
    v_cache_[i] = std::make_shared<UnifiedBuffer>(sizeof(uint16_t) * cache_elems);
  }

  // Slots 0..13 are the prefill (Forward) scratch, grown to the prefill length
  // Tq. Slots 14..26 are DEDICATED decode-body scratch (Tq==1). They MUST be
  // disjoint: the per-token body is captured as a CUDA graph with these buffer
  // addresses baked in, and a later segment's prefill must never reallocate
  // (and thus move) them out from under the captured graph.
  work_.resize(27);
  work_cap_.assign(27, 0);
  d_logits_ = std::make_shared<UnifiedBuffer>(sizeof(float) * config_.vocab_size);
  d_argmax_ = std::make_shared<UnifiedBuffer>(sizeof(int));

  // Plain host shadow of the embedding table (bf16) so Embed() can read it
  // from the host at any time without the managed-memory migration hazard.
  const size_t embed_elems =
      static_cast<size_t>(config_.vocab_size) * config_.hidden_size;
  embed_host_.assign(embed_.p, embed_.p + embed_elems);
}

void AsrTextDecoder::ResetCache() { cache_len_ = 0; }

void AsrTextDecoder::Embed(int token_id, float* out) const {
  // Read from the plain host copy -- never triggers managed-memory migration.
  const uint16_t* row =
      embed_host_.data() + static_cast<size_t>(token_id) * config_.hidden_size;
  for (int i = 0; i < config_.hidden_size; ++i)
    out[i] = __builtin_bit_cast(float, static_cast<uint32_t>(row[i]) << 16);
}

float* AsrTextDecoder::Work(int which, size_t floats) {
  if (work_cap_[which] < floats) {
    work_[which] = std::make_shared<UnifiedBuffer>(sizeof(float) * floats);
    work_cap_[which] = floats;
  }
  return static_cast<float*>(work_[which]->data());
}

// Records the 28-layer + lm_head forward for a single token (Tq=1) onto
// `stream`, reading the absolute position from device scalar `d_pos`. Pure
// stream work with no host sync -> capturable as a CUDA graph and replayable
// at any position. d_x is the residual stream [hidden].
void AsrTextDecoder::DecodeForwardOnStream(float* d_x, const int* d_pos,
                                           cudaStream_t s) {
  const int Dh = config_.head_dim, Hq = config_.num_q_heads, Hkv = config_.num_kv_heads;
  const int Hh = config_.hidden_size, Qd = Hq * Dh, KVd = Hkv * Dh;
  const int I = config_.intermediate_size;
  const float eps = config_.rms_norm_eps;
  const float scale = 1.0f / std::sqrt(static_cast<float>(Dh));

  // Profiling uses cudaEventSynchronize, which is illegal during graph capture,
  // so only time the eager default-stream (s==0) path.
  const bool prof = (s == 0) && std::getenv("ORATOR_ASR_PROFILE2") != nullptr;
  cudaEvent_t e0, e1;
  if (prof) { cudaEventCreate(&e0); cudaEventCreate(&e1); cudaEventRecord(e0, s); }

  float* d_norm = Work(14, Hh);
  float* d_q = Work(15, Qd);
  float* d_k = Work(16, KVd);
  float* d_v = Work(17, KVd);
  float* d_attn = Work(18, Qd);
  float* d_proj = Work(19, Hh);
  float* d_gate = Work(20, I);
  float* d_up = Work(21, I);
  auto* norm_bf16 = reinterpret_cast<uint16_t*>(Work(23, Hh / 2 + 1));
  // Dedicated bf16 cast buffers so the M=1 GEMMs use LinearPre (no global
  // scratch / no lazy cudaMalloc) -> the whole decode body is allocation-free
  // and cuBLAS-free, hence safe to capture as a CUDA graph.
  auto* attn_bf16 = reinterpret_cast<uint16_t*>(Work(24, Qd / 2 + 1));
  auto* gate_bf16 = reinterpret_cast<uint16_t*>(Work(25, I / 2 + 1));
  auto* lastn_bf16 = reinterpret_cast<uint16_t*>(Work(26, Hh / 2 + 1));

  for (int li = 0; li < config_.num_layers; ++li) {
    const Layer& L = layers_[li];
    uint16_t* kc = static_cast<uint16_t*>(k_cache_[li]->data());
    uint16_t* vc = static_cast<uint16_t*>(v_cache_[li]->data());

    asr_ops::RmsNorm(d_x, L.in_ln.p, d_norm, 1, Hh, eps, s);
    asr_gemm::F32ToBf16(d_norm, norm_bf16, Hh, s);
    asr_gemm::LinearPre(norm_bf16, L.q_w.p, nullptr, d_q, 1, Hh, Qd, 0, s);
    asr_gemm::LinearPre(norm_bf16, L.k_w.p, nullptr, d_k, 1, Hh, KVd, 0, s);
    asr_gemm::LinearPre(norm_bf16, L.v_w.p, nullptr, d_v, 1, Hh, KVd, 0, s);
    asr_ops::RmsNorm(d_q, L.q_norm.p, d_q, Hq, Dh, eps, s);
    asr_ops::RmsNorm(d_k, L.k_norm.p, d_k, Hkv, Dh, eps, s);
    asr_ops::RopeHalf(d_q, d_pos, 1, Hq, Dh, config_.rope_theta, s);
    asr_ops::RopeHalf(d_k, d_pos, 1, Hkv, Dh, config_.rope_theta, s);
    WriteKvCacheKernel<<<Blocks(KVd), kThreads, 0, s>>>(d_k, d_v, kc, vc, 1, Hkv, Dh, d_pos);
    // Decode attention: one block per query head, block-parallel over the cache.
    const size_t attn_shmem = (Dh + kMaxCtx + Dh) * sizeof(float);
    GqaDecodeAttnKernel<<<Hq, Dh, attn_shmem, s>>>(
        d_q, kc, vc, d_attn, Hq, Hkv, Dh, d_pos, scale);
    asr_gemm::F32ToBf16(d_attn, attn_bf16, Qd, s);
    asr_gemm::LinearPre(attn_bf16, L.o_w.p, nullptr, d_proj, 1, Qd, Hh, 0, s);
    AddResidualKernel<<<Blocks(Hh), kThreads, 0, s>>>(d_x, d_proj, Hh);

    asr_ops::RmsNorm(d_x, L.post_ln.p, d_norm, 1, Hh, eps, s);
    asr_gemm::F32ToBf16(d_norm, norm_bf16, Hh, s);
    asr_gemm::LinearPre(norm_bf16, L.gate_w.p, nullptr, d_gate, 1, Hh, I, 0, s);
    asr_gemm::LinearPre(norm_bf16, L.up_w.p, nullptr, d_up, 1, Hh, I, 0, s);
    asr_ops::SwiGLU(d_gate, d_up, d_gate, I, s);
    asr_gemm::F32ToBf16(d_gate, gate_bf16, I, s);
    asr_gemm::LinearPre(gate_bf16, L.down_w.p, nullptr, d_proj, 1, I, Hh, 0, s);
    AddResidualKernel<<<Blocks(Hh), kThreads, 0, s>>>(d_x, d_proj, Hh);
  }

  float* d_lastn = Work(22, Hh);
  asr_ops::RmsNorm(d_x, final_norm_.p, d_lastn, 1, Hh, eps, s);
  asr_gemm::F32ToBf16(d_lastn, lastn_bf16, Hh, s);
  asr_gemm::LinearPre(lastn_bf16, lm_head_.p, nullptr,
                      static_cast<float*>(d_logits_->data()), 1, Hh,
                      config_.vocab_size, 0, s);
  if (prof) {
    cudaEventRecord(e1, s);
    cudaEventSynchronize(e1);
    float gpu_ms = 0;
    cudaEventElapsedTime(&gpu_ms, e0, e1);
    static int cnt = 0;
    static float sum = 0;
    sum += gpu_ms; ++cnt;
    if (cnt % 16 == 0)
      std::fprintf(stderr, "[dec2] per-token GPU compute avg = %.3f ms (n=%d)\n",
                   sum / cnt, cnt);
    cudaEventDestroy(e0); cudaEventDestroy(e1);
  }
}

void AsrTextDecoder::Forward(float* d_x, int Tq, int pos0, cudaStream_t stream) {
  const int Dh = config_.head_dim;
  const int Hq = config_.num_q_heads;
  const int Hkv = config_.num_kv_heads;
  const int Hh = config_.hidden_size;
  const int Qd = Hq * Dh;
  const int KVd = Hkv * Dh;
  const int I = config_.intermediate_size;
  const float eps = config_.rms_norm_eps;
  const float scale = 1.0f / std::sqrt(static_cast<float>(Dh));

  float* d_norm = Work(0, static_cast<size_t>(Tq) * Hh);
  float* d_q = Work(1, static_cast<size_t>(Tq) * Qd);
  float* d_k = Work(2, static_cast<size_t>(Tq) * KVd);
  float* d_v = Work(3, static_cast<size_t>(Tq) * KVd);
  float* d_attn = Work(4, static_cast<size_t>(Tq) * Qd);
  float* d_proj = Work(5, static_cast<size_t>(Tq) * Hh);
  float* d_gate = Work(6, static_cast<size_t>(Tq) * I);
  float* d_up = Work(7, static_cast<size_t>(Tq) * I);
  int* d_pos = reinterpret_cast<int*>(Work(8, static_cast<size_t>(Tq) + 1));
  for (int t = 0; t < Tq; ++t) d_pos[t] = pos0 + t;
  int* d_pos0 = d_pos + Tq;  // base position scalar for KV write + attention
  *d_pos0 = pos0;

  // BF16 scratch for the post-norm activation, cast once and shared by the
  // q/k/v projections (and again by gate/up) to cut redundant cast launches.
  auto* norm_bf16 = reinterpret_cast<uint16_t*>(Work(10, static_cast<size_t>(Tq) * Hh / 2 + 1));

  const bool prof = (Tq == 1) && std::getenv("ORATOR_ASR_PROFILE2") != nullptr;
  cudaEvent_t e0, e1, e2;
  if (prof) {
    cudaEventCreate(&e0); cudaEventCreate(&e1); cudaEventCreate(&e2);
    cudaEventRecord(e0);
  }

  for (int li = 0; li < config_.num_layers; ++li) {
    const Layer& L = layers_[li];
    uint16_t* kc = static_cast<uint16_t*>(k_cache_[li]->data());
    uint16_t* vc = static_cast<uint16_t*>(v_cache_[li]->data());

    asr_ops::RmsNorm(d_x, L.in_ln.p, d_norm, Tq, Hh, eps, stream);
    asr_gemm::F32ToBf16(d_norm, norm_bf16, static_cast<long>(Tq) * Hh, stream);
    asr_gemm::LinearPre(norm_bf16, L.q_w.p, nullptr, d_q, Tq, Hh, Qd, 0, stream);
    asr_gemm::LinearPre(norm_bf16, L.k_w.p, nullptr, d_k, Tq, Hh, KVd, 0, stream);
    asr_gemm::LinearPre(norm_bf16, L.v_w.p, nullptr, d_v, Tq, Hh, KVd, 0, stream);
    asr_ops::RmsNorm(d_q, L.q_norm.p, d_q, Tq * Hq, Dh, eps, stream);
    asr_ops::RmsNorm(d_k, L.k_norm.p, d_k, Tq * Hkv, Dh, eps, stream);
    asr_ops::RopeHalf(d_q, d_pos, Tq, Hq, Dh, config_.rope_theta, stream);
    asr_ops::RopeHalf(d_k, d_pos, Tq, Hkv, Dh, config_.rope_theta, stream);
    WriteKvCacheKernel<<<Blocks(static_cast<long>(Tq) * KVd), kThreads, 0, stream>>>(
        d_k, d_v, kc, vc, Tq, Hkv, Dh, d_pos0);
    CheckCudaError(cudaGetLastError(), __FILE__, __LINE__);
    const int warps = Tq * Hq;
    GqaCacheAttnKernel<<<Blocks(static_cast<long>(warps) * kWarp), kThreads, 0, stream>>>(
        d_q, kc, vc, d_attn, Tq, Hq, Hkv, Dh, d_pos0, scale);
    CheckCudaError(cudaGetLastError(), __FILE__, __LINE__);
    asr_gemm::Linear(d_attn, L.o_w.p, nullptr, d_proj, Tq, Qd, Hh, 0, stream);
    AddResidualKernel<<<Blocks(static_cast<long>(Tq) * Hh), kThreads, 0, stream>>>(
        d_x, d_proj, static_cast<long>(Tq) * Hh);

    asr_ops::RmsNorm(d_x, L.post_ln.p, d_norm, Tq, Hh, eps, stream);
    asr_gemm::F32ToBf16(d_norm, norm_bf16, static_cast<long>(Tq) * Hh, stream);
    asr_gemm::LinearPre(norm_bf16, L.gate_w.p, nullptr, d_gate, Tq, Hh, I, 0, stream);
    asr_gemm::LinearPre(norm_bf16, L.up_w.p, nullptr, d_up, Tq, Hh, I, 0, stream);
    asr_ops::SwiGLU(d_gate, d_up, d_gate, Tq * I, stream);
    asr_gemm::Linear(d_gate, L.down_w.p, nullptr, d_proj, Tq, I, Hh, 0, stream);
    AddResidualKernel<<<Blocks(static_cast<long>(Tq) * Hh), kThreads, 0, stream>>>(
        d_x, d_proj, static_cast<long>(Tq) * Hh);
    CheckCudaError(cudaGetLastError(), __FILE__, __LINE__);
  }
  cache_len_ = pos0 + Tq;

  // Final norm on the LAST token only, then lm_head -> logits.
  if (prof) cudaEventRecord(e1, stream);
  float* last = d_x + static_cast<size_t>(Tq - 1) * Hh;
  float* d_lastn = Work(9, Hh);
  asr_ops::RmsNorm(last, final_norm_.p, d_lastn, 1, Hh, eps, stream);
  asr_gemm::Linear(d_lastn, lm_head_.p, nullptr,
                   static_cast<float*>(d_logits_->data()), 1, Hh,
                   config_.vocab_size, 0, stream);
  CheckCudaError(cudaGetLastError(), __FILE__, __LINE__);
  if (prof) {
    cudaEventRecord(e2, stream);
    cudaEventSynchronize(e2);
    float layers_ms = 0, lm_ms = 0;
    cudaEventElapsedTime(&layers_ms, e0, e1);
    cudaEventElapsedTime(&lm_ms, e1, e2);
    static int once = 0;
    if (once++ < 1)
      std::fprintf(stderr, "[dec2] 28-layer loop=%.2fms lm_head+norm=%.2fms\n",
                   layers_ms, lm_ms);
    cudaEventDestroy(e0); cudaEventDestroy(e1); cudaEventDestroy(e2);
  }
}

void AsrTextDecoder::Prefill(const float* embeds, int T, cudaStream_t stream) {
  // Dedicated residual-stream buffer so it doesn't collide with Work(0..9).
  static thread_local std::shared_ptr<UnifiedBuffer> stream_buf;
  static thread_local size_t stream_cap = 0;
  const size_t need = static_cast<size_t>(T) * config_.hidden_size;
  if (stream_cap < need) {
    stream_buf = std::make_shared<UnifiedBuffer>(sizeof(float) * need);
    stream_cap = need;
  }
  float* x = static_cast<float*>(stream_buf->data());
  std::memcpy(x, embeds, sizeof(float) * need);
  Forward(x, T, 0, stream);
}

void AsrTextDecoder::DecodeStep(const float* embed, int pos, cudaStream_t stream) {
  const int Hh = config_.hidden_size;
  if (!step_x_) {
    step_x_ = std::make_shared<UnifiedBuffer>(sizeof(float) * Hh);
    step_pos_ = std::make_shared<PinnedBuffer>(sizeof(int));
    cudaStreamCreate(&capture_stream_);
  }
  float* x = static_cast<float*>(step_x_->data());
  int* p = static_cast<int*>(step_pos_->data());
  std::memcpy(x, embed, sizeof(float) * Hh);
  *p = pos;
  cache_len_ = pos + 1;
  DecodeForwardOnStream(x, p, stream);
  CheckCudaError(cudaStreamSynchronize(stream), __FILE__, __LINE__);
}

int AsrTextDecoder::Argmax(int ban0, int ban1, cudaStream_t stream) {
  asr_ops::ArgmaxBanned(static_cast<float*>(d_logits_->data()), config_.vocab_size,
                        ban0, ban1, static_cast<int*>(d_argmax_->data()), stream);
  CheckCudaError(cudaStreamSynchronize(stream), __FILE__, __LINE__);
  return *static_cast<int*>(d_argmax_->data());
}

void AsrTextDecoder::ArgmaxToDevice(int* d_out, int ban0, int ban1,
                                    cudaStream_t stream) {
  asr_ops::ArgmaxBanned(static_cast<float*>(d_logits_->data()), config_.vocab_size,
                        ban0, ban1, d_out, stream);
}

int AsrTextDecoder::ReadTokenId(const int* d_token, cudaStream_t stream) const {
  CheckCudaError(cudaStreamSynchronize(stream), __FILE__, __LINE__);
  return *d_token;
}

void AsrTextDecoder::DecodeStepDevice(const int* d_token, int pos,
                                      cudaStream_t stream) {
  const int Hh = config_.hidden_size;
  if (!step_x_) {
    step_x_ = std::make_shared<UnifiedBuffer>(sizeof(float) * Hh);
    step_pos_ = std::make_shared<PinnedBuffer>(sizeof(int));
    cudaStreamCreate(&capture_stream_);
  }
  float* x = static_cast<float*>(step_x_->data());
  int* p = static_cast<int*>(step_pos_->data());
  *p = pos;
  cache_len_ = pos + 1;
  EmbedGatherKernel<<<Blocks(Hh), kThreads, 0, stream>>>(embed_.p, d_token, x, Hh);
  CheckCudaError(cudaGetLastError(), __FILE__, __LINE__);
  DecodeForwardOnStream(x, p, stream);
}

// One autoregressive decode step body recorded onto stream `s`: record the
// current token, gather its embedding, run the 28-layer forward at *step_pos_,
// argmax the next token into d_tok_, and advance the position. All state lives
// in device scalars so the body is replayable (graph) at any position.
void AsrTextDecoder::DecodeBodyImpl(cudaStream_t s, int ban0, int ban1) {
  const int Hh = config_.hidden_size;
  int* tok = static_cast<int*>(d_tok_->data());
  int* cnt = static_cast<int*>(d_count_->data());
  int* outp = static_cast<int*>(d_out_->data());
  int* pos = static_cast<int*>(step_pos_->data());
  float* x = static_cast<float*>(step_x_->data());

  RecordTokenKernel<<<1, 1, 0, s>>>(outp, cnt, tok);
  EmbedGatherKernel<<<Blocks(Hh), kThreads, 0, s>>>(embed_.p, tok, x, Hh);
  DecodeForwardOnStream(x, pos, s);
  asr_ops::ArgmaxBanned(static_cast<float*>(d_logits_->data()), config_.vocab_size,
                        ban0, ban1, tok, s);
  IncPosKernel<<<1, 1, 0, s>>>(pos);
}

// Full greedy decode on the GPU. The per-token body is captured once and
// replayed back-to-back in batches with one host sync per batch, so the GPU
// stays saturated (DVFS can boost off the min clock) and launch overhead is
// amortized. The first `ban_steps` tokens run eagerly with EOS banned (their
// per-step ban can't be baked into the single captured graph).
std::vector<int> AsrTextDecoder::DecodeGreedy(int start_pos, int max_new,
                                              int eos0, int eos1, int ban_steps,
                                              int batch, cudaStream_t stream) {
  // The graph loop checks the stop condition (EOS / repetition) only at batch
  // boundaries, so a large batch over-generates tokens past EOS for short
  // utterances. ORATOR_ASR_BATCH overrides the batch size for tuning.
  if (const char* b = std::getenv("ORATOR_ASR_BATCH")) {
    const int v = std::atoi(b);
    if (v > 0) batch = v;
  }
  const int Hh = config_.hidden_size;
  if (!step_x_) {
    step_x_ = std::make_shared<UnifiedBuffer>(sizeof(float) * Hh);
    step_pos_ = std::make_shared<PinnedBuffer>(sizeof(int));
    cudaStreamCreate(&capture_stream_);
  }
  if (!d_tok_) {
    d_tok_ = std::make_shared<PinnedBuffer>(sizeof(int));
    d_count_ = std::make_shared<PinnedBuffer>(sizeof(int));
  }
  if (!d_out_ || d_out_->size() < sizeof(int) * static_cast<size_t>(max_new)) {
    d_out_ = std::make_shared<PinnedBuffer>(sizeof(int) * static_cast<size_t>(max_new));
    if (graph_exec_) {
      cudaGraphExecDestroy(static_cast<cudaGraphExec_t>(graph_exec_));
      graph_exec_ = nullptr;
    }
    graph_ready_ = false;  // d_out_ moved -> stale graph
  }

  int* tok = static_cast<int*>(d_tok_->data());
  int* cnt = static_cast<int*>(d_count_->data());
  int* outp = static_cast<int*>(d_out_->data());
  int* pos = static_cast<int*>(step_pos_->data());

  // Sync the ASR stream before writing to pinned scalars from the host.
  // Pinned memory is host-safe regardless of other streams (no managed-memory
  // migration hazard), but we must ensure asr_stream_ has no in-flight kernel
  // that reads these scalars.
  CheckCudaError(cudaStreamSynchronize(stream), __FILE__, __LINE__);
  *pos = start_pos;
  *cnt = 0;

  std::vector<int> out;
  out.reserve(max_new);

  auto harvest = [&](int upto) -> bool {  // true => stop (EOS or repetition)
    for (int i = static_cast<int>(out.size()); i < upto; ++i) {
      const int t = outp[i];
      if (t == eos0 || t == eos1) return true;
      out.push_back(t);
      if (out.size() >= 6) {
        bool same = true;
        for (size_t k = out.size() - 6; k + 1 < out.size(); ++k)
          if (out[k] != out.back()) { same = false; break; }
        if (same) return true;
      }
    }
    return false;
  };

  // Seed: argmax of the current (prefill) logits -> first token (step 0). Run
  // on the default stream, then drain so the device scalar `tok` is visible to
  // the capture/replay stream before any body reads it.
  const bool dprof = std::getenv("ORATOR_ASR_DECPROF") != nullptr;
  auto clk = [] { return std::chrono::steady_clock::now(); };
  auto ms = [](auto a, auto b) {
    return std::chrono::duration<double, std::milli>(b - a).count();
  };
  auto t_seed0 = clk();
  const bool seed_ban = 0 < ban_steps;
  asr_ops::ArgmaxBanned(static_cast<float*>(d_logits_->data()), config_.vocab_size,
                        seed_ban ? eos0 : -1, seed_ban ? eos1 : -1, tok, stream);
  CheckCudaError(cudaStreamSynchronize(stream), __FILE__, __LINE__);
  auto t_seed1 = clk();

  int emitted = 0;
  const bool use_graph = std::getenv("ORATOR_ASR_NOGRAPH") == nullptr;

  // If a previously captured banned graph baked different EOS ids, it is stale.
  if (graph_ready_ && (graph_ban0_ != eos0 || graph_ban1_ != eos1)) {
    if (graph_exec_) cudaGraphExecDestroy(static_cast<cudaGraphExec_t>(graph_exec_));
    if (graph_exec_banned_) cudaGraphExecDestroy(static_cast<cudaGraphExec_t>(graph_exec_banned_));
    graph_exec_ = graph_exec_banned_ = nullptr;
    graph_ready_ = false;
  }

  // Capture TWO graphs ONCE: a banned form (EOS suppressed -- replayed for the
  // first `ban_steps` argmaxes so the model can't emit EOS immediately) and a
  // ban-free form. Both are allocation-free single-token bodies replayable at
  // any position. Capture needs the decode Work buffers to exist, so the very
  // first call runs ONE eager step (which also emits token 0) to grow them.
  // Eliminating the old eager ban phase removes ~3 launch-bound tokens
  // (~220ms each on this Tegra) from every segment.
  if (use_graph && !graph_ready_) {
    const bool ban0 = (0 + 1) < ban_steps;
    DecodeBodyImpl(stream, ban0 ? eos0 : -1, ban0 ? eos1 : -1);  // warmup + token 0
    CheckCudaError(cudaStreamSynchronize(stream), __FILE__, __LINE__);
    ++emitted;

    auto capture = [&](int b0, int b1) -> void* {
      cudaGraph_t g;
      CheckCudaError(cudaStreamBeginCapture(capture_stream_,
                     cudaStreamCaptureModeThreadLocal), __FILE__, __LINE__);
      DecodeBodyImpl(capture_stream_, b0, b1);
      cudaGraph_t captured;
      if (cudaStreamEndCapture(capture_stream_, &captured) != cudaSuccess) return nullptr;
      cudaGraphExec_t exec = nullptr;
      cudaError_t inst = cudaGraphInstantiate(&exec, captured, nullptr, nullptr, 0);
      cudaGraphDestroy(captured);
      return (inst == cudaSuccess) ? exec : nullptr;
    };
    graph_ban0_ = eos0; graph_ban1_ = eos1;
    graph_exec_banned_ = capture(eos0, eos1);
    graph_exec_ = capture(-1, -1);
    graph_ready_ = true;
    if (std::getenv("ORATOR_ASR_PROFILE"))
      std::fprintf(stderr, "[graph] ready (banned=%p normal=%p)\n",
                   graph_exec_banned_, graph_exec_);
    if (harvest(emitted)) { cache_len_ = start_pos + emitted; return out; }
  }

  const bool have_graph =
      use_graph && graph_exec_ != nullptr && graph_exec_banned_ != nullptr;

  if (!have_graph) {
    // Eager fallback (NOGRAPH or capture failed): ban the first ban_steps
    // argmaxes one at a time, then run unbanned in batches, syncing per batch.
    while (emitted < max_new) {
      const bool next_ban = (emitted + 1) < ban_steps;
      const int this_batch = next_ban ? 1 : std::min(batch, max_new - emitted);
      for (int b = 0; b < this_batch; ++b)
        DecodeBodyImpl(stream, next_ban ? eos0 : -1, next_ban ? eos1 : -1);
      CheckCudaError(cudaStreamSynchronize(stream), __FILE__, __LINE__);
      emitted += this_batch;
      if (harvest(emitted)) break;
    }
    cache_len_ = start_pos + emitted;
    return out;
  }

  // Graph path: replay the banned graph until `ban_steps` argmaxes are done,
  // then the ban-free graph in batches. One host sync per batch harvests tokens
  // and checks the stop condition. Every token is now a single graph launch.
  auto t_loop0 = clk();
  double first_sync_ms = 0;
  int batches = 0;
  while (emitted < max_new) {
    const bool in_ban = (emitted + 1) < ban_steps;
    cudaGraphExec_t g = static_cast<cudaGraphExec_t>(
        in_ban ? graph_exec_banned_ : graph_exec_);
    const int limit = in_ban ? std::min(ban_steps - 1, max_new) : max_new;
    const int this_batch = std::max(1, std::min(batch, limit - emitted));
    auto s0 = clk();
    for (int b = 0; b < this_batch; ++b)
      CheckCudaError(cudaGraphLaunch(g, stream), __FILE__, __LINE__);
    CheckCudaError(cudaStreamSynchronize(stream), __FILE__, __LINE__);
    if (batches == 0) first_sync_ms = ms(s0, clk());
    ++batches;
    emitted += this_batch;
    if (harvest(emitted)) break;
  }
  if (dprof)
    std::fprintf(stderr,
                 "[decprof] seed+drain=%.1fms loop=%.1fms (emitted=%d batches=%d"
                 " first_batch_sync=%.1fms)\n",
                 ms(t_seed0, t_seed1), ms(t_loop0, clk()), emitted, batches,
                 first_sync_ms);
  cache_len_ = start_pos + emitted;
  return out;
}

std::vector<float> AsrTextDecoder::CopyLogits() const {
  // Forward leaves the lm_head GEMM in flight (no drain); sync before the host
  // read. This path is verification-only; the engine uses GPU Argmax instead.
  CheckCudaError(cudaDeviceSynchronize(), __FILE__, __LINE__);
  std::vector<float> out(config_.vocab_size);
  std::memcpy(out.data(), d_logits_->data(), sizeof(float) * config_.vocab_size);
  return out;
}

}  // namespace model
}  // namespace orator
