#include <cuda_runtime.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "gpu/memory.h"
#include "model/asr_ops.cuh"

using orator::gpu::UnifiedBuffer;
namespace ops = orator::model::asr_ops;

namespace {

// Reference oracle = PyTorch on the GPU (tools/dump_asr_ops.py). These files
// hold the exact inputs and torch's CUDA outputs; the kernels are validated
// against the framework the model actually runs in, not a CPU sequential sum.
const char* kRefDir = "models/reference/asr_ops";

std::vector<float> ReadF32(const std::string& path, bool* ok) {
  std::ifstream f(path, std::ios::binary | std::ios::ate);
  if (!f) {
    *ok = false;
    return {};
  }
  std::streamsize n = f.tellg();
  f.seekg(0);
  std::vector<float> v(n / sizeof(float));
  f.read(reinterpret_cast<char*>(v.data()), n);
  *ok = true;
  return v;
}

std::vector<int> ReadI32(const std::string& path, bool* ok) {
  std::ifstream f(path, std::ios::binary | std::ios::ate);
  if (!f) {
    *ok = false;
    return {};
  }
  std::streamsize n = f.tellg();
  f.seekg(0);
  std::vector<int> v(n / sizeof(int));
  f.read(reinterpret_cast<char*>(v.data()), n);
  *ok = true;
  return v;
}

float* F(UnifiedBuffer& b) { return static_cast<float*>(b.data()); }

double MaxAbsErr(const std::vector<float>& ref, const float* got, size_t n) {
  double e = 0.0;
  for (size_t i = 0; i < n; ++i)
    e = std::max(e, std::abs(static_cast<double>(ref[i]) - got[i]));
  return e;
}

void Up(UnifiedBuffer& b, const std::vector<float>& h) {
  std::copy(h.begin(), h.end(), F(b));
}

}  // namespace

int main() {
  std::printf("Testing ASR core operators against PyTorch-GPU reference...\n");
  const std::string d = std::string(kRefDir) + "/";
  bool all = true;

  // ----- RMSNorm -----
  {
    const int rows = 8, dim = 2048;
    const float eps = 1e-6f;
    bool a, b, c;
    auto hx = ReadF32(d + "rmsnorm_x.f32", &a);
    auto hw = ReadF32(d + "rmsnorm_w.f32", &b);
    auto hr = ReadF32(d + "rmsnorm_out.f32", &c);
    if (!(a && b && c)) {
      std::printf("  RMSNorm    [skipped: missing reference]\n");
      all = false;
    } else {
      UnifiedBuffer x(sizeof(float) * hx.size()), w(sizeof(float) * hw.size()),
          y(sizeof(float) * hr.size());
      Up(x, hx);
      Up(w, hw);
      ops::RmsNorm(F(x), F(w), F(y), rows, dim, eps);
      cudaDeviceSynchronize();
      const double e = MaxAbsErr(hr, F(y), hr.size());
      std::printf("  RMSNorm    max abs err vs torch = %.3e\n", e);
      assert(e < 1e-4);
    }
  }

  // ----- RoPE (interleaved) -----
  {
    const int T = 12, H = 16, Dh = 128;
    const float base = 1000000.0f;
    bool a, p, c;
    auto hx = ReadF32(d + "rope_x.f32", &a);
    auto hpos = ReadI32(d + "rope_pos.i32", &p);
    auto hr = ReadF32(d + "rope_out.f32", &c);
    if (!(a && p && c)) {
      std::printf("  RoPE       [skipped: missing reference]\n");
      all = false;
    } else {
      UnifiedBuffer x(sizeof(float) * hx.size());
      UnifiedBuffer pos(sizeof(int) * hpos.size());
      Up(x, hx);
      int* dp = static_cast<int*>(pos.data());
      std::copy(hpos.begin(), hpos.end(), dp);
      ops::RopeInterleaved(F(x), dp, T, H, Dh, base);
      cudaDeviceSynchronize();
      const double e = MaxAbsErr(hr, F(x), hr.size());
      std::printf("  RoPE       max abs err vs torch = %.3e\n", e);
      assert(e < 1e-4);
    }
  }

  // ----- SwiGLU -----
  {
    bool a, b, c;
    auto hg = ReadF32(d + "swiglu_gate.f32", &a);
    auto hu = ReadF32(d + "swiglu_up.f32", &b);
    auto hr = ReadF32(d + "swiglu_out.f32", &c);
    if (!(a && b && c)) {
      std::printf("  SwiGLU     [skipped: missing reference]\n");
      all = false;
    } else {
      const int n = static_cast<int>(hg.size());
      UnifiedBuffer g(sizeof(float) * n), u(sizeof(float) * n), o(sizeof(float) * n);
      Up(g, hg);
      Up(u, hu);
      ops::SwiGLU(F(g), F(u), F(o), n);
      cudaDeviceSynchronize();
      const double e = MaxAbsErr(hr, F(o), hr.size());
      std::printf("  SwiGLU     max abs err vs torch = %.3e\n", e);
      assert(e < 1e-5);
    }
  }

  // ----- GQA attention (causal, vs torch scaled_dot_product_attention) -----
  {
    const int T = 24, Hq = 16, Hkv = 8, Dh = 128;
    const float scale = 1.0f / std::sqrt(static_cast<float>(Dh));
    bool a, b, c, r;
    auto hq = ReadF32(d + "gqa_q.f32", &a);
    auto hk = ReadF32(d + "gqa_k.f32", &b);
    auto hv = ReadF32(d + "gqa_v.f32", &c);
    auto hr = ReadF32(d + "gqa_out.f32", &r);
    if (!(a && b && c && r)) {
      std::printf("  GQA attn   [skipped: missing reference]\n");
      all = false;
    } else {
      UnifiedBuffer q(sizeof(float) * hq.size()), k(sizeof(float) * hk.size()),
          v(sizeof(float) * hv.size()), out(sizeof(float) * hr.size());
      Up(q, hq);
      Up(k, hk);
      Up(v, hv);
      ops::GqaAttention(F(q), F(k), F(v), F(out), T, Hq, Hkv, Dh, scale, true);
      cudaDeviceSynchronize();
      const double e = MaxAbsErr(hr, F(out), hr.size());
      std::printf("  GQA attn   max abs err vs torch = %.3e\n", e);
      assert(e < 5e-4);
    }
  }

  if (!all) {
    std::printf(
        "\n[!] Some references were missing. Regenerate with:\n"
        "    source tools/torchenv.sh && python tools/dump_asr_ops.py\n");
  }

  // ----- Throughput: representative Qwen3-ASR decoder self-attention -----
  {
    const int T = 256, Hq = 16, Hkv = 8, Dh = 128;
    const float scale = 1.0f / std::sqrt(static_cast<float>(Dh));
    UnifiedBuffer q(sizeof(float) * T * Hq * Dh), k(sizeof(float) * T * Hkv * Dh),
        v(sizeof(float) * T * Hkv * Dh), out(sizeof(float) * T * Hq * Dh);
    for (int i = 0; i < T * Hq * Dh; ++i) F(q)[i] = 0.1f;
    for (int i = 0; i < T * Hkv * Dh; ++i) {
      F(k)[i] = 0.1f;
      F(v)[i] = 0.1f;
    }
    ops::GqaAttention(F(q), F(k), F(v), F(out), T, Hq, Hkv, Dh, scale, true);
    cudaDeviceSynchronize();
    const int iters = 50;
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int it = 0; it < iters; ++it)
      ops::GqaAttention(F(q), F(k), F(v), F(out), T, Hq, Hkv, Dh, scale, true);
    cudaDeviceSynchronize();
    auto t1 = std::chrono::high_resolution_clock::now();
    const double ms =
        std::chrono::duration<double, std::milli>(t1 - t0).count() / iters;
    std::printf(
        "  GQA attn   T=%d Hq=%d Dh=%d: %.3f ms/pass (causal self-attn)\n", T,
        Hq, Dh, ms);
  }

  std::printf("ASR core operators test PASSED\n");
  return 0;
}
