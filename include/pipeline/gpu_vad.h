#pragma once

// GpuVad: the Silero speech-endpoint detector with its per-window compute on the
// GPU, driven in batches (Spec 004 Phase 5, FR6).
//
// The legacy detector (AsrSileroVad) runs the per-window STFT, convolutional
// encoder, and LSTM on the CPU, one window at a time, synchronously on the
// streaming thread. When the endpoint pipeline falls behind and a large span of
// audio is drained at once, that path spends a long stretch on a single CPU core
// with the GPU idle. GpuVad removes that hazard: each buffered read is processed
// as ONE batched GPU pass over all ready 512-sample windows.
//
// Batching is sound because a window's STFT input is
// [64 prior audio samples | 512 window samples] -- the "context" is prior AUDIO,
// not recurrent compute state -- so the STFT and the convolutional encoder are
// independent per window and run as one batch. Only the LSTM hidden/cell
// recurrence is sequential; it is a single scan over the batch carried in shared
// memory. The endpoint state machine (speech/silence accumulation) then runs on
// the host over the per-window probabilities (no compute, just counters).
//
// Numerics: GpuVad reproduces AsrSileroVad's fp32 computation with the same
// weights; test_vad gates the per-window probability against the CPU reference
// within a recorded tolerance (the CPU implementation is the reference of
// record for the endpoint detector).

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <cuda_runtime.h>

namespace orator {
namespace pipeline {

class GpuVad {
 public:
  struct Params {
    int sample_rate = 16000;
    std::string silero_model_path = "models/asr/silero_vad.safetensors";
    float silero_threshold = 0.5f;
    int silero_min_speech_ms = 250;
    int silero_min_silence_ms = 120;
    int silero_speech_pad_ms = 60;  // accepted for parity; unused for endpoints
    cudaStream_t stream = nullptr;  // Spec 002: dedicated CUDA stream (lock-free)
  };

  explicit GpuVad(const Params& params);
  ~GpuVad();

  GpuVad(const GpuVad&) = delete;
  GpuVad& operator=(const GpuVad&) = delete;

  // Append audio to the staging buffer (host side; cheap).
  void Push(const float* samples, int n);

  // Process every ready full window on the GPU in one batch and append each
  // completed SPEECH SEGMENT as an absolute-sample [start,end) pair to *segs
  // (the VAD pipeline's data: voice-activity regions on the common time base).
  // `finalize` flushes a still-open speech segment at end of stream. Runs on its
  // own dedicated CUDA stream (Spec 002); no GPU lock is acquired.
  void DrainSegments(bool finalize, std::vector<std::pair<long, long>>* segs);

  // Accumulated GPU compute time (for the VAD track's real-time-factor meta).
  double compute_sec() const { return compute_sec_; }

  // Clear streaming state (audio staging, LSTM state, endpoint counters).
  void Reset();

  long base_sample() const { return next_window_abs_; }

  // Numeric-gate probe: from a fresh state, return the per-window speech
  // probability for every full window in [pcm, pcm+n). Used by test_vad to
  // compare against the CPU reference. Does not affect streaming state of a
  // separate instance.
  std::vector<float> DebugWindowProbs(const float* pcm, int n);

 private:
  void InitModel();
  void UploadWeights();
  // Run the GPU pipeline over `n_windows` consecutive windows whose extended
  // input (64-sample history + n_windows*512 audio) is in host `ext`. Writes
  // n_windows probabilities to *probs. Advances the LSTM device state.
  void RunBatch(const float* ext, int n_windows, std::vector<float>* probs);

  Params params_;
  cudaStream_t stream_;

  // Host weights (loaded from safetensors, then uploaded to device).
  std::vector<float> stft_basis_;     // [258*256]
  std::vector<float> enc_w_[4];
  std::vector<float> enc_b_[4];
  std::vector<float> lstm_wih_;       // [512*128]
  std::vector<float> lstm_whh_;       // [512*128]
  std::vector<float> lstm_bih_;       // [512]
  std::vector<float> lstm_bhh_;       // [512]
  std::vector<float> dec_w_;          // [128]
  float dec_b_ = 0.0f;

  // Device weights.
  float* d_stft_basis_ = nullptr;
  float* d_enc_w_[4] = {nullptr, nullptr, nullptr, nullptr};
  float* d_enc_b_[4] = {nullptr, nullptr, nullptr, nullptr};
  float* d_lstm_wih_ = nullptr;
  float* d_lstm_whh_ = nullptr;
  float* d_lstm_bih_ = nullptr;
  float* d_lstm_bhh_ = nullptr;
  float* d_dec_w_ = nullptr;

  // Device scratch (sized for kMaxBatch windows).
  float* d_ext_ = nullptr;
  float* d_padded_ = nullptr;
  float* d_mag_ = nullptr;
  float* d_enc0_ = nullptr;
  float* d_enc1_ = nullptr;
  float* d_enc2_ = nullptr;
  float* d_enc3_ = nullptr;
  float* d_prob_ = nullptr;

  // Device LSTM state (carried across batches).
  float* d_h_ = nullptr;  // [128]
  float* d_c_ = nullptr;  // [128]

  // Host audio staging with a 64-sample history prefix. buf_[win_start_-64 ..
  // win_start_] is the context for the window at win_start_.
  std::vector<float> buf_;
  int win_start_ = 64;
  long next_window_abs_ = 0;  // absolute sample index of buf_[win_start_]

  // Endpoint state machine (host).
  bool in_speech_ = false;
  int speech_samples_ = 0;
  int silence_samples_ = 0;
  long seg_start_abs_ = 0;       // absolute sample where the open speech segment began
  double compute_sec_ = 0.0;
};

}  // namespace pipeline
}  // namespace orator
