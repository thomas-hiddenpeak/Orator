#include "model/asr_ops.cuh"

#include <cuda_runtime.h>

#include "gpu/memory.h"

namespace orator {
namespace model {
namespace asr_ops {

using ::orator::gpu::CheckCudaError;

namespace {

constexpr int kThreads = 256;
inline int Blocks(int n) { return (n + kThreads - 1) / kThreads; }

// ---------------------------------------------------------------------------
// RMSNorm: one block per row, block-parallel reduction over the feature dim.
// ---------------------------------------------------------------------------
__global__ void RmsNormKernel(const float* __restrict__ x,
                              const float* __restrict__ w,
                              float* __restrict__ out, int rows, int dim,
                              float eps) {
  const int r = blockIdx.x;
  if (r >= rows) return;
  const float* xr = x + static_cast<size_t>(r) * dim;
  float* yr = out + static_cast<size_t>(r) * dim;

  __shared__ float red[kThreads];
  __shared__ float s_inv;

  float local = 0.0f;
  for (int d = threadIdx.x; d < dim; d += blockDim.x) {
    const float v = xr[d];
    local += v * v;
  }
  red[threadIdx.x] = local;
  __syncthreads();
  for (int s = blockDim.x / 2; s > 0; s >>= 1) {
    if (threadIdx.x < s) red[threadIdx.x] += red[threadIdx.x + s];
    __syncthreads();
  }
  if (threadIdx.x == 0) s_inv = rsqrtf(red[0] / dim + eps);
  __syncthreads();

  const float inv = s_inv;
  for (int d = threadIdx.x; d < dim; d += blockDim.x) {
    yr[d] = xr[d] * inv * w[d];
  }
}

// ---------------------------------------------------------------------------
// Interleaved RoPE: one block per (token, head); threads cover dim/2 pairs.
// Precise sinf/cosf (not the fast intrinsics) keep angular error minimal.
// ---------------------------------------------------------------------------
__global__ void RopeKernel(float* __restrict__ x, const int* __restrict__ pos,
                           int T, int H, int Dh, float base) {
  const int t = blockIdx.x;
  const int h = blockIdx.y;
  if (t >= T || h >= H) return;
  float* xr = x + ((static_cast<size_t>(t) * H + h) * Dh);
  const int half = Dh / 2;
  const float p = static_cast<float>(pos[t]);
  for (int i = threadIdx.x; i < half; i += blockDim.x) {
    const float inv_freq = powf(base, -2.0f * i / Dh);
    const float angle = p * inv_freq;
    const float c = cosf(angle);
    const float s = sinf(angle);
    const float a = xr[2 * i];
    const float b = xr[2 * i + 1];
    xr[2 * i] = a * c - b * s;
    xr[2 * i + 1] = a * s + b * c;
  }
}

// ---------------------------------------------------------------------------
// SwiGLU: out = SiLU(gate) * up. Precise expf for fidelity.
// ---------------------------------------------------------------------------
__global__ void SwiGLUKernel(const float* __restrict__ g,
                             const float* __restrict__ u,
                             float* __restrict__ o, int n) {
  const int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i >= n) return;
  const float gv = g[i];
  o[i] = (gv / (1.0f + expf(-gv))) * u[i];
}

// ---------------------------------------------------------------------------
// rotate_half RoPE: one block per (token, head); threads cover half pairs.
// ---------------------------------------------------------------------------
__global__ void RopeHalfKernel(float* __restrict__ x, const int* __restrict__ pos,
                               int T, int H, int Dh, float base) {
  const int t = blockIdx.x;
  const int h = blockIdx.y;
  if (t >= T || h >= H) return;
  float* xr = x + ((static_cast<size_t>(t) * H + h) * Dh);
  const int half = Dh / 2;
  const float p = static_cast<float>(pos[t]);
  for (int j = threadIdx.x; j < half; j += blockDim.x) {
    const float inv_freq = powf(base, -2.0f * j / Dh);
    const float angle = p * inv_freq;
    const float c = cosf(angle);
    const float s = sinf(angle);
    const float a = xr[j];
    const float b = xr[j + half];
    xr[j] = a * c - b * s;
    xr[j + half] = b * c + a * s;
  }
}

// ---------------------------------------------------------------------------
// Grouped-query attention, flash-style: one WARP per (query token, query head).
// Each lane owns ELEMS = ceil(Dh/32) elements of the head dim, held in
// registers; the query row and the running weighted-sum accumulator never
// leave registers. The per-key score is a warp butterfly all-reduce
// (__shfl_xor) of each lane's partial dot product, so the inner key loop has
// no __syncthreads and no shared memory. The softmax is the numerically stable
// online (flash) recurrence. Precise expf keeps fidelity; the tree reduction
// is at least as accurate as a sequential sum.
//
// WARPS_PER_BLOCK warps are packed per block for occupancy; warp y handles
// global (token,head) pair = blockIdx.x * WARPS_PER_BLOCK + threadIdx.y.
// ---------------------------------------------------------------------------
constexpr int kWarp = 32;
constexpr int kWarpsPerBlock = 4;

template <int ELEMS>
__global__ void GqaAttnWarpKernel(const float* __restrict__ q,
                                  const float* __restrict__ k,
                                  const float* __restrict__ v,
                                  float* __restrict__ out, int T, int Hq,
                                  int Hkv, int Dh, float scale, int causal,
                                  int n_pairs) {
  const int lane = threadIdx.x;  // 0..31
  const int pair = blockIdx.x * blockDim.y + threadIdx.y;
  if (pair >= n_pairs) return;
  const int i = pair / Hq;  // query token
  const int h = pair % Hq;  // query head
  const int group = Hq / Hkv;
  const int kvh = h / group;

  const float* qrow = q + (static_cast<size_t>(i) * Hq + h) * Dh;
  float qreg[ELEMS];
#pragma unroll
  for (int e = 0; e < ELEMS; ++e) {
    const int d = lane + e * kWarp;
    qreg[e] = (d < Dh) ? qrow[d] : 0.0f;
  }

  float m = -1e30f;  // running max
  float l = 0.0f;    // running denominator
  float acc[ELEMS];
#pragma unroll
  for (int e = 0; e < ELEMS; ++e) acc[e] = 0.0f;

  const int jmax = causal ? i : (T - 1);
  for (int j = 0; j <= jmax; ++j) {
    const float* krow = k + (static_cast<size_t>(j) * Hkv + kvh) * Dh;
    const float* vrow = v + (static_cast<size_t>(j) * Hkv + kvh) * Dh;

    float partial = 0.0f;
#pragma unroll
    for (int e = 0; e < ELEMS; ++e) {
      const int d = lane + e * kWarp;
      if (d < Dh) partial += qreg[e] * krow[d];
    }
    // Butterfly all-reduce: every lane ends with the full dot product.
#pragma unroll
    for (int mask = kWarp / 2; mask > 0; mask >>= 1)
      partial += __shfl_xor_sync(0xffffffffu, partial, mask);
    const float score = partial * scale;

    const float new_m = fmaxf(m, score);
    const float corr = expf(m - new_m);
    const float p = expf(score - new_m);
    l = l * corr + p;
#pragma unroll
    for (int e = 0; e < ELEMS; ++e) {
      const int d = lane + e * kWarp;
      const float vv = (d < Dh) ? vrow[d] : 0.0f;
      acc[e] = acc[e] * corr + p * vv;
    }
    m = new_m;
  }

  const float invl = (l > 0.0f) ? 1.0f / l : 0.0f;
  float* orow = out + (static_cast<size_t>(i) * Hq + h) * Dh;
#pragma unroll
  for (int e = 0; e < ELEMS; ++e) {
    const int d = lane + e * kWarp;
    if (d < Dh) orow[d] = acc[e] * invl;
  }
}

}  // namespace

void RmsNorm(const float* x, const float* weight, float* out, int rows, int dim,
             float eps, cudaStream_t stream) {
  if (rows <= 0 || dim <= 0) return;
  RmsNormKernel<<<rows, kThreads, 0, stream>>>(x, weight, out, rows, dim, eps);
  CheckCudaError(cudaGetLastError(), __FILE__, __LINE__);
}

void RopeInterleaved(float* x, const int* positions, int n_tokens, int n_heads,
                     int head_dim, float theta_base) {
  if (n_tokens <= 0 || n_heads <= 0 || head_dim <= 0) return;
  dim3 grid(n_tokens, n_heads);
  const int threads = head_dim / 2 < kThreads ? (head_dim / 2 > 0 ? head_dim / 2 : 1)
                                              : kThreads;
  RopeKernel<<<grid, threads>>>(x, positions, n_tokens, n_heads, head_dim,
                                theta_base);
  CheckCudaError(cudaGetLastError(), __FILE__, __LINE__);
}

void SwiGLU(const float* gate, const float* up, float* out, int n,
            cudaStream_t stream) {
  if (n <= 0) return;
  SwiGLUKernel<<<Blocks(n), kThreads, 0, stream>>>(gate, up, out, n);
  CheckCudaError(cudaGetLastError(), __FILE__, __LINE__);
}

void RopeHalf(float* x, const int* positions, int n_tokens, int n_heads,
              int head_dim, float theta_base, cudaStream_t stream) {
  if (n_tokens <= 0 || n_heads <= 0 || head_dim <= 0) return;
  dim3 grid(n_tokens, n_heads);
  const int threads = head_dim / 2 < kThreads ? (head_dim / 2 > 0 ? head_dim / 2 : 1)
                                              : kThreads;
  RopeHalfKernel<<<grid, threads, 0, stream>>>(x, positions, n_tokens, n_heads, head_dim,
                                    theta_base);
  CheckCudaError(cudaGetLastError(), __FILE__, __LINE__);
}

namespace {

// Launches the one-warp-per-query kernel for a compile-time ELEMS (= Dh/32).
template <int ELEMS>
inline void LaunchWarp(const float* q, const float* k, const float* v,
                       float* out, int T, int Hq, int Hkv, int Dh, float scale,
                       int c, int n_pairs) {
  dim3 block(kWarp, kWarpsPerBlock);
  const int grid = (n_pairs + kWarpsPerBlock - 1) / kWarpsPerBlock;
  GqaAttnWarpKernel<ELEMS><<<grid, block>>>(q, k, v, out, T, Hq, Hkv, Dh, scale,
                                            c, n_pairs);
}

}  // namespace

void GqaAttention(const float* q, const float* k, const float* v, float* out,
                  int T, int Hq, int Hkv, int Dh, float scale, bool causal) {
  if (T <= 0 || Hq <= 0 || Hkv <= 0 || Dh <= 0) return;
  const int c = causal ? 1 : 0;
  const int n_pairs = T * Hq;
  const int elems = (Dh + kWarp - 1) / kWarp;  // head-dim elements per lane

  // One warp per (query token, query head). Flash-style online softmax with a
  // warp-butterfly dot product; K/V (a few MB here) is served from L2, so no
  // shared-memory tiling is used -- a tiled variant was measured slower in this
  // regime. ELEMS is rounded up to a supported register-array width spanning Dh
  // (Qwen3-ASR uses Dh=128 -> ELEMS=4).
  const int e = elems <= 1 ? 1 : elems <= 2 ? 2 : elems <= 3 ? 3
                : elems <= 4 ? 4 : elems <= 8 ? 8 : elems <= 16 ? 16 : 32;
  switch (e) {
    case 1: LaunchWarp<1>(q, k, v, out, T, Hq, Hkv, Dh, scale, c, n_pairs); break;
    case 2: LaunchWarp<2>(q, k, v, out, T, Hq, Hkv, Dh, scale, c, n_pairs); break;
    case 3: LaunchWarp<3>(q, k, v, out, T, Hq, Hkv, Dh, scale, c, n_pairs); break;
    case 4: LaunchWarp<4>(q, k, v, out, T, Hq, Hkv, Dh, scale, c, n_pairs); break;
    case 8: LaunchWarp<8>(q, k, v, out, T, Hq, Hkv, Dh, scale, c, n_pairs); break;
    case 16: LaunchWarp<16>(q, k, v, out, T, Hq, Hkv, Dh, scale, c, n_pairs); break;
    default: LaunchWarp<32>(q, k, v, out, T, Hq, Hkv, Dh, scale, c, n_pairs); break;
  }
  CheckCudaError(cudaGetLastError(), __FILE__, __LINE__);
}

namespace {
// Single-block argmax: each thread scans a strided slice, then a shared-memory
// reduction picks the global max index. Up to two indices may be banned.
__global__ void ArgmaxBannedKernel(const float* __restrict__ x, int n, int ban0,
                                   int ban1, int* __restrict__ out_idx) {
  __shared__ float sval[256];
  __shared__ int sidx[256];
  float best = -1e30f;
  int bi = 0;
  for (int i = threadIdx.x; i < n; i += blockDim.x) {
    if (i == ban0 || i == ban1) continue;
    const float v = x[i];
    if (v > best) { best = v; bi = i; }
  }
  sval[threadIdx.x] = best;
  sidx[threadIdx.x] = bi;
  __syncthreads();
  for (int s = blockDim.x / 2; s > 0; s >>= 1) {
    if (threadIdx.x < s && sval[threadIdx.x + s] > sval[threadIdx.x]) {
      sval[threadIdx.x] = sval[threadIdx.x + s];
      sidx[threadIdx.x] = sidx[threadIdx.x + s];
    }
    __syncthreads();
  }
  if (threadIdx.x == 0) *out_idx = sidx[0];
}
}  // namespace

void ArgmaxBanned(const float* logits, int n, int ban0, int ban1, int* out_idx,
                  cudaStream_t stream) {
  if (n <= 0) return;
  ArgmaxBannedKernel<<<1, 256, 0, stream>>>(logits, n, ban0, ban1, out_idx);
  CheckCudaError(cudaGetLastError(), __FILE__, __LINE__);
}

}  // namespace asr_ops
}  // namespace model
}  // namespace orator
