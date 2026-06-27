#pragma once

// Qwen3-ASR audio tower (encoder), FP32.
//
// Whisper-style front-end + 24 bidirectional Transformer layers:
//   mel[128, T]
//     -> chunk into windows of n_window*2 (=100) frames
//     -> per-chunk Conv2d x3 (stride2, pad1, k3, GELU) : 128->64->32->16 freq,
//        100->50->25->13 time ; channels 1->480
//     -> reshape [chunk, t, 480*16=7680] -> conv_out Linear -> [chunk, t, 1024]
//     -> add sinusoidal positional embedding (per chunk, PE[0:t])
//     -> drop padding -> hidden[N, 1024]
//     -> 24 x { LN+bias; windowed bidirectional MHA (16 heads, dim 64); +res;
//               LN+bias; fc1->GELU->fc2; +res }
//        attention windows: blocks of n_window_infer/(n_window*2)*t_per_chunk
//     -> ln_post -> proj1 -> GELU -> proj2 -> [N, output_dim=2048]
//
// Weights are stored BF16 in the checkpoint and upcast to FP32 at load time.
// Verified against the FP32 PyTorch oracle
// (models/reference/asr/audio_features_fp32.f32).

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <cuda_runtime.h>

#include "gpu/device_scratch.h"
#include "gpu/memory.h"
#include "io/sharded_safetensor.h"

namespace orator {
namespace model {

struct AsrAudioConfig {
  int num_mel_bins = 128;
  int d_model = 1024;
  int encoder_layers = 24;
  int encoder_heads = 16;
  int head_dim = 64;  // d_model / heads
  int ffn_dim = 4096;
  int downsample_hidden = 480;
  int output_dim = 2048;
  int n_window = 50;  // chunk = n_window*2 mel frames
  int n_window_infer = 800;
  int max_source_positions = 1500;
};

class AsrAudioTower {
 public:
  explicit AsrAudioTower(const AsrAudioConfig& config = {});

  // Checkpoint tensor names. The Qwen3-ASR encoder and the Qwen3 Forced Aligner
  // encoder are the same architecture but differ in (a) the module prefix and
  // (b) where the output projection lives: ASR keeps proj1/proj2 inside the
  // tower; the aligner uses a separate multi_modal_projector. The defaults
  // match ASR; the aligner overrides them.
  struct WeightNames {
    std::string prefix = "thinker.audio_tower.";      // conv/layers/ln_post
    std::string proj1 = "thinker.audio_tower.proj1";  // D -> output_dim
    std::string proj2 =
        "thinker.audio_tower.proj2";  // output_dim -> output_dim
  };

  // Loads + upcasts all audio_tower weights from the sharded checkpoint.
  void LoadWeights(const io::ShardedSafeTensors& weights) {
    LoadWeights(weights, WeightNames{});
  }
  void LoadWeights(const io::ShardedSafeTensors& weights,
                   const WeightNames& names);

  // mel: row-major [num_mel_bins, n_frames] FP32 (host). Returns row-major
  // [N_out, output_dim] FP32 (host); *out_tokens receives N_out.
  std::vector<float> Forward(const float* mel, int n_frames, int* out_tokens,
                             cudaStream_t stream = 0) const;

  // Output sequence length after the CNN front-end for a given mel length
  // (matches the reference _get_feat_extract_output_lengths).
  static int OutputLength(int mel_frames);

  const AsrAudioConfig& config() const { return config_; }

 private:
  struct DevBuf {
    std::shared_ptr<gpu::UnifiedBuffer> buf;
    float* p = nullptr;
  };
  struct BfBuf {
    std::shared_ptr<gpu::UnifiedBuffer> buf;
    uint16_t* p = nullptr;
  };
  DevBuf Load(const io::ShardedSafeTensors& w, const std::string& name);  // f32
  BfBuf LoadBf16(const io::ShardedSafeTensors& w,
                 const std::string& name);  // bf16

  struct Layer {
    DevBuf ln1_w, ln1_b;  // norms + biases stay f32
    DevBuf q_b, k_b, v_b, o_b;
    BfBuf q_w, k_w, v_w, o_w;  // matmul weights bf16 (cuBLAS)
    DevBuf ln2_w, ln2_b;
    DevBuf fc1_b, fc2_b;
    BfBuf fc1_w, fc2_w;
  };

  AsrAudioConfig config_;
  DevBuf conv1_b_, conv2_b_, conv3_b_;
  BfBuf conv1_w_, conv2_w_, conv3_w_;     // conv weights bf16 (im2col+GEMM)
  BfBuf conv_out_w_, proj1_w_, proj2_w_;  // big matmul weights bf16
  DevBuf ln_post_w_, ln_post_b_, proj1_b_, proj2_b_;
  std::vector<Layer> layers_;
  std::vector<float>
      pe_;  // [max_source_positions * d_model] host sinusoidal PE
  // Per-instance device scratch pool: Forward's working buffers are reused
  // across calls (grow-on-demand) so the steady-state hot path makes no
  // cudaMalloc -- a prerequisite for CUDA-graph capture (Spec 002 P2.2). Each
  // pipeline holds its own tower, so the pool is single-thread per instance.
  mutable gpu::DeviceScratch scratch_;
};

}  // namespace model
}  // namespace orator
