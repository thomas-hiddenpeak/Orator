// TitaNet-Large speaker embedder forward (pure C++/CUDA).
// See include/model/titanet_embedder.h for the architecture/contract.

#include "model/titanet_embedder.h"

#include <cuda_runtime.h>

#include <cmath>
#include <stdexcept>

#include "model/gemm.cuh"

namespace orator {
namespace model {

namespace {

inline void CudaCheck(cudaError_t e, const char* what) {
  if (e != cudaSuccess) {
    throw std::runtime_error(std::string("titanet: ") + what + ": " +
                             cudaGetErrorString(e));
  }
}

constexpr int kThreads = 256;
inline int Blocks(int n) { return (n + kThreads - 1) / kThreads; }

// activation codes: 0 none, 2 ReLU, 4 tanh.
__device__ __forceinline__ float Act(float v, int act) {
  if (act == 2) return fmaxf(v, 0.0f);
  if (act == 4) return tanhf(v);
  return v;
}

// Per-feature (per-channel over time) normalization, NeMo `normalize_batch`
// per_feature: mean over T, unbiased std (N-1) + 1e-5, in place. in [T, C].
// One thread per channel.
__global__ void PerFeatureNormKernel(float* x, int T, int C) {
  int c = blockIdx.x * blockDim.x + threadIdx.x;
  if (c >= C) return;
  double mean = 0.0;
  for (int t = 0; t < T; ++t) mean += x[static_cast<size_t>(t) * C + c];
  mean /= T;
  double ss = 0.0;
  for (int t = 0; t < T; ++t) {
    double d = x[static_cast<size_t>(t) * C + c] - mean;
    ss += d * d;
  }
  double std = sqrt(ss / (T - 1.0)) + 1e-5;
  for (int t = 0; t < T; ++t) {
    size_t i = static_cast<size_t>(t) * C + c;
    x[i] = static_cast<float>((x[i] - mean) / std);
  }
}

// Depthwise conv1d over time, kernel K, padding (K-1)/2, groups=C. in [T,C],
// w [C,1,K], no bias. Out-of-range reads are zero (matches NeMo MaskedConv1d
// over the valid region).
__global__ void DepthwiseConvKernel(const float* __restrict__ in,
                                    const float* __restrict__ w,
                                    float* __restrict__ out, int T, int C,
                                    int K) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx >= T * C) return;
  int c = idx % C, t = idx / C;
  int pad = (K - 1) / 2;
  float acc = 0.0f;
  const float* wc = w + static_cast<size_t>(c) * K;
  for (int kk = 0; kk < K; ++kk) {
    int ti = t + kk - pad;
    if (ti >= 0 && ti < T) acc += in[static_cast<size_t>(ti) * C + c] * wc[kk];
  }
  out[idx] = acc;
}

// BatchNorm1d eval + optional activation. in [T,C], per-channel affine.
__global__ void BatchNormActKernel(float* x, const float* mean,
                                   const float* var, const float* gamma,
                                   const float* beta, int T, int C, float eps,
                                   int act) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx >= T * C) return;
  int c = idx % C;
  float y = (x[idx] - mean[c]) * rsqrtf(var[c] + eps) * gamma[c] + beta[c];
  x[idx] = Act(y, act);
}

// out = ReLU(a + b). in [T,C].
__global__ void AddReluKernel(float* out, const float* a, const float* b,
                              int n) {
  int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i >= n) return;
  out[i] = fmaxf(a[i] + b[i], 0.0f);
}

// out = ReLU(a). in [T,C].
__global__ void ReluKernel(float* out, const float* a, int n) {
  int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i >= n) return;
  out[i] = fmaxf(a[i], 0.0f);
}

// Mean over time per channel -> s[C]. in [T,C]. One thread per channel.
__global__ void ChannelMeanKernel(const float* x, float* s, int T, int C) {
  int c = blockIdx.x * blockDim.x + threadIdx.x;
  if (c >= C) return;
  double acc = 0.0;
  for (int t = 0; t < T; ++t) acc += x[static_cast<size_t>(t) * C + c];
  s[c] = static_cast<float>(acc / T);
}

// x[T,C] *= sigmoid(gate[C]). SqueezeExcite gating.
__global__ void SigmoidScaleKernel(float* x, const float* gate, int T, int C) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx >= T * C) return;
  int c = idx % C;
  x[idx] *= 1.0f / (1.0f + __expf(-gate[c]));
}

// Population mean + std over time per channel (NeMo get_statistics_with_mask
// with uniform mask 1/T; std clamped at eps before sqrt). in [T,C].
__global__ void ChannelMeanStdKernel(const float* x, float* mean, float* std,
                                     int T, int C, float eps) {
  int c = blockIdx.x * blockDim.x + threadIdx.x;
  if (c >= C) return;
  double m = 0.0;
  for (int t = 0; t < T; ++t) m += x[static_cast<size_t>(t) * C + c];
  m /= T;
  double v = 0.0;
  for (int t = 0; t < T; ++t) {
    double d = x[static_cast<size_t>(t) * C + c] - m;
    v += d * d;
  }
  v /= T;
  mean[c] = static_cast<float>(m);
  std[c] = static_cast<float>(sqrt(fmax(v, static_cast<double>(eps))));
}

// ctx[t, :] = [x[t,:], mean, std]  -> width 3*C. in [T,C], out [T, 3C].
__global__ void BuildContextKernel(const float* x, const float* mean,
                                   const float* std, float* ctx, int T, int C) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx >= T * C) return;
  int c = idx % C, t = idx / C;
  size_t row = static_cast<size_t>(t) * 3 * C;
  ctx[row + c] = x[idx];
  ctx[row + C + c] = mean[c];
  ctx[row + 2 * C + c] = std[c];
}

// Softmax over time per channel (column). in/out attn [T,C]. One thread per
// channel. Writes alpha in place.
__global__ void SoftmaxOverTimeKernel(float* attn, int T, int C) {
  int c = blockIdx.x * blockDim.x + threadIdx.x;
  if (c >= C) return;
  float mx = -1e30f;
  for (int t = 0; t < T; ++t) {
    float v = attn[static_cast<size_t>(t) * C + c];
    mx = v > mx ? v : mx;
  }
  double sum = 0.0;
  for (int t = 0; t < T; ++t) {
    double e = exp(static_cast<double>(attn[static_cast<size_t>(t) * C + c] - mx));
    attn[static_cast<size_t>(t) * C + c] = static_cast<float>(e);
    sum += e;
  }
  float inv = static_cast<float>(1.0 / sum);
  for (int t = 0; t < T; ++t) attn[static_cast<size_t>(t) * C + c] *= inv;
}

// Attentive statistics: mu[c]=sum_t a*x, sg[c]=sqrt(sum_t a*(x-mu)^2 clamp eps).
// Writes pooled[c]=mu, pooled[C+c]=sg. x,alpha [T,C].
__global__ void WeightedStatsKernel(const float* x, const float* alpha,
                                    float* pooled, int T, int C, float eps) {
  int c = blockIdx.x * blockDim.x + threadIdx.x;
  if (c >= C) return;
  double mu = 0.0;
  for (int t = 0; t < T; ++t) {
    size_t i = static_cast<size_t>(t) * C + c;
    mu += static_cast<double>(alpha[i]) * x[i];
  }
  double var = 0.0;
  for (int t = 0; t < T; ++t) {
    size_t i = static_cast<size_t>(t) * C + c;
    double d = x[i] - mu;
    var += static_cast<double>(alpha[i]) * d * d;
  }
  pooled[c] = static_cast<float>(mu);
  pooled[C + c] = static_cast<float>(sqrt(fmax(var, static_cast<double>(eps))));
}

}  // namespace

const float* TitaNetEmbedder::W(const std::string& name) const {
  auto it = w_.find(name);
  if (it == w_.end()) throw std::runtime_error("titanet: missing weight " + name);
  return static_cast<const float*>(it->second->data());
}

bool TitaNetEmbedder::Has(const std::string& name) const {
  return w_.find(name) != w_.end();
}

void TitaNetEmbedder::LoadWeights(const std::string& path) {
  if (path.empty()) throw std::invalid_argument("titanet: weights path empty");
  reader_ = std::make_unique<io::SafeTensorReader>(path);

  // Mel front-end with the model's own window + filterbank for exact parity.
  feature::MelConfig mcfg;
  mcfg.sample_rate = config_.sample_rate;
  mcfg.n_fft = config_.n_fft;
  mcfg.n_mels = config_.n_mels;
  const auto& wmeta = reader_->GetMetadata("preprocessor.featurizer.window");
  std::vector<float> window(wmeta.data_size / sizeof(float));
  reader_->ReadWeight("preprocessor.featurizer.window", window.data(),
                      wmeta.data_size);
  const auto& fbmeta = reader_->GetMetadata("preprocessor.featurizer.fb");
  std::vector<float> fb(fbmeta.data_size / sizeof(float));  // [1,80,257]->flat
  reader_->ReadWeight("preprocessor.featurizer.fb", fb.data(), fbmeta.data_size);
  mcfg.win_length = static_cast<int>(window.size());
  mel_ = std::make_unique<feature::MelSpectrogram>(mcfg, window, fb);

  // Load every tensor except the inference-irrelevant classifier head.
  for (const auto& n : reader_->GetWeightNames()) {
    if (n.rfind("decoder.final", 0) == 0) continue;
    const auto& meta = reader_->GetMetadata(n);
    auto buf = std::make_unique<gpu::UnifiedBuffer>(meta.data_size);
    reader_->ReadWeight(n, buf->data(), meta.data_size);
    w_[n] = std::move(buf);
  }

  // Block descriptors (titanet_large model_config.yaml).
  auto mk = [](int in, int out, int k, int rep, bool res, int se,
               const std::string& pfx) {
    Block b;
    b.in_ch = in;
    b.out_ch = out;
    b.kernel = k;
    b.repeats = rep;
    b.residual = res;
    b.se_channels = se;
    b.prefix = pfx;
    if (rep == 1) {
      b.sub_base = {0};
      b.se_index = 3;
    } else {
      b.sub_base = {0, 5, 10};
      b.se_index = 13;
    }
    return b;
  };
  blocks_ = {
      mk(80, 1024, 3, 1, false, 128, "encoder.encoder.0"),
      mk(1024, 1024, 7, 3, true, 128, "encoder.encoder.1"),
      mk(1024, 1024, 11, 3, true, 128, "encoder.encoder.2"),
      mk(1024, 1024, 15, 3, true, 128, "encoder.encoder.3"),
      mk(1024, 3072, 1, 1, false, 384, "encoder.encoder.4"),
  };
  loaded_ = true;
}

// Slot map for DeviceScratch (distinct buffers live simultaneously).
namespace {
enum Slot {
  kLevel0 = 0,
  kLevel1 = 1,
  kDw = 2,
  kPw0 = 3,
  kPw1 = 4,
  kRes = 5,
  kSeS = 6,
  kSeMid = 7,
  kSeGate = 8,
  kGMean = 9,
  kGStd = 10,
  kCtx = 11,
  kAttnA = 12,
  kAttnB = 13,
  kPooled = 14,
  kEmb = 15,
};
}  // namespace

float* TitaNetEmbedder::RunBlock(const Block& b, float* x_in, int T,
                                 int* out_ch_out, cudaStream_t stream) {
  const int T_C = T * b.out_ch;
  float* pw[2] = {scratch_.GetT<float>(kPw0, static_cast<size_t>(T) * b.out_ch),
                  scratch_.GetT<float>(kPw1, static_cast<size_t>(T) * b.out_ch)};
  float* dw = scratch_.GetT<float>(kDw, static_cast<size_t>(T) * b.out_ch);

  float* h = x_in;
  int cur = 0;
  for (int s = 0; s < b.repeats; ++s) {
    const int cin = (s == 0) ? b.in_ch : b.out_ch;
    const std::string base =
        b.prefix + ".mconv." + std::to_string(b.sub_base[s]);
    const std::string dwn = base + ".conv.weight";
    const std::string pwn =
        b.prefix + ".mconv." + std::to_string(b.sub_base[s] + 1) + ".conv.weight";
    const std::string bnn =
        b.prefix + ".mconv." + std::to_string(b.sub_base[s] + 2);
    DepthwiseConvKernel<<<Blocks(T * cin), kThreads, 0, stream>>>(
        h, W(dwn), dw, T, cin, b.kernel);
    gemm::LaunchSgemm(dw, W(pwn), nullptr, pw[cur], T, cin, b.out_ch, 0, stream);
    const int act = (s < b.repeats - 1) ? 2 : 0;  // ReLU except last sub-unit
    BatchNormActKernel<<<Blocks(T_C), kThreads, 0, stream>>>(
        pw[cur], W(bnn + ".running_mean"), W(bnn + ".running_var"),
        W(bnn + ".weight"), W(bnn + ".bias"), T, b.out_ch,
        config_.bn_eps_encoder, act);
    h = pw[cur];
    cur ^= 1;
  }

  // SqueezeExcite: masked mean over time -> Linear(relu) -> Linear -> sigmoid.
  const std::string se =
      b.prefix + ".mconv." + std::to_string(b.se_index) + ".fc";
  float* se_s = scratch_.GetT<float>(kSeS, b.out_ch);
  float* se_mid = scratch_.GetT<float>(kSeMid, b.se_channels);
  float* se_gate = scratch_.GetT<float>(kSeGate, b.out_ch);
  ChannelMeanKernel<<<Blocks(b.out_ch), kThreads, 0, stream>>>(h, se_s, T,
                                                               b.out_ch);
  gemm::LaunchSgemm(se_s, W(se + ".0.weight"), nullptr, se_mid, 1, b.out_ch,
                    b.se_channels, 2, stream);  // ReLU
  gemm::LaunchSgemm(se_mid, W(se + ".2.weight"), nullptr, se_gate, 1,
                    b.se_channels, b.out_ch, 0, stream);
  SigmoidScaleKernel<<<Blocks(T_C), kThreads, 0, stream>>>(h, se_gate, T,
                                                           b.out_ch);

  // Returns the SqueezeExcite result in an internal buffer; the caller applies
  // the residual branch + final ReLU into its own destination level buffer.
  *out_ch_out = b.out_ch;
  return h;
}

void TitaNetEmbedder::RunPooledHead(float* enc, int T, std::vector<float>* out,
                                    cudaStream_t stream) {
  const int C = config_.epilog_dim;  // 3072
  float* gmean = scratch_.GetT<float>(kGMean, C);
  float* gstd = scratch_.GetT<float>(kGStd, C);
  ChannelMeanStdKernel<<<Blocks(C), kThreads, 0, stream>>>(enc, gmean, gstd, T,
                                                           C, config_.pool_eps);
  float* ctx = scratch_.GetT<float>(kCtx, static_cast<size_t>(T) * 3 * C);
  BuildContextKernel<<<Blocks(T * C), kThreads, 0, stream>>>(enc, gmean, gstd,
                                                             ctx, T, C);

  const std::string ap = "decoder._pooling.attention_layer";
  float* attn_a = scratch_.GetT<float>(kAttnA, static_cast<size_t>(T) * 128);
  gemm::LaunchSgemm(ctx, W(ap + ".0.conv_layer.weight"),
                    W(ap + ".0.conv_layer.bias"), attn_a, T, 3 * C, 128, 2,
                    stream);  // ReLU (TDNN: conv -> relu -> bn)
  BatchNormActKernel<<<Blocks(T * 128), kThreads, 0, stream>>>(
      attn_a, W(ap + ".0.bn.running_mean"), W(ap + ".0.bn.running_var"),
      W(ap + ".0.bn.weight"), W(ap + ".0.bn.bias"), T, 128,
      config_.bn_eps_decoder, 4);  // tanh
  float* attn_b = scratch_.GetT<float>(kAttnB, static_cast<size_t>(T) * C);
  gemm::LaunchSgemm(attn_a, W(ap + ".2.weight"), W(ap + ".2.bias"), attn_b, T,
                    128, C, 0, stream);
  SoftmaxOverTimeKernel<<<Blocks(C), kThreads, 0, stream>>>(attn_b, T, C);

  float* pooled = scratch_.GetT<float>(kPooled, 2 * C);  // 6144
  WeightedStatsKernel<<<Blocks(C), kThreads, 0, stream>>>(enc, attn_b, pooled,
                                                          T, C, config_.pool_eps);

  // Embedding head: BatchNorm1d(6144) -> linear(6144 -> 192).
  BatchNormActKernel<<<Blocks(2 * C), kThreads, 0, stream>>>(
      pooled, W("decoder.emb_layers.0.0.running_mean"),
      W("decoder.emb_layers.0.0.running_var"),
      W("decoder.emb_layers.0.0.weight"), W("decoder.emb_layers.0.0.bias"), 1,
      2 * C, config_.bn_eps_decoder, 0);
  float* emb = scratch_.GetT<float>(kEmb, config_.embedding_dim);
  gemm::LaunchSgemm(pooled, W("decoder.emb_layers.0.1.weight"),
                    W("decoder.emb_layers.0.1.bias"), emb, 1, 2 * C,
                    config_.embedding_dim, 0, stream);

  out->assign(config_.embedding_dim, 0.0f);
  CudaCheck(cudaMemcpyAsync(out->data(), emb,
                            config_.embedding_dim * sizeof(float),
                            cudaMemcpyDeviceToHost, stream),
            "copy emb");
  CudaCheck(cudaStreamSynchronize(stream), "sync");

  // L2 normalize on host.
  double nrm = 0.0;
  for (float v : *out) nrm += static_cast<double>(v) * v;
  nrm = std::sqrt(nrm) + 1e-12;
  for (float& v : *out) v = static_cast<float>(v / nrm);
}

std::vector<float> TitaNetEmbedder::Embed(const core::AudioChunk& chunk) {
  if (!loaded_) throw std::runtime_error("titanet: weights not loaded");
  cudaStream_t stream = nullptr;

  // 1. log-mel [T, 80] (frame-major == [T, C]).
  int T = 0;
  std::vector<float> mel_host =
      mel_->Compute(chunk.samples, chunk.num_samples, &T);
  if (T < 2) return std::vector<float>(config_.embedding_dim, 0.0f);

  float* level0 = scratch_.GetT<float>(kLevel0, static_cast<size_t>(T) * 3072);
  float* level1 = scratch_.GetT<float>(kLevel1, static_cast<size_t>(T) * 3072);
  CudaCheck(cudaMemcpyAsync(level0, mel_host.data(),
                            mel_host.size() * sizeof(float),
                            cudaMemcpyHostToDevice, stream),
            "upload mel");

  // 2. per-feature normalization.
  PerFeatureNormKernel<<<Blocks(config_.n_mels), kThreads, 0, stream>>>(
      level0, T, config_.n_mels);

  // 3. encoder: 5 ContextNet blocks, ping-ponging the two level buffers.
  float* x_in = level0;
  for (size_t i = 0; i < blocks_.size(); ++i) {
    const Block& b = blocks_[i];
    float* dst = (x_in == level0) ? level1 : level0;
    int out_ch = 0;
    float* h = RunBlock(b, x_in, T, &out_ch, stream);

    const int n = T * out_ch;
    if (b.residual) {
      float* res = scratch_.GetT<float>(kRes, static_cast<size_t>(T) * out_ch);
      gemm::LaunchSgemm(x_in, W(b.prefix + ".res.0.0.conv.weight"), nullptr,
                        res, T, b.in_ch, out_ch, 0, stream);
      const std::string rbn = b.prefix + ".res.0.1";
      BatchNormActKernel<<<Blocks(n), kThreads, 0, stream>>>(
          res, W(rbn + ".running_mean"), W(rbn + ".running_var"),
          W(rbn + ".weight"), W(rbn + ".bias"), T, out_ch,
          config_.bn_eps_encoder, 0);
      AddReluKernel<<<Blocks(n), kThreads, 0, stream>>>(dst, h, res, n);
    } else {
      ReluKernel<<<Blocks(n), kThreads, 0, stream>>>(dst, h, n);
    }
    x_in = dst;
  }
  CudaCheck(cudaGetLastError(), "encoder kernels");

  // 4. attentive statistics pooling + embedding head.
  std::vector<float> emb;
  RunPooledHead(x_in, T, &emb, stream);
  return emb;
}

}  // namespace model
}  // namespace orator
