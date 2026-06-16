#include "model/conformer_preencode.h"

#include <cuda_runtime.h>

#include <cmath>
#include <stdexcept>

#include "gpu/memory.h"
#include "model/gemm.cuh"

namespace orator {
namespace model {

using ::orator::gpu::CheckCudaError;
using ::orator::gpu::UnifiedAllocator;
using ::orator::gpu::UnifiedBuffer;

namespace {

// Grouped 2D convolution. Layout: in[Cin,H,W], w[Cout, Cin/groups, KH, KW],
// bias[Cout], out[Cout, Hout, Wout]. Symmetric padding `pad`, square stride.
__global__ void Conv2dKernel(const float* __restrict__ in,
                             const float* __restrict__ w,
                             const float* __restrict__ bias, float* __restrict__ out,
                             int Cin, int H, int W, int Cout, int KH, int KW,
                             int Hout, int Wout, int stride, int pad, int groups) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  int total = Cout * Hout * Wout;
  if (idx >= total) return;
  int ox = idx % Wout;
  int oy = (idx / Wout) % Hout;
  int oc = idx / (Wout * Hout);

  int cinG = Cin / groups;
  int coutG = Cout / groups;
  int group = oc / coutG;
  int ic0 = group * cinG;

  float acc = bias ? bias[oc] : 0.0f;
  for (int kc = 0; kc < cinG; ++kc) {
    int ic = ic0 + kc;
    const float* inC = in + static_cast<size_t>(ic) * H * W;
    const float* wC = w + ((static_cast<size_t>(oc) * cinG + kc) * KH) * KW;
    for (int ky = 0; ky < KH; ++ky) {
      int iy = oy * stride - pad + ky;
      if (iy < 0 || iy >= H) continue;
      for (int kx = 0; kx < KW; ++kx) {
        int ix = ox * stride - pad + kx;
        if (ix < 0 || ix >= W) continue;
        acc += inC[iy * W + ix] * wC[ky * KW + kx];
      }
    }
  }
  out[idx] = acc;
}

__global__ void ReluKernel(float* x, int n) {
  int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i < n && x[i] < 0.0f) x[i] = 0.0f;
}

// Transpose [R,C] row-major -> [C,R] row-major: out[c,r] = in[r,c].
__global__ void TransposeKernel(const float* __restrict__ in,
                                float* __restrict__ out, int R, int C) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx >= R * C) return;
  int c = idx % C;
  int r = idx / C;
  out[static_cast<size_t>(c) * R + r] = in[idx];
}

// Add per-channel bias to a channel-major [Cout, HW] tensor: out[oc,p]+=bias[oc].
__global__ void AddChannelBiasKernel(float* __restrict__ x,
                                     const float* __restrict__ bias, int Cout,
                                     int HW) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx >= Cout * HW) return;
  x[idx] += bias[idx / HW];
}

// Zero time rows >= valid in a [C,H,W] tensor.
__global__ void MaskTimeKernel(float* x, int C, int H, int W, int valid) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  int total = C * H * W;
  if (idx >= total) return;
  int h = (idx / W) % H;
  if (h >= valid) x[idx] = 0.0f;
}

// Gather [C,T,F] (channel-major per frame) into row-major [T, C*F] so that
// the output projection becomes a standard Linear(in=C*F -> D) GEMM.
// flat[t, c*F+f] = x[(c*T+t)*F + f].
__global__ void FlattenGatherKernel(const float* __restrict__ x,
                                    float* __restrict__ flat, int C, int T,
                                    int F) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  int K = C * F;
  if (idx >= T * K) return;
  int k = idx % K;
  int t = idx / K;
  int c = k / F;
  int f = k % F;
  flat[idx] = x[(static_cast<size_t>(c) * T + t) * F + f];
}

int ConvOutLen(int L, int k, int s, int pad) {
  return (L + 2 * pad - k) / s + 1;
}

std::unique_ptr<UnifiedBuffer> CopyWeight(const io::SafeTensorReader& reader,
                                          const std::string& name) {
  const auto& meta = reader.GetMetadata(name);
  auto buf = std::make_unique<UnifiedBuffer>(meta.data_size);
  reader.ReadWeight(name, buf->data(), meta.data_size);
  return buf;
}

void LaunchConv(const float* in, const UnifiedBuffer& w, const UnifiedBuffer& b,
                float* out, int Cin, int H, int W, int Cout, int K, int stride,
                int pad, int groups, int* Hout, int* Wout) {
  *Hout = ConvOutLen(H, K, stride, pad);
  *Wout = ConvOutLen(W, K, stride, pad);
  int total = Cout * (*Hout) * (*Wout);
  int threads = 256;
  Conv2dKernel<<<(total + threads - 1) / threads, threads>>>(
      in, static_cast<const float*>(w.data()), static_cast<const float*>(b.data()),
      out, Cin, H, W, Cout, K, K, *Hout, *Wout, stride, pad, groups);
  CUDA_CHECK(cudaGetLastError());
}

void LaunchRelu(float* x, int n) {
  int threads = 256;
  ReluKernel<<<(n + threads - 1) / threads, threads>>>(x, n);
  CUDA_CHECK(cudaGetLastError());
}

// 1x1 pointwise convolution (groups=1) as a GEMM. in[Cin,HW], w[Cout,Cin],
// out[Cout,HW]: out[oc,p] = bias[oc] + sum_ic w[oc,ic]*in[ic,p].
// Transpose in -> in_T[HW,Cin], then out[Cout,HW] = w[Cout,Cin] * in_T^T.
void LaunchPointwise(const float* in, const UnifiedBuffer& w,
                     const UnifiedBuffer& b, float* out, int Cin, int Cout,
                     int HW) {
  UnifiedBuffer in_t(static_cast<size_t>(HW) * Cin * sizeof(float));
  int threads = 256;
  int total = Cin * HW;
  TransposeKernel<<<(total + threads - 1) / threads, threads>>>(
      in, static_cast<float*>(in_t.data()), Cin, HW);
  CUDA_CHECK(cudaGetLastError());
  gemm::LaunchSgemm(static_cast<const float*>(w.data()),
                    static_cast<const float*>(in_t.data()),
                    /*bias=*/nullptr, out, Cout, Cin, HW, 0);
  CUDA_CHECK(cudaGetLastError());
  // Bias is per output channel (the M dimension here), so add it separately.
  int btotal = Cout * HW;
  AddChannelBiasKernel<<<(btotal + threads - 1) / threads, threads>>>(
      out, static_cast<const float*>(b.data()), Cout, HW);
  CUDA_CHECK(cudaGetLastError());
}

void LaunchMask(float* x, int C, int H, int W, int valid) {
  int total = C * H * W;
  int threads = 256;
  MaskTimeKernel<<<(total + threads - 1) / threads, threads>>>(x, C, H, W, valid);
  CUDA_CHECK(cudaGetLastError());
}

}  // namespace

ConformerPreEncode::ConformerPreEncode() = default;

void ConformerPreEncode::LoadWeights(const io::SafeTensorReader& reader,
                                     const std::string& p) {
  w0_ = CopyWeight(reader, p + ".conv.0.weight");
  b0_ = CopyWeight(reader, p + ".conv.0.bias");
  w2_ = CopyWeight(reader, p + ".conv.2.weight");
  b2_ = CopyWeight(reader, p + ".conv.2.bias");
  w3_ = CopyWeight(reader, p + ".conv.3.weight");
  b3_ = CopyWeight(reader, p + ".conv.3.bias");
  w5_ = CopyWeight(reader, p + ".conv.5.weight");
  b5_ = CopyWeight(reader, p + ".conv.5.bias");
  w6_ = CopyWeight(reader, p + ".conv.6.weight");
  b6_ = CopyWeight(reader, p + ".conv.6.bias");
  wout_ = CopyWeight(reader, p + ".out.weight");
  bout_ = CopyWeight(reader, p + ".out.bias");
  loaded_ = true;
}

std::vector<float> ConformerPreEncode::Forward(const float* mel, int n_mels,
                                               int n_frames, int valid_len,
                                               int* out_frames,
                                               int* out_valid_len) {
  if (!loaded_) throw std::runtime_error("ConformerPreEncode: weights not loaded");
  const int C = conv_channels_;

  // Input as a single-channel image [1, H=n_frames, W=n_mels]. The caller passes
  // mel as [n_mels, n_frames] (freq-major, NeMo's feat-dim first), but the conv
  // image is time-major [T, n_mels], so transpose on host first.
  std::vector<float> mel_t(static_cast<size_t>(n_frames) * n_mels);
  for (int m = 0; m < n_mels; ++m)
    for (int t = 0; t < n_frames; ++t)
      mel_t[static_cast<size_t>(t) * n_mels + m] =
          mel[static_cast<size_t>(m) * n_frames + t];
  UnifiedBuffer in0(static_cast<size_t>(n_frames) * n_mels * sizeof(float));
  CUDA_CHECK(cudaMemcpy(in0.data(), mel_t.data(),
                        static_cast<size_t>(n_frames) * n_mels * sizeof(float),
                        cudaMemcpyHostToDevice));
  LaunchMask(static_cast<float*>(in0.data()), 1, n_frames, n_mels, valid_len);

  // Stage 0: Conv2d(1->256,k3,s2,p1) + ReLU.
  int H0 = ConvOutLen(n_frames, 3, 2, 1), W0 = ConvOutLen(n_mels, 3, 2, 1);
  UnifiedBuffer buf1(static_cast<size_t>(C) * H0 * W0 * sizeof(float));
  int h, w;
  LaunchConv(static_cast<float*>(in0.data()), *w0_, *b0_,
             static_cast<float*>(buf1.data()), 1, n_frames, n_mels, C, 3, 2, 1, 1,
             &h, &w);
  int len0 = ConvOutLen(valid_len, 3, 2, 1);
  LaunchMask(static_cast<float*>(buf1.data()), C, H0, W0, len0);
  LaunchRelu(static_cast<float*>(buf1.data()), C * H0 * W0);
  LaunchMask(static_cast<float*>(buf1.data()), C, H0, W0, len0);

  // Stage 1: depthwise(k3,s2,p1,g256) -> pointwise(k1) + ReLU.
  int H1 = ConvOutLen(H0, 3, 2, 1), W1 = ConvOutLen(W0, 3, 2, 1);
  UnifiedBuffer dw1(static_cast<size_t>(C) * H1 * W1 * sizeof(float));
  LaunchConv(static_cast<float*>(buf1.data()), *w2_, *b2_,
             static_cast<float*>(dw1.data()), C, H0, W0, C, 3, 2, 1, C, &h, &w);
  int len1 = ConvOutLen(len0, 3, 2, 1);
  UnifiedBuffer pw1(static_cast<size_t>(C) * H1 * W1 * sizeof(float));
  LaunchPointwise(static_cast<float*>(dw1.data()), *w3_, *b3_,
                  static_cast<float*>(pw1.data()), C, C, H1 * W1);
  LaunchRelu(static_cast<float*>(pw1.data()), C * H1 * W1);
  LaunchMask(static_cast<float*>(pw1.data()), C, H1, W1, len1);

  // Stage 2: depthwise -> pointwise + ReLU.
  int H2 = ConvOutLen(H1, 3, 2, 1), W2 = ConvOutLen(W1, 3, 2, 1);
  UnifiedBuffer dw2(static_cast<size_t>(C) * H2 * W2 * sizeof(float));
  LaunchConv(static_cast<float*>(pw1.data()), *w5_, *b5_,
             static_cast<float*>(dw2.data()), C, H1, W1, C, 3, 2, 1, C, &h, &w);
  int len2 = ConvOutLen(len1, 3, 2, 1);
  UnifiedBuffer pw2(static_cast<size_t>(C) * H2 * W2 * sizeof(float));
  LaunchPointwise(static_cast<float*>(dw2.data()), *w6_, *b6_,
                  static_cast<float*>(pw2.data()), C, C, H2 * W2);
  LaunchRelu(static_cast<float*>(pw2.data()), C * H2 * W2);
  LaunchMask(static_cast<float*>(pw2.data()), C, H2, W2, len2);

  // Flatten [C,H2,W2] + Linear(C*W2 -> 512) via gather + register-blocked GEMM.
  const int D = d_model_;
  const int Kflat = C * W2;
  UnifiedBuffer flatbuf(static_cast<size_t>(H2) * Kflat * sizeof(float));
  UnifiedBuffer outbuf(static_cast<size_t>(H2) * D * sizeof(float));
  int threads = 256;
  int gtotal = H2 * Kflat;
  FlattenGatherKernel<<<(gtotal + threads - 1) / threads, threads>>>(
      static_cast<float*>(pw2.data()), static_cast<float*>(flatbuf.data()), C,
      H2, W2);
  CUDA_CHECK(cudaGetLastError());
  gemm::LaunchSgemm(static_cast<const float*>(flatbuf.data()),
                    static_cast<const float*>(wout_->data()),
                    static_cast<const float*>(bout_->data()),
                    static_cast<float*>(outbuf.data()), H2, Kflat, D, 0);
  CUDA_CHECK(cudaGetLastError());
  // Spec 002: drain the DEFAULT stream only (diarization runs on it), not the
  // whole device, so concurrent ASR work on its own stream is not waited on.
  CUDA_CHECK(cudaStreamSynchronize(0));

  std::vector<float> result(static_cast<size_t>(H2) * D);
  CUDA_CHECK(cudaMemcpy(result.data(), outbuf.data(), result.size() * sizeof(float),
                        cudaMemcpyDeviceToHost));
  *out_frames = H2;
  *out_valid_len = len2;
  return result;
}

}  // namespace model
}  // namespace orator
