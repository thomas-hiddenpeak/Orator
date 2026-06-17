#include "feature/mel_spectrogram.h"

#include <cuda_runtime.h>

#include <cmath>
#include <stdexcept>
#include <vector>

#include "gpu/memory.h"

namespace orator {
namespace feature {

using ::orator::gpu::CheckCudaError;  // enables CUDA_CHECK macro in this TU

namespace {

inline float HzToMel(float hz) {
  return 2595.0f * std::log10(1.0f + hz / 700.0f);
}
inline float MelToHz(float mel) {
  return 700.0f * (std::pow(10.0f, mel / 2595.0f) - 1.0f);
}

// One thread per (frame, freq bin). Replicates torch.stft(center=True,
// pad_mode="constant") with a win_length<=n_fft window centered inside the
// n_fft frame, then the power spectrum (mag_power=2 -> re^2+im^2).
__global__ void PowerSpectrumKernel(const float* signal, int num_samples,
                                    const float* window, int win_length,
                                    int hop_length, int n_fft, int n_freqs,
                                    int num_frames, int pad_left, int win_off,
                                    int input_offset, float* power) {
  const int idx = blockIdx.x * blockDim.x + threadIdx.x;
  const long total = static_cast<long>(num_frames) * n_freqs;
  if (idx >= total) return;

  const int frame = idx / n_freqs;
  const int k = idx % n_freqs;
  // start index into the (possibly offset) signal buffer
  const int base = frame * hop_length + input_offset;

  float re = 0.0f;
  float im = 0.0f;
  const float two_pi_k = -2.0f * 3.14159265358979323846f * k / n_fft;
  for (int w = 0; w < win_length; ++w) {
    const int j = win_off + w;            // position within the n_fft frame
    const int s = base + j - pad_left;    // index into the real signal
    if (s < 0 || s >= num_samples) continue;  // constant (zero) padding
    const float x = signal[s] * window[w];
    const float angle = two_pi_k * j;
    re += x * cosf(angle);
    im += x * sinf(angle);
  }
  power[idx] = re * re + im * im;  // mag_power = 2.0
}

// One thread per (frame, mel bin): mel = fb . power, then log(x + guard).
__global__ void MelLogKernel(const float* power, int n_freqs,
                             const float* mel_filters, int n_mels,
                             int num_frames, float log_guard, float* mel_out) {
  const int idx = blockIdx.x * blockDim.x + threadIdx.x;
  const long total = static_cast<long>(num_frames) * n_mels;
  if (idx >= total) return;

  const int frame = idx / n_mels;
  const int m = idx % n_mels;
  const float* pw = power + static_cast<long>(frame) * n_freqs;
  const float* filt = mel_filters + static_cast<long>(m) * n_freqs;

  float acc = 0.0f;
  for (int k = 0; k < n_freqs; ++k) {
    acc += filt[k] * pw[k];
  }
  mel_out[idx] = logf(acc + log_guard);
}

}  // namespace

MelSpectrogram::MelSpectrogram(const MelConfig& config) : config_(config) {
  if (config_.win_length > config_.n_fft) {
    throw std::invalid_argument("win_length must be <= n_fft");
  }
  BuildHannWindow();
  BuildMelFilterbank();
}

MelSpectrogram::MelSpectrogram(const MelConfig& config,
                               const std::vector<float>& window,
                               const std::vector<float>& filterbank)
    : config_(config) {
  if (config_.win_length > config_.n_fft) {
    throw std::invalid_argument("win_length must be <= n_fft");
  }
  if (static_cast<int>(window.size()) != config_.win_length) {
    throw std::invalid_argument("window size must equal win_length");
  }
  if (static_cast<int>(filterbank.size()) !=
      config_.n_mels * (config_.n_fft / 2 + 1)) {
    throw std::invalid_argument("filterbank size must be n_mels * n_freqs");
  }
  hann_ = window;
  mel_filters_ = filterbank;
}

void MelSpectrogram::BuildHannWindow() {
  hann_.resize(config_.win_length);
  const int N = config_.win_length;
  // Periodic Hann (torch.hann_window default periodic=True): denominator N.
  for (int n = 0; n < N; ++n) {
    hann_[n] = 0.5f * (1.0f - std::cos(2.0f * 3.14159265358979323846f * n / N));
  }
}

void MelSpectrogram::BuildMelFilterbank() {
  const int n_freqs = this->n_freqs();
  const int n_mels = config_.n_mels;
  mel_filters_.assign(static_cast<size_t>(n_mels) * n_freqs, 0.0f);

  const float mel_min = HzToMel(config_.fmin);
  const float mel_max = HzToMel(config_.fmax);
  std::vector<float> mel_points(n_mels + 2);
  for (int i = 0; i < n_mels + 2; ++i) {
    const float mel = mel_min + (mel_max - mel_min) * i / (n_mels + 1);
    mel_points[i] = MelToHz(mel);
  }

  std::vector<float> bin_freq(n_freqs);
  for (int k = 0; k < n_freqs; ++k) {
    bin_freq[k] = static_cast<float>(k) * config_.sample_rate / config_.n_fft;
  }

  for (int m = 0; m < n_mels; ++m) {
    const float left = mel_points[m];
    const float center = mel_points[m + 1];
    const float right = mel_points[m + 2];
    for (int k = 0; k < n_freqs; ++k) {
      const float f = bin_freq[k];
      float w = 0.0f;
      if (f >= left && f <= center && center > left) {
        w = (f - left) / (center - left);
      } else if (f > center && f <= right && right > center) {
        w = (right - f) / (right - center);
      }
      mel_filters_[static_cast<size_t>(m) * n_freqs + k] = w;
    }
  }
}

std::vector<float> MelSpectrogram::Compute(const float* samples, int num_samples,
                                           int* out_num_frames) const {
  const int num_frames = NumFrames(num_samples);
  if (out_num_frames) *out_num_frames = num_frames;
  if (num_frames <= 0) return {};

  // Pre-emphasis on host: y[0]=x[0]; y[n]=x[n]-preemph*x[n-1].
  std::vector<float> sig(samples, samples + num_samples);
  if (config_.preemph != 0.0f && num_samples > 1) {
    float prev = sig[0];
    for (int n = 1; n < num_samples; ++n) {
      const float cur = sig[n];
      sig[n] = cur - config_.preemph * prev;
      prev = cur;
    }
  }

  return RunStftMel(sig.data(), num_samples, /*input_offset=*/0, num_frames,
                    /*stream=*/nullptr);
}

// Streaming frame producer: computes `num_frames` log-mel frames from an
// already-pre-emphasized signal buffer, where the first produced frame's window
// starts at `input_offset` samples into `sig`. Out-of-range samples (start or
// final-tail) read as zero, matching torch.stft(center=True, pad="constant").
// Returns frame-major [num_frames * n_mels], bit-identical to the offline path.
std::vector<float> MelSpectrogram::ComputeStreamFrames(const float* sig,
                                                        int num_samples,
                                                        int input_offset,
                                                        int num_frames,
                                                        cudaStream_t stream) const {
  if (num_frames <= 0) return {};
  return RunStftMel(sig, num_samples, input_offset, num_frames, stream);
}

std::vector<float> MelSpectrogram::RunStftMel(const float* sig, int num_samples,
                                              int input_offset,
                                              int num_frames,
                                              cudaStream_t stream) const {
  const int n_freqs = this->n_freqs();
  const int n_mels = config_.n_mels;
  const int pad_left = config_.center ? config_.n_fft / 2 : 0;
  const int win_off = (config_.n_fft - config_.win_length) / 2;

  gpu::DeviceBuffer d_samples(static_cast<size_t>(num_samples) * sizeof(float));
  gpu::DeviceBuffer d_win(static_cast<size_t>(config_.win_length) * sizeof(float));
  gpu::DeviceBuffer d_filters(mel_filters_.size() * sizeof(float));
  gpu::DeviceBuffer d_power(static_cast<size_t>(num_frames) * n_freqs *
                            sizeof(float));
  gpu::DeviceBuffer d_mel(static_cast<size_t>(num_frames) * n_mels *
                          sizeof(float));

  gpu::GpuMemory::CopyHostToDevice(d_samples.data(), sig,
                                    static_cast<size_t>(num_samples) * sizeof(float));
  gpu::GpuMemory::CopyHostToDevice(d_win.data(), hann_.data(),
                                    hann_.size() * sizeof(float));
  gpu::GpuMemory::CopyHostToDevice(d_filters.data(), mel_filters_.data(),
                                    mel_filters_.size() * sizeof(float));

  const int block = 256;
  const long power_total = static_cast<long>(num_frames) * n_freqs;
  const int power_grid = static_cast<int>((power_total + block - 1) / block);
  PowerSpectrumKernel<<<power_grid, block, 0, stream>>>(
      static_cast<const float*>(d_samples.data()), num_samples,
      static_cast<const float*>(d_win.data()), config_.win_length,
      config_.hop_length, config_.n_fft, n_freqs, num_frames, pad_left, win_off,
      input_offset, static_cast<float*>(d_power.data()));
  CUDA_CHECK(cudaGetLastError());

  const long mel_total = static_cast<long>(num_frames) * n_mels;
  const int mel_grid = static_cast<int>((mel_total + block - 1) / block);
  MelLogKernel<<<mel_grid, block, 0, stream>>>(
      static_cast<const float*>(d_power.data()), n_freqs,
      static_cast<const float*>(d_filters.data()), n_mels, num_frames,
      config_.log_zero_guard, static_cast<float*>(d_mel.data()));
  CUDA_CHECK(cudaGetLastError());
  CUDA_CHECK(cudaStreamSynchronize(stream));

  std::vector<float> mel(static_cast<size_t>(num_frames) * n_mels);
  CUDA_CHECK(cudaMemcpyAsync(mel.data(), d_mel.data(),
                              mel.size() * sizeof(float), cudaMemcpyDeviceToHost,
                              stream));
  CUDA_CHECK(cudaStreamSynchronize(stream));
  return mel;
}

}  // namespace feature
}  // namespace orator
