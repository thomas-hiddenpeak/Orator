#include "feature/whisper_mel.h"

#include <cuda_runtime.h>

#include <cmath>
#include <cstring>
#include <stdexcept>

#include "gpu/memory.h"

namespace orator {
namespace feature {

using ::orator::gpu::CheckCudaError;

namespace {

constexpr float kPi = 3.14159265358979323846f;

// Reflect an index into [0, n) without repeating the edge sample
// (numpy np.pad mode="reflect" / torch reflect).
__host__ __device__ inline int Reflect(int i, int n) {
  if (n <= 1) return 0;
  const int m = 2 * (n - 1);
  i = i < 0 ? -i : i;
  i %= m;
  if (i >= n) i = m - i;
  return i;
}

// Power spectrum via direct DFT over a reflect-padded, Hann-windowed frame.
// One block per frame; threads cover frequency bins. power: [num_frames, n_freqs].
__global__ void PowerSpectrumKernel(const float* __restrict__ signal,
                                    int num_samples, const float* __restrict__ window,
                                    int n_fft, int hop_length, int n_freqs,
                                    int num_frames, int pad,
                                    float* __restrict__ power) {
  const int frame = blockIdx.x;
  if (frame >= num_frames) return;
  for (int k = threadIdx.x; k < n_freqs; k += blockDim.x) {
    const float ang = -2.0f * kPi * k / n_fft;
    float re = 0.0f, im = 0.0f;
    const int base = frame * hop_length - pad;  // padded->original offset
    for (int w = 0; w < n_fft; ++w) {
      const int s = Reflect(base + w, num_samples);
      const float x = signal[s] * window[w];
      const float a = ang * w;
      re += x * __cosf(a);
      im += x * __sinf(a);
    }
    power[static_cast<size_t>(frame) * n_freqs + k] = re * re + im * im;
  }
}

// mel[m, t] = sum_k filt[m,k] * power[t,k]; written as log10(max(mel, 1e-10)).
// out is [n_mels, num_frames] row-major. One block per (mel, frame) tile.
__global__ void MelLog10Kernel(const float* __restrict__ power, int n_freqs,
                               const float* __restrict__ filt, int n_mels,
                               int num_frames, float* __restrict__ out) {
  const int idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx >= n_mels * num_frames) return;
  const int t = idx % num_frames;
  const int m = idx / num_frames;
  const float* fr = filt + static_cast<size_t>(m) * n_freqs;
  const float* pr = power + static_cast<size_t>(t) * n_freqs;
  float acc = 0.0f;
  for (int k = 0; k < n_freqs; ++k) acc += fr[k] * pr[k];
  acc = acc < 1e-10f ? 1e-10f : acc;
  out[static_cast<size_t>(m) * num_frames + t] = log10f(acc);
}

// Whisper normalization: out = (max(out, gmax - 8) + 4) / 4.
__global__ void WhisperNormKernel(float* __restrict__ out, int n, float gmax) {
  const int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i >= n) return;
  float v = out[i];
  const float floor_v = gmax - 8.0f;
  if (v < floor_v) v = floor_v;
  out[i] = (v + 4.0f) / 4.0f;
}

// Slaney mel scale (matches transformers mel_filter_bank mel_scale="slaney").
double HzToMelSlaney(double hz) {
  const double f_sp = 200.0 / 3.0;
  const double min_log_hz = 1000.0;
  const double min_log_mel = min_log_hz / f_sp;  // 15
  const double logstep = std::log(6.4) / 27.0;
  if (hz >= min_log_hz) return min_log_mel + std::log(hz / min_log_hz) / logstep;
  return hz / f_sp;
}
double MelToHzSlaney(double mel) {
  const double f_sp = 200.0 / 3.0;
  const double min_log_hz = 1000.0;
  const double min_log_mel = min_log_hz / f_sp;  // 15
  const double logstep = std::log(6.4) / 27.0;
  if (mel >= min_log_mel) return min_log_hz * std::exp(logstep * (mel - min_log_mel));
  return f_sp * mel;
}

}  // namespace

WhisperMel::WhisperMel(const WhisperMelConfig& config) : config_(config) {
  BuildHannWindow();
  BuildMelFilterbank();
}

void WhisperMel::BuildHannWindow() {
  const int N = config_.n_fft;
  hann_.resize(N);
  // Periodic Hann (transformers window_function "hann", periodic=True):
  //   w[n] = 0.5 - 0.5*cos(2*pi*n/N)
  for (int n = 0; n < N; ++n) {
    hann_[n] = 0.5f - 0.5f * std::cos(2.0f * kPi * n / N);
  }
}

void WhisperMel::BuildMelFilterbank() {
  const int nfreq = n_freqs();
  const int n_mels = config_.n_mels;
  mel_filters_.assign(static_cast<size_t>(n_mels) * nfreq, 0.0f);

  // FFT bin center frequencies: linspace(0, sr/2, n_freqs).
  std::vector<double> fft_freqs(nfreq);
  for (int k = 0; k < nfreq; ++k)
    fft_freqs[k] = static_cast<double>(config_.sample_rate) / 2.0 * k / (nfreq - 1);

  // n_mels+2 mel points, evenly spaced in mel, mapped back to Hz.
  const double mel_min = HzToMelSlaney(config_.fmin);
  const double mel_max = HzToMelSlaney(config_.fmax);
  std::vector<double> freq_pts(n_mels + 2);
  for (int i = 0; i < n_mels + 2; ++i) {
    const double mel = mel_min + (mel_max - mel_min) * i / (n_mels + 1);
    freq_pts[i] = MelToHzSlaney(mel);
  }

  for (int m = 0; m < n_mels; ++m) {
    const double lower = freq_pts[m];
    const double center = freq_pts[m + 1];
    const double upper = freq_pts[m + 2];
    const double enorm = 2.0 / (upper - lower);  // Slaney area normalization
    for (int k = 0; k < nfreq; ++k) {
      const double f = fft_freqs[k];
      const double down = (f - lower) / (center - lower);
      const double up = (upper - f) / (upper - center);
      double v = down < up ? down : up;
      if (v < 0.0) v = 0.0;
      mel_filters_[static_cast<size_t>(m) * nfreq + k] =
          static_cast<float>(v * enorm);
    }
  }
}

std::vector<float> WhisperMel::Compute(const float* samples, int num_samples,
                                       int* out_num_frames,
                                       cudaStream_t stream,
                                       float* running_max,
                                       int max_valid_from) const {
  const int n_fft = config_.n_fft;
  const int hop = config_.hop_length;
  const int nfreq = n_freqs();
  const int n_mels = config_.n_mels;
  const int pad = n_fft / 2;

  // center=True: padded length = num_samples + 2*pad; frames = 1 + (padded -
  // n_fft)/hop = 1 + num_samples/hop. Whisper then drops the last frame.
  const int frames_full = 1 + num_samples / hop;
  const int num_frames = frames_full - 1;  // drop last
  if (num_frames <= 0) {
    if (out_num_frames) *out_num_frames = 0;
    return {};
  }

    gpu::DeviceBuffer d_sig(sizeof(float) * num_samples);
    gpu::DeviceBuffer d_win(sizeof(float) * n_fft);
    gpu::DeviceBuffer d_filt(sizeof(float) * mel_filters_.size());
    gpu::DeviceBuffer d_power(sizeof(float) * static_cast<size_t>(num_frames) * nfreq);
    gpu::DeviceBuffer d_out(sizeof(float) * static_cast<size_t>(n_mels) * num_frames);

    CheckCudaError(
      cudaMemcpyAsync(d_sig.data(), samples, sizeof(float) * num_samples,
              cudaMemcpyHostToDevice, stream),
      __FILE__, __LINE__);
    CheckCudaError(
      cudaMemcpyAsync(d_win.data(), hann_.data(), sizeof(float) * n_fft,
              cudaMemcpyHostToDevice, stream),
      __FILE__, __LINE__);
    CheckCudaError(
      cudaMemcpyAsync(d_filt.data(), mel_filters_.data(),
              sizeof(float) * mel_filters_.size(), cudaMemcpyHostToDevice,
              stream),
      __FILE__, __LINE__);

  PowerSpectrumKernel<<<num_frames, 128, 0, stream>>>(
      static_cast<float*>(d_sig.data()), num_samples,
      static_cast<float*>(d_win.data()), n_fft, hop, nfreq, num_frames, pad,
      static_cast<float*>(d_power.data()));
  CheckCudaError(cudaGetLastError(), __FILE__, __LINE__);

  const int total = n_mels * num_frames;
  MelLog10Kernel<<<(total + 255) / 256, 256, 0, stream>>>(
      static_cast<float*>(d_power.data()), nfreq,
      static_cast<float*>(d_filt.data()), n_mels, num_frames,
      static_cast<float*>(d_out.data()));
  CheckCudaError(cudaGetLastError(), __FILE__, __LINE__);
      CheckCudaError(cudaStreamSynchronize(stream), __FILE__, __LINE__);

  // Global max over the (kept) log-mel for the Whisper floor/normalization.
  std::vector<float> logmel(static_cast<size_t>(total));
  CheckCudaError(
      cudaMemcpyAsync(logmel.data(), d_out.data(), sizeof(float) * logmel.size(),
                      cudaMemcpyDeviceToHost, stream),
      __FILE__, __LINE__);
  CheckCudaError(cudaStreamSynchronize(stream), __FILE__, __LINE__);

  // Local maximum over the valid frame range [max_valid_from, num_frames) for
  // every mel bin (logmel is row-major [n_mels, num_frames]).
  float local_max = -1e30f;
  if (max_valid_from <= 0) {
    for (int i = 0; i < total; ++i)
      local_max = logmel[static_cast<size_t>(i)] > local_max
                      ? logmel[static_cast<size_t>(i)]
                      : local_max;
  } else {
    for (int m = 0; m < n_mels; ++m)
      for (int fr = max_valid_from; fr < num_frames; ++fr) {
        const float v = logmel[static_cast<size_t>(m) * num_frames + fr];
        local_max = v > local_max ? v : local_max;
      }
  }

  // Fold into the caller's running per-segment maximum so windowed calls match
  // a single full-segment computation.
  float gmax = local_max;
  if (running_max != nullptr) {
    gmax = *running_max > local_max ? *running_max : local_max;
    *running_max = gmax;
  }

  WhisperNormKernel<<<(total + 255) / 256, 256, 0, stream>>>(
      static_cast<float*>(d_out.data()), total, gmax);
  CheckCudaError(cudaGetLastError(), __FILE__, __LINE__);
  CheckCudaError(cudaStreamSynchronize(stream), __FILE__, __LINE__);

  std::vector<float> out(static_cast<size_t>(n_mels) * num_frames);
  CheckCudaError(
      cudaMemcpyAsync(out.data(), d_out.data(), sizeof(float) * out.size(),
                      cudaMemcpyDeviceToHost, stream),
      __FILE__, __LINE__);
  CheckCudaError(cudaStreamSynchronize(stream), __FILE__, __LINE__);
  if (out_num_frames) *out_num_frames = num_frames;
  return out;
}

}  // namespace feature
}  // namespace orator
