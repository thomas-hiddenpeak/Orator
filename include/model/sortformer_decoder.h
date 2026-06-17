#pragma once

#include <cuda_runtime.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "gpu/memory.h"
#include "io/safetensor.h"

// Sortformer decoder stage: encoder_proj (512->192) + 18-layer post-LN
// TransformerEncoder (hidden=192, 8 heads, FFN=768, relu) + speaker head
// (relu -> first_hidden_to_hidden(192) -> relu -> single_hidden_to_spks(4) ->
// sigmoid), producing per-frame speaker activity [T, 4].
//
// Mirrors NeMo's SortformerEncLabelModel.frontend_encoder(encoder_proj part) +
// forward_infer + forward_speaker_sigmoids. Manual MHA: query/key pre-divided by
// sqrt(sqrt(d_k)) so scores = q.k / sqrt(d_k); padded frames masked, preds*mask.

namespace orator {
namespace model {

class SortformerDecoder {
 public:
  SortformerDecoder(int d_enc = 512, int d_model = 192, int n_heads = 8,
                    int d_ff = 768, int n_layers = 18, int n_spk = 4);

  void LoadWeights(const io::SafeTensorReader& reader);

  // conformer_out: [T, d_enc] (frame-major) device/unified.
  // `stream` is the CUDA stream for kernel launches and synchronization.
  // Returns preds [T, n_spk] (host vector). valid_len frames are kept, rest 0.
  std::vector<float> Forward(const float* conformer_out, int T, int valid_len,
                              cudaStream_t stream = nullptr);

 private:
  const float* W(const std::string& name) const;
  int d_enc_, d_model_, n_heads_, d_ff_, d_k_, n_layers_, n_spk_;
  std::map<std::string, std::unique_ptr<gpu::UnifiedBuffer>> w_;
};

}  // namespace model
}  // namespace orator
