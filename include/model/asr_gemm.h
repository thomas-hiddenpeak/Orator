#pragma once

// cuBLAS-backed linear layer for the ASR engine (bf16 tensor cores).
//
// Computes out[M,N] (FP32) = in[M,K] (FP32) @ W[N,K]^T  (nn.Linear layout),
// optionally + bias[N], using bf16 tensor-core GEMM with FP32 accumulation.
// This mirrors PyTorch's bf16 linear (the oracle's compute path) while keeping
// activations FP32 between ops (slightly more precise, and lets the verified
// FP32 elementwise kernels stay unchanged).
//
// Weights are kept in their native BF16 (no upcast) -> half the memory traffic,
// which is what makes the memory-bound decode-time GEMV fast. The `in` operand
// is cast to BF16 into an internally-managed scratch buffer before the GEMM.

#include <cstdint>

#include <cuda_runtime.h>

namespace orator {
namespace model {
namespace asr_gemm {

// Cast n FP32 values to BF16 (round-to-nearest-even) and vice versa.
void F32ToBf16(const float* in, uint16_t* out, long n, cudaStream_t stream = 0);
void Bf16ToF32(const uint16_t* in, float* out, long n, cudaStream_t stream = 0);

// out[M,N] = in[M,K] @ W[N,K]^T (+ bias). W_bf16 points to native BF16 weights.
// bias may be null. act: 0=none, 1=GELU(exact), 2=ReLU.
void Linear(const float* in_f32, const uint16_t* W_bf16, const float* bias_f32,
            float* out_f32, int M, int K, int N, int act, cudaStream_t stream = 0);

// Same as Linear but `in` is already BF16 (no internal cast) -- use when one
// activation feeds several projections (e.g. q/k/v share the post-norm input),
// casting it once via F32ToBf16 and reusing it across the GEMMs.
void LinearPre(const uint16_t* in_bf16, const uint16_t* W_bf16,
               const float* bias_f32, float* out_f32, int M, int K, int N,
               int act, cudaStream_t stream = 0);

// Frees the cuBLAS handle + scratch (optional; process exit also reclaims).
void Shutdown();

}  // namespace asr_gemm
}  // namespace model
}  // namespace orator
