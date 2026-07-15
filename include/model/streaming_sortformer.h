#pragma once

// Streaming Sortformer diarizer.
//
// Implements core::IDiarizer. Configuration and streaming-state fields mirror
// NVIDIA's `diar_streaming_sortformer_4spk-v2` and v2.1 checkpoints (see
// third_party/streaming_sortformer/research_notes.txt). The full CUDA forward
// pass is implemented (mel -> pre_encode -> 17 Conformer layers -> encoder_proj
// + 18 transformer layers -> speaker head -> sigmoid) and verified numerically
// against the NeMo reference (see tools/verify_forward.cc and
// tools/verify_streaming.cc). The streaming path is bit-identical to the
// offline ProcessChunk over the same audio
// (tools/verify_streaming_incremental).

#include <cstdint>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "core/stages.h"
#include "feature/mel_spectrogram.h"
#include "gpu/device_scratch.h"
#include "gpu/memory.h"
#include "io/safetensor.h"
#include "model/conformer_layer.h"
#include "model/conformer_preencode.h"
#include "model/sortformer_decoder.h"

namespace orator {
namespace model {

// Tunable streaming parameters that can be adjusted at runtime.
// Zero/negative values mean "keep current default".
struct SortformerTuning {
  int spkcache_len = 0;
  int chunk_len = 0;
  int spkcache_update_period = 0;
  int chunk_left_context = -1;
  int chunk_right_context = -1;
  int spkcache_sil_frames = -1;
  int fifo_len = -1;
  int show_progress = -1;
};

// Model-specific architecture/streaming parameters (from model_config.yaml).
struct SortformerConfig {
  int sample_rate = 16000;
  int mel_features = 128;
  int n_fft = 512;
  float window_size_sec = 0.025f;
  float hop_size_sec = 0.01f;

  int max_num_speakers = 4;

  int encoder_d_model = 512;
  int encoder_subsampling_factor = 8;

  int transformer_layers = 18;
  int transformer_hidden_size = 192;
  int transformer_heads = 8;

  int spkcache_len = 188;
  int fifo_len = 0;
  int chunk_len = 340;
  int spkcache_update_period = 300;
  int chunk_left_context = 1;
  int chunk_right_context = 40;
  int spkcache_sil_frames_per_spk = 3;
  bool show_progress = false;

  bool Validate() const;
  // diar frame period = hop * subsampling (e.g. 0.01 * 8 = 0.08s).
  double FramePeriodSec() const {
    return static_cast<double>(hop_size_sec) * encoder_subsampling_factor;
  }
};

// Host-side state mirroring NeMo StreamingSortformerState.
// Tensors here are tiny (<=188*512 floats), inherently sequential control data,
// so they live on the CPU while the heavy encoder/decoder run on the GPU.
struct HostStreamState {
  std::vector<float> spkcache;        // [spk_len * fc_d_model]
  std::vector<float> spkcache_preds;  // [spk_len * n_spk]
  int spk_len = 0;
  bool spkcache_preds_valid = false;  // mirrors "spkcache_preds is not None"
  std::vector<float> mean_sil_emb;    // [fc_d_model]
  long n_sil_frames = 0;
  // FIFO for async streaming. Valid FIFO frames are included between spkcache
  // and the current chunk on every forward. Overflow transfers the oldest
  // frames to spkcache without discarding current-chunk evidence.
  std::vector<float> fifo_embs;   // [fifo_max_len * fc_d_model]
  std::vector<float> fifo_preds;  // [fifo_max_len * n_spk]
  int fifo_max_len = 0;
  int fifo_count = 0;
  void Clear() {
    spkcache.clear();
    spkcache_preds.clear();
    spk_len = 0;
    spkcache_preds_valid = false;
    std::fill(mean_sil_emb.begin(), mean_sil_emb.end(), 0.0f);
    n_sil_frames = 0;
    fifo_embs.clear();
    fifo_preds.clear();
    fifo_max_len = 0;
    fifo_count = 0;
  }
  // Lazy-init FIFO buffers when needed (called on first chunk when
  // fifo_len > 0). emb_dim = fc_d_model, n_spk = max_speakers.
  void InitFifo(int emb_dim, int n_spk, int cap) {
    fifo_max_len = cap;
    fifo_embs.assign(static_cast<size_t>(cap) * emb_dim, 0.0f);
    fifo_preds.assign(static_cast<size_t>(cap) * n_spk, 0.0f);
    fifo_count = 0;
  }
};

// Streaming state mirroring NeMo's StreamingSortformerState.
struct SortformerState {
  gpu::UnifiedBuffer spkcache;
  gpu::UnifiedBuffer spkcache_lengths;
  gpu::UnifiedBuffer spkcache_preds;
  gpu::UnifiedBuffer fifo;
  gpu::UnifiedBuffer fifo_lengths;
  gpu::UnifiedBuffer fifo_preds;
  gpu::UnifiedBuffer spk_perm;
  gpu::UnifiedBuffer mean_sil_emb;
  gpu::UnifiedBuffer n_sil_frames;

  explicit SortformerState(const SortformerConfig& cfg);
  void Clear();
};

class SortformerDiarizer final : public core::IDiarizer {
 public:
  SortformerDiarizer();
  explicit SortformerDiarizer(const SortformerConfig& cfg);

  void Initialize(const core::DiarizationConfig& config) override;
  void LoadWeights(const std::string& path) override;
  void Reset() override;
  void ApplyStreamingTuning(const SortformerTuning& tuning);
  core::DiarizationFrames ProcessChunk(const core::AudioChunk& chunk) override;

  // Incremental real-time streaming. Feeds `num_samples` new mono 16k samples
  // and returns ONLY the diarization frames newly finalized by this call (may
  // be empty if not enough audio has accumulated for a chunk). State (mel
  // continuity + speaker cache) persists across calls so speaker identity is
  // stable for the whole session; call Reset() to start a new session. Pass
  // final=true on the last call to flush the trailing partial chunk. Compute is
  // O(total audio) and memory is bounded regardless of session length, and the
  // emitted frames are bit-identical to the offline ProcessChunk over the same
  // audio. The returned frames' t_start_sec is the absolute stream time.
  // `stream` is the CUDA stream for all GPU work in this call.
  core::DiarizationFrames StreamAudio(const float* samples, int num_samples,
                                      bool final, cudaStream_t stream) override;

  int max_speakers() const override { return config_.max_num_speakers; }
  double frame_period_sec() const override { return config_.FramePeriodSec(); }
  std::string name() const override { return "sortformer"; }

  const SortformerConfig& config() const { return config_; }

  // Streaming forward over a full freq-major log-mel tensor [n_mels, t_mel]
  // (valid_mel real frames). Mirrors NeMo forward_streaming: chunked encode
  // with persistent speaker cache, returning concatenated per-chunk sigmoids.
  // Resets streaming state on entry. t_start_sec sets the output time origin.
  core::DiarizationFrames RunStreaming(const float* mel_fm, int n_mels,
                                       int t_mel, int valid_mel,
                                       double t_start_sec);

 private:
  // Runs xscale -> 17 Conformer layers -> decoder on a host emb sequence
  // [T, fc_d_model] and returns masked per-frame sigmoids [T, n_spk].
  std::vector<float> ForwardEncoderDecoder(const std::vector<float>& emb_seq,
                                           int T, int valid,
                                           cudaStream_t stream = nullptr);

  // One streaming chunk step over a freq-major mel buffer covering absolute
  // frames [buf_base_frame, buf_base_frame+buf_len). Advances the persistent
  // stream_state_ and appends the chunk's center sigmoids to out_preds. Shared
  // by the offline RunStreaming loop and the incremental StreamAudio path.
  void StreamMelChunk(const float* mel_buf, int n_mels, int buf_len,
                      long buf_base_frame, long stt_abs, long valid_abs,
                      long avail_abs, std::vector<float>& out_preds,
                      int& out_frames, cudaStream_t stream = nullptr);

  SortformerConfig config_;
  bool initialized_ = false;
  bool weights_loaded_ = false;
  std::string weights_path_;
  double stream_time_sec_ = 0.0;
  std::unique_ptr<SortformerState> state_;

  // Real CUDA forward (offline path: mel -> pre_encode -> 17 Conformer layers
  // -> encoder_proj + 18 transformer + speaker head -> sigmoid). Verified
  // numerically against NeMo (see tools/verify_forward.cc).
  std::unique_ptr<io::SafeTensorReader> reader_;
  std::unique_ptr<feature::MelSpectrogram> mel_;
  std::unique_ptr<ConformerPreEncode> pre_encode_;
  std::vector<std::unique_ptr<ConformerLayer>> conformer_layers_;
  std::unique_ptr<SortformerDecoder> decoder_;
  int num_conformer_layers_ = 17;
  HostStreamState stream_state_;
  // Device scratch for ForwardEncoderDecoder's per-call dx/dpe buffers (single
  // diarization worker thread -> single-thread-of-control per instance).
  gpu::DeviceScratch enc_scratch_;

  // --- Incremental real-time streaming state (persists across StreamAudio
  // calls for speaker-identity continuity; reset by Reset()). ---
  std::vector<float>
      sig_;           // pre-emphasized signal; sig_[0] = abs sample sig_abs_
  long sig_abs_ = 0;  // absolute sample index of sig_[0]
  float last_raw_ = 0.0f;        // last raw sample, for pre-emphasis continuity
  bool stream_started_ = false;  // false until the first sample of the session
  std::vector<float>
      mel_seq_;  // freq-major [mel_features, mel_w_]; col0 = frame mel_base_
  long mel_base_ = 0;   // absolute mel-frame index of mel_seq_ column 0
  long mel_avail_ = 0;  // absolute count of mel frames computed so far
  int mel_w_ = 0;       // columns currently held in mel_seq_
  long stt_feat_ = 0;   // absolute mel-frame index of next chunk to process
  long emitted_frames_ = 0;  // diar frames emitted so far (output time origin)

  // Appends new pre-emphasized samples (cheap; no GPU work).
  void AppendRaw(const float* samples, int num_samples);
  // Computes newly-stable mel frames up to the current stable horizon (or all
  // floor(total/hop) frames when final). Batched: called at chunk granularity.
  void EnsureMel(bool final, cudaStream_t stream = nullptr);
  // Drops mel columns / signal samples no longer needed for future chunks.
  void TrimStreamingBuffers();
};

}  // namespace model
}  // namespace orator
