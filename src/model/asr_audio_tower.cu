#include "model/asr_audio_tower.h"

#include <cuda_runtime.h>
#include <cuda_bf16.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

#include "core/log.h"
#include "model/asr_gemm.h"
#include "model/gemm.cuh"

namespace orator {
namespace model {

using ::orator::gpu::CheckCudaError;
using ::orator::gpu::DeviceBuffer;
using ::orator::gpu::PinnedBuffer;
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

// im2col for stride-2 pad-1 3x3 conv: build col[B*Hout*Wout, Cin*9] (bf16) so a
// single bf16 tensor-core GEMM with the weight [Cout, Cin*9] computes the conv.
// Row r = (b*Hout+ho)*Wout+wo; column ci*9 + kh*3 + kw =
// input[b,ci,ho*2-1+kh,wo*2-1+kw].
__global__ void Im2ColKernel(const float* __restrict__ in,
                             uint16_t* __restrict__ col, int B, int Cin, int H,
                             int W, int Hout, int Wout) {
  const long r =
      static_cast<long>(blockIdx.y) * blockDim.y + threadIdx.y;  // spatial row
  const int k = blockIdx.x * blockDim.x + threadIdx.x;           // 0..Cin*9-1
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
  col[r * (static_cast<long>(Cin) * 9) + k] =
      __bfloat16_as_ushort(__float2bfloat16(v));
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
__global__ void LayerNormKernel(const float* __restrict__ in,
                                const float* __restrict__ g,
                                const float* __restrict__ b,
                                float* __restrict__ out, int M, int D,
                                float eps) {
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

__global__ void AddResidualKernel(float* __restrict__ a,
                                  const float* __restrict__ b, int n) {
  const int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i < n) a[i] += b[i];
}

// Add positional embedding per chunk: hidden[chunk*Tc + t, :] += pe[t, :].
// hidden: [chunks*Tc, D] (row-major), pe: [Tc, D].
__global__ void AddPeChunkedKernel(float* __restrict__ hidden,
                                   const float* __restrict__ pe, int chunks,
                                   int Tc, int D) {
  const long idx = static_cast<long>(blockIdx.x) * blockDim.x + threadIdx.x;
  const long total = static_cast<long>(chunks) * Tc * D;
  if (idx >= total) return;
  const int d = idx % D;
  const int t = (idx / D) % Tc;
  hidden[idx] += pe[static_cast<size_t>(t) * D + d];
}

// Reshape conv3 output [chunks, dh, Hc3, Wc] (b,co,f,t) -> conv_out input
// [chunks*Wc, dh*Hc3] (row (b,t), col (co,f)). Pure layout permute (no math),
// done on the GPU to replace the prior DtoH -> host-permute -> HtoD round-trip.
// One thread per output element. CF = dh*Hc3.
__global__ void ReshapeC3Kernel(const float* __restrict__ c3,
                                float* __restrict__ feat, int chunks, int dh,
                                int Hc3, int Wc, int CF) {
  const long idx = static_cast<long>(blockIdx.x) * blockDim.x + threadIdx.x;
  const long total = static_cast<long>(chunks) * Wc * CF;
  if (idx >= total) return;
  const int col = static_cast<int>(idx % CF);
  const long row = idx / CF;
  const int t = static_cast<int>(row % Wc);
  const int b = static_cast<int>(row / Wc);
  const int f = col % Hc3;
  const int co = col / Hc3;
  feat[idx] = c3[(((static_cast<long>(b) * dh + co) * Hc3 + f) * Wc) + t];
}

// Build the padded conv input [chunks, F, max_cl] from mel [F, n_frames] on the
// GPU (replaces the host fill + HtoD). Element (c,f,t) = mel[f, c*win+t] when
// that frame exists (t < chunk_len[c] <=> c*win+t < n_frames), else 0.
__global__ void BuildConvInputKernel(const float* __restrict__ mel,
                                     float* __restrict__ d_in, int chunks,
                                     int F, int max_cl, int win, int n_frames) {
  const long idx = static_cast<long>(blockIdx.x) * blockDim.x + threadIdx.x;
  const long total = static_cast<long>(chunks) * F * max_cl;
  if (idx >= total) return;
  const int t = static_cast<int>(idx % max_cl);
  const int f = static_cast<int>((idx / max_cl) % F);
  const int c = static_cast<int>(idx / (static_cast<long>(max_cl) * F));
  const int src = c * win + t;
  d_in[idx] =
      (src < n_frames) ? mel[static_cast<long>(f) * n_frames + src] : 0.0f;
}

// Drop per-chunk padding on the GPU: output row r belongs to chunk c (found via
// the cumulative valid_prefix [chunks+1]) at local time t = r -
// valid_prefix[c]; copy d_h[(c*Wc+t), :] -> d_hid[r, :]. Replaces DtoH -> host
// gather -> HtoD.
__global__ void DropPadGatherKernel(const float* __restrict__ d_h,
                                    float* __restrict__ d_hid,
                                    const int* __restrict__ valid_prefix,
                                    int chunks, int Wc, int D, int N) {
  const long idx = static_cast<long>(blockIdx.x) * blockDim.x + threadIdx.x;
  const long total = static_cast<long>(N) * D;
  if (idx >= total) return;
  const int d = static_cast<int>(idx % D);
  const int r = static_cast<int>(idx / D);
  int c = 0;
  while (c < chunks - 1 && valid_prefix[c + 1] <= r) ++c;
  const int t = r - valid_prefix[c];
  d_hid[idx] = d_h[(static_cast<long>(c) * Wc + t) * D + d];
}

// Windowed bidirectional attention. q/k/v: [N, H, Dh] (row-major after
// reshape). Each query token i attends to keys j in [seg_start[i], seg_end[i]).
// One warp per (token, head); Dh=64 -> 2 elems/lane. Online softmax.
constexpr int kWarp = 32;
__global__ void WindowAttnKernel(const float* __restrict__ q,
                                 const float* __restrict__ k,
                                 const float* __restrict__ v,
                                 float* __restrict__ out,
                                 const int* __restrict__ seg_start,
                                 const int* __restrict__ seg_end, int N, int H,
                                 int Dh, float scale) {
  const int lane = threadIdx.x % kWarp;
  const int warp = (blockIdx.x * blockDim.x + threadIdx.x) / kWarp;
  const int i = warp / H;  // query token
  const int h = warp % H;  // head
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
      pe_[static_cast<size_t>(pos) * C + C / 2 + i] =
          static_cast<float>(std::cos(t));
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

void AsrAudioTower::LoadWeights(const io::ShardedSafeTensors& w,
                                const WeightNames& names) {
  const std::string A = names.prefix;
  conv1_w_ = LoadBf16(w, A + "conv2d1.weight");
  conv1_b_ = Load(w, A + "conv2d1.bias");
  conv2_w_ = LoadBf16(w, A + "conv2d2.weight");
  conv2_b_ = Load(w, A + "conv2d2.bias");
  conv3_w_ = LoadBf16(w, A + "conv2d3.weight");
  conv3_b_ = Load(w, A + "conv2d3.bias");
  conv_out_w_ = LoadBf16(w, A + "conv_out.weight");
  ln_post_w_ = Load(w, A + "ln_post.weight");
  ln_post_b_ = Load(w, A + "ln_post.bias");
  proj1_w_ = LoadBf16(w, names.proj1 + ".weight");
  proj1_b_ = Load(w, names.proj1 + ".bias");
  proj2_w_ = LoadBf16(w, names.proj2 + ".weight");
  proj2_b_ = Load(w, names.proj2 + ".bias");

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
  const int F = config_.num_mel_bins;    // 128
  const int D = config_.d_model;         // 1024
  const int H = config_.encoder_heads;   // 16
  const int Dh = config_.head_dim;       // 64
  const int win = config_.n_window * 2;  // 100 mel frames per chunk

  const bool prof = std::getenv("ORATOR_TOWER_PROFILE") != nullptr;
  auto pnow = [] { return std::chrono::steady_clock::now(); };
  const auto pt0 = pnow();

  // ---- Chunking (matches reference: ceil division, tail = remainder) ----
  const int chunks = (n_frames + win - 1) / win;
  std::vector<int> chunk_len(chunks, win);
  const int rem = n_frames % win;
  if (rem != 0) chunk_len[chunks - 1] = rem;
  // pad_sequence pads every chunk to the max chunk length.
  int max_cl = 0;
  for (int c : chunk_len) max_cl = c > max_cl ? c : max_cl;

  // Conv output time per padded chunk.
  const int Hc1 = ConvOut(F), Hc2 = ConvOut(Hc1),
            Hc3 = ConvOut(Hc2);                      // 64,32,16
  const int Wc = ConvOut(ConvOut(ConvOut(max_cl)));  // time after cnn
  const int CF = config_.downsample_hidden * Hc3;    // 7680

  // ---- Build padded conv input [chunks, 1, F, max_cl] on the GPU ----
  // Upload mel [F, n_frames] to the device, then scatter it into [chunks, F,
  // max_cl] with zero padding (replaces the host fill + memset + HtoD). All
  // device memory; values identical.
  const size_t in_elems = static_cast<size_t>(chunks) * F * max_cl;
  float* d_mel = scratch_.GetT<float>(0, static_cast<size_t>(F) * n_frames);
  CheckCudaError(
      cudaMemcpyAsync(d_mel, mel,
                      sizeof(float) * static_cast<size_t>(F) * n_frames,
                      cudaMemcpyHostToDevice, stream),
      __FILE__, __LINE__);
  float* in_p = scratch_.GetT<float>(1, in_elems);
  BuildConvInputKernel<<<Blocks(static_cast<long>(in_elems)), kThreads, 0,
                         stream>>>(d_mel, in_p, chunks, F, max_cl, win,
                                   n_frames);
  CheckCudaError(cudaGetLastError(), __FILE__, __LINE__);

  // ---- Conv2d x3 + GELU via im2col + bf16 tensor-core GEMM ----
  // For each conv: col[B*Ho*Wo, Cin*9] (bf16) = im2col(in); out_rows[rows,
  // Cout] = col @ W[Cout, Cin*9]^T (bf16 GEMM); then rearrange+bias+GELU ->
  // [B,Cout,Ho,Wo].
  auto conv = [&](const float* in, const BfBuf& wt, const DevBuf& bias, int Cin,
                  int Hh, int Ww, int Cout, int Ho, int Wo,
                  int out_slot) -> float* {
    const long rows = static_cast<long>(chunks) * Ho * Wo;
    const int K = Cin * 9;
    // Conv-internal temporaries reuse shared scratch slots (stream-ordered):
    // col (im2col, slot 2) and outrows (GEMM output, slot 3). The conv output
    // uses a caller-supplied distinct slot so chained convs (c1->c2->c3) and
    // the reshape that follows never alias each other. All device memory:
    // written and read only by kernels, so the managed page-fault hazard is
    // gone too.
    uint16_t* col = scratch_.GetT<uint16_t>(2, static_cast<size_t>(rows) * K);
    float* outrows = scratch_.GetT<float>(3, static_cast<size_t>(rows) * Cout);
    float* out = scratch_.GetT<float>(
        out_slot, static_cast<size_t>(chunks) * Cout * Ho * Wo);
    dim3 blk(32, 8);
    dim3 grd((K + 31) / 32, static_cast<unsigned>((rows + 7) / 8));
    Im2ColKernel<<<grd, blk, 0, stream>>>(in, col, chunks, Cin, Hh, Ww, Ho, Wo);
    CheckCudaError(cudaGetLastError(), __FILE__, __LINE__);
    // bf16 GEMM: rows x K @ (Cout x K)^T -> rows x Cout (no bias/act; fused
    // next).
    asr_gemm::LinearPre(col, wt.p, nullptr, outrows, static_cast<int>(rows), K,
                        Cout, 0, stream);
    const long total = static_cast<long>(chunks) * Cout * Ho * Wo;
    ConvOutRearrangeKernel<<<Blocks(total), kThreads, 0, stream>>>(
        outrows, bias.p, out, chunks, Cout, Ho, Wo);
    CheckCudaError(cudaGetLastError(), __FILE__, __LINE__);
    return out;
  };
  const int dh = config_.downsample_hidden;
  float* c1 =
      conv(in_p, conv1_w_, conv1_b_, 1, F, max_cl, dh, Hc1, ConvOut(max_cl), 4);
  float* c2 = conv(c1, conv2_w_, conv2_b_, dh, Hc1, ConvOut(max_cl), dh, Hc2,
                   ConvOut(ConvOut(max_cl)), 5);
  float* c3 = conv(c2, conv3_w_, conv3_b_, dh, Hc2, ConvOut(ConvOut(max_cl)),
                   dh, Hc3, Wc, 6);
  const auto pt_conv = pnow();

  // ---- Reshape [chunks, dh, Hc3, Wc] -> [chunks*Wc, dh*Hc3] on the GPU ----
  // Pure layout permute (b,co,f,t)->((b,t),(co,f)); a device kernel replaces
  // the prior DtoH -> host-permute -> HtoD round-trip (eliminates two copies
  // and a sync; values identical). c3 and d_feat are both DEVICE memory.
  const int rows = chunks * Wc;
  const size_t feat_elems = static_cast<size_t>(rows) * CF;
  float* d_feat = scratch_.GetT<float>(7, feat_elems);
  ReshapeC3Kernel<<<Blocks(static_cast<long>(feat_elems)), kThreads, 0,
                    stream>>>(c3, d_feat, chunks, dh, Hc3, Wc, CF);
  CheckCudaError(cudaGetLastError(), __FILE__, __LINE__);

  // ---- conv_out Linear [rows, 7680] -> [rows, 1024] (no bias) ----
  float* d_h = scratch_.GetT<float>(8, static_cast<size_t>(rows) * D);
  asr_gemm::Linear(d_feat, conv_out_w_.p, nullptr, d_h, rows, CF, D, 0, stream);
  CheckCudaError(cudaGetLastError(), __FILE__, __LINE__);
  // (no sync: PE kernel below is ordered after this GEMM on the same stream)
  const auto pt_convout = pnow();

  // ---- Add positional embedding (per chunk, PE[0:Wc]) ----
  // Spec 002 Phase 7 (T072): d_pe is DEVICE memory uploaded from the host PE
  // table; the host never touches managed memory here.
  float* d_pe = scratch_.GetT<float>(9, static_cast<size_t>(Wc) * D);
  CheckCudaError(cudaMemcpyAsync(d_pe, pe_.data(),
                                 sizeof(float) * static_cast<size_t>(Wc) * D,
                                 cudaMemcpyHostToDevice, stream),
                 __FILE__, __LINE__);
  AddPeChunkedKernel<<<Blocks(static_cast<long>(rows) * D), kThreads, 0,
                       stream>>>(d_h, d_pe, chunks, Wc, D);
  CheckCudaError(cudaGetLastError(), __FILE__, __LINE__);
  // (no sync: the drop-pad DtoH below is ordered after this kernel on the
  // stream
  //  and is followed by its own sync before the host reads)
  const auto pt_pe = pnow();

  // Drop padding: keep valid frames per chunk -> hidden [N, D] ----
  std::vector<int> valid(chunks);
  std::vector<int> valid_prefix(chunks + 1, 0);
  int N = 0;
  for (int c = 0; c < chunks; ++c) {
    valid[c] = OutputLength(chunk_len[c]);
    valid_prefix[c + 1] = valid_prefix[c] + valid[c];
    N += valid[c];
  }
  // Drop the per-chunk padding on the GPU: upload the cumulative valid_prefix,
  // then gather d_h[(c*Wc+t), :] -> d_hid[r, :] (replaces DtoH -> host gather
  // -> HtoD and its sync). All device memory; values identical.
  int* d_vp = scratch_.GetT<int>(10, static_cast<size_t>(chunks) + 1);
  CheckCudaError(
      cudaMemcpyAsync(d_vp, valid_prefix.data(), sizeof(int) * (chunks + 1),
                      cudaMemcpyHostToDevice, stream),
      __FILE__, __LINE__);
  float* hid = scratch_.GetT<float>(11, static_cast<size_t>(N) * D);
  DropPadGatherKernel<<<Blocks(static_cast<long>(N) * D), kThreads, 0,
                        stream>>>(d_h, hid, d_vp, chunks, Wc, D, N);
  CheckCudaError(cudaGetLastError(), __FILE__, __LINE__);
  // The transformer layers operate in place on the device hidden state.

  // ---- Attention windows (cu_seqlens) ----
  const int window_aftercnn = Wc * (config_.n_window_infer / win);  // 13*8=104
  const int aftercnn = OutputLength(n_frames);  // == N for single utterance
  std::vector<int> seg_bounds;                  // cumulative
  seg_bounds.push_back(0);
  {
    int acc = 0;
    const int full = aftercnn / window_aftercnn;
    for (int i = 0; i < full; ++i) {
      acc += window_aftercnn;
      seg_bounds.push_back(acc);
    }
    const int r = aftercnn % window_aftercnn;
    if (r != 0) {
      acc += r;
      seg_bounds.push_back(acc);
    }
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
    for (int i = 0; i < N; ++i) {
      h_seg_start[i] = 0;
      h_seg_end[i] = N;
    }
  }
  // Spec 002 Phase 7 (T072): segment bounds are DEVICE memory uploaded from the
  // host vectors; the host never writes managed memory here.
  int* d_ss = scratch_.GetT<int>(12, static_cast<size_t>(N));
  int* d_se = scratch_.GetT<int>(13, static_cast<size_t>(N));
  CheckCudaError(cudaMemcpyAsync(d_ss, h_seg_start.data(), sizeof(int) * N,
                                 cudaMemcpyHostToDevice, stream),
                 __FILE__, __LINE__);
  CheckCudaError(cudaMemcpyAsync(d_se, h_seg_end.data(), sizeof(int) * N,
                                 cudaMemcpyHostToDevice, stream),
                 __FILE__, __LINE__);

  // ---- 24 transformer layers ----
  const auto pt_prep = pnow();
  float* d_norm = scratch_.GetT<float>(14, static_cast<size_t>(N) * D);
  float* d_q = scratch_.GetT<float>(15, static_cast<size_t>(N) * D);
  float* d_k = scratch_.GetT<float>(16, static_cast<size_t>(N) * D);
  float* d_v = scratch_.GetT<float>(17, static_cast<size_t>(N) * D);
  float* d_attn = scratch_.GetT<float>(18, static_cast<size_t>(N) * D);
  float* d_proj = scratch_.GetT<float>(19, static_cast<size_t>(N) * D);
  float* d_ff =
      scratch_.GetT<float>(20, static_cast<size_t>(N) * config_.ffn_dim);
  const float scale = 1.0f / std::sqrt(static_cast<float>(Dh));
  const float eps = 1e-5f;

  for (int li = 0; li < config_.encoder_layers; ++li) {
    const Layer& L = layers_[li];
    LayerNormKernel<<<N, kThreads, 0, stream>>>(hid, L.ln1_w.p, L.ln1_b.p,
                                                d_norm, N, D, eps);
    asr_gemm::Linear(d_norm, L.q_w.p, L.q_b.p, d_q, N, D, D, 0, stream);
    asr_gemm::Linear(d_norm, L.k_w.p, L.k_b.p, d_k, N, D, D, 0, stream);
    asr_gemm::Linear(d_norm, L.v_w.p, L.v_b.p, d_v, N, D, D, 0, stream);
    const int warps = N * H;
    WindowAttnKernel<<<Blocks(warps * kWarp), kThreads, 0, stream>>>(
        d_q, d_k, d_v, d_attn, d_ss, d_se, N, H, Dh, scale);
    asr_gemm::Linear(d_attn, L.o_w.p, L.o_b.p, d_proj, N, D, D, 0, stream);
    AddResidualKernel<<<Blocks(N * D), kThreads, 0, stream>>>(hid, d_proj,
                                                              N * D);
    // mlp block
    LayerNormKernel<<<N, kThreads, 0, stream>>>(hid, L.ln2_w.p, L.ln2_b.p,
                                                d_norm, N, D, eps);
    asr_gemm::Linear(d_norm, L.fc1_w.p, L.fc1_b.p, d_ff, N, D, config_.ffn_dim,
                     1, stream);
    asr_gemm::Linear(d_ff, L.fc2_w.p, L.fc2_b.p, d_proj, N, config_.ffn_dim, D,
                     0, stream);
    AddResidualKernel<<<Blocks(N * D), kThreads, 0, stream>>>(hid, d_proj,
                                                              N * D);
    CheckCudaError(cudaGetLastError(), __FILE__, __LINE__);
  }
  // (no sync: ln_post below is ordered after the layer loop on the same stream)
  const auto pt_layers = pnow();

  // ---- ln_post -> proj1 -> GELU -> proj2 ----
  LayerNormKernel<<<N, kThreads, 0, stream>>>(hid, ln_post_w_.p, ln_post_b_.p,
                                              d_norm, N, D, eps);
  asr_gemm::Linear(d_norm, proj1_w_.p, proj1_b_.p, d_proj, N, D, D, 1, stream);
  // Spec 002 Phase 7 (T072): the encoder output is copied to the host below. It
  // MUST be DEVICE memory, not managed: the raw host memcpy from managed memory
  // while the concurrent diarization pipeline runs a kernel faults on Tegra
  // (R1). Device memory does not page-migrate, so the DtoH copy is safe
  // regardless of concurrent kernels. The GPU-only working buffers above stay
  // managed.
  float* d_out =
      scratch_.GetT<float>(21, static_cast<size_t>(N) * config_.output_dim);
  asr_gemm::Linear(d_proj, proj2_w_.p, proj2_b_.p, d_out, N, D,
                   config_.output_dim, 0, stream);
  CheckCudaError(cudaGetLastError(), __FILE__, __LINE__);
  // (no sync: the output DtoH below is ordered after proj2 on the same stream
  //  and is followed by its own sync before the host reads `out`)

  std::vector<float> out(static_cast<size_t>(N) * config_.output_dim);
  // DtoH copy from DEVICE memory on the ASR stream (R1-safe). Replaces the raw
  // host memcpy from managed memory.
  CheckCudaError(cudaMemcpyAsync(out.data(), d_out, sizeof(float) * out.size(),
                                 cudaMemcpyDeviceToHost, stream),
                 __FILE__, __LINE__);
  CheckCudaError(cudaStreamSynchronize(stream), __FILE__, __LINE__);
  if (prof) {
    auto ms = [](auto a, auto b) {
      return std::chrono::duration<double, std::milli>(b - a).count();
    };
    LOG_INFO(
        "[tower-profile] conv=%.1f reshape=%.1f pe=%.1f prep=%.1f "
        "layers=%.1f proj=%.1f ms (N=%d chunks=%d)\n",
        ms(pt0, pt_conv), ms(pt_conv, pt_convout), ms(pt_convout, pt_pe),
        ms(pt_pe, pt_prep), ms(pt_prep, pt_layers), ms(pt_layers, pnow()), N,
        chunks);
  }
  if (out_tokens) *out_tokens = N;
  return out;
}

}  // namespace model
}  // namespace orator
