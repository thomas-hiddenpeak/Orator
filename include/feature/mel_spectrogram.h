#pragma once

#include <cuda_runtime.h>

#include <vector>

#include "gpu/device_scratch.h"

// Log-mel spectrogram: the real Streaming Sortformer audio front-end.
//
// Matches the model's preprocessor config (model_config.yaml):
//   sample_rate=16000, window=25ms (400), hop=10ms (160),
//   n_fft=512, n_mels=128, Hann window, log magnitude.
//
// Computation runs on the GPU (direct DFT + triangular mel filterbank), keeping
// with the project's GPU-first rule and avoiding any external FFT dependency.

namespace orator {
namespace feature {

struct MelConfig {
  int sample_rate = 16000;
  int n_fft = 512;
  int win_length = 400;  // 25 ms @ 16 kHz
  int hop_length = 160;  // 10 ms @ 16 kHz
  int n_mels = 128;
  float fmin = 0.0f;
  float fmax = 8000.0f;  // Nyquist
  float log_eps = 1e-5f;
  // NeMo AudioToMelSpectrogramPreprocessor parity:
  float preemph = 0.97f;  // pre-emphasis coefficient (0 to disable)
  bool center = true;     // torch.stft center=True (constant pad n_fft/2)
  float log_zero_guard = 5.9604644775390625e-08f;  // 2^-24, log(x + guard)
};

class MelSpectrogram {
 public:
  explicit MelSpectrogram(const MelConfig& config);

  // Constructs with externally supplied window + mel filterbank (e.g. loaded
  // from the model's safetensors: preprocessor.featurizer.window [win_length]
  // and preprocessor.featurizer.fb [n_mels, n_freqs]). This guarantees exact
  // numerical parity with the trained model's front-end.
  MelSpectrogram(const MelConfig& config, const std::vector<float>& window,
                 const std::vector<float>& filterbank);

  // Computes log-mel features for a mono signal.
  // Returns row-major [num_frames * n_mels]; *out_num_frames receives the
  // number of frames produced.
  std::vector<float> Compute(const float* samples, int num_samples,
                             int* out_num_frames,
                             cudaStream_t stream = nullptr) const;

  // Streaming frame producer for continuous real-time use. `sig` is an
  // already-pre-emphasized signal buffer; the first produced frame's window
  // starts at `input_offset` samples into `sig`. Out-of-range reads (start pad
  // or final-tail pad) are zero, matching torch.stft(center=True). Returns
  // frame-major [num_frames * n_mels], bit-identical to Compute over the same
  // underlying samples. Pre-emphasis continuity is the caller's responsibility.
  std::vector<float> ComputeStreamFrames(const float* sig, int num_samples,
                                         int input_offset, int num_frames,
                                         cudaStream_t stream = nullptr) const;

  int n_mels() const { return config_.n_mels; }
  int n_freqs() const { return config_.n_fft / 2 + 1; }
  const MelConfig& config() const { return config_; }

  int NumFrames(int num_samples) const {
    if (config_.center) {
      // torch.stft(center=True) yields 1 + L/hop frames; NeMo's get_seq_len
      // truncates to floor(L/hop) valid frames.
      return num_samples / config_.hop_length;
    }
    if (num_samples < config_.win_length) return 0;
    return 1 + (num_samples - config_.win_length) / config_.hop_length;
  }

 private:
  MelConfig config_;
  std::vector<float> hann_;         // [win_length]
  std::vector<float> mel_filters_;  // [n_mels * n_freqs]

  void BuildHannWindow();
  void BuildMelFilterbank();
  // Shared GPU STFT+mel core (used by both Compute and ComputeStreamFrames).
  // `stream` is the CUDA stream for kernel launches and synchronization.
  std::vector<float> RunStftMel(const float* sig, int num_samples,
                                int input_offset, int num_frames,
                                cudaStream_t stream) const;
  // Per-instance device scratch for RunStftMel's working buffers (one diar
  // worker -> single-thread-of-control per instance).
  mutable gpu::DeviceScratch scratch_;
};

}  // namespace feature
}  // namespace orator
