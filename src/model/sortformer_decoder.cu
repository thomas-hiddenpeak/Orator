#include "model/sortformer_decoder.h"

#include <cuda_runtime.h>

#include <cmath>
#include <stdexcept>

#include "model/gemm.cuh"

namespace orator {
namespace model {

using ::orator::gpu::CheckCudaError;
using ::orator::gpu::DeviceBuffer;
using ::orator::gpu::UnifiedBuffer;

namespace {

constexpr int kThreads = 256;
inline int Blocks(int n) { return (n + kThreads - 1) / kThreads; }

inline void LaunchLinear(const float* in, const float* W, const float* bias,
                         float* out, int M, int K, int N, int act,
                         cudaStream_t stream) {
  // Local act codes: 0 none, 1 ReLU, 2 sigmoid. Remap to universal SGEMM
  // codes: 0 none, 2 ReLU, 3 sigmoid.
  int gact = (act == 1) ? 2 : (act == 2) ? 3 : 0;
  gemm::LaunchSgemm(in, W, bias, out, M, K, N, gact, stream);
}

__global__ void ReluKernel(const float* in, float* out, int n) {
  int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i < n) out[i] = fmaxf(in[i], 0.0f);
}

__global__ void LayerNormKernel(const float* __restrict__ in,
                                const float* __restrict__ gamma,
                                const float* __restrict__ beta,
                                float* __restrict__ out, int M, int D,
                                float eps) {
  int m = blockIdx.x;
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
  float mean = s_mean, lv = 0.0f;
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

__global__ void AddKernel(float* r, const float* x, int n) {
  int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i < n) r[i] += x[i];
}

__global__ void MaskRowsKernel(float* x, int T, int D, int valid) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx >= T * D) return;
  if (idx / D >= valid) x[idx] = 0.0f;
}

// ---- GEMM-decomposed standard MHA helpers (head-major batched SGEMM) ----
// out[h,t,d] = in[t,h,d]   (q/k reorder to contiguous [H,T,Dk] per head)
__global__ void GatherHeadMajorKernel(const float* __restrict__ in,
                                      float* __restrict__ out, int T, int H,
                                      int Dk) {
  long n = static_cast<long>(blockIdx.x) * blockDim.x + threadIdx.x;
  long total = static_cast<long>(H) * T * Dk;
  if (n >= total) return;
  int d = static_cast<int>(n % Dk);
  int t = static_cast<int>((n / Dk) % T);
  int h = static_cast<int>(n / (static_cast<long>(Dk) * T));
  out[n] = in[(static_cast<size_t>(t) * H + h) * Dk + d];
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

// scores[h,i,j] = raw[h,i,j] * scale, masked at j>=valid.
__global__ void ScaleMaskScoresKernel(float* __restrict__ scores, int T, int H,
                                      int valid, float scale) {
  long n = static_cast<long>(blockIdx.x) * blockDim.x + threadIdx.x;
  long total = static_cast<long>(H) * T * T;
  if (n >= total) return;
  int j = static_cast<int>(n % T);
  scores[n] = (j >= valid) ? -1e30f : scores[n] * scale;
}

// Row softmax over j for each (h,i); writes normalized probabilities in place.
__global__ void AttnSoftmaxKernel(float* __restrict__ scores, int T) {
  int row = blockIdx.x;  // one (h,i) per block
  float* s = scores + static_cast<size_t>(row) * T;
  __shared__ float red[kThreads];
  __shared__ float s_max, s_sum;
  float lmax = -1e30f;
  for (int j = threadIdx.x; j < T; j += blockDim.x) lmax = fmaxf(lmax, s[j]);
  red[threadIdx.x] = lmax;
  __syncthreads();
  for (int st = blockDim.x / 2; st > 0; st >>= 1) {
    if (threadIdx.x < st)
      red[threadIdx.x] = fmaxf(red[threadIdx.x], red[threadIdx.x + st]);
    __syncthreads();
  }
  if (threadIdx.x == 0) s_max = red[0];
  __syncthreads();
  float mx = s_max, lsum = 0.0f;
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

// cp[t,h,d] = ctxh[h,t,d]
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

}  // namespace

SortformerDecoder::SortformerDecoder(int d_enc, int d_model, int n_heads,
                                     int d_ff, int n_layers, int n_spk)
    : d_enc_(d_enc),
      d_model_(d_model),
      n_heads_(n_heads),
      d_ff_(d_ff),
      d_k_(d_model / n_heads),
      n_layers_(n_layers),
      n_spk_(n_spk) {}

const float* SortformerDecoder::W(const std::string& name) const {
  auto it = w_.find(name);
  if (it == w_.end()) throw std::runtime_error("missing weight " + name);
  return static_cast<const float*>(it->second->data());
}

void SortformerDecoder::LoadWeights(const io::SafeTensorReader& reader) {
  auto load = [&](const std::string& full) {
    const auto& meta = reader.GetMetadata(full);
    auto buf = std::make_unique<UnifiedBuffer>(meta.data_size);
    reader.ReadWeight(full, buf->data(), meta.data_size);
    w_[full] = std::move(buf);
  };
  load("sortformer_modules.encoder_proj.weight");
  load("sortformer_modules.encoder_proj.bias");
  load("sortformer_modules.first_hidden_to_hidden.weight");
  load("sortformer_modules.first_hidden_to_hidden.bias");
  load("sortformer_modules.single_hidden_to_spks.weight");
  load("sortformer_modules.single_hidden_to_spks.bias");
  for (int l = 0; l < n_layers_; ++l) {
    std::string p = "transformer_encoder.layers." + std::to_string(l) + ".";
    for (const char* s :
         {"first_sub_layer.query_net", "first_sub_layer.key_net",
          "first_sub_layer.value_net", "first_sub_layer.out_projection",
          "second_sub_layer.dense_in", "second_sub_layer.dense_out"}) {
      load(p + s + ".weight");
      load(p + s + ".bias");
    }
    for (const char* s : {"layer_norm_1", "layer_norm_2"}) {
      load(p + s + ".weight");
      load(p + s + ".bias");
    }
  }
}

void SortformerDecoder::Scratch::Ensure(int T, int D, int H, int Dff,
                                        int n_spk) {
  if (T <= cap_T) return;
  const size_t TD = static_cast<size_t>(T) * D * sizeof(float);
  const size_t TFF = static_cast<size_t>(T) * Dff * sizeof(float);
  const size_t HTT = static_cast<size_t>(H) * T * T * sizeof(float);
  const size_t TS = static_cast<size_t>(T) * n_spk * sizeof(float);
  xb = gpu::UnifiedBuffer(TD);
  lnb = gpu::UnifiedBuffer(TD);
  qb = gpu::UnifiedBuffer(TD);
  kb = gpu::UnifiedBuffer(TD);
  vb = gpu::UnifiedBuffer(TD);
  cb = gpu::UnifiedBuffer(TD);
  tb = gpu::UnifiedBuffer(TD);
  ffb = gpu::UnifiedBuffer(TFF);
  qhb = gpu::UnifiedBuffer(TD);
  khb = gpu::UnifiedBuffer(TD);
  vtb = gpu::UnifiedBuffer(TD);
  scb = gpu::UnifiedBuffer(HTT);
  predb = gpu::DeviceBuffer(TS);
  cap_T = T;
}

std::vector<float> SortformerDecoder::Forward(const float* conformer_out, int T,
                                              int valid_len,
                                              cudaStream_t stream) {
  const int De = d_enc_, D = d_model_, H = n_heads_, Dk = d_k_, Dff = d_ff_;
  const float eps = 1e-5f;

  // Reused persistent scratch (allocated once, grown when T increases).
  scr_.Ensure(T, D, H, Dff, n_spk_);
  float* x = static_cast<float*>(scr_.xb.data());
  // encoder_proj: 512 -> 192
  LaunchLinear(conformer_out, W("sortformer_modules.encoder_proj.weight"),
               W("sortformer_modules.encoder_proj.bias"), x, T, De, D, 0,
               stream);

  float* lnp = static_cast<float*>(scr_.lnb.data());
  float* qp = static_cast<float*>(scr_.qb.data());
  float* kp = static_cast<float*>(scr_.kb.data());
  float* vp = static_cast<float*>(scr_.vb.data());
  float* cp = static_cast<float*>(scr_.cb.data());
  float* tp = static_cast<float*>(scr_.tb.data());
  float* ffp = static_cast<float*>(scr_.ffb.data());
  float* qhp = static_cast<float*>(scr_.qhb.data());
  float* khp = static_cast<float*>(scr_.khb.data());
  float* vtp = static_cast<float*>(scr_.vtb.data());
  float* scp = static_cast<float*>(scr_.scb.data());

  // q/k pre-divided by sqrt(sqrt(Dk)) in NeMo -> scores scale == 1/sqrt(Dk).
  float attn_scale = 1.0f / std::sqrt(static_cast<float>(Dk));

  for (int l = 0; l < n_layers_; ++l) {
    std::string p = "transformer_encoder.layers." + std::to_string(l) + ".";
    LaunchLinear(x, W(p + "first_sub_layer.query_net.weight"),
                 W(p + "first_sub_layer.query_net.bias"), qp, T, D, D, 0,
                 stream);
    LaunchLinear(x, W(p + "first_sub_layer.key_net.weight"),
                 W(p + "first_sub_layer.key_net.bias"), kp, T, D, D, 0, stream);
    LaunchLinear(x, W(p + "first_sub_layer.value_net.weight"),
                 W(p + "first_sub_layer.value_net.bias"), vp, T, D, D, 0,
                 stream);
    dim3 grid(T, H);
    // GEMM-decomposed MHA: scores = Qh @ Kh^T, softmax, ctx = scores @ V.
    GatherHeadMajorKernel<<<Blocks(H * T * Dk), kThreads, 0, stream>>>(
        qp, qhp, T, H, Dk);
    GatherHeadMajorKernel<<<Blocks(H * T * Dk), kThreads, 0, stream>>>(
        kp, khp, T, H, Dk);
    GatherVtHeadMajorKernel<<<Blocks(H * Dk * T), kThreads, 0, stream>>>(
        vp, vtp, T, H, Dk);
    gemm::LaunchSgemmBatched(
        qhp, khp, scp, T, Dk, T, 0, H, static_cast<long>(T) * Dk,
        static_cast<long>(T) * Dk, static_cast<long>(T) * T, stream);
    ScaleMaskScoresKernel<<<Blocks(H * T * T), kThreads, 0, stream>>>(
        scp, T, H, valid_len, attn_scale);
    AttnSoftmaxKernel<<<H * T, kThreads, 0, stream>>>(scp, T);
    gemm::LaunchSgemmBatched(
        scp, vtp, qhp, T, T, Dk, 0, H, static_cast<long>(T) * T,
        static_cast<long>(Dk) * T, static_cast<long>(T) * Dk, stream);
    ScatterHeadMajorKernel<<<Blocks(H * T * Dk), kThreads, 0, stream>>>(
        qhp, cp, T, H, Dk);
    // out_projection -> tp, residual add to x, then LN1.
    LaunchLinear(cp, W(p + "first_sub_layer.out_projection.weight"),
                 W(p + "first_sub_layer.out_projection.bias"), tp, T, D, D, 0,
                 stream);
    AddKernel<<<Blocks(T * D), kThreads, 0, stream>>>(tp, x, T * D);
    LayerNormKernel<<<T, kThreads, 0, stream>>>(
        tp, W(p + "layer_norm_1.weight"), W(p + "layer_norm_1.bias"), x, T, D,
        eps);
    // FFN: dense_in(relu) -> dense_out, residual, LN2.
    LaunchLinear(x, W(p + "second_sub_layer.dense_in.weight"),
                 W(p + "second_sub_layer.dense_in.bias"), ffp, T, D, Dff, 1,
                 stream);
    LaunchLinear(ffp, W(p + "second_sub_layer.dense_out.weight"),
                 W(p + "second_sub_layer.dense_out.bias"), tp, T, Dff, D, 0,
                 stream);
    AddKernel<<<Blocks(T * D), kThreads, 0, stream>>>(tp, x, T * D);
    LayerNormKernel<<<T, kThreads, 0, stream>>>(
        tp, W(p + "layer_norm_2.weight"), W(p + "layer_norm_2.bias"), x, T, D,
        eps);
  }

  // Speaker head: relu -> first_hidden_to_hidden -> relu ->
  // single_hidden_to_spks
  // -> sigmoid -> *mask.
  ReluKernel<<<Blocks(T * D), kThreads, 0, stream>>>(x, lnp, T * D);
  LaunchLinear(lnp, W("sortformer_modules.first_hidden_to_hidden.weight"),
               W("sortformer_modules.first_hidden_to_hidden.bias"), tp, T, D, D,
               1, stream);
  // Spec 002 Phase 7 (T071): the per-frame sigmoid result is copied to the host
  // (cudaMemcpy DtoH below). It MUST be DEVICE memory, not managed: a host copy
  // from managed memory while the concurrent (lock-free) ASR pipeline runs a
  // kernel faults on Tegra (R1). Device memory does not page-migrate, so the
  // DtoH copy is safe regardless of concurrent kernels. GPU-only working
  // buffers above stay managed (only the GPU touches them).
  float* predp = static_cast<float*>(scr_.predb.data());
  LaunchLinear(tp, W("sortformer_modules.single_hidden_to_spks.weight"),
               W("sortformer_modules.single_hidden_to_spks.bias"), predp, T, D,
               n_spk_, 2, stream);
  MaskRowsKernel<<<Blocks(T * n_spk_), kThreads, 0, stream>>>(predp, T, n_spk_,
                                                              valid_len);
  CUDA_CHECK(cudaStreamSynchronize(stream));

  std::vector<float> preds(static_cast<size_t>(T) * n_spk_);
  CUDA_CHECK(cudaMemcpyAsync(preds.data(), predp, preds.size() * sizeof(float),
                             cudaMemcpyDeviceToHost, stream));
  CUDA_CHECK(cudaStreamSynchronize(stream));
  return preds;
}

}  // namespace model
}  // namespace orator
