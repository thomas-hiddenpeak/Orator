// GpuVad: GPU-batched Silero speech-endpoint detector (Spec 004 Phase 5, FR6).
// See include/pipeline/gpu_vad.h for the design rationale.

#include "pipeline/gpu_vad.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <mutex>
#include <stdexcept>

#include <cuda_runtime.h>

#include "gpu/memory.h"
#include "io/safetensor.h"

namespace orator {
namespace pipeline {

// CUDA_CHECK (gpu/memory.h) expands to an unqualified CheckCudaError; bring the
// gpu-namespace symbol into scope so it resolves inside orator::pipeline.
using gpu::CheckCudaError;

namespace {

// Per-window detector dimensions (identical to the CPU AsrSileroVad).
constexpr int kContext = 64;
constexpr int kWindow = 512;
constexpr int kInput = 576;   // context + window
constexpr int kPadded = 640;  // reflect-padded input (pad 64 on the right)
constexpr int kNfft = 256;
constexpr int kHop = 128;
constexpr int kStftBins = 129;
constexpr int kStftFrames = 4;
constexpr int kHidden = 128;

// Upper bound on windows processed per GPU launch (bounds scratch memory). One
// buffered read larger than this is processed as several sequential sub-batches
// that share the carried LSTM state, so correctness is unchanged.
constexpr int kMaxBatch = 2048;

int CeilDiv(int a, int b) { return (a + b - 1) / b; }

// Build the reflect-padded input for each window from the extended audio
// [64 history | N*512 audio]. Window n's input is ext[n*512 .. n*512+576];
// padded right by 64 via reflection (out[576+i] = in[574-i]).
__global__ void BuildPaddedKernel(const float* ext, float* padded, int n_win) {
  const int idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx >= n_win * kPadded) return;
  const int n = idx / kPadded;
  const int j = idx % kPadded;
  float v;
  if (j < kInput) {
    v = ext[n * kWindow + j];
  } else {
    v = ext[n * kWindow + (2 * kInput - 2 - j)];  // 1150 - j
  }
  padded[n * kPadded + j] = v;
}

// STFT via the fixed conv basis, then magnitude. For each (window n, bin f,
// frame t): real/imag are dot products of a 256-sample slice with basis rows
// f and (129+f); magnitude = sqrt(re^2 + im^2). mag layout [n][f*4 + t].
__global__ void StftMagKernel(const float* padded, const float* basis,
                              float* mag, int n_win) {
  const int idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx >= n_win * kStftBins * kStftFrames) return;
  const int t = idx % kStftFrames;
  const int f = (idx / kStftFrames) % kStftBins;
  const int n = idx / (kStftFrames * kStftBins);
  const float* pad = padded + n * kPadded + t * kHop;
  const float* wr = basis + f * kNfft;
  const float* wi = basis + (kStftBins + f) * kNfft;
  float re = 0.0f, im = 0.0f;
  for (int k = 0; k < kNfft; ++k) {
    const float p = pad[k];
    re += p * wr[k];
    im += p * wi[k];
  }
  mag[n * (kStftBins * kStftFrames) + f * kStftFrames + t] =
      sqrtf(re * re + im * im);
}

// Batched 1D convolution (optionally followed by ReLU). Channel-major layout:
// input[n][ci*Lin + pos], weight[co*Cin*K + ci*K + k], output[n][co*Lout + t].
__global__ void Conv1dReluKernel(const float* in, const float* w,
                                 const float* b, float* out, int n_win, int Cin,
                                 int Lin, int Cout, int K, int stride, int pad,
                                 int Lout, int do_relu) {
  const int idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx >= n_win * Cout * Lout) return;
  const int t = idx % Lout;
  const int co = (idx / Lout) % Cout;
  const int n = idx / (Lout * Cout);
  const float* inb = in + n * Cin * Lin;
  float sum = b[co];
  const int t_start = t * stride - pad;
  for (int ci = 0; ci < Cin; ++ci) {
    const float* wr = w + co * Cin * K + ci * K;
    const float* ir = inb + ci * Lin;
    for (int k = 0; k < K; ++k) {
      const int pos = t_start + k;
      if (pos >= 0 && pos < Lin) sum += ir[pos] * wr[k];
    }
  }
  if (do_relu && sum < 0.0f) sum = 0.0f;
  out[n * Cout * Lout + co * Lout + t] = sum;
}

// Sequential LSTM scan over the batch (carried h/c) plus the linear+sigmoid
// decoder, producing one speech probability per window. One block of 128
// threads (one per hidden unit); h is held in shared memory across the scan.
__global__ void LstmDecoderKernel(const float* lstm_in, const float* wih,
                                  const float* whh, const float* bih,
                                  const float* bhh, const float* dec_w,
                                  float dec_b, float* h_state, float* c_state,
                                  float* prob, int n_win) {
  const int i = threadIdx.x;  // hidden unit index
  __shared__ float sh_h[kHidden];
  __shared__ float sh_x[kHidden];
  __shared__ float sh_red[kHidden];
  float c = c_state[i];
  sh_h[i] = h_state[i];
  __syncthreads();

  for (int s = 0; s < n_win; ++s) {
    sh_x[i] = lstm_in[s * kHidden + i];
    __syncthreads();

    float g0 = bih[0 * kHidden + i] + bhh[0 * kHidden + i];
    float g1 = bih[1 * kHidden + i] + bhh[1 * kHidden + i];
    float g2 = bih[2 * kHidden + i] + bhh[2 * kHidden + i];
    float g3 = bih[3 * kHidden + i] + bhh[3 * kHidden + i];
    const float* w0 = wih + (0 * kHidden + i) * kHidden;
    const float* w1 = wih + (1 * kHidden + i) * kHidden;
    const float* w2 = wih + (2 * kHidden + i) * kHidden;
    const float* w3 = wih + (3 * kHidden + i) * kHidden;
    const float* u0 = whh + (0 * kHidden + i) * kHidden;
    const float* u1 = whh + (1 * kHidden + i) * kHidden;
    const float* u2 = whh + (2 * kHidden + i) * kHidden;
    const float* u3 = whh + (3 * kHidden + i) * kHidden;
    for (int j = 0; j < kHidden; ++j) {
      const float x = sh_x[j];
      const float h = sh_h[j];
      g0 += w0[j] * x + u0[j] * h;
      g1 += w1[j] * x + u1[j] * h;
      g2 += w2[j] * x + u2[j] * h;
      g3 += w3[j] * x + u3[j] * h;
    }
    const float gi = 1.0f / (1.0f + expf(-g0));
    const float gf = 1.0f / (1.0f + expf(-g1));
    const float gg = tanhf(g2);
    const float go = 1.0f / (1.0f + expf(-g3));
    c = gf * c + gi * gg;
    const float h_new = go * tanhf(c);
    __syncthreads();  // all threads finished reading sh_h for this step

    sh_h[i] = h_new;
    sh_red[i] = (h_new > 0.0f ? h_new : 0.0f) * dec_w[i];
    __syncthreads();

    for (int stride = kHidden / 2; stride > 0; stride >>= 1) {
      if (i < stride) sh_red[i] += sh_red[i + stride];
      __syncthreads();
    }
    if (i == 0) prob[s] = 1.0f / (1.0f + expf(-(dec_b + sh_red[0])));
    __syncthreads();
  }

  h_state[i] = sh_h[i];
  c_state[i] = c;
}

template <typename T>
void LoadVec(const io::SafeTensorReader& r, const char* name,
             std::vector<T>* dst) {
  const auto meta = r.GetMetadata(name);
  const size_t n = static_cast<size_t>(meta.data_size) / sizeof(T);
  dst->resize(n);
  r.ReadWeight(name, dst->data(), n * sizeof(T));
}

float* DeviceUpload(gpu::DeviceScratch& pool, int slot,
                    const std::vector<float>& host) {
  float* d = pool.GetT<float>(slot, host.size());
  CUDA_CHECK(cudaMemcpy(d, host.data(), host.size() * sizeof(float),
                        cudaMemcpyHostToDevice));
  return d;
}

}  // namespace

GpuVad::GpuVad(const Params& params) : params_(params), stream_(params.stream) {
  InitModel();
  UploadWeights();
  buf_.assign(kContext, 0.0f);
  win_start_ = kContext;
  next_window_abs_ = 0;
}

GpuVad::~GpuVad() { FreeDeviceMemory(); }

void GpuVad::FreeDeviceMemory() {
  // dev_buffers_ (DeviceScratch) owns all device memory and frees it on
  // destruction; here we only drop the non-owning views. LoadWeights calls this
  // then UploadWeights, which re-acquires the same slots from the pool.
  d_stft_basis_ = nullptr;
  for (int i = 0; i < 4; ++i) {
    d_enc_w_[i] = nullptr;
    d_enc_b_[i] = nullptr;
  }
  d_lstm_wih_ = nullptr;
  d_lstm_whh_ = nullptr;
  d_lstm_bih_ = nullptr;
  d_lstm_bhh_ = nullptr;
  d_dec_w_ = nullptr;
  d_ext_ = nullptr;
  d_padded_ = nullptr;
  d_mag_ = nullptr;
  d_enc0_ = nullptr;
  d_enc1_ = nullptr;
  d_enc2_ = nullptr;
  d_enc3_ = nullptr;
  d_prob_ = nullptr;
  d_h_ = nullptr;
  d_c_ = nullptr;
}

void GpuVad::Initialize(const core::VadConfig& config) {
  params_.sample_rate = config.sample_rate;
  params_.silero_threshold = config.threshold;
  params_.silero_min_speech_ms = config.min_speech_ms;
  params_.silero_min_silence_ms = config.min_silence_ms;
}

void GpuVad::LoadWeights(const std::string& path) {
  params_.silero_model_path = path;
  FreeDeviceMemory();
  InitModel();
  UploadWeights();
}

void GpuVad::InitModel() {
  io::SafeTensorReader reader(params_.silero_model_path);
  LoadVec(reader, "stft.forward_basis_buffer", &stft_basis_);
  const char* enc_w_names[] = {
      "encoder.0.reparam_conv.weight", "encoder.1.reparam_conv.weight",
      "encoder.2.reparam_conv.weight", "encoder.3.reparam_conv.weight"};
  const char* enc_b_names[] = {
      "encoder.0.reparam_conv.bias", "encoder.1.reparam_conv.bias",
      "encoder.2.reparam_conv.bias", "encoder.3.reparam_conv.bias"};
  for (int i = 0; i < 4; ++i) {
    LoadVec(reader, enc_w_names[i], &enc_w_[i]);
    LoadVec(reader, enc_b_names[i], &enc_b_[i]);
  }
  LoadVec(reader, "decoder.rnn.weight_ih", &lstm_wih_);
  LoadVec(reader, "decoder.rnn.weight_hh", &lstm_whh_);
  LoadVec(reader, "decoder.rnn.bias_ih", &lstm_bih_);
  LoadVec(reader, "decoder.rnn.bias_hh", &lstm_bhh_);
  LoadVec(reader, "decoder.decoder.2.weight", &dec_w_);
  if (reader.Has("decoder.decoder.2.bias")) {
    std::vector<float> b;
    LoadVec(reader, "decoder.decoder.2.bias", &b);
    dec_b_ = b.empty() ? 0.0f : b[0];
  }
  if (dec_w_.size() > static_cast<size_t>(kHidden)) {
    dec_w_.resize(kHidden);
  } else if (dec_w_.size() < static_cast<size_t>(kHidden)) {
    throw std::runtime_error("GpuVad: decoder weight has invalid size");
  }
}

void GpuVad::UploadWeights() {
  // Fixed scratch-slot map for dev_buffers_ (the owning pool). Re-uploading
  // (LoadWeights) reuses the same slots: GetT returns the existing buffer and
  // DeviceUpload re-copies, so no reallocation churn.
  d_stft_basis_ = DeviceUpload(dev_buffers_, 0, stft_basis_);
  for (int i = 0; i < 4; ++i) {
    d_enc_w_[i] = DeviceUpload(dev_buffers_, 1 + i, enc_w_[i]);
    d_enc_b_[i] = DeviceUpload(dev_buffers_, 5 + i, enc_b_[i]);
  }
  d_lstm_wih_ = DeviceUpload(dev_buffers_, 9, lstm_wih_);
  d_lstm_whh_ = DeviceUpload(dev_buffers_, 10, lstm_whh_);
  d_lstm_bih_ = DeviceUpload(dev_buffers_, 11, lstm_bih_);
  d_lstm_bhh_ = DeviceUpload(dev_buffers_, 12, lstm_bhh_);
  d_dec_w_ = DeviceUpload(dev_buffers_, 13, dec_w_);

  d_ext_ = dev_buffers_.GetT<float>(14, kContext + kMaxBatch * kWindow);
  d_padded_ = dev_buffers_.GetT<float>(15, kMaxBatch * kPadded);
  d_mag_ = dev_buffers_.GetT<float>(16, kMaxBatch * kStftBins * kStftFrames);
  d_enc0_ = dev_buffers_.GetT<float>(17, kMaxBatch * 128 * 4);
  d_enc1_ = dev_buffers_.GetT<float>(18, kMaxBatch * 64 * 2);
  d_enc2_ = dev_buffers_.GetT<float>(19, kMaxBatch * 64 * 1);
  d_enc3_ = dev_buffers_.GetT<float>(20, kMaxBatch * 128 * 1);
  d_prob_ = dev_buffers_.GetT<float>(21, kMaxBatch);
  d_h_ = dev_buffers_.GetT<float>(22, kHidden);
  d_c_ = dev_buffers_.GetT<float>(23, kHidden);
  CUDA_CHECK(cudaMemset(d_h_, 0, kHidden * sizeof(float)));
  CUDA_CHECK(cudaMemset(d_c_, 0, kHidden * sizeof(float)));
}

void GpuVad::Push(const float* samples, int n) {
  if (samples == nullptr || n <= 0) return;
  buf_.insert(buf_.end(), samples, samples + n);
}

void GpuVad::RunBatch(const float* ext, int n_windows,
                      std::vector<float>* probs) {
  probs->resize(n_windows);
  const auto t0 = std::chrono::steady_clock::now();
  // Spec 002: VAD runs on its own dedicated CUDA stream (lock-free).
  // Stream is owned by the scheduler; we only hold a handle.
  const int kThreads = 256;
  int done = 0;
  while (done < n_windows) {
    const int nb = std::min(n_windows - done, kMaxBatch);
    CUDA_CHECK(cudaMemcpyAsync(d_ext_, ext + done * kWindow,
                               (kContext + nb * kWindow) * sizeof(float),
                               cudaMemcpyHostToDevice, stream_));

    BuildPaddedKernel<<<CeilDiv(nb * kPadded, kThreads), kThreads, 0,
                        stream_>>>(d_ext_, d_padded_, nb);
    StftMagKernel<<<CeilDiv(nb * kStftBins * kStftFrames, kThreads), kThreads,
                    0, stream_>>>(d_padded_, d_stft_basis_, d_mag_, nb);
    // enc0: Cin=129 Lin=4 -> Cout=128 Lout=4, K3 s1 p1, relu
    Conv1dReluKernel<<<CeilDiv(nb * 128 * 4, kThreads), kThreads, 0, stream_>>>(
        d_mag_, d_enc_w_[0], d_enc_b_[0], d_enc0_, nb, 129, 4, 128, 3, 1, 1, 4,
        1);
    // enc1: Cin=128 Lin=4 -> Cout=64 Lout=2, K3 s2 p1, relu
    Conv1dReluKernel<<<CeilDiv(nb * 64 * 2, kThreads), kThreads, 0, stream_>>>(
        d_enc0_, d_enc_w_[1], d_enc_b_[1], d_enc1_, nb, 128, 4, 64, 3, 2, 1, 2,
        1);
    // enc2: Cin=64 Lin=2 -> Cout=64 Lout=1, K3 s2 p1, relu
    Conv1dReluKernel<<<CeilDiv(nb * 64 * 1, kThreads), kThreads, 0, stream_>>>(
        d_enc1_, d_enc_w_[2], d_enc_b_[2], d_enc2_, nb, 64, 2, 64, 3, 2, 1, 1,
        1);
    // enc3: Cin=64 Lin=1 -> Cout=128 Lout=1, K3 s1 p1, relu
    Conv1dReluKernel<<<CeilDiv(nb * 128 * 1, kThreads), kThreads, 0, stream_>>>(
        d_enc2_, d_enc_w_[3], d_enc_b_[3], d_enc3_, nb, 64, 1, 128, 3, 1, 1, 1,
        1);
    // LSTM scan + decoder (one block, carried state).
    LstmDecoderKernel<<<1, kHidden, 0, stream_>>>(
        d_enc3_, d_lstm_wih_, d_lstm_whh_, d_lstm_bih_, d_lstm_bhh_, d_dec_w_,
        dec_b_, d_h_, d_c_, d_prob_, nb);
    CUDA_CHECK(cudaGetLastError());
    CUDA_CHECK(cudaMemcpyAsync(probs->data() + done, d_prob_,
                               nb * sizeof(float), cudaMemcpyDeviceToHost,
                               stream_));
    // Wait for this sub-batch's probs to be ready before the host reads them
    // in the endpoint state machine (DrainSegments).
    CUDA_CHECK(cudaStreamSynchronize(stream_));
    done += nb;
  }
  compute_sec_ +=
      std::chrono::duration<double>(std::chrono::steady_clock::now() - t0)
          .count();
}

void GpuVad::DrainSegments(bool finalize,
                           std::vector<core::VadSegmentResult>* segs) {
  if (segs == nullptr) return;
  const int avail = static_cast<int>(buf_.size()) - win_start_;
  const int n_windows = avail / kWindow;
  const int sr = params_.sample_rate;
  const int min_speech = params_.silero_min_speech_ms * sr / 1000;
  const int min_silence = params_.silero_min_silence_ms * sr / 1000;

  if (n_windows > 0) {
    const float* ext = buf_.data() + (win_start_ - kContext);
    std::vector<float> probs;
    RunBatch(ext, n_windows, &probs);

    for (int i = 0; i < n_windows; ++i) {
      const float p = probs[i];
      const long win_end_abs =
          next_window_abs_ + static_cast<long>(i + 1) * kWindow;
      if (p >= params_.silero_threshold) {
        silence_samples_ = 0;
        speech_samples_ += kWindow;
        if (!in_speech_ && speech_samples_ >= min_speech) {
          in_speech_ = true;
          // Speech began speech_samples_ ago (when the silence counter reset).
          seg_start_abs_ = win_end_abs - speech_samples_;
        }
      } else if (in_speech_) {
        silence_samples_ += kWindow;
        if (silence_samples_ >= min_silence) {
          in_speech_ = false;
          // Speech actually stopped where this silence run began.
          const long seg_end = win_end_abs - silence_samples_;
          if (seg_end > seg_start_abs_)
            segs->push_back({seg_start_abs_, seg_end});
          speech_samples_ = 0;
        }
      } else {
        speech_samples_ = 0;
      }
    }

    win_start_ += n_windows * kWindow;
    next_window_abs_ += static_cast<long>(n_windows) * kWindow;
    const int drop = win_start_ - kContext;  // keep 64 samples of history
    if (drop > 0) {
      buf_.erase(buf_.begin(), buf_.begin() + drop);
      win_start_ = kContext;
    }
  }

  if (finalize && in_speech_) {
    // Flush the open speech segment up to the last processed sample.
    if (next_window_abs_ > seg_start_abs_)
      segs->push_back({seg_start_abs_, next_window_abs_});
    in_speech_ = false;
    speech_samples_ = 0;
    silence_samples_ = 0;
  }
}

void GpuVad::Reset() {
  buf_.assign(kContext, 0.0f);
  win_start_ = kContext;
  next_window_abs_ = 0;
  in_speech_ = false;
  speech_samples_ = 0;
  silence_samples_ = 0;
  if (d_h_) CUDA_CHECK(cudaMemset(d_h_, 0, kHidden * sizeof(float)));
  if (d_c_) CUDA_CHECK(cudaMemset(d_c_, 0, kHidden * sizeof(float)));
}

std::vector<float> GpuVad::DebugWindowProbs(const float* pcm, int n) {
  Reset();
  Push(pcm, n);
  std::vector<float> probs;
  const int avail = static_cast<int>(buf_.size()) - win_start_;
  const int n_windows = avail / kWindow;
  if (n_windows > 0) {
    const float* ext = buf_.data() + (win_start_ - kContext);
    RunBatch(ext, n_windows, &probs);
  }
  return probs;
}

}  // namespace pipeline
}  // namespace orator
