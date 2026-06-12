#pragma once

// ASR core operators (Qwen3-ASR), pure CUDA, no external dependencies.
//
// These are the numerically-critical building blocks shared by the Qwen3-ASR
// audio encoder and the Qwen3 text decoder. They are intentionally small,
// self-contained host launchers over device (or unified) pointers so each one
// can be verified in isolation against an independent CPU reference. All
// compute is FP32: accuracy is the primary objective here, and weights (stored
// as BF16 in the checkpoint) are upcast to FP32 before these kernels run.
//
// Layout convention: row-major. Sequence tensors are [tokens, heads, head_dim]
// or [rows, dim]; an nn.Linear weight is [out, in] (consumed by orator::gemm).

#include <cuda_runtime.h>

namespace orator {
namespace model {
namespace asr_ops {

// RMSNorm over the last dimension (Qwen3 uses RMSNorm, eps = 1e-6).
//   out[r,d] = x[r,d] * rsqrt(mean_d(x[r,:]^2) + eps) * weight[d]
// x, out: [rows, dim] row-major; weight: [dim]. In-place is allowed (out == x).
void RmsNorm(const float* x, const float* weight, float* out, int rows, int dim,
             float eps, cudaStream_t stream = 0);

// Interleaved rotary position embedding, applied in place.
//   for pair i in [0, head_dim/2):  theta = base^(-2i/head_dim)
//   angle = pos * theta;  (x[2i], x[2i+1]) rotated by `angle`
// x: [n_tokens, n_heads, head_dim] row-major; positions: [n_tokens].
// head_dim must be even. This is the 1D form used for text decoding; mRoPE
// (config mrope_section) generalizes it for multimodal position ids.
void RopeInterleaved(float* x, const int* positions, int n_tokens, int n_heads,
                     int head_dim, float theta_base);

// SwiGLU gating used by the Qwen3 MLP: out[i] = SiLU(gate[i]) * up[i].
// gate, up, out: contiguous length-n buffers. In-place is allowed.
void SwiGLU(const float* gate, const float* up, float* out, int n,
            cudaStream_t stream = 0);

// "rotate_half" rotary position embedding (the HF Qwen3 form, NOT interleaved),
// applied in place. For head dim D and half=D/2:
//   freq_j = base^(-2j/D) for j in [0,half);  angle = pos * freq_j
//   out[d<half]  = x[d]*cos(angle_d)      - x[d+half]*sin(angle_d)
//   out[d>=half] = x[d]*cos(angle_{d-half}) + x[d-half]*sin(angle_{d-half})
// x: [n_tokens, n_heads, head_dim] row-major; positions: [n_tokens]. D even.
void RopeHalf(float* x, const int* positions, int n_tokens, int n_heads,
              int head_dim, float theta_base, cudaStream_t stream = 0);

// Grouped-query attention with optional causal masking.
//   q: [T, Hq, Dh],  k/v: [T, Hkv, Dh],  out: [T, Hq, Dh]
// Hq must be a multiple of Hkv (each KV head is shared by Hq/Hkv query heads).
// Uses an online (flash-style) softmax so no [T,T] score matrix is allocated.
// `scale` is usually 1/sqrt(Dh). head_dim (Dh) must be a power of two <= 1024.
void GqaAttention(const float* q, const float* k, const float* v, float* out,
                  int T, int Hq, int Hkv, int Dh, float scale, bool causal);

// GPU argmax over `n` FP32 logits, optionally banning up to two indices
// (set ban0/ban1 to -1 to disable). Writes the winning index to *out_idx
// (device int). Single-block reduction; n may be large (e.g. 151936).
void ArgmaxBanned(const float* logits, int n, int ban0, int ban1, int* out_idx,
                  cudaStream_t stream = 0);

}  // namespace asr_ops
}  // namespace model
}  // namespace orator
