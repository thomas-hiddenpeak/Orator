#include "model/asr_gemm.h"

#include <cublas_v2.h>
#include <cuda_bf16.h>

#include <cstdlib>
#include <stdexcept>

#include "gpu/memory.h"

namespace orator {
namespace model {
namespace asr_gemm {

using ::orator::gpu::CheckCudaError;

namespace {

constexpr int kThreads = 256;
inline int Blocks(long n) { return static_cast<int>((n + kThreads - 1) / kThreads); }

// Per-THREAD cuBLAS handle + bf16 cast scratch. asr_gemm::Linear is called
// concurrently from independent pipeline threads (the ASR worker on asr_stream
// and the forced-alignment worker on the default stream) which, in the
// production lock-free concurrency mode, are NOT mutually excluded. A single
// shared handle/scratch would race: cublasSetStream mutates the handle's stream
// from two threads at once, and Scratch()'s grow path (cudaFree + cudaMalloc)
// could free the buffer out from under another thread's queued GEMM
// (use-after-free on device memory). thread_local gives each pipeline thread its
// own handle and scratch, removing the race and letting the pipelines run truly
// concurrently. (Per-thread buffers are reclaimed at process exit.)
thread_local cublasHandle_t g_handle = nullptr;
thread_local uint16_t* g_scratch = nullptr;   // bf16 cast scratch for the `in` operand
thread_local long g_scratch_cap = 0;

cublasHandle_t Handle() {
  if (g_handle == nullptr) {
    if (cublasCreate(&g_handle) != CUBLAS_STATUS_SUCCESS)
      throw std::runtime_error("cublasCreate failed");
  }
  return g_handle;
}

uint16_t* Scratch(long n) {
  if (n > g_scratch_cap) {
    if (g_scratch) CUDA_CHECK(cudaFree(g_scratch));
    CheckCudaError(cudaMalloc(&g_scratch, sizeof(uint16_t) * n), __FILE__, __LINE__);
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

  const __nv_bfloat162* w2 = reinterpret_cast<const __nv_bfloat162*>(W + static_cast<size_t>(row) * K);
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
    if (act == 1) acc = 0.5f * acc * (1.0f + erff(acc * 0.70710678118654752440f));
    else if (act == 2) acc = fmaxf(acc, 0.0f);
    out[row] = acc;
  }
}

// out[m,n] += bias[n]; act: 0 none, 1 GELU(exact erf), 2 ReLU.
__global__ void BiasActKernel(float* __restrict__ out, const float* __restrict__ bias,
                             int M, int N, int act) {
  const long idx = static_cast<long>(blockIdx.x) * blockDim.x + threadIdx.x;
  const long total = static_cast<long>(M) * N;
  if (idx >= total) return;
  float v = out[idx];
  if (bias) v += bias[idx % N];
  if (act == 1) v = 0.5f * v * (1.0f + erff(v * 0.70710678118654752440f));
  else if (act == 2) v = fmaxf(v, 0.0f);
  out[idx] = v;
}

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

  // M==1 (autoregressive decode): memory-bound GEMV with coalesced weight reads.
  // K is even for every Qwen3 projection (1024/2048/6144) so the half2 path is safe.
  // ORATOR_ASR_CUBLAS_GEMV=1 falls back to cuBLAS for A/B comparison.
  static const bool kUseCublasGemv = std::getenv("ORATOR_ASR_CUBLAS_GEMV") != nullptr;
  if (M == 1 && (K & 1) == 0 && !kUseCublasGemv) {
    const int grid = (N + kGemvWarps - 1) / kGemvWarps;
    const size_t shmem = static_cast<size_t>(K) * sizeof(__nv_bfloat16);
    GemvBf16Kernel<<<grid, kGemvWarps * 32, shmem, stream>>>(
        reinterpret_cast<const __nv_bfloat16*>(in_bf16),
        reinterpret_cast<const __nv_bfloat16*>(W_bf16), bias_f32, out_f32, K, N, act);
    CheckCudaError(cudaGetLastError(), __FILE__, __LINE__);
    return;
  }

  cublasHandle_t h = Handle();
  cublasSetStream(h, stream);

  // Row-major out[M,N] = in[M,K] @ W[N,K]^T. In cuBLAS column-major this is
  //   out_cm[N,M] = W_cm[K,N]^T @ in_cm[K,M]
  // i.e. op(A)=T on A=W (CUDA_R_16BF, lda=K), op(B)=N on B=in (lda=K),
  //      C=out (CUDA_R_32F, ldc=N), m=N, n=M, k=K, FP32 accumulate.
  const float alpha = 1.0f, beta = 0.0f;
  cublasStatus_t st = cublasGemmEx(
      h, CUBLAS_OP_T, CUBLAS_OP_N, N, M, K, &alpha,
      reinterpret_cast<const __nv_bfloat16*>(W_bf16), CUDA_R_16BF, K,
      reinterpret_cast<const __nv_bfloat16*>(in_bf16), CUDA_R_16BF, K, &beta,
      out_f32, CUDA_R_32F, N, CUBLAS_COMPUTE_32F, CUBLAS_GEMM_DEFAULT);
  if (st != CUBLAS_STATUS_SUCCESS)
    throw std::runtime_error("cublasGemmEx failed: " + std::to_string(st));

  if (bias_f32 != nullptr || act != 0) {
    BiasActKernel<<<Blocks(static_cast<long>(M) * N), kThreads, 0, stream>>>(
        out_f32, bias_f32, M, N, act);
    CheckCudaError(cudaGetLastError(), __FILE__, __LINE__);
  }
}

void Shutdown() {
  if (g_scratch) { CUDA_CHECK(cudaFree(g_scratch)); g_scratch = nullptr; g_scratch_cap = 0; }
  if (g_handle) { cublasDestroy(g_handle); g_handle = nullptr; }
}

}  // namespace asr_gemm
}  // namespace model
}  // namespace orator
