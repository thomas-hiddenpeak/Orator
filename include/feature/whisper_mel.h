#pragma once

// Whisper-style log-mel front-end for Qwen3-ASR.
//
// Replicates transformers WhisperFeatureExtractor exactly:
//   - reflect-pad n_fft/2 on each side (spectrogram center=True)
//   - hann periodic window of length n_fft (=400)
//   - power spectrum (|STFT|^2) via direct DFT (no external FFT dependency)
//   - Slaney mel filterbank [n_mels=128, n_freqs=201]
//   - log10, drop last frame, then log_spec = max(log_spec, max-8); (.+4)/4
//
// Output: row-major [n_mels=128, num_frames] FP32 on the host (the engine casts
// to BF16 before the conv front-end). num_frames = num_samples / hop_length.

#include <vector>

namespace orator {
namespace feature {

struct WhisperMelConfig {
  int sample_rate = 16000;
  int n_fft = 400;       // = win_length = frame_length
  int hop_length = 160;
  int n_mels = 128;
  float fmin = 0.0f;
  float fmax = 8000.0f;  // sample_rate / 2
};

class WhisperMel {
 public:
  explicit WhisperMel(const WhisperMelConfig& config = {});

  // Computes the log-mel spectrogram for a mono 16 kHz signal.
  // Returns row-major [n_mels * num_frames]; *out_num_frames gets num_frames.
  std::vector<float> Compute(const float* samples, int num_samples,
                             int* out_num_frames) const;

  int n_mels() const { return config_.n_mels; }
  int n_freqs() const { return config_.n_fft / 2 + 1; }

 private:
  void BuildHannWindow();
  void BuildMelFilterbank();

  WhisperMelConfig config_;
  std::vector<float> hann_;         // [n_fft] periodic Hann
  std::vector<float> mel_filters_;  // [n_mels * n_freqs] row-major
};

}  // namespace feature
}  // namespace orator
