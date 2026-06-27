#include "model/asr_gemm.h"

#include <cuda_bf16.h>

#include <cstdlib>
#include <stdexcept>

#include "gpu/memory.h"
#include "model/gemm.cuh"

namespace orator {
namespace model {
namespace asr_gemm {

using ::orator::gpu::CheckCudaError;

namespace {

constexpr int kThreads = 256;
inline int Blocks(long n) {
  return static_cast<int>((n + kThreads - 1) / kThreads);
}

// Per-THREAD bf16 cast scratch for the `in` operand. asr_gemm::Linear is called
// concurrently from independent pipeline threads (the ASR worker on asr_stream
// and the forced-alignment worker on the default stream) which, in the
// production lock-free concurrency mode, are NOT mutually excluded. A single
// shared scratch would race: Scratch()'s grow path (cudaFree + cudaMalloc)
// could free the buffer out from under another thread's queued GEMM
// (use-after-free on device memory). thread_local gives each pipeline thread
// its own scratch. (Per-thread buffers are reclaimed at process exit.)
thread_local uint16_t* g_scratch =
    nullptr;  // bf16 cast scratch for the `in` operand
thread_local long g_scratch_cap = 0;

uint16_t* Scratch(long n) {
  if (n > g_scratch_cap) {
    if (g_scratch) CUDA_CHECK(cudaFree(g_scratch));
    CheckCudaError(cudaMalloc(&g_scratch, sizeof(uint16_t) * n), __FILE__,
                   __LINE__);
    g_scratch_cap = n;
  }
  return g_scratch;
}

__global__ void F32ToBf16Kernel(const float* __restrict__ in,
                                uint16_t* __restrict__ out, long n) {
  const long i = static_cast<long>(blockIdx.x) * blockDim.x + threadIdx.x;
  if (i >= n) return;
  out[i] = __bfloat16_as_ushort(__float2bfloat16(in[i]));
}

__global__ void Bf16ToF32Kernel(const uint16_t* __restrict__ in,
                                float* __restrict__ out, long n) {
  const long i = static_cast<long>(blockIdx.x) * blockDim.x + threadIdx.x;
  if (i >= n) return;
  out[i] = __bfloat162float(__ushort_as_bfloat16(in[i]));
}

// Memory-bound bf16 GEMV for the M=1 decode step: out[N] = in[K] @ W[N,K]^T.
// One warp per output row; the 32 lanes stride across K so each step issues a
// coalesced 128-byte read of the row's contiguous bf16 weights (the access
// pattern cuBLAS's transposed M=1 path does NOT achieve). x is staged in shared
// memory and reused across the block's WARPS rows. half2-vectorized (K even).
constexpr int kGemvWarps = 8;
__global__ void GemvBf16Kernel(const __nv_bfloat16* __restrict__ in,
                               const __nv_bfloat16* __restrict__ W,
                               const float* __restrict__ bias,
                               float* __restrict__ out, int K, int N, int act) {
  extern __shared__ __nv_bfloat16 sx[];  // [K]
  const int tid = threadIdx.x;
  const int lane = tid & 31;
  const int warp = tid >> 5;
  for (int i = tid; i < K; i += blockDim.x) sx[i] = in[i];
  __syncthreads();

  const int row = blockIdx.x * kGemvWarps + warp;
  if (row >= N) return;

  const __nv_bfloat162* w2 =
      reinterpret_cast<const __nv_bfloat162*>(W + static_cast<size_t>(row) * K);
  const __nv_bfloat162* x2 = reinterpret_cast<const __nv_bfloat162*>(sx);
  const int K2 = K >> 1;
  float acc = 0.0f;
  for (int k = lane; k < K2; k += 32) {
    const float2 wf = __bfloat1622float2(w2[k]);
    const float2 xf = __bfloat1622float2(x2[k]);
    acc += wf.x * xf.x + wf.y * xf.y;
  }
#pragma unroll
  for (int m = 16; m > 0; m >>= 1) acc += __shfl_xor_sync(0xffffffffu, acc, m);
  if (lane == 0) {
    if (bias) acc += bias[row];
    if (act == 1)
      acc = 0.5f * acc * (1.0f + erff(acc * 0.70710678118654752440f));
    else if (act == 2)
      acc = fmaxf(acc, 0.0f);
    out[row] = acc;
  }
}

// 128-bit (float4 = 8 bf16) vectorized variant of GemvBf16Kernel. Each lane
// issues a single 16-byte load per step, so the 32-lane warp reads a 512-byte
// coalesced transaction -- higher memory-level parallelism (more bytes in
// flight per instruction, 4x fewer loop iterations) than the half2 path on the
// bandwidth-bound M=1 decode. Requires K % 8 == 0 (true for every Qwen3
// projection: 1024/2048/6144); the row offset row*K is then a multiple of 8, so
// each row's float4 stream is 16-byte aligned. x is staged in shared (16-byte
// aligned dynamic shared memory) and reused across the block's WARPS rows.
__global__ void GemvBf16Vec4Kernel(const __nv_bfloat16* __restrict__ in,
                                   const __nv_bfloat16* __restrict__ W,
                                   const float* __restrict__ bias,
                                   float* __restrict__ out, int K, int N,
                                   int act) {
  extern __shared__ __nv_bfloat16 sx[];  // [K]
  const int tid = threadIdx.x;
  const int lane = tid & 31;
  const int warp = tid >> 5;
  for (int i = tid; i < K; i += blockDim.x) sx[i] = in[i];
  __syncthreads();

  const int row = blockIdx.x * kGemvWarps + warp;
  if (row >= N) return;

  const float4* w4 =
      reinterpret_cast<const float4*>(W + static_cast<size_t>(row) * K);
  const float4* x4 = reinterpret_cast<const float4*>(sx);
  const int K8 = K >> 3;  // float4 (8 bf16) chunks
  union Pack {
    float4 v;
    __nv_bfloat162 h[4];
  };
  float acc = 0.0f;
  for (int k = lane; k < K8; k += 32) {
    Pack wp, xp;
    wp.v = w4[k];
    xp.v = x4[k];
#pragma unroll
    for (int j = 0; j < 4; ++j) {
      const float2 wf = __bfloat1622float2(wp.h[j]);
      const float2 xf = __bfloat1622float2(xp.h[j]);
      acc += wf.x * xf.x + wf.y * xf.y;
    }
  }
#pragma unroll
  for (int m = 16; m > 0; m >>= 1) acc += __shfl_xor_sync(0xffffffffu, acc, m);
  if (lane == 0) {
    if (bias) acc += bias[row];
    if (act == 1)
      acc = 0.5f * acc * (1.0f + erff(acc * 0.70710678118654752440f));
    else if (act == 2)
      acc = fmaxf(acc, 0.0f);
    out[row] = acc;
  }
}

// out[m,n] += bias[n]; act: 0 none, 1 GELU(exact erf), 2 ReLU.
// (Retained: the M>1 epilogue is now fused into the in-project bf16 GEMM; this
// kernel is no longer launched.)

}  // namespace

void F32ToBf16(const float* in, uint16_t* out, long n, cudaStream_t stream) {
  if (n <= 0) return;
  F32ToBf16Kernel<<<Blocks(n), kThreads, 0, stream>>>(in, out, n);
  CheckCudaError(cudaGetLastError(), __FILE__, __LINE__);
}

void Bf16ToF32(const uint16_t* in, float* out, long n, cudaStream_t stream) {
  if (n <= 0) return;
  Bf16ToF32Kernel<<<Blocks(n), kThreads, 0, stream>>>(in, out, n);
  CheckCudaError(cudaGetLastError(), __FILE__, __LINE__);
}

void Linear(const float* in_f32, const uint16_t* W_bf16, const float* bias_f32,
            float* out_f32, int M, int K, int N, int act, cudaStream_t stream) {
  if (M <= 0 || K <= 0 || N <= 0) return;
  // Cast the activation operand to BF16 (weights are already BF16).
  uint16_t* in_bf16 = Scratch(static_cast<long>(M) * K);
  F32ToBf16(in_f32, in_bf16, static_cast<long>(M) * K, stream);
  LinearPre(in_bf16, W_bf16, bias_f32, out_f32, M, K, N, act, stream);
}

void LinearPre(const uint16_t* in_bf16, const uint16_t* W_bf16,
               const float* bias_f32, float* out_f32, int M, int K, int N,
               int act, cudaStream_t stream) {
  if (M <= 0 || K <= 0 || N <= 0) return;

  // M==1 (autoregressive decode): memory-bound GEMV with coalesced weight
  // reads. K is even for every Qwen3 projection (1024/2048/6144) so the half2
  // path is safe; K % 8 == 0 holds for all of them too, so the 128-bit float4
  // path is preferred.
  if (M == 1 && (K & 1) == 0) {
    const int grid = (N + kGemvWarps - 1) / kGemvWarps;
    const size_t shmem = static_cast<size_t>(K) * sizeof(__nv_bfloat16);
    // ORATOR_GEMV_HALF2=1 forces the legacy half2 kernel (A/B + safety
    // fallback).
    static const bool force_half2 = std::getenv("ORATOR_GEMV_HALF2") != nullptr;
    if ((K & 7) == 0 && !force_half2) {
      GemvBf16Vec4Kernel<<<grid, kGemvWarps * 32, shmem, stream>>>(
          reinterpret_cast<const __nv_bfloat16*>(in_bf16),
          reinterpret_cast<const __nv_bfloat16*>(W_bf16), bias_f32, out_f32, K,
          N, act);
    } else {
      GemvBf16Kernel<<<grid, kGemvWarps * 32, shmem, stream>>>(
          reinterpret_cast<const __nv_bfloat16*>(in_bf16),
          reinterpret_cast<const __nv_bfloat16*>(W_bf16), bias_f32, out_f32, K,
          N, act);
    }
    CheckCudaError(cudaGetLastError(), __FILE__, __LINE__);
    return;
  }

  // M>1 (and the rare M==1 odd-K): in-project tiled bf16 GEMM with the bias +
  // activation epilogue fused. Replaces cuBLAS (Spec 002 P2.1, Constitution
  // Art. I): allocation-free, stream-explicit, no global handle -> capturable.
  gemm::LaunchBf16Gemm(in_bf16, W_bf16, bias_f32, out_f32, M, K, N, act,
                       stream);
}

void Shutdown() {
  if (g_scratch) {
    CUDA_CHECK(cudaFree(g_scratch));
    g_scratch = nullptr;
    g_scratch_cap = 0;
  }
}

}  // namespace asr_gemm
}  // namespace model
}  // namespace orator
