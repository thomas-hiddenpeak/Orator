#include "model/conformer_layer.h"

#include <cuda_runtime.h>

#include <cmath>
#include <stdexcept>

#include "model/gemm.cuh"

namespace orator {
namespace model {

using ::orator::gpu::CheckCudaError;
using ::orator::gpu::UnifiedBuffer;

namespace {

constexpr int kThreads = 256;
inline int Blocks(int n) { return (n + kThreads - 1) / kThreads; }

inline void LaunchLinear(const float* in, const float* W, const float* bias,
                          float* out, int M, int K, int N, int act,
                          cudaStream_t stream) {
  gemm::LaunchSgemm(in, W, bias, out, M, K, N, act, stream);
}

// LayerNorm over last dim D for each of M rows.
__global__ void LayerNormKernel(const float* __restrict__ in,
                                const float* __restrict__ gamma,
                                const float* __restrict__ beta,
                                float* __restrict__ out, int M, int D, float eps) {
  int m = blockIdx.x;
  if (m >= M) return;
  const float* xr = in + static_cast<size_t>(m) * D;
  float* yr = out + static_cast<size_t>(m) * D;
  __shared__ float s_mean, s_inv;
  float local = 0.0f;
  for (int d = threadIdx.x; d < D; d += blockDim.x) local += xr[d];
  __shared__ float red[kThreads];
  red[threadIdx.x] = local;
  __syncthreads();
  for (int s = blockDim.x / 2; s > 0; s >>= 1) {
    if (threadIdx.x < s) red[threadIdx.x] += red[threadIdx.x + s];
    __syncthreads();
  }
  if (threadIdx.x == 0) s_mean = red[0] / D;
  __syncthreads();
  float mean = s_mean;
  float lv = 0.0f;
  for (int d = threadIdx.x; d < D; d += blockDim.x) {
    float t = xr[d] - mean;
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
  float inv = s_inv;
  for (int d = threadIdx.x; d < D; d += blockDim.x)
    yr[d] = (xr[d] - mean) * inv * gamma[d] + beta[d];
}

// r[i] += scale * x[i]
__global__ void AddScaledKernel(float* r, const float* x, float scale, int n) {
  int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i < n) r[i] += scale * x[i];
}

// Zero rows >= valid in a [T, D] tensor.
__global__ void MaskRowsKernel(float* x, int T, int D, int valid) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx >= T * D) return;
  if (idx / D >= valid) x[idx] = 0.0f;
}

// ---- GEMM-decomposed relative-position attention helpers ----
// Layouts: q/k/v are [T,H,Dk] interleaved; p is [P,H,Dk]; head-major outputs
// are contiguous [H,T,Dk] (or [H,P,Dk], [H,Dk,T]) for batched SGEMM.

// out[h,t,d] = q[t,h,d] + (bias ? bias[h,d] : 0)
__global__ void GatherAddHeadMajorKernel(const float* __restrict__ q,
                                         const float* __restrict__ bias,
                                         float* __restrict__ out, int T, int H,
                                         int Dk) {
  long n = static_cast<long>(blockIdx.x) * blockDim.x + threadIdx.x;
  long total = static_cast<long>(H) * T * Dk;
  if (n >= total) return;
  int d = static_cast<int>(n % Dk);
  int t = static_cast<int>((n / Dk) % T);
  int h = static_cast<int>(n / (static_cast<long>(Dk) * T));
  float b = bias ? bias[h * Dk + d] : 0.0f;
  out[n] = q[(static_cast<size_t>(t) * H + h) * Dk + d] + b;
}

// out[h,m,d] = p[m,h,d]
__global__ void GatherPHeadMajorKernel(const float* __restrict__ p,
                                       float* __restrict__ out, int P, int H,
                                       int Dk) {
  long n = static_cast<long>(blockIdx.x) * blockDim.x + threadIdx.x;
  long total = static_cast<long>(H) * P * Dk;
  if (n >= total) return;
  int d = static_cast<int>(n % Dk);
  int m = static_cast<int>((n / Dk) % P);
  int h = static_cast<int>(n / (static_cast<long>(Dk) * P));
  out[n] = p[(static_cast<size_t>(m) * H + h) * Dk + d];
}

// out[h,d,t] = v[t,h,d]  (V transposed per head for the ctx GEMM)
__global__ void GatherVtHeadMajorKernel(const float* __restrict__ v,
                                        float* __restrict__ out, int T, int H,
                                        int Dk) {
  long n = static_cast<long>(blockIdx.x) * blockDim.x + threadIdx.x;
  long total = static_cast<long>(H) * Dk * T;
  if (n >= total) return;
  int t = static_cast<int>(n % T);
  int d = static_cast<int>((n / T) % Dk);
  int h = static_cast<int>(n / (static_cast<long>(T) * Dk));
  out[n] = v[(static_cast<size_t>(t) * H + h) * Dk + d];
}

// scores[h,i,j] = (AC[h,i,j] + rel_shift(BD)[h,i,j]) * scale, masked at j>=valid.
// rel_shift: idx = T + i*(2T-1) + j; ip = idx/(2T); c2 = idx%(2T);
// bd = (c2 != 0) ? BD[h, ip, c2-1] : 0.
__global__ void RelShiftScoresKernel(const float* __restrict__ ac,
                                     const float* __restrict__ bd,
                                     float* __restrict__ scores, int T, int H,
                                     int valid, float scale) {
  long n = static_cast<long>(blockIdx.x) * blockDim.x + threadIdx.x;
  long total = static_cast<long>(H) * T * T;
  if (n >= total) return;
  int j = static_cast<int>(n % T);
  int i = static_cast<int>((n / T) % T);
  int h = static_cast<int>(n / (static_cast<long>(T) * T));
  if (j >= valid) {
    scores[n] = -1e30f;
    return;
  }
  int P = 2 * T - 1;
  float acv = ac[n];
  long idx = static_cast<long>(T) + static_cast<long>(i) * P + j;
  int ip = static_cast<int>(idx / (2 * T));
  int c2 = static_cast<int>(idx % (2 * T));
  float bdv = 0.0f;
  if (c2 != 0) {
    int m = c2 - 1;
    bdv = bd[(static_cast<size_t>(h) * T + ip) * P + m];
  }
  scores[n] = (acv + bdv) * scale;
}

// Row softmax over j for each (h,i); writes normalized probabilities in place.
__global__ void AttnSoftmaxKernel(float* __restrict__ scores, int T, int valid) {
  int row = blockIdx.x;  // 0..H*T-1, one (h,i) per block
  float* s = scores + static_cast<size_t>(row) * T;
  __shared__ float red[kThreads];
  float lmax = -1e30f;
  for (int j = threadIdx.x; j < T; j += blockDim.x) lmax = fmaxf(lmax, s[j]);
  red[threadIdx.x] = lmax;
  __syncthreads();
  for (int st = blockDim.x / 2; st > 0; st >>= 1) {
    if (threadIdx.x < st)
      red[threadIdx.x] = fmaxf(red[threadIdx.x], red[threadIdx.x + st]);
    __syncthreads();
  }
  __shared__ float s_max, s_sum;
  if (threadIdx.x == 0) s_max = red[0];
  __syncthreads();
  float mx = s_max;
  float lsum = 0.0f;
  for (int j = threadIdx.x; j < T; j += blockDim.x) {
    float e = __expf(s[j] - mx);
    s[j] = e;
    lsum += e;
  }
  red[threadIdx.x] = lsum;
  __syncthreads();
  for (int st = blockDim.x / 2; st > 0; st >>= 1) {
    if (threadIdx.x < st) red[threadIdx.x] += red[threadIdx.x + st];
    __syncthreads();
  }
  if (threadIdx.x == 0) s_sum = red[0];
  __syncthreads();
  float inv = 1.0f / s_sum;
  for (int j = threadIdx.x; j < T; j += blockDim.x) s[j] *= inv;
}

// cp[t,h,d] = ctxh[h,t,d]  (scatter head-major context back to interleaved)
__global__ void ScatterHeadMajorKernel(const float* __restrict__ ctxh,
                                       float* __restrict__ cp, int T, int H,
                                       int Dk) {
  long n = static_cast<long>(blockIdx.x) * blockDim.x + threadIdx.x;
  long total = static_cast<long>(H) * T * Dk;
  if (n >= total) return;
  int d = static_cast<int>(n % Dk);
  int t = static_cast<int>((n / Dk) % T);
  int h = static_cast<int>(n / (static_cast<long>(Dk) * T));
  cp[(static_cast<size_t>(t) * H + h) * Dk + d] = ctxh[n];
}

// GLU over channel: out[t,c] = a[t,c]*sigmoid(b[t,c]); in is [T, 2C].
__global__ void GluKernel(const float* in, float* out, int T, int C) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx >= T * C) return;
  int c = idx % C, t = idx / C;
  float a = in[static_cast<size_t>(t) * 2 * C + c];
  float b = in[static_cast<size_t>(t) * 2 * C + C + c];
  out[idx] = a * (1.0f / (1.0f + __expf(-b)));
}

// Depthwise conv1d over time, kernel K, padding (K-1)/2, groups=C. in [T,C].
__global__ void DepthwiseConvKernel(const float* __restrict__ in,
                                    const float* __restrict__ w,   // [C,1,K]
                                    const float* __restrict__ bias,
                                    float* __restrict__ out, int T, int C, int K) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx >= T * C) return;
  int c = idx % C, t = idx / C;
  int pad = (K - 1) / 2;
  float acc = bias ? bias[c] : 0.0f;
  const float* wc = w + c * K;
  for (int kk = 0; kk < K; ++kk) {
    int ti = t + kk - pad;
    if (ti >= 0 && ti < T) acc += in[static_cast<size_t>(ti) * C + c] * wc[kk];
  }
  out[idx] = acc;
}

// BatchNorm1d eval + SiLU.  in [T,C].
__global__ void BatchNormSiluKernel(float* x, const float* mean, const float* var,
                                    const float* gamma, const float* beta, int T,
                                    int C, float eps) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx >= T * C) return;
  int c = idx % C;
  float y = (x[idx] - mean[c]) * rsqrtf(var[c] + eps) * gamma[c] + beta[c];
  x[idx] = y / (1.0f + __expf(-y));  // SiLU
}

}  // namespace

ConformerLayer::ConformerLayer(int d_model, int n_heads, int d_ff, int conv_kernel)
    : d_model_(d_model),
      n_heads_(n_heads),
      d_ff_(d_ff),
      d_k_(d_model / n_heads),
      conv_kernel_(conv_kernel) {}

const float* ConformerLayer::W(const std::string& name) const {
  auto it = w_.find(name);
  if (it == w_.end()) throw std::runtime_error("missing weight " + name);
  return static_cast<const float*>(it->second->data());
}

void ConformerLayer::LoadWeights(const io::SafeTensorReader& reader,
                                 const std::string& prefix) {
  static const char* names[] = {
      "norm_feed_forward1.weight", "norm_feed_forward1.bias",
      "feed_forward1.linear1.weight", "feed_forward1.linear1.bias",
      "feed_forward1.linear2.weight", "feed_forward1.linear2.bias",
      "norm_self_att.weight", "norm_self_att.bias",
      "self_attn.linear_q.weight", "self_attn.linear_q.bias",
      "self_attn.linear_k.weight", "self_attn.linear_k.bias",
      "self_attn.linear_v.weight", "self_attn.linear_v.bias",
      "self_attn.linear_out.weight", "self_attn.linear_out.bias",
      "self_attn.linear_pos.weight", "self_attn.pos_bias_u",
      "self_attn.pos_bias_v", "norm_conv.weight", "norm_conv.bias",
      "conv.pointwise_conv1.weight", "conv.pointwise_conv1.bias",
      "conv.depthwise_conv.weight", "conv.depthwise_conv.bias",
      "conv.batch_norm.weight", "conv.batch_norm.bias",
      "conv.batch_norm.running_mean", "conv.batch_norm.running_var",
      "conv.pointwise_conv2.weight", "conv.pointwise_conv2.bias",
      "norm_feed_forward2.weight", "norm_feed_forward2.bias",
      "feed_forward2.linear1.weight", "feed_forward2.linear1.bias",
      "feed_forward2.linear2.weight", "feed_forward2.linear2.bias",
      "norm_out.weight", "norm_out.bias"};
  for (const char* n : names) {
    std::string full = prefix + "." + n;
    const auto& meta = reader.GetMetadata(full);
    auto buf = std::make_unique<UnifiedBuffer>(meta.data_size);
    reader.ReadWeight(full, buf->data(), meta.data_size);
    w_[n] = std::move(buf);
  }
}

std::vector<float> ConformerLayer::BuildPosEmb(int T, int d_model) {
  int P = 2 * T - 1;
  std::vector<float> pe(static_cast<size_t>(P) * d_model);
  // positions from (T-1) down to -(T-1)
  for (int idx = 0; idx < P; ++idx) {
    float pos = static_cast<float>(T - 1 - idx);
    for (int i = 0; i < d_model; i += 2) {
      float div = std::exp(-(std::log(10000.0f)) * static_cast<float>(i) / d_model);
      pe[static_cast<size_t>(idx) * d_model + i] = std::sin(pos * div);
      if (i + 1 < d_model)
        pe[static_cast<size_t>(idx) * d_model + i + 1] = std::cos(pos * div);
    }
  }
  return pe;
}

void ConformerLayer::Scratch::Ensure(int T, int D, int Dff, int H) {
  if (T <= cap_T) return;
  const size_t td = static_cast<size_t>(T) * D * sizeof(float);
  const size_t tff = static_cast<size_t>(T) * Dff * sizeof(float);
  const size_t pd = static_cast<size_t>(2 * T - 1) * D * sizeof(float);
  const size_t t2d = static_cast<size_t>(T) * 2 * D * sizeof(float);
  const int P = 2 * T - 1;
  const size_t htt = static_cast<size_t>(H) * T * T * sizeof(float);
  const size_t htp = static_cast<size_t>(H) * T * P * sizeof(float);
  ln = gpu::UnifiedBuffer(td);
  ff = gpu::UnifiedBuffer(tff);
  tmp = gpu::UnifiedBuffer(td);
  qb = gpu::UnifiedBuffer(td);
  kb = gpu::UnifiedBuffer(td);
  vb = gpu::UnifiedBuffer(td);
  pb = gpu::UnifiedBuffer(pd);
  cb = gpu::UnifiedBuffer(td);
  pw1 = gpu::UnifiedBuffer(t2d);
  glu = gpu::UnifiedBuffer(td);
  dw = gpu::UnifiedBuffer(td);
  // Head-major attention scratch.
  qbu = gpu::UnifiedBuffer(td);
  qbv = gpu::UnifiedBuffer(td);
  kh = gpu::UnifiedBuffer(td);
  vt = gpu::UnifiedBuffer(td);
  ph = gpu::UnifiedBuffer(pd);
  ac = gpu::UnifiedBuffer(htt);
  bd = gpu::UnifiedBuffer(htp);
  sc = gpu::UnifiedBuffer(htt);
  cap_T = T;
}

void ConformerLayer::Forward(float* x, int T, int valid_len, const float* pos_emb,
                              cudaStream_t stream) {
  const int D = d_model_, H = n_heads_, Dk = d_k_, Dff = d_ff_;
  const float eps = 1e-5f;
  size_t TD = static_cast<size_t>(T) * D;
  scr_.Ensure(T, D, Dff, H);
  float* lnp = static_cast<float*>(scr_.ln.data());
  float* ffp = static_cast<float*>(scr_.ff.data());
  float* tmpp = static_cast<float*>(scr_.tmp.data());

  auto LN = [&](const float* in, const float* g, const float* b, float* out) {
    LayerNormKernel<<<T, kThreads, 0, stream>>>(in, g, b, out, T, D, eps);
  };

  // ---- FFN1 ----
  LN(x, W("norm_feed_forward1.weight"), W("norm_feed_forward1.bias"), lnp);
  LaunchLinear(lnp, W("feed_forward1.linear1.weight"),
                                               W("feed_forward1.linear1.bias"), ffp,
                                               T, D, Dff, 1, stream);
  LaunchLinear(ffp, W("feed_forward1.linear2.weight"),
                                             W("feed_forward1.linear2.bias"), tmpp,
                                             T, Dff, D, 0, stream);
  AddScaledKernel<<<Blocks(T * D), kThreads, 0, stream>>>(x, tmpp, 0.5f, T * D);

  // ---- Self-attention ----
  LN(x, W("norm_self_att.weight"), W("norm_self_att.bias"), lnp);
  int P = 2 * T - 1;
  float* qp = static_cast<float*>(scr_.qb.data());
  float* kp = static_cast<float*>(scr_.kb.data());
  float* vp = static_cast<float*>(scr_.vb.data());
  float* pp = static_cast<float*>(scr_.pb.data());
  float* cp = static_cast<float*>(scr_.cb.data());
  LaunchLinear(lnp, W("self_attn.linear_q.weight"),
                                             W("self_attn.linear_q.bias"), qp, T, D, D, 0, stream);
  LaunchLinear(lnp, W("self_attn.linear_k.weight"),
                                             W("self_attn.linear_k.bias"), kp, T, D, D, 0, stream);
  LaunchLinear(lnp, W("self_attn.linear_v.weight"),
                                             W("self_attn.linear_v.bias"), vp, T, D, D, 0, stream);
  LaunchLinear(pos_emb, W("self_attn.linear_pos.weight"),
                                             nullptr, pp, P, D, D, 0, stream);
  float scale = 1.0f / std::sqrt(static_cast<float>(Dk));

  // ---- GEMM-decomposed relative-position attention ----
  // Head-major operands for batched SGEMM.
  float* qbu = static_cast<float*>(scr_.qbu.data());
  float* qbv = static_cast<float*>(scr_.qbv.data());
  float* kh = static_cast<float*>(scr_.kh.data());
  float* vt = static_cast<float*>(scr_.vt.data());
  float* ph = static_cast<float*>(scr_.ph.data());
  float* acm = static_cast<float*>(scr_.ac.data());
  float* bdm = static_cast<float*>(scr_.bd.data());
  float* scm = static_cast<float*>(scr_.sc.data());
  const float* buw = W("self_attn.pos_bias_u");
  const float* bvw = W("self_attn.pos_bias_v");

  GatherAddHeadMajorKernel<<<Blocks(H * T * Dk), kThreads, 0, stream>>>(qp, buw, qbu, T, H, Dk);
  GatherAddHeadMajorKernel<<<Blocks(H * T * Dk), kThreads, 0, stream>>>(qp, bvw, qbv, T, H, Dk);
  GatherAddHeadMajorKernel<<<Blocks(H * T * Dk), kThreads, 0, stream>>>(kp, nullptr, kh, T, H, Dk);
  GatherVtHeadMajorKernel<<<Blocks(H * Dk * T), kThreads, 0, stream>>>(vp, vt, T, H, Dk);
  GatherPHeadMajorKernel<<<Blocks(H * P * Dk), kThreads, 0, stream>>>(pp, ph, P, H, Dk);

  // AC[h,i,j] = (q_i+bu)·k_j ; BD[h,i,m] = (q_i+bv)·p_m
  gemm::LaunchSgemmBatched(qbu, kh, acm, T, Dk, T, 0, H,
                            static_cast<long>(T) * Dk, static_cast<long>(T) * Dk,
                            static_cast<long>(T) * T, stream);
  gemm::LaunchSgemmBatched(qbv, ph, bdm, T, Dk, P, 0, H,
                            static_cast<long>(T) * Dk, static_cast<long>(P) * Dk,
                            static_cast<long>(T) * P, stream);
  RelShiftScoresKernel<<<Blocks(H * T * T), kThreads, 0, stream>>>(acm, bdm, scm, T, H,
                                                         valid_len, scale);
  AttnSoftmaxKernel<<<H * T, kThreads, 0, stream>>>(scm, T, valid_len);
  // ctx[h,i,d] = sum_j scores[h,i,j] * v[j,d]  (vt is [H,Dk,T] => W^T form)
  gemm::LaunchSgemmBatched(scm, vt, acm, T, T, Dk, 0, H, static_cast<long>(T) * T,
                            static_cast<long>(Dk) * T, static_cast<long>(T) * Dk, stream);
  ScatterHeadMajorKernel<<<Blocks(H * T * Dk), kThreads, 0, stream>>>(acm, cp, T, H, Dk);
  // linear_out on context [T,D] -> tmp
  LaunchLinear(cp, W("self_attn.linear_out.weight"),
                                             W("self_attn.linear_out.bias"), tmpp,
                                             T, D, D, 0, stream);
  AddScaledKernel<<<Blocks(T * D), kThreads, 0, stream>>>(x, tmpp, 1.0f, T * D);

  // ---- Conv module ----
  LN(x, W("norm_conv.weight"), W("norm_conv.bias"), lnp);
  float* pw1p = static_cast<float*>(scr_.pw1.data());
  float* glup = static_cast<float*>(scr_.glu.data());
  float* dwp = static_cast<float*>(scr_.dw.data());
  LaunchLinear(lnp,
      W("conv.pointwise_conv1.weight"), W("conv.pointwise_conv1.bias"), pw1p, T, D, 2 * D, 0, stream);
  GluKernel<<<Blocks(T * D), kThreads, 0, stream>>>(pw1p, glup, T, D);
  MaskRowsKernel<<<Blocks(T * D), kThreads, 0, stream>>>(glup, T, D, valid_len);
  DepthwiseConvKernel<<<Blocks(T * D), kThreads, 0, stream>>>(glup,
      W("conv.depthwise_conv.weight"), W("conv.depthwise_conv.bias"), dwp, T, D, conv_kernel_);
  BatchNormSiluKernel<<<Blocks(T * D), kThreads, 0, stream>>>(dwp, W("conv.batch_norm.running_mean"),
      W("conv.batch_norm.running_var"), W("conv.batch_norm.weight"),
      W("conv.batch_norm.bias"), T, D, eps);
  LaunchLinear(dwp,
      W("conv.pointwise_conv2.weight"), W("conv.pointwise_conv2.bias"), tmpp, T, D, D, 0, stream);
  AddScaledKernel<<<Blocks(T * D), kThreads, 0, stream>>>(x, tmpp, 1.0f, T * D);

  // ---- FFN2 ----
  LN(x, W("norm_feed_forward2.weight"), W("norm_feed_forward2.bias"), lnp);
  LaunchLinear(lnp, W("feed_forward2.linear1.weight"),
                                               W("feed_forward2.linear1.bias"), ffp,
                                               T, D, Dff, 1, stream);
  LaunchLinear(ffp, W("feed_forward2.linear2.weight"),
                                             W("feed_forward2.linear2.bias"), tmpp,
                                             T, Dff, D, 0, stream);
  AddScaledKernel<<<Blocks(T * D), kThreads, 0, stream>>>(x, tmpp, 0.5f, T * D);

  // ---- norm_out ----
  LN(x, W("norm_out.weight"), W("norm_out.bias"), lnp);
  CUDA_CHECK(cudaMemcpyAsync(x, lnp, TD * sizeof(float), cudaMemcpyDeviceToDevice, stream));
}

}  // namespace model
}  // namespace orator
