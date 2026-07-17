#pragma once

// TitaNet-Large speaker embedder (NVIDIA `titanet_large`).
//
// Implements core::ISpeakerEmbedder. The full forward is pure C++/CUDA and is
// validated stage-by-stage against the NeMo reference oracle
// (tools/reference/titanet_oracle.py -> models/reference/speaker/):
//
//   audio -> log-mel (80, per_feature norm)
//         -> ConvASR encoder: 5 ContextNet blocks
//              (depthwise-separable conv + BatchNorm + ReLU + SqueezeExcite,
//               blocks 1-3 repeat x3 with a residual branch)
//         -> attentive statistics pooling (mean (+) std over time) -> 6144
//         -> BatchNorm + linear -> 192-d embedding -> L2 normalize
//
// Layout convention: every activation is time-major [T, C] row-major, so a 1x1
// (pointwise) convolution is exactly a row-wise nn.Linear and maps directly to
// the project SGEMM (out[T,Cout] = in[T,Cin] * W[Cout,Cin]^T). Depthwise conv,
// BatchNorm, SqueezeExcite and the pooling reductions run as small custom
// kernels. Mel output is already [T, 80], so no transpose is needed.
//
// Weights are F32 (models/speaker/titanet_large.safetensors, converted from the
// .nemo). The trailing 16681-way classifier head (`decoder.final`) is
// inference-irrelevant and never loaded.

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/stages.h"
#include "feature/mel_spectrogram.h"
#include "gpu/device_scratch.h"
#include "gpu/memory.h"
#include "io/safetensor.h"

namespace orator {
namespace model {

// Architecture constants from titanet_large model_config.yaml.
struct TitaNetConfig {
  int sample_rate = 16000;
  int n_fft = 512;
  int n_mels = 80;
  int win_length = 400;  // 25 ms
  int hop_length = 160;  // 10 ms
  int enc_dim = 1024;    // hidden channels in blocks 0-3
  int epilog_dim = 3072;  // block 4 output channels (decoder feat_in)
  int embedding_dim = 192;
  float bn_eps_encoder = 1e-3f;  // NeMo Jasper BatchNorm eps
  float bn_eps_decoder = 1e-5f;  // NeMo TDNN / emb BatchNorm eps (torch default)
  float pool_eps = 1e-10f;       // attentive-pool std clamp
};

class TitaNetEmbedder : public core::ISpeakerEmbedder {
 public:
  TitaNetEmbedder() = default;
  explicit TitaNetEmbedder(const TitaNetConfig& config) : config_(config) {}

  // core::ISpeakerEmbedder
  void LoadWeights(const std::string& path) override;
  int dim() const override { return config_.embedding_dim; }
  std::vector<float> Embed(const core::AudioChunk& chunk) override;
  std::string name() const override { return "titanet_large"; }

  // The stream is scheduler-owned. Warmup allocates maximum session scratch
  // before live audio starts, avoiding device-wide allocation stalls.
  void SetStream(cudaStream_t stream) { stream_ = stream; }
  void Warmup(int num_samples);

  const TitaNetConfig& config() const { return config_; }

 private:
  // One ContextNet block descriptor (resolved tensor names + shape facts).
  struct Block {
    int in_ch = 0;
    int out_ch = 0;
    int kernel = 0;
    int repeats = 0;       // separable conv sub-units (1 or 3)
    bool residual = false;
    int se_channels = 0;   // SqueezeExcite squeeze width (out_ch / 8)
    std::string prefix;    // "encoder.encoder.N"
    // mconv sub-unit base indices (each = depthwise, pointwise, BN at +0,+1,+2)
    std::vector<int> sub_base;  // e.g. {0,5,10}
    int se_index = 0;           // mconv index of the SqueezeExcite fc
  };

  const float* W(const std::string& name) const;
  bool Has(const std::string& name) const;

  // Forward helpers (all operate on device pointers, [T, C] row-major).
  // Returns the device pointer holding the block output and writes out_ch.
  float* RunBlock(const Block& b, float* x_in, int T, int* out_ch,
                  cudaStream_t stream);
  // Attentive statistics pooling + embedding head. `enc` is [T, epilog_dim].
  // Writes the final L2-normalized 192-d embedding to host `out`.
  void RunPooledHead(float* enc, int T, std::vector<float>* out,
                     cudaStream_t stream);

  TitaNetConfig config_;
  std::unique_ptr<io::SafeTensorReader> reader_;
  std::unique_ptr<feature::MelSpectrogram> mel_;
  std::map<std::string, std::unique_ptr<gpu::UnifiedBuffer>> w_;
  std::vector<Block> blocks_;
  mutable gpu::DeviceScratch scratch_;
  cudaStream_t stream_ = nullptr;  // non-owning; owned by GpuScheduler
  bool loaded_ = false;
};

}  // namespace model
}  // namespace orator
