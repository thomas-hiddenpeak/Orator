#pragma once

// High-performance register-blocked, double-buffered SGEMM shared by the
// Conformer encoder and the decoder.
//
// Computes out[M,N] = (bias ? bias[n] : 0) + sum_k in[m,k] * W[n,k]
// where in is [M,K] row-major and W is [N,K] row-major (i.e. out = in * W^T,
// the standard nn.Linear weight layout). An activation is fused on the result.
//
// Layout: a 128x128 output tile per block, an 8x8 micro-tile per thread held in
// registers, operands staged through double-buffered shared memory with float4
// vectorized global loads and float4 register-fragment loads. The next K-tile's
// global loads are prefetched into registers while the current tile is being
// multiplied, hiding global/shared latency behind compute (CUTLASS-style
// software pipelining).
//
// K is assumed a multiple of 8 (true for every projection in this model:
// 192/512/2048/4096), so the K dimension is never partial; only the M/N block
// edges are guarded.

#include <cuda_runtime.h>
#include <cuda_bf16.h>
#include <mma.h>

#include <cstdint>

namespace orator {
namespace gemm {

constexpr int BM = 128;  // output rows per block
constexpr int BN = 128;  // output cols per block
constexpr int BK = 8;    // K-tile depth
constexpr int TM = 8;    // output rows per thread
constexpr int TN = 8;    // output cols per thread
// threads per block = (BM/TM) * (BN/TN) = 16 * 16 = 256
constexpr int kThreadsGemm = (BM / TM) * (BN / TN);

// Activation codes (universal across modules):
//   0 = none, 1 = SiLU, 2 = ReLU, 3 = sigmoid
__device__ __forceinline__ float ApplyAct(float v, int act) {
  if (act == 1) return v / (1.0f + __expf(-v));
  if (act == 2) return fmaxf(v, 0.0f);
  if (act == 3) return 1.0f / (1.0f + __expf(-v));
  return v;
}

static __global__ void SgemmKernel(const float* __restrict__ in,
                                   const float* __restrict__ W,
                                   const float* __restrict__ bias,
                                   float* __restrict__ out, int M, int K, int N,
                                   int act) {
  __shared__ float As[2][BK][BM];  // transposed: As[buf][k][m]
  __shared__ float Ws[2][BK][BN];  // transposed: Ws[buf][k][n]

  const int tid = threadIdx.x;            // 0..255
  const int threadCol = tid % (BN / TN);  // 0..15
  const int threadRow = tid / (BN / TN);  // 0..15
  const int m0 = blockIdx.y * BM;
  const int n0 = blockIdx.x * BN;

  // Each thread loads one float4 from A and one from W per K-tile.
  // A tile is BM x BK = 128 x 8 floats -> 128*8/4 = 256 float4 -> 1/thread.
  const int aRow = tid / (BK / 4);         // 0..127 (which m)
  const int aCol4 = (tid % (BK / 4)) * 4;  // 0 or 4 (which k)
  const int wRow = tid / (BK / 4);         // 0..127 (which n)
  const int wCol4 = (tid % (BK / 4)) * 4;  // 0 or 4 (which k)

  const int aM = m0 + aRow;
  const int wN = n0 + wRow;
  const bool aValid = aM < M;
  const bool wValid = wN < N;
  const float* aPtr = in + (size_t(aM) * K + aCol4);
  const float* wPtr = W + (size_t(wN) * K + wCol4);

  float acc[TM][TN];
#pragma unroll
  for (int i = 0; i < TM; ++i)
#pragma unroll
    for (int j = 0; j < TN; ++j) acc[i][j] = 0.0f;

  float aFrag[TM], bFrag[TN];

  auto storeTile = [&](int buf, int k0) {
    float4 av = make_float4(0.f, 0.f, 0.f, 0.f);
    float4 wv = make_float4(0.f, 0.f, 0.f, 0.f);
    if (aValid) av = *reinterpret_cast<const float4*>(aPtr + k0);
    if (wValid) wv = *reinterpret_cast<const float4*>(wPtr + k0);
    As[buf][aCol4 + 0][aRow] = av.x;
    As[buf][aCol4 + 1][aRow] = av.y;
    As[buf][aCol4 + 2][aRow] = av.z;
    As[buf][aCol4 + 3][aRow] = av.w;
    Ws[buf][wCol4 + 0][wRow] = wv.x;
    Ws[buf][wCol4 + 1][wRow] = wv.y;
    Ws[buf][wCol4 + 2][wRow] = wv.z;
    Ws[buf][wCol4 + 3][wRow] = wv.w;
  };

  // Preload first K-tile into buffer 0.
  storeTile(0, 0);
  __syncthreads();

  int buf = 0;
  for (int k0 = 0; k0 < K; k0 += BK) {
    // Prefetch next tile's global loads into registers while computing.
    int k0n = k0 + BK;
    float4 avNext = make_float4(0.f, 0.f, 0.f, 0.f);
    float4 wvNext = make_float4(0.f, 0.f, 0.f, 0.f);
    if (k0n < K) {
      if (aValid) avNext = *reinterpret_cast<const float4*>(aPtr + k0n);
      if (wValid) wvNext = *reinterpret_cast<const float4*>(wPtr + k0n);
    }

#pragma unroll
    for (int k = 0; k < BK; ++k) {
      // float4 vectorized shared-memory fragment loads (TM/TN are mult of 4).
#pragma unroll
      for (int i = 0; i < TM; i += 4)
        *reinterpret_cast<float4*>(&aFrag[i]) =
            *reinterpret_cast<const float4*>(&As[buf][k][threadRow * TM + i]);
#pragma unroll
      for (int j = 0; j < TN; j += 4)
        *reinterpret_cast<float4*>(&bFrag[j]) =
            *reinterpret_cast<const float4*>(&Ws[buf][k][threadCol * TN + j]);
#pragma unroll
      for (int i = 0; i < TM; ++i)
#pragma unroll
        for (int j = 0; j < TN; ++j) acc[i][j] += aFrag[i] * bFrag[j];
    }

    if (k0n < K) {
      int nextBuf = buf ^ 1;
      As[nextBuf][aCol4 + 0][aRow] = avNext.x;
      As[nextBuf][aCol4 + 1][aRow] = avNext.y;
      As[nextBuf][aCol4 + 2][aRow] = avNext.z;
      As[nextBuf][aCol4 + 3][aRow] = avNext.w;
      Ws[nextBuf][wCol4 + 0][wRow] = wvNext.x;
      Ws[nextBuf][wCol4 + 1][wRow] = wvNext.y;
      Ws[nextBuf][wCol4 + 2][wRow] = wvNext.z;
      Ws[nextBuf][wCol4 + 3][wRow] = wvNext.w;
      __syncthreads();
      buf = nextBuf;
    }
  }

#pragma unroll
  for (int i = 0; i < TM; ++i) {
    int m = m0 + threadRow * TM + i;
    if (m >= M) continue;
#pragma unroll
    for (int j = 0; j < TN; ++j) {
      int n = n0 + threadCol * TN + j;
      if (n >= N) continue;
      float v = acc[i][j];
      if (bias) v += bias[n];
      out[size_t(m) * N + n] = ApplyAct(v, act);
    }
  }
}

// act: 0 none, 1 SiLU, 2 ReLU, 3 sigmoid.
inline void LaunchSgemm(const float* in, const float* W, const float* bias,
                        float* out, int M, int K, int N, int act) {
  dim3 block(kThreadsGemm);
  dim3 grid((N + BN - 1) / BN, (M + BM - 1) / BM);
  SgemmKernel<<<grid, block>>>(in, W, bias, out, M, K, N, act);
}

inline void LaunchSgemm(const float* in, const float* W, const float* bias,
                        float* out, int M, int K, int N, int act,
                        cudaStream_t stream) {
  dim3 block(kThreadsGemm);
  dim3 grid((N + BN - 1) / BN, (M + BM - 1) / BM);
  SgemmKernel<<<grid, block, 0, stream>>>(in, W, bias, out, M, K, N, act);
}

// Strided-batched variant. Batch index comes from blockIdx.z; each batch b uses
// in + b*strideIn, W + b*strideW, out + b*strideOut. No bias (attention path).
// Same contract per batch: out[M,N] = sum_k in[m,k]*W[n,k]. Robust to arbitrary
// K, M, N and alignment (scalar guarded global loads) — used by attention where
// the contracted dim is the sequence length and may not be a multiple of 8.
static __global__ void SgemmBatchedKernel(const float* __restrict__ in,
                                          const float* __restrict__ W,
                                          float* __restrict__ out, int M, int K,
                                          int N, int act, long strideIn,
                                          long strideW, long strideOut) {
  __shared__ float As[BK][BM];  // transposed: As[k][m]
  __shared__ float Ws[BK][BN];  // transposed: Ws[k][n]

  const int tid = threadIdx.x;
  const int threadCol = tid % (BN / TN);
  const int threadRow = tid / (BN / TN);
  const int m0 = blockIdx.y * BM;
  const int n0 = blockIdx.x * BN;
  const int b = blockIdx.z;

  const float* inB = in + b * strideIn;
  const float* WB = W + b * strideW;
  float* outB = out + b * strideOut;

  float acc[TM][TN];
#pragma unroll
  for (int i = 0; i < TM; ++i)
#pragma unroll
    for (int j = 0; j < TN; ++j) acc[i][j] = 0.0f;

  float aFrag[TM], bFrag[TN];

  for (int k0 = 0; k0 < K; k0 += BK) {
    for (int idx = tid; idx < BM * BK; idx += blockDim.x) {
      int m = idx / BK, k = idx % BK;
      int gm = m0 + m, gk = k0 + k;
      As[k][m] = (gm < M && gk < K) ? inB[size_t(gm) * K + gk] : 0.0f;
    }
    for (int idx = tid; idx < BN * BK; idx += blockDim.x) {
      int n = idx / BK, k = idx % BK;
      int gn = n0 + n, gk = k0 + k;
      Ws[k][n] = (gn < N && gk < K) ? WB[size_t(gn) * K + gk] : 0.0f;
    }
    __syncthreads();
#pragma unroll
    for (int k = 0; k < BK; ++k) {
#pragma unroll
      for (int i = 0; i < TM; i += 4)
        *reinterpret_cast<float4*>(&aFrag[i]) =
            *reinterpret_cast<const float4*>(&As[k][threadRow * TM + i]);
#pragma unroll
      for (int j = 0; j < TN; j += 4)
        *reinterpret_cast<float4*>(&bFrag[j]) =
            *reinterpret_cast<const float4*>(&Ws[k][threadCol * TN + j]);
#pragma unroll
      for (int i = 0; i < TM; ++i)
#pragma unroll
        for (int j = 0; j < TN; ++j) acc[i][j] += aFrag[i] * bFrag[j];
    }
    __syncthreads();
  }

#pragma unroll
  for (int i = 0; i < TM; ++i) {
    int m = m0 + threadRow * TM + i;
    if (m >= M) continue;
#pragma unroll
    for (int j = 0; j < TN; ++j) {
      int n = n0 + threadCol * TN + j;
      if (n >= N) continue;
      outB[size_t(m) * N + n] = ApplyAct(acc[i][j], act);
    }
  }
}

inline void LaunchSgemmBatched(const float* in, const float* W, float* out,
                               int M, int K, int N, int act, int batch,
                               long strideIn, long strideW, long strideOut) {
  dim3 block(kThreadsGemm);
  dim3 grid((N + BN - 1) / BN, (M + BM - 1) / BM, batch);
  SgemmBatchedKernel<<<grid, block>>>(in, W, out, M, K, N, act, strideIn,
                                      strideW, strideOut);
}

inline void LaunchSgemmBatched(const float* in, const float* W, float* out,
                               int M, int K, int N, int act, int batch,
                               long strideIn, long strideW, long strideOut,
                               cudaStream_t stream) {
  dim3 block(kThreadsGemm);
  dim3 grid((N + BN - 1) / BN, (M + BM - 1) / BM, batch);
  SgemmBatchedKernel<<<grid, block, 0, stream>>>(in, W, out, M, K, N, act,
                                                 strideIn, strideW, strideOut);
}

// ---------------------------------------------------------------------------
// In-project bf16 GEMM (Spec 002 P2.1) — replaces cuBLAS on the ASR path.
//
// out[M,N] (FP32) = act( bias[n] + sum_k in[m,k] * W[n,k] ), with `in` and `W`
// stored as native BF16 (uint16) and the dot product accumulated in FP32 (the
// cublasGemmEx CUBLAS_COMPUTE_32F contract). BF16 -> FP32 is lossless (BF16 is
// the high 16 bits of an FP32). The bias + activation are FUSED in the epilogue
// (no separate pass). `actAsr`: 0 none, 1 GELU(exact erf), 2 ReLU.
//
// Two kernels: a 16x16x16 bf16 tensor-core (WMMA) path (requires K % 16 == 0,
// true for every projection in the model), and a robust scalar fallback for the
// one odd K (the first conv's im2col K = Cin*9 = 9). Both are allocation-free
// and run on the caller's stream — capturable in a CUDA graph (unlike cuBLAS).
// ---------------------------------------------------------------------------

__device__ __forceinline__ float Bf2f(uint16_t b) {
  return __uint_as_float(static_cast<uint32_t>(b) << 16);
}

__device__ __forceinline__ float ApplyActAsr(float v, int act) {
  if (act == 1) return 0.5f * v * (1.0f + erff(v * 0.70710678118654752440f));
  if (act == 2) return fmaxf(v, 0.0f);
  return v;
}

// WMMA tensor-core tiling: a 64x64 block output tile (16 warps), one 16x16x16
// bf16 MMA fragment per warp with FP32 accumulation, operands staged through
// shared memory. (A warp-tiled 32x32-per-warp variant was faster in isolation
// but exposed a concurrency-mode instability; this simpler form is stable under
// the production lock-free concurrency. Closing the remaining throughput gap to
// the former cuBLAS path is ongoing Spec 002 P2.1 work.)
namespace wmma_cfg {
constexpr int WM = 16, WN = 16, WK = 16;     // one MMA fragment
constexpr int BWM = 64, BWN = 64, BWK = 16;  // block output tile + K step
constexpr int WARPS_M = BWM / WM;            // 4
constexpr int WARPS_N = BWN / WN;            // 4
constexpr int WARPS = WARPS_M * WARPS_N;     // 16
constexpr int THREADS = WARPS * 32;          // 512
}  // namespace wmma_cfg

// Tensor-core bf16 GEMM: out[M,N] = act(bias[n] + in[M,K] @ W[N,K]^T).
// A = in (row-major [M,K]); B = W viewed as W^T -- a col-major load of the
// [N,K] row-major W with ldm=K yields B[k][n] = W[n][k] = W^T[k][n]. Requires
// K % 16 == 0; bias + activation fused in the epilogue.
static __global__ void Bf16WmmaKernel(const __nv_bfloat16* __restrict__ in,
                                      const __nv_bfloat16* __restrict__ W,
                                      const float* __restrict__ bias,
                                      float* __restrict__ out, int M, int K,
                                      int N, int act) {
  using namespace nvcuda;
  using namespace wmma_cfg;
  __shared__ __nv_bfloat16 As[BWM][BWK];  // in[m][k]
  __shared__ __nv_bfloat16 Bs[BWN][BWK];  // W[n][k]
  __shared__ float Cs[BWM][BWN];          // staged output tile

  const int m0 = blockIdx.y * BWM;
  const int n0 = blockIdx.x * BWN;
  const int tid = threadIdx.x;
  const int warp = tid / 32;
  const int wm = warp / WARPS_N;  // 0..3
  const int wn = warp % WARPS_N;  // 0..3

  wmma::fragment<wmma::accumulator, WM, WN, WK, float> cFrag;
  wmma::fill_fragment(cFrag, 0.0f);

  const __nv_bfloat16 kZero = __float2bfloat16(0.0f);
  for (int k0 = 0; k0 < K; k0 += BWK) {
    for (int idx = tid; idx < BWM * BWK; idx += THREADS) {
      const int mm = idx / BWK, kk = idx % BWK;
      const int gm = m0 + mm;
      As[mm][kk] =
          (gm < M) ? in[static_cast<size_t>(gm) * K + (k0 + kk)] : kZero;
    }
    for (int idx = tid; idx < BWN * BWK; idx += THREADS) {
      const int nn = idx / BWK, kk = idx % BWK;
      const int gn = n0 + nn;
      Bs[nn][kk] =
          (gn < N) ? W[static_cast<size_t>(gn) * K + (k0 + kk)] : kZero;
    }
    __syncthreads();

    wmma::fragment<wmma::matrix_a, WM, WN, WK, __nv_bfloat16, wmma::row_major>
        aFrag;
    wmma::fragment<wmma::matrix_b, WM, WN, WK, __nv_bfloat16, wmma::col_major>
        bFrag;
    wmma::load_matrix_sync(aFrag, &As[wm * WM][0], BWK);
    wmma::load_matrix_sync(bFrag, &Bs[wn * WN][0], BWK);
    wmma::mma_sync(cFrag, aFrag, bFrag, cFrag);
    __syncthreads();
  }

  wmma::store_matrix_sync(&Cs[wm * WM][wn * WN], cFrag, BWN,
                          wmma::mem_row_major);
  __syncthreads();
  for (int idx = tid; idx < BWM * BWN; idx += THREADS) {
    const int mm = idx / BWN, nn = idx % BWN;
    const int gm = m0 + mm, gn = n0 + nn;
    if (gm < M && gn < N) {
      float v = Cs[mm][nn];
      if (bias) v += bias[gn];
      out[static_cast<size_t>(gm) * N + gn] = ApplyActAsr(v, act);
    }
  }
}

// Warp-tiled WMMA: each warp computes a 16x(WN*FN) output strip = FN bf16 MMA
// fragments in N, reusing ONE loaded A fragment across all FN B fragments. This
// raises arithmetic intensity per warp (fewer shared-memory fragment loads per
// MMA) than the 1-fragment Bf16WmmaKernel, closing part of the small-GEMM gap
// to cuBLAS. Ragged M/N edges are zero-padded on load and bounds-checked in the
// epilogue (validated against the f64 oracle, including non-tile-aligned shapes
// such as N=5000, so a stray out-of-bounds access shows up as a numerical error
// here rather than as the concurrency corruption an earlier warp-tiled attempt
// hit). Portable nvcuda::wmma (SM 8.0+, runs on Orin); requires K % 16 == 0.
namespace wmma_cfg2 {
constexpr int WM = 16, WN = 16, WK = 16;
constexpr int FN = 2;                         // N fragments per warp (16x32)
constexpr int BWM = 64, BWN = 128, BWK = 16;  // block output tile + K step
constexpr int WARPS_M = BWM / WM;             // 4
constexpr int WARPS_N = BWN / (WN * FN);      // 4
constexpr int WARPS = WARPS_M * WARPS_N;      // 16
constexpr int THREADS = WARPS * 32;           // 512
}  // namespace wmma_cfg2

static __global__ void Bf16WmmaKernel2(const __nv_bfloat16* __restrict__ in,
                                       const __nv_bfloat16* __restrict__ W,
                                       const float* __restrict__ bias,
                                       float* __restrict__ out, int M, int K,
                                       int N, int act) {
  using namespace nvcuda;
  using namespace wmma_cfg2;
  __shared__ __nv_bfloat16 As[BWM][BWK];  // in[m][k]  (64x16)
  __shared__ __nv_bfloat16 Bs[BWN][BWK];  // W[n][k]   (128x16)
  __shared__ float Cs[BWM][BWN];          // staged output tile (64x128)

  const int m0 = blockIdx.y * BWM;
  const int n0 = blockIdx.x * BWN;
  const int tid = threadIdx.x;
  const int warp = tid / 32;
  const int wm = warp / WARPS_N;  // 0..3
  const int wn = warp % WARPS_N;  // 0..3

  wmma::fragment<wmma::accumulator, WM, WN, WK, float> cFrag[FN];
#pragma unroll
  for (int f = 0; f < FN; ++f) wmma::fill_fragment(cFrag[f], 0.0f);

  const __nv_bfloat16 kZero = __float2bfloat16(0.0f);
  for (int k0 = 0; k0 < K; k0 += BWK) {
    for (int idx = tid; idx < BWM * BWK; idx += THREADS) {
      const int mm = idx / BWK, kk = idx % BWK;
      const int gm = m0 + mm;
      As[mm][kk] =
          (gm < M) ? in[static_cast<size_t>(gm) * K + (k0 + kk)] : kZero;
    }
    for (int idx = tid; idx < BWN * BWK; idx += THREADS) {
      const int nn = idx / BWK, kk = idx % BWK;
      const int gn = n0 + nn;
      Bs[nn][kk] =
          (gn < N) ? W[static_cast<size_t>(gn) * K + (k0 + kk)] : kZero;
    }
    __syncthreads();

    wmma::fragment<wmma::matrix_a, WM, WN, WK, __nv_bfloat16, wmma::row_major>
        aFrag;
    wmma::load_matrix_sync(aFrag, &As[wm * WM][0], BWK);
#pragma unroll
    for (int f = 0; f < FN; ++f) {
      wmma::fragment<wmma::matrix_b, WM, WN, WK, __nv_bfloat16, wmma::col_major>
          bFrag;
      wmma::load_matrix_sync(bFrag, &Bs[(wn * FN + f) * WN][0], BWK);
      wmma::mma_sync(cFrag[f], aFrag, bFrag, cFrag[f]);
    }
    __syncthreads();
  }

#pragma unroll
  for (int f = 0; f < FN; ++f)
    wmma::store_matrix_sync(&Cs[wm * WM][(wn * FN + f) * WN], cFrag[f], BWN,
                            wmma::mem_row_major);
  __syncthreads();
  for (int idx = tid; idx < BWM * BWN; idx += THREADS) {
    const int mm = idx / BWN, nn = idx % BWN;
    const int gm = m0 + mm, gn = n0 + nn;
    if (gm < M && gn < N) {
      float v = Cs[mm][nn];
      if (bias) v += bias[gn];
      out[static_cast<size_t>(gm) * N + gn] = ApplyActAsr(v, act);
    }
  }
}

// Robust scalar fallback for arbitrary K (one thread per output element).
static __global__ void Bf16GemmGenericKernel(const uint16_t* __restrict__ in,
                                             const uint16_t* __restrict__ W,
                                             const float* __restrict__ bias,
                                             float* __restrict__ out, int M,
                                             int K, int N, int act) {
  const int n = blockIdx.x * blockDim.x + threadIdx.x;
  if (n >= N) return;
  // Grid-stride over rows so a grid capped at the CUDA 65535 y-dim limit covers
  // arbitrarily large M (long forced-alignment segments yield M > 65535). For
  // bounded M (ASR) gridDim.y >= M, so each block runs the body once — identical
  // behaviour to a one-row-per-block launch.
  for (int m = blockIdx.y; m < M; m += gridDim.y) {
    const uint16_t* a = in + size_t(m) * K;
    const uint16_t* w = W + size_t(n) * K;
    float acc = 0.0f;
    for (int k = 0; k < K; ++k) acc += Bf2f(a[k]) * Bf2f(w[k]);
    if (bias) acc += bias[n];
    out[size_t(m) * N + n] = ApplyActAsr(acc, act);
  }
}

// out[M,N] = actAsr(bias + in[M,K] @ W[N,K]^T), bf16 operands, FP32 accumulate.
inline void LaunchBf16Gemm(const uint16_t* in, const uint16_t* W,
                           const float* bias, float* out, int M, int K, int N,
                           int act, cudaStream_t stream) {
  if (M <= 0 || K <= 0 || N <= 0) return;
  if (K % wmma_cfg::WK == 0) {
    dim3 block(wmma_cfg2::THREADS);
    dim3 grid((N + wmma_cfg2::BWN - 1) / wmma_cfg2::BWN,
              (M + wmma_cfg2::BWM - 1) / wmma_cfg2::BWM);
    Bf16WmmaKernel2<<<grid, block, 0, stream>>>(
        reinterpret_cast<const __nv_bfloat16*>(in),
        reinterpret_cast<const __nv_bfloat16*>(W), bias, out, M, K, N, act);
  } else {
    dim3 block(256);
    dim3 grid((N + 255) / 256, static_cast<unsigned>(M < 65535 ? M : 65535));
    Bf16GemmGenericKernel<<<grid, block, 0, stream>>>(in, W, bias, out, M, K, N,
                                                      act);
  }
}

}  // namespace gemm
}  // namespace orator
