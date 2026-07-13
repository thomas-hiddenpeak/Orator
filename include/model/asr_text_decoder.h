#pragma once

// Qwen3-ASR text decoder (thinker.model). Matmul weights are kept in native
// BF16 and run through cuBLAS tensor-core GEMM (FP32 accumulate); the small
// norm weights are FP32 and feed the verified FP32 elementwise kernels.
//
// 28-layer Qwen3 decoder operating on input embeddings (audio features already
// injected at the audio_pad positions by the engine):
//   RMSNorm(input_ln) -> q/k/v proj -> per-head q_norm/k_norm (RMSNorm on
//     head_dim) -> rotate_half RoPE -> causal GQA w/ KV cache -> o_proj + res
//   RMSNorm(post_ln) -> SwiGLU MLP -> + res
//   final RMSNorm -> lm_head (tied embed_tokens) -> logits[vocab]
//
// Verified against the FP32 PyTorch oracle (argmax + close logits) and end to
// end against the oracle transcript.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <cuda_runtime.h>

#include "gpu/memory.h"
#include "io/sharded_safetensor.h"

namespace orator {
namespace model {

struct AsrTextConfig {
  int hidden_size = 2048;
  int num_layers = 28;
  int num_q_heads = 16;
  int num_kv_heads = 8;
  int head_dim = 128;
  int intermediate_size = 6144;
  int vocab_size = 151936;
  float rms_norm_eps = 1e-6f;
  float rope_theta = 1000000.0f;
  int max_seq_len = 2048;
  bool profile = false;
  bool cuda_graph_enabled = true;
};

class AsrTextDecoder {
 public:
  explicit AsrTextDecoder(const AsrTextConfig& config = {});

  void LoadWeights(const io::ShardedSafeTensors& weights);

  void ResetCache();

  // Look up the embedding for a token id into `out` [hidden_size] (host).
  void Embed(int token_id, float* out) const;
  int hidden_size() const { return config_.hidden_size; }
  int vocab_size() const { return config_.vocab_size; }

  // Prefill T input embeddings [T, hidden] (host); populate the KV cache and
  // leave logits for the last token on the device (queryable via Argmax).
  void Prefill(const float* embeds, int T, cudaStream_t stream = 0);

  // Append T input embeddings [T, hidden] (host) to the KV cache STARTING at
  // absolute position `pos0` (instead of resetting to 0). The existing KV at
  // positions [0, pos0) is left intact, so a streaming caller can keep the
  // already-encoded audio-block KV across steps and prefill only the new
  // tokens. Leaves logits for the last appended token on the device. `Prefill`
  // is the special case pos0 == 0 (after ResetCache).
  void PrefillAt(const float* embeds, int T, int pos0, cudaStream_t stream = 0);

  // Set the logical cache length back to `len` (the KV buffers are retained).
  // Used by the streaming caller to drop the suffix + generated KV after a step
  // while keeping the persistent audio-block KV for the next step.
  void TruncateCache(int len) { cache_len_ = len; }

  // Current logical cache length (number of valid KV positions).
  int cache_len() const { return cache_len_; }

  // Decode one step from embedding [hidden] (host) at absolute position `pos`.
  void DecodeStep(const float* embed, int pos, cudaStream_t stream = 0);

  // Fused GPU decode step: gathers the embedding for the token id held in the
  // device scalar `d_token` (e.g. the previous Argmax result), runs the forward
  // at absolute position `pos`, and leaves logits on the device. No host embed
  // lookup and no D->H sync of the embedding -> the autoregressive loop stays
  // on the GPU between Argmax calls.
  void DecodeStepDevice(const int* d_token, int pos, cudaStream_t stream = 0);

  // GPU argmax over the current logits, banning up to two ids (-1 = none).
  int Argmax(int ban0 = -1, int ban1 = -1, cudaStream_t stream = 0);

  // GPU argmax that writes the winning id to a device scalar (no host sync).
  void ArgmaxToDevice(int* d_out, int ban0 = -1, int ban1 = -1,
                      cudaStream_t stream = 0);

  // Read a device int scalar to host (single 4-byte copy + sync).
  int ReadTokenId(const int* d_token, cudaStream_t stream = 0) const;

  // Greedy autoregressive decode driven entirely on the GPU. Seeds from the
  // current prefill logits, emits up to max_new tokens, stops at eos0/eos1 or a
  // 6-in-a-row repetition. The per-token body (embed-gather -> 28-layer forward
  // -> argmax) is captured ONCE as a CUDA graph and replayed back-to-back in
  // batches of `batch` steps with a single host sync per batch -- so the GPU
  // never idles between tokens (lets DVFS boost off the min clock). The first
  // `ban_steps` tokens are decoded eagerly with EOS banned (matches the
  // reference engine). Returns the emitted token ids (EOS excluded). The graph
  // is captured on `capture_stream_` (internal) and launched on `stream`.
  std::vector<int> DecodeGreedy(int start_pos, int max_new, int eos0, int eos1,
                                int ban_steps = 3, int batch = 32,
                                cudaStream_t stream = 0);

  // Copy the current logits to host (for verification only).
  std::vector<float> CopyLogits() const;

 private:
  struct F32Buf {
    std::shared_ptr<gpu::UnifiedBuffer> buf;
    float* p = nullptr;
  };
  struct BfBuf {
    std::shared_ptr<gpu::UnifiedBuffer> buf;
    uint16_t* p = nullptr;
  };
  F32Buf LoadF32(const io::ShardedSafeTensors& w, const std::string& name);
  BfBuf LoadBf16(const io::ShardedSafeTensors& w, const std::string& name);

  struct Layer {
    F32Buf in_ln, q_norm, k_norm, post_ln;           // small norm weights (f32)
    BfBuf q_w, k_w, v_w, o_w, gate_w, up_w, down_w;  // matmul weights (bf16)
    // Fused projections (built at load time, parts then released): one GEMV for
    // q|k|v and one for gate|up cuts per-token kernel launches on the decode
    // hot path. qkv_w = [Qd+2*KVd, Hh], gateup_w = [2*I, Hh]; outputs are
    // contiguous so q/k/v and gate/up are read as offset slices. Numerically
    // identical.
    BfBuf qkv_w, gateup_w;
  };

  void Forward(float* d_x, int Tq, int pos0, cudaStream_t stream = 0);
  // Records the per-token (Tq==1) forward onto `stream`, reading the absolute
  // position from `d_pos` (device int). Used for both eager launch and graph
  // capture. d_x is the fixed step buffer step_x_.
  void DecodeForwardOnStream(float* d_x, const int* d_pos, cudaStream_t stream);
  float* Work(int which, size_t floats);  // persistent, grow-on-demand

  AsrTextConfig config_;
  BfBuf embed_, lm_head_;  // bf16; lm_head aliases embed_ when tied
  F32Buf final_norm_;
  std::vector<Layer> layers_;
  // Plain host copy of embed_ weights (bf16) for the Embed() function. Reading
  // from plain host memory is safe regardless of what GPU kernels are in flight
  // (no managed-memory migration hazard on Tegra).
  std::vector<uint16_t> embed_host_;

  std::vector<std::shared_ptr<gpu::UnifiedBuffer>> k_cache_, v_cache_;
  int cache_len_ = 0;

  std::vector<std::shared_ptr<gpu::UnifiedBuffer>> work_;
  std::vector<size_t> work_cap_;
  std::shared_ptr<gpu::UnifiedBuffer> d_logits_, d_argmax_;

  // CUDA-graph fast path for the per-token (M=1) decode step. The forward is
  // launch-bound (~370 tiny kernels) AND the per-token host sync pins the GPU
  // at its min DVFS clock. We capture the body ONCE and replay it back-to-back
  // for a whole batch on one stream with no intervening host work, so the GPU
  // stays saturated (DVFS boosts) and launch overhead is paid once. Every
  // mutable input lives in a device scalar (token id, position, output index)
  // so a single captured graph serves all steps.
  void* graph_exec_ = nullptr;  // cudaGraphExec_t (one decode step, no ban)
  void* graph_exec_banned_ = nullptr;  // cudaGraphExec_t (one step, EOS banned)
  int graph_ban0_ = -1,
      graph_ban1_ = -1;  // EOS ids baked into the banned graph
  bool graph_ready_ = false;
  std::shared_ptr<gpu::UnifiedBuffer> step_x_;  // [hidden] gathered embedding
  // Pinned (page-locked) host memory: device writes, host reads after stream
  // sync. Safe on Tegra even when another stream is still in flight.
  std::shared_ptr<gpu::PinnedBuffer> step_pos_;  // [1] absolute position
  std::shared_ptr<gpu::PinnedBuffer> d_tok_;     // [1] current token id
  std::shared_ptr<gpu::PinnedBuffer> d_count_;   // [1] emitted-token counter
  std::shared_ptr<gpu::PinnedBuffer> d_out_;     // [max] recorded token ids
  cudaStream_t capture_stream_ = nullptr;

  // Records the per-token decode body onto `s` (embed-gather d_tok_ -> forward
  // at step_pos_ -> argmax(ban0,ban1) -> record into d_out_[d_count_], advance
  // counters). RecordDecodeBody is the ban-free form captured into the graph.
  void DecodeBodyImpl(cudaStream_t s, int ban0, int ban1);
  void RecordDecodeBody(cudaStream_t s) { DecodeBodyImpl(s, -1, -1); }
};

}  // namespace model
}  // namespace orator
