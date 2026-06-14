#include "model/asr_audio_tower.h"

#include <cuda_runtime.h>
#include <cuda_bf16.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

#include "model/asr_gemm.h"
#include "model/gemm.cuh"

namespace orator {
namespace model {

using ::orator::gpu::CheckCudaError;
using ::orator::gpu::UnifiedBuffer;

namespace {

constexpr int kThreads = 256;
inline int Blocks(int n) { return (n + kThreads - 1) / kThreads; }

// BF16 (top 16 bits of FP32) -> FP32.
__global__ void Bf16ToF32Kernel(const uint16_t* __restrict__ in,
                                float* __restrict__ out, int n) {
  const int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i >= n) return;
  out[i] = __uint_as_float(static_cast<uint32_t>(in[i]) << 16);
}

// Exact GELU (torch F.gelu default): 0.5 x (1 + erf(x/sqrt2)).
__global__ void GeluKernel(float* __restrict__ x, int n) {
  const int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i >= n) return;
  const float v = x[i];
  x[i] = 0.5f * v * (1.0f + erff(v * 0.70710678118654752440f));
}

// Conv2d, stride 2, pad 1, kernel 3. in: [B,Cin,H,W], W:[Cout,Cin,3,3],
// bias:[Cout]. out: [B,Cout,Hout,Wout], Hout=(H-1)/2+1, Wout=(W-1)/2+1.
// One thread per output element. (Kept for reference / fallback.)
__global__ void Conv2dKernel(const float* __restrict__ in, const float* __restrict__ wt,
                             const float* __restrict__ bias, float* __restrict__ out,
                             int B, int Cin, int H, int W, int Cout, int Hout, int Wout) {
  const long idx = static_cast<long>(blockIdx.x) * blockDim.x + threadIdx.x;
  const long total = static_cast<long>(B) * Cout * Hout * Wout;
  if (idx >= total) return;
  const int wo = idx % Wout;
  const int ho = (idx / Wout) % Hout;
  const int co = (idx / (static_cast<long>(Wout) * Hout)) % Cout;
  const int b = idx / (static_cast<long>(Wout) * Hout * Cout);

  float acc = bias ? bias[co] : 0.0f;
  for (int ci = 0; ci < Cin; ++ci) {
    const float* inb = in + ((static_cast<long>(b) * Cin + ci) * H) * W;
    const float* wcc = wt + ((static_cast<long>(co) * Cin + ci) * 3) * 3;
    for (int kh = 0; kh < 3; ++kh) {
      const int hi = ho * 2 - 1 + kh;
      if (hi < 0 || hi >= H) continue;
      for (int kw = 0; kw < 3; ++kw) {
        const int wi = wo * 2 - 1 + kw;
        if (wi < 0 || wi >= W) continue;
        acc += inb[hi * W + wi] * wcc[kh * 3 + kw];
      }
    }
  }
  out[idx] = acc;
}

// im2col for stride-2 pad-1 3x3 conv: build col[B*Hout*Wout, Cin*9] (bf16) so a
// single bf16 tensor-core GEMM with the weight [Cout, Cin*9] computes the conv.
// Row r = (b*Hout+ho)*Wout+wo; column ci*9 + kh*3 + kw = input[b,ci,ho*2-1+kh,wo*2-1+kw].
__global__ void Im2ColKernel(const float* __restrict__ in, uint16_t* __restrict__ col,
                            int B, int Cin, int H, int W, int Hout, int Wout) {
  const long r = static_cast<long>(blockIdx.y) * blockDim.y + threadIdx.y;  // spatial row
  const int k = blockIdx.x * blockDim.x + threadIdx.x;                      // 0..Cin*9-1
  const long rows = static_cast<long>(B) * Hout * Wout;
  if (r >= rows || k >= Cin * 9) return;
  const int wo = r % Wout;
  const int ho = (r / Wout) % Hout;
  const int b = r / (static_cast<long>(Wout) * Hout);
  const int ci = k / 9;
  const int kh = (k % 9) / 3;
  const int kw = k % 3;
  const int hi = ho * 2 - 1 + kh;
  const int wi = wo * 2 - 1 + kw;
  float v = 0.0f;
  if (hi >= 0 && hi < H && wi >= 0 && wi < W)
    v = in[((static_cast<long>(b) * Cin + ci) * H + hi) * W + wi];
  col[r * (static_cast<long>(Cin) * 9) + k] = __bfloat16_as_ushort(__float2bfloat16(v));
}

// Conv GEMM output is [rows=B*Ho*Wo, Cout]; rearrange to [B, Cout, Ho, Wo]
// (the layout the downstream conv stack + reshape expect) while adding bias and
// GELU. dst[b,co,ho,wo] = gelu(src[(b*Ho+ho)*Wo+wo, co] + bias[co]).
__global__ void ConvOutRearrangeKernel(const float* __restrict__ src,
                                       const float* __restrict__ bias,
                                       float* __restrict__ dst, int B, int Cout,
                                       int Ho, int Wo) {
  const long idx = static_cast<long>(blockIdx.x) * blockDim.x + threadIdx.x;
  const long total = static_cast<long>(B) * Cout * Ho * Wo;
  if (idx >= total) return;
  const int wo = idx % Wo;
  const int ho = (idx / Wo) % Ho;
  const int co = (idx / (static_cast<long>(Wo) * Ho)) % Cout;
  const int b = idx / (static_cast<long>(Wo) * Ho * Cout);
  const long row = (static_cast<long>(b) * Ho + ho) * Wo + wo;
  float v = src[row * Cout + co] + bias[co];
  dst[idx] = 0.5f * v * (1.0f + erff(v * 0.70710678118654752440f));
}

// LayerNorm with bias over the last dim D. in/out: [M, D].
__global__ void LayerNormKernel(const float* __restrict__ in, const float* __restrict__ g,
                               const float* __restrict__ b, float* __restrict__ out,
                               int M, int D, float eps) {
  const int m = blockIdx.x;
  if (m >= M) return;
  const float* xr = in + static_cast<size_t>(m) * D;
  float* yr = out + static_cast<size_t>(m) * D;
  __shared__ float red[kThreads];
  __shared__ float s_mean, s_inv;
  float local = 0.0f;
  for (int d = threadIdx.x; d < D; d += blockDim.x) local += xr[d];
  red[threadIdx.x] = local;
  __syncthreads();
  for (int s = blockDim.x / 2; s > 0; s >>= 1) {
    if (threadIdx.x < s) red[threadIdx.x] += red[threadIdx.x + s];
    __syncthreads();
  }
  if (threadIdx.x == 0) s_mean = red[0] / D;
  __syncthreads();
  const float mean = s_mean;
  float lv = 0.0f;
  for (int d = threadIdx.x; d < D; d += blockDim.x) {
    const float t = xr[d] - mean;
    lv += t * t;
  }
  red[threadIdx.x] = lv;
  __syncthreads();
  for (int s = blockDim.x / 2; s > 0; s >>= 1) {
    if (threadIdx.x < s) red[threadIdx.x] += red[threadIdx.x + s];
    __syncthreads();
  }
  if (threadIdx.x == 0) s_inv = rsqrtf(red[0] / D + eps);
  __syncthreads();
  const float inv = s_inv;
  for (int d = threadIdx.x; d < D; d += blockDim.x)
    yr[d] = (xr[d] - mean) * inv * g[d] + b[d];
}

__global__ void AddResidualKernel(float* __restrict__ a, const float* __restrict__ b, int n) {
  const int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i < n) a[i] += b[i];
}

// Add positional embedding per chunk: hidden[chunk*Tc + t, :] += pe[t, :].
// hidden: [chunks*Tc, D] (row-major), pe: [Tc, D].
__global__ void AddPeChunkedKernel(float* __restrict__ hidden, const float* __restrict__ pe,
                                  int chunks, int Tc, int D) {
  const long idx = static_cast<long>(blockIdx.x) * blockDim.x + threadIdx.x;
  const long total = static_cast<long>(chunks) * Tc * D;
  if (idx >= total) return;
  const int d = idx % D;
  const int t = (idx / D) % Tc;
  hidden[idx] += pe[static_cast<size_t>(t) * D + d];
}

// Windowed bidirectional attention. q/k/v: [N, H, Dh] (row-major after reshape).
// Each query token i attends to keys j in [seg_start[i], seg_end[i]).
// One warp per (token, head); Dh=64 -> 2 elems/lane. Online softmax.
constexpr int kWarp = 32;
__global__ void WindowAttnKernel(const float* __restrict__ q, const float* __restrict__ k,
                                const float* __restrict__ v, float* __restrict__ out,
                                const int* __restrict__ seg_start, const int* __restrict__ seg_end,
                                int N, int H, int Dh, float scale) {
  const int lane = threadIdx.x % kWarp;
  const int warp = (blockIdx.x * blockDim.x + threadIdx.x) / kWarp;
  const int i = warp / H;   // query token
  const int h = warp % H;   // head
  if (i >= N) return;
  const int ELEMS = (Dh + kWarp - 1) / kWarp;  // 2 for Dh=64

  const float* qrow = q + (static_cast<size_t>(i) * H + h) * Dh;
  float qreg[8];
  for (int e = 0; e < ELEMS; ++e) {
    const int d = lane + e * kWarp;
    qreg[e] = (d < Dh) ? qrow[d] : 0.0f;
  }
  float m = -1e30f, l = 0.0f, acc[8];
  for (int e = 0; e < ELEMS; ++e) acc[e] = 0.0f;

  const int j0 = seg_start[i], j1 = seg_end[i];
  for (int j = j0; j < j1; ++j) {
    const float* krow = k + (static_cast<size_t>(j) * H + h) * Dh;
    const float* vrow = v + (static_cast<size_t>(j) * H + h) * Dh;
    float partial = 0.0f;
    for (int e = 0; e < ELEMS; ++e) {
      const int d = lane + e * kWarp;
      if (d < Dh) partial += qreg[e] * krow[d];
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
      const float vv = (d < Dh) ? vrow[d] : 0.0f;
      acc[e] = acc[e] * corr + p * vv;
    }
    m = nm;
  }
  const float invl = (l > 0.0f) ? 1.0f / l : 0.0f;
  float* orow = out + (static_cast<size_t>(i) * H + h) * Dh;
  for (int e = 0; e < ELEMS; ++e) {
    const int d = lane + e * kWarp;
    if (d < Dh) orow[d] = acc[e] * invl;
  }
}

inline int ConvOut(int x) { return x <= 0 ? 0 : (x - 1) / 2 + 1; }

}  // namespace

int AsrAudioTower::OutputLength(int mel_frames) {
  const int leave = mel_frames % 100;
  const int t = ConvOut(ConvOut(ConvOut(leave)));
  return t + (mel_frames / 100) * 13;
}

AsrAudioTower::AsrAudioTower(const AsrAudioConfig& config) : config_(config) {
  // Sinusoidal positional embedding (Whisper SinusoidsPositionEmbedding).
  const int L = config_.max_source_positions, C = config_.d_model;
  pe_.assign(static_cast<size_t>(L) * C, 0.0f);
  const double log_inc = std::log(10000.0) / (C / 2 - 1);
  for (int pos = 0; pos < L; ++pos) {
    for (int i = 0; i < C / 2; ++i) {
      const double inv = std::exp(-log_inc * i);
      const double t = pos * inv;
      pe_[static_cast<size_t>(pos) * C + i] = static_cast<float>(std::sin(t));
      pe_[static_cast<size_t>(pos) * C + C / 2 + i] = static_cast<float>(std::cos(t));
    }
  }
}

AsrAudioTower::DevBuf AsrAudioTower::Load(const io::ShardedSafeTensors& w,
                                         const std::string& name) {
  core::Tensor t = w.GetTensorView(name);
  const int64_t n = t.numel();
  DevBuf out;
  out.buf = std::make_shared<UnifiedBuffer>(sizeof(float) * n);
  out.p = static_cast<float*>(out.buf->data());
  if (t.dtype() == core::DType::BF16) {
    UnifiedBuffer raw(sizeof(uint16_t) * n);
    std::memcpy(raw.data(), t.data(), sizeof(uint16_t) * n);
    Bf16ToF32Kernel<<<Blocks(n), kThreads>>>(
        static_cast<const uint16_t*>(raw.data()), out.p, static_cast<int>(n));
    CheckCudaError(cudaGetLastError(), __FILE__, __LINE__);
    CheckCudaError(cudaDeviceSynchronize(), __FILE__, __LINE__);
  } else if (t.dtype() == core::DType::F32) {
    std::memcpy(out.p, t.data(), sizeof(float) * n);
  } else {
    throw std::runtime_error("unsupported dtype for " + name);
  }
  return out;
}

AsrAudioTower::BfBuf AsrAudioTower::LoadBf16(const io::ShardedSafeTensors& w,
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

void AsrAudioTower::LoadWeights(const io::ShardedSafeTensors& w) {
  const std::string A = "thinker.audio_tower.";
  conv1_w_ = LoadBf16(w, A + "conv2d1.weight");
  conv1_b_ = Load(w, A + "conv2d1.bias");
  conv2_w_ = LoadBf16(w, A + "conv2d2.weight");
  conv2_b_ = Load(w, A + "conv2d2.bias");
  conv3_w_ = LoadBf16(w, A + "conv2d3.weight");
  conv3_b_ = Load(w, A + "conv2d3.bias");
  conv_out_w_ = LoadBf16(w, A + "conv_out.weight");
  ln_post_w_ = Load(w, A + "ln_post.weight");
  ln_post_b_ = Load(w, A + "ln_post.bias");
  proj1_w_ = LoadBf16(w, A + "proj1.weight");
  proj1_b_ = Load(w, A + "proj1.bias");
  proj2_w_ = LoadBf16(w, A + "proj2.weight");
  proj2_b_ = Load(w, A + "proj2.bias");

  layers_.resize(config_.encoder_layers);
  for (int i = 0; i < config_.encoder_layers; ++i) {
    const std::string p = A + "layers." + std::to_string(i) + ".";
    Layer& L = layers_[i];
    L.ln1_w = Load(w, p + "self_attn_layer_norm.weight");
    L.ln1_b = Load(w, p + "self_attn_layer_norm.bias");
    L.q_w = LoadBf16(w, p + "self_attn.q_proj.weight");
    L.q_b = Load(w, p + "self_attn.q_proj.bias");
    L.k_w = LoadBf16(w, p + "self_attn.k_proj.weight");
    L.k_b = Load(w, p + "self_attn.k_proj.bias");
    L.v_w = LoadBf16(w, p + "self_attn.v_proj.weight");
    L.v_b = Load(w, p + "self_attn.v_proj.bias");
    L.o_w = LoadBf16(w, p + "self_attn.out_proj.weight");
    L.o_b = Load(w, p + "self_attn.out_proj.bias");
    L.ln2_w = Load(w, p + "final_layer_norm.weight");
    L.ln2_b = Load(w, p + "final_layer_norm.bias");
    L.fc1_w = LoadBf16(w, p + "fc1.weight");
    L.fc1_b = Load(w, p + "fc1.bias");
    L.fc2_w = LoadBf16(w, p + "fc2.weight");
    L.fc2_b = Load(w, p + "fc2.bias");
  }
}

std::vector<float> AsrAudioTower::Forward(const float* mel, int n_frames,
                                          int* out_tokens,
                                          cudaStream_t stream) const {
  const int F = config_.num_mel_bins;        // 128
  const int D = config_.d_model;             // 1024
  const int H = config_.encoder_heads;       // 16
  const int Dh = config_.head_dim;           // 64
  const int win = config_.n_window * 2;      // 100 mel frames per chunk

  // ---- Chunking (matches reference: ceil division, tail = remainder) ----
  const int chunks = (n_frames + win - 1) / win;
  std::vector<int> chunk_len(chunks, win);
  const int rem = n_frames % win;
  if (rem != 0) chunk_len[chunks - 1] = rem;
  // pad_sequence pads every chunk to the max chunk length.
  int max_cl = 0;
  for (int c : chunk_len) max_cl = c > max_cl ? c : max_cl;

  // Conv output time per padded chunk.
  const int Hc1 = ConvOut(F), Hc2 = ConvOut(Hc1), Hc3 = ConvOut(Hc2);  // 64,32,16
  const int Wc = ConvOut(ConvOut(ConvOut(max_cl)));                    // time after cnn
  const int CF = config_.downsample_hidden * Hc3;                      // 7680

  // ---- Build padded conv input [chunks, 1, F, max_cl] (freq=H, time=W) ----
  UnifiedBuffer d_in(sizeof(float) * static_cast<size_t>(chunks) * F * max_cl);
  std::memset(d_in.data(), 0, sizeof(float) * static_cast<size_t>(chunks) * F * max_cl);
  float* in_p = static_cast<float*>(d_in.data());
  // mel is [F, n_frames]; chunk c covers mel frames [c*win, c*win+chunk_len[c]).
  for (int c = 0; c < chunks; ++c) {
    const int off = c * win;
    for (int f = 0; f < F; ++f)
      for (int t = 0; t < chunk_len[c]; ++t)
        in_p[(static_cast<size_t>(c) * F + f) * max_cl + t] =
            mel[static_cast<size_t>(f) * n_frames + (off + t)];
  }

  // ---- Conv2d x3 + GELU via im2col + bf16 tensor-core GEMM ----
  // For each conv: col[B*Ho*Wo, Cin*9] (bf16) = im2col(in); out_rows[rows, Cout]
  // = col @ W[Cout, Cin*9]^T (bf16 GEMM); then rearrange+bias+GELU -> [B,Cout,Ho,Wo].
  auto conv = [&](const float* in, const BfBuf& wt, const DevBuf& bias, int Cin,
                  int Hh, int Ww, int Cout, int Ho, int Wo) {
    const long rows = static_cast<long>(chunks) * Ho * Wo;
    const int K = Cin * 9;
    UnifiedBuffer col(sizeof(uint16_t) * rows * K);
    UnifiedBuffer outrows(sizeof(float) * rows * Cout);
    auto out = std::make_shared<UnifiedBuffer>(
        sizeof(float) * static_cast<size_t>(chunks) * Cout * Ho * Wo);
    dim3 blk(32, 8);
    dim3 grd((K + 31) / 32, static_cast<unsigned>((rows + 7) / 8));
            Im2ColKernel<<<grd, blk, 0, stream>>>(in, static_cast<uint16_t*>(col.data()), chunks, Cin,
                               Hh, Ww, Ho, Wo);
    CheckCudaError(cudaGetLastError(), __FILE__, __LINE__);
    // bf16 GEMM: rows x K @ (Cout x K)^T -> rows x Cout (no bias/act; fused next).
    asr_gemm::LinearPre(static_cast<uint16_t*>(col.data()), wt.p, nullptr,
                        static_cast<float*>(outrows.data()),
                      static_cast<int>(rows), K, Cout, 0, stream);
    const long total = static_cast<long>(chunks) * Cout * Ho * Wo;
            ConvOutRearrangeKernel<<<Blocks(total), kThreads, 0, stream>>>(
        static_cast<float*>(outrows.data()), bias.p,
        static_cast<float*>(out->data()), chunks, Cout, Ho, Wo);
    CheckCudaError(cudaGetLastError(), __FILE__, __LINE__);
    return out;
  };
  const int dh = config_.downsample_hidden;
  auto c1 = conv(in_p, conv1_w_, conv1_b_, 1, F, max_cl, dh, Hc1, ConvOut(max_cl));
  auto c2 = conv(static_cast<float*>(c1->data()), conv2_w_, conv2_b_, dh, Hc1,
                 ConvOut(max_cl), dh, Hc2, ConvOut(ConvOut(max_cl)));
  auto c3 = conv(static_cast<float*>(c2->data()), conv3_w_, conv3_b_, dh, Hc2,
                 ConvOut(ConvOut(max_cl)), dh, Hc3, Wc);
    CheckCudaError(cudaStreamSynchronize(stream), __FILE__, __LINE__);

  // ---- Reshape [chunks, dh, Hc3, Wc] -> [chunks*Wc, dh*Hc3] (permute b,t,c,f) ----
  const int rows = chunks * Wc;
  UnifiedBuffer d_feat(sizeof(float) * static_cast<size_t>(rows) * CF);
  float* feat = static_cast<float*>(d_feat.data());
  const float* c3p = static_cast<const float*>(c3->data());
  for (int b = 0; b < chunks; ++b)
    for (int t = 0; t < Wc; ++t)
      for (int co = 0; co < dh; ++co)
        for (int f = 0; f < Hc3; ++f)
          feat[(static_cast<size_t>(b) * Wc + t) * CF + (co * Hc3 + f)] =
              c3p[((static_cast<size_t>(b) * dh + co) * Hc3 + f) * Wc + t];


  // ---- conv_out Linear [rows, 7680] -> [rows, 1024] (no bias) ----
  UnifiedBuffer d_h(sizeof(float) * static_cast<size_t>(rows) * D);
  asr_gemm::Linear(feat, conv_out_w_.p, nullptr, static_cast<float*>(d_h.data()),
                     rows, CF, D, 0, stream);
  CheckCudaError(cudaGetLastError(), __FILE__, __LINE__);
    CheckCudaError(cudaStreamSynchronize(stream), __FILE__, __LINE__);

  // ---- Add positional embedding (per chunk, PE[0:Wc]) ----
  UnifiedBuffer d_pe(sizeof(float) * static_cast<size_t>(Wc) * D);
  std::memcpy(d_pe.data(), pe_.data(), sizeof(float) * static_cast<size_t>(Wc) * D);
    AddPeChunkedKernel<<<Blocks(static_cast<long>(rows) * D), kThreads, 0, stream>>>(
      static_cast<float*>(d_h.data()), static_cast<float*>(d_pe.data()), chunks, Wc, D);
  CheckCudaError(cudaGetLastError(), __FILE__, __LINE__);
    CheckCudaError(cudaStreamSynchronize(stream), __FILE__, __LINE__);

// Drop padding: keep valid frames per chunk -> hidden [N, D] ----
  std::vector<int> valid(chunks);
  int N = 0;
  for (int c = 0; c < chunks; ++c) { valid[c] = OutputLength(chunk_len[c]); N += valid[c]; }
  UnifiedBuffer d_hid(sizeof(float) * static_cast<size_t>(N) * D);
  float* hid = static_cast<float*>(d_hid.data());
  const float* hp = static_cast<const float*>(d_h.data());
  int row = 0;
  for (int c = 0; c < chunks; ++c)
    for (int t = 0; t < valid[c]; ++t, ++row)
      std::memcpy(hid + static_cast<size_t>(row) * D,
                  hp + (static_cast<size_t>(c) * Wc + t) * D, sizeof(float) * D);

  // ---- Attention windows (cu_seqlens) ----
  const int window_aftercnn = Wc * (config_.n_window_infer / win);  // 13*8=104
  const int aftercnn = OutputLength(n_frames);  // == N for single utterance
  std::vector<int> seg_bounds;  // cumulative
  seg_bounds.push_back(0);
  {
    int acc = 0;
    const int full = aftercnn / window_aftercnn;
    for (int i = 0; i < full; ++i) { acc += window_aftercnn; seg_bounds.push_back(acc); }
    const int r = aftercnn % window_aftercnn;
    if (r != 0) { acc += r; seg_bounds.push_back(acc); }
  }
  std::vector<int> h_seg_start(N), h_seg_end(N);
  for (size_t s = 1; s < seg_bounds.size(); ++s)
    for (int i = seg_bounds[s - 1]; i < seg_bounds[s] && i < N; ++i) {
      h_seg_start[i] = seg_bounds[s - 1];
      h_seg_end[i] = seg_bounds[s];
    }
  // NOTE: the transformers sdpa/eager path (no flash-attn) ignores cu_seqlens
  // and runs FULL bidirectional attention over the utterance. Match that here;
  // the trained windowing only diverges for very long sequences.
  if (const char* w = std::getenv("ORATOR_ASR_WINDOWED"); !w || w[0] != '1') {
    for (int i = 0; i < N; ++i) { h_seg_start[i] = 0; h_seg_end[i] = N; }
  }
  UnifiedBuffer d_ss(sizeof(int) * N), d_se(sizeof(int) * N);
  std::memcpy(d_ss.data(), h_seg_start.data(), sizeof(int) * N);
  std::memcpy(d_se.data(), h_seg_end.data(), sizeof(int) * N);

  // ---- 24 transformer layers ----
  UnifiedBuffer d_norm(sizeof(float) * static_cast<size_t>(N) * D);
  UnifiedBuffer d_q(sizeof(float) * static_cast<size_t>(N) * D);
  UnifiedBuffer d_k(sizeof(float) * static_cast<size_t>(N) * D);
  UnifiedBuffer d_v(sizeof(float) * static_cast<size_t>(N) * D);
  UnifiedBuffer d_attn(sizeof(float) * static_cast<size_t>(N) * D);
  UnifiedBuffer d_proj(sizeof(float) * static_cast<size_t>(N) * D);
  UnifiedBuffer d_ff(sizeof(float) * static_cast<size_t>(N) * config_.ffn_dim);
  const float scale = 1.0f / std::sqrt(static_cast<float>(Dh));
  const float eps = 1e-5f;

  for (int li = 0; li < config_.encoder_layers; ++li) {
    const Layer& L = layers_[li];
      LayerNormKernel<<<N, kThreads, 0, stream>>>(hid, L.ln1_w.p, L.ln1_b.p,
        static_cast<float*>(d_norm.data()), N, D, eps);
    asr_gemm::Linear(static_cast<float*>(d_norm.data()), L.q_w.p, L.q_b.p,
               static_cast<float*>(d_q.data()), N, D, D, 0, stream);
    asr_gemm::Linear(static_cast<float*>(d_norm.data()), L.k_w.p, L.k_b.p,
               static_cast<float*>(d_k.data()), N, D, D, 0, stream);
    asr_gemm::Linear(static_cast<float*>(d_norm.data()), L.v_w.p, L.v_b.p,
               static_cast<float*>(d_v.data()), N, D, D, 0, stream);
    const int warps = N * H;
      WindowAttnKernel<<<Blocks(warps * kWarp), kThreads, 0, stream>>>(
        static_cast<float*>(d_q.data()), static_cast<float*>(d_k.data()),
        static_cast<float*>(d_v.data()), static_cast<float*>(d_attn.data()),
        static_cast<int*>(d_ss.data()), static_cast<int*>(d_se.data()), N, H, Dh, scale);
    asr_gemm::Linear(static_cast<float*>(d_attn.data()), L.o_w.p, L.o_b.p,
               static_cast<float*>(d_proj.data()), N, D, D, 0, stream);
      AddResidualKernel<<<Blocks(N * D), kThreads, 0, stream>>>(hid,
        static_cast<float*>(d_proj.data()), N * D);
    // mlp block
      LayerNormKernel<<<N, kThreads, 0, stream>>>(hid, L.ln2_w.p, L.ln2_b.p,
        static_cast<float*>(d_norm.data()), N, D, eps);
    asr_gemm::Linear(static_cast<float*>(d_norm.data()), L.fc1_w.p, L.fc1_b.p,
               static_cast<float*>(d_ff.data()), N, D, config_.ffn_dim, 1, stream);
    asr_gemm::Linear(static_cast<float*>(d_ff.data()), L.fc2_w.p, L.fc2_b.p,
               static_cast<float*>(d_proj.data()), N, config_.ffn_dim, D, 0, stream);
      AddResidualKernel<<<Blocks(N * D), kThreads, 0, stream>>>(hid,
        static_cast<float*>(d_proj.data()), N * D);
    CheckCudaError(cudaGetLastError(), __FILE__, __LINE__);
  }
      CheckCudaError(cudaStreamSynchronize(stream), __FILE__, __LINE__);

  // ---- ln_post -> proj1 -> GELU -> proj2 ----
      LayerNormKernel<<<N, kThreads, 0, stream>>>(hid, ln_post_w_.p, ln_post_b_.p,
      static_cast<float*>(d_norm.data()), N, D, eps);
  asr_gemm::Linear(static_cast<float*>(d_norm.data()), proj1_w_.p, proj1_b_.p,
               static_cast<float*>(d_proj.data()), N, D, D, 1, stream);
  UnifiedBuffer d_out(sizeof(float) * static_cast<size_t>(N) * config_.output_dim);
  asr_gemm::Linear(static_cast<float*>(d_proj.data()), proj2_w_.p, proj2_b_.p,
               static_cast<float*>(d_out.data()), N, D, config_.output_dim, 0, stream);
  CheckCudaError(cudaGetLastError(), __FILE__, __LINE__);
      CheckCudaError(cudaStreamSynchronize(stream), __FILE__, __LINE__);

  std::vector<float> out(static_cast<size_t>(N) * config_.output_dim);
  std::memcpy(out.data(), d_out.data(), sizeof(float) * out.size());
  if (out_tokens) *out_tokens = N;
  return out;
}

}  // namespace model
}  // namespace orator
