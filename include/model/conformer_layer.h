#pragma once

// A single Conformer encoder layer (Sortformer / FastConformer config:
// d_model=512, 8 heads, FFN 2048 (x4), conv kernel 9, batch_norm, rel_pos).
//
// Mirrors NeMo's ConformerLayer.forward (eval, no dropout):
//   r = x;                r += 0.5*FF1(LN(r))
//   r += SelfAttn(LN(r), pos_emb, mask)          [RelPositionMultiHeadAttention]
//   r += Conv(LN(r), pad_mask)                   [pw1->GLU->dw(k9)->BN->SiLU->pw2]
//   r += 0.5*FF2(LN(r))
//   out = LN_out(r)
// Manual (non-SDPA) attention: scores = (matrix_ac + rel_shift(matrix_bd))/sqrt(d_k).
//
// Weights are copied from a SafeTensorReader into unified memory. The same
// instance type is reused for all 17 layers (different prefixes).

#include <cuda_runtime.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "gpu/memory.h"
#include "io/safetensor.h"

namespace orator {
namespace model {

class ConformerLayer {
 public:
  ConformerLayer(int d_model = 512, int n_heads = 8, int d_ff = 2048,
                 int conv_kernel = 9);

  void LoadWeights(const io::SafeTensorReader& reader, const std::string& prefix);

  // x: [T, d_model] device/unified, modified in place to the layer output.
  // pos_emb: [2T-1, d_model] relative positional encoding (device/unified).
  // valid_len: number of valid (non-padded) frames; rest masked like NeMo.
  void Forward(float* x, int T, int valid_len, const float* pos_emb,
               cudaStream_t stream = nullptr);

  // Builds the RelPositionalEncoding pos_emb [2T-1, d_model] on host.
  static std::vector<float> BuildPosEmb(int T, int d_model);

 private:
  const float* W(const std::string& name) const;

  // Persistent scratch buffers (grown lazily with T) to avoid per-call
  // cudaMallocManaged churn across 17 layers x many streaming chunks.
  struct Scratch {
    gpu::UnifiedBuffer ln{0}, ff{0}, tmp{0}, qb{0}, kb{0}, vb{0}, pb{0}, cb{0},
        pw1{0}, glu{0}, dw{0};
    // GEMM-decomposed relative-position attention scratch (head-major).
    gpu::UnifiedBuffer qbu{0}, qbv{0}, kh{0}, vt{0}, ph{0}, ac{0}, bd{0}, sc{0};
    int cap_T = 0;
    void Ensure(int T, int D, int Dff, int H);
  };
  Scratch scr_;

  int d_model_, n_heads_, d_ff_, d_k_, conv_kernel_;
  std::map<std::string, std::unique_ptr<gpu::UnifiedBuffer>> w_;
};

}  // namespace model
}  // namespace orator
