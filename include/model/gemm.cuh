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

}  // namespace gemm
}  // namespace orator
