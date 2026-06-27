// CUDA GEMM unit tests — validate asr_gemm functions against known inputs.
// Skips gracefully if no CUDA device is available.

#include "model/asr_gemm.h"

#include <cuda_runtime.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>

namespace gemm = orator::model::asr_gemm;

// Host bf16 round-to-nearest-even, matching __float2bfloat16 for finite values.
// Used by the independent f64 reference (T100): the GEMM quantizes both operands
// to bf16, so the reference must too.
static uint16_t HostF32ToBf16(float f) {
  uint32_t x;
  std::memcpy(&x, &f, sizeof(x));
  const uint32_t lsb = (x >> 16) & 1u;
  x += 0x7fffu + lsb;
  return static_cast<uint16_t>(x >> 16);
}
static float HostBf16ToF32(uint16_t b) {
  const uint32_t x = static_cast<uint32_t>(b) << 16;
  float f;
  std::memcpy(&f, &x, sizeof(f));
  return f;
}

// ---------------------------------------------------------------------------
// GPU memory RAII helper (mirrors test_kernels.cc pattern)
// ---------------------------------------------------------------------------
struct GpuBuf {
  float* ptr = nullptr;
  size_t n = 0;
  GpuBuf() = default;
  explicit GpuBuf(size_t count) : n(count) {
    cudaMalloc(&ptr, n * sizeof(float));
  }
  ~GpuBuf() { if (ptr) cudaFree(ptr); }
  GpuBuf(GpuBuf&& o) noexcept : ptr(o.ptr), n(o.n) { o.ptr = nullptr; o.n = 0; }
  GpuBuf& operator=(GpuBuf&& o) noexcept {
    if (ptr) cudaFree(ptr);
    ptr = o.ptr; n = o.n; o.ptr = nullptr; o.n = 0;
    return *this;
  }
  void Upload(const float* src) {
    cudaMemcpy(ptr, src, n * sizeof(float), cudaMemcpyHostToDevice);
  }
  void Download(float* dst) {
    cudaMemcpy(dst, ptr, n * sizeof(float), cudaMemcpyDeviceToHost);
  }
};

struct GpuBufU16 {
  uint16_t* ptr = nullptr;
  size_t n = 0;
  GpuBufU16() = default;
  explicit GpuBufU16(size_t count) : n(count) {
    cudaMalloc(&ptr, n * sizeof(uint16_t));
  }
  ~GpuBufU16() { if (ptr) cudaFree(ptr); }
  GpuBufU16(GpuBufU16&& o) noexcept : ptr(o.ptr), n(o.n) { o.ptr = nullptr; o.n = 0; }
  GpuBufU16& operator=(GpuBufU16&& o) noexcept {
    if (ptr) cudaFree(ptr);
    ptr = o.ptr; n = o.n; o.ptr = nullptr; o.n = 0;
    return *this;
  }
};

// ---------------------------------------------------------------------------
// Test helpers
// ---------------------------------------------------------------------------
static int g_failures = 0;
static int g_tests = 0;

#define TEST(name) do { ++g_tests; printf("  RUN  %s\n", name); } while (0)
#define ASSERT_NEAR(a, b, tol) do {                                           \
  float va_ = (a), vb_ = (b);                                                 \
  if (std::abs(va_ - vb_) > tol) {                                           \
    printf("  FAIL %s:%d: expected %.6f ≈ %.6f (tol=%.2e)\n",                \
           __FILE__, __LINE__, double(va_), double(vb_), double(tol));        \
    ++g_failures;                                                             \
  }                                                                           \
} while (0)
#define ASSERT_TRUE(cond) do {                                                \
  if (!(cond)) {                                                              \
    printf("  FAIL %s:%d: expected true\n", __FILE__, __LINE__);              \
    ++g_failures;                                                             \
  }                                                                           \
} while (0)

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static void TestF32Bf16Roundtrip() {
  TEST("TestF32Bf16Roundtrip: F32 -> BF16 -> F32 preserves values");
  const int N = 256;
  std::vector<float> in(N), out(N);
  for (int i = 0; i < N; ++i) in[i] = static_cast<float>(i) * 0.1f;

  GpuBuf d_in(N);
  GpuBufU16 d_bf16(N);
  GpuBuf d_out(N);
  d_in.Upload(in.data());

  gemm::F32ToBf16(d_in.ptr, d_bf16.ptr, N);
  gemm::Bf16ToF32(d_bf16.ptr, d_out.ptr, N);
  cudaDeviceSynchronize();
  d_out.Download(out.data());

  // BF16 has ~7 mantissa bits; relative error ≤ 1% for all magnitudes.
  for (int i = 0; i < N; ++i) {
    const float abs_err = std::abs(out[i] - in[i]);
    const float rel_err = abs_err / std::max(1e-6f, std::abs(in[i]));
    if (rel_err > 0.01f && abs_err > 1e-4f) {
      printf("  FAIL %s:%d: BF16 roundtrip rel_err=%.2e abs_err=%.2e at [%d]: "
             "got %.6f expected %.6f\n",
             __FILE__, __LINE__, double(rel_err), double(abs_err), i,
             double(out[i]), double(in[i]));
      ++g_failures;
    }
  }
}

static void TestF32Bf16Empty() {
  TEST("TestF32Bf16Empty: n=0 should not crash");
  gemm::F32ToBf16(nullptr, nullptr, 0);
  gemm::Bf16ToF32(nullptr, nullptr, 0);
}

static void TestLinearIdentity() {
  TEST("TestLinearIdentity: out = in @ I (+ no bias, no act)");
  const int M = 2, K = 3, N = 3;
  // Input: row-major [M, K]
  std::vector<float> in = {1.0f, 2.0f, 3.0f,
                           4.0f, 5.0f, 6.0f};
  // Identity weights: W[N,K] row-major, then Linear does out = in @ W^T
  // For identity: W = I (3x3), so W^T = I, out = in @ I = in
  std::vector<float> W_f32 = {1.0f, 0.0f, 0.0f,
                              0.0f, 1.0f, 0.0f,
                              0.0f, 0.0f, 1.0f};
  std::vector<float> out(M * N, 0.0f);

  GpuBuf d_in(M * K);
  GpuBuf d_out(M * N);
  GpuBuf d_W_f32(N * K);
  GpuBufU16 d_W_bf16(N * K);
  d_in.Upload(in.data());
  d_W_f32.Upload(W_f32.data());

  // Convert float weights to BF16 on GPU
  gemm::F32ToBf16(d_W_f32.ptr, d_W_bf16.ptr, static_cast<long>(N) * K);
  cudaDeviceSynchronize();

  gemm::Linear(d_in.ptr, d_W_bf16.ptr, nullptr, d_out.ptr, M, K, N, /*act=*/0);
  cudaDeviceSynchronize();
  d_out.Download(out.data());

  for (int i = 0; i < M * N; ++i)
    ASSERT_NEAR(out[i], in[i], 1e-1f);
}

static void TestLinearWithBias() {
  TEST("TestLinearWithBias: out = in @ I + bias");
  const int M = 2, K = 2, N = 2;
  std::vector<float> in = {1.0f, 2.0f,
                           3.0f, 4.0f};
  std::vector<float> W_f32 = {1.0f, 0.0f,
                              0.0f, 1.0f};
  std::vector<float> bias = {10.0f, 20.0f};
  std::vector<float> out(M * N, 0.0f);
  // Expected: in @ I + bias = in + bias
  std::vector<float> expected = {11.0f, 22.0f,
                                 13.0f, 24.0f};

  GpuBuf d_in(M * K);
  GpuBuf d_out(M * N);
  GpuBuf d_W_f32(N * K);
  GpuBufU16 d_W_bf16(N * K);
  GpuBuf d_bias(N);
  d_in.Upload(in.data());
  d_W_f32.Upload(W_f32.data());
  d_bias.Upload(bias.data());

  gemm::F32ToBf16(d_W_f32.ptr, d_W_bf16.ptr, static_cast<long>(N) * K);
  cudaDeviceSynchronize();

  gemm::Linear(d_in.ptr, d_W_bf16.ptr, d_bias.ptr, d_out.ptr, M, K, N, /*act=*/0);
  cudaDeviceSynchronize();
  d_out.Download(out.data());

  for (int i = 0; i < M * N; ++i)
    ASSERT_NEAR(out[i], expected[i], 1e-1f);
}

static void TestLinearReLU() {
  TEST("TestLinearReLU: out = ReLU(in @ I)");
  const int M = 1, K = 4, N = 4;
  // Mix of positive and negative values
  std::vector<float> in = {-2.0f, -1.0f, 1.0f, 3.0f};
  std::vector<float> W_f32 = {1.0f, 0.0f, 0.0f, 0.0f,
                              0.0f, 1.0f, 0.0f, 0.0f,
                              0.0f, 0.0f, 1.0f, 0.0f,
                              0.0f, 0.0f, 0.0f, 1.0f};
  std::vector<float> out(M * N, 0.0f);
  // ReLU clamps negatives to 0
  std::vector<float> expected = {0.0f, 0.0f, 1.0f, 3.0f};

  GpuBuf d_in(M * K);
  GpuBuf d_out(M * N);
  GpuBuf d_W_f32(N * K);
  GpuBufU16 d_W_bf16(N * K);
  d_in.Upload(in.data());
  d_W_f32.Upload(W_f32.data());

  gemm::F32ToBf16(d_W_f32.ptr, d_W_bf16.ptr, static_cast<long>(N) * K);
  cudaDeviceSynchronize();

  gemm::Linear(d_in.ptr, d_W_bf16.ptr, nullptr, d_out.ptr, M, K, N, /*act=*/2);
  cudaDeviceSynchronize();
  d_out.Download(out.data());

  for (int i = 0; i < M * N; ++i)
    ASSERT_NEAR(out[i], expected[i], 1e-1f);
}

static void TestLinearPre() {
  TEST("TestLinearPre: LinearPre with pre-cast BF16 input");
  const int M = 1, K = 3, N = 3;
  std::vector<float> in = {1.0f, 2.0f, 3.0f};
  std::vector<float> W_f32 = {1.0f, 0.0f, 0.0f,
                              0.0f, 1.0f, 0.0f,
                              0.0f, 0.0f, 1.0f};
  std::vector<float> out(M * N, 0.0f);

  GpuBuf d_in_f32(M * K);
  GpuBufU16 d_in_bf16(M * K);
  GpuBuf d_out(M * N);
  GpuBuf d_W_f32(N * K);
  GpuBufU16 d_W_bf16(N * K);
  d_in_f32.Upload(in.data());
  d_W_f32.Upload(W_f32.data());

  gemm::F32ToBf16(d_in_f32.ptr, d_in_bf16.ptr, static_cast<long>(M) * K);
  gemm::F32ToBf16(d_W_f32.ptr, d_W_bf16.ptr, static_cast<long>(N) * K);
  cudaDeviceSynchronize();

  gemm::LinearPre(d_in_bf16.ptr, d_W_bf16.ptr, nullptr, d_out.ptr, M, K, N, /*act=*/0);
  cudaDeviceSynchronize();
  d_out.Download(out.data());

  for (int i = 0; i < M * N; ++i)
    ASSERT_NEAR(out[i], in[i], 1e-1f);
}

// ---------------------------------------------------------------------------
// T100 — independent f64 reference across the production shape set.
//
// The GEMM is bf16-in / FP32-accumulate. The reference quantizes both operands
// to bf16 (matching the kernel), accumulates each dot product in f64 (the
// precise truth), then applies bias + activation exactly as the epilogue does.
// Establishes the oracle the in-project bf16 GEMM (Spec 002 P2.1) must meet
// BEFORE cuBLAS is removed. Tolerance covers the FP32-accumulation order
// difference vs the f64 reference.
// ---------------------------------------------------------------------------
static void TestLinearVsF64Reference() {
  TEST("TestLinearVsF64Reference: bf16 GEMM matches f64 reference (production shapes)");
  struct Shape { int M, K, N; const char* tag; };
  // (M,K,N) sampled from the encoder / decoder / aligner production shapes;
  // M>1 so the cuBLAS (soon in-project) path is exercised, not the M=1 GEMV.
  // The f64 CPU reference is O(M*N*K); the default ctest run uses a fast
  // representative subset, and ORATOR_GEMM_FULL=1 adds the large-K/N production
  // shapes (used when validating the in-project GEMM, Spec 002 P2.1).
  std::vector<Shape> shapes = {
    {4, 1024, 1024, "attn-qkvo"},
    {4, 1024, 2048, "q_proj/proj2"},
    {2, 1024, 1024, "prefill-small"},
  };
  if (std::getenv("ORATOR_GEMM_FULL") != nullptr) {
    shapes.push_back({6, 1024, 3072, "ffn-fc1"});
    shapes.push_back({6, 3072, 1024, "ffn-fc2"});
    shapes.push_back({4, 7680, 1024, "conv_out"});
    shapes.push_back({4, 1024, 5000, "score-head"});
    shapes.push_back({8, 1024, 2048, "encoder-proj"});
  }
  std::mt19937 rng(20260627u);
  std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

  double worst = 0.0;
  for (const auto& s : shapes) {
    const int M = s.M, K = s.K, N = s.N;
    std::vector<float> in(static_cast<size_t>(M) * K), W(static_cast<size_t>(N) * K),
        bias(N), out(static_cast<size_t>(M) * N);
    for (auto& v : in) v = dist(rng);
    for (auto& v : W) v = dist(rng);
    for (auto& v : bias) v = dist(rng);
    // Pre-quantize operands to bf16 once (shared across the activation variants).
    std::vector<float> inq(in.size()), Wq(W.size());
    for (size_t i = 0; i < in.size(); ++i) inq[i] = HostBf16ToF32(HostF32ToBf16(in[i]));
    for (size_t i = 0; i < W.size(); ++i) Wq[i] = HostBf16ToF32(HostF32ToBf16(W[i]));

    GpuBuf d_in(static_cast<size_t>(M) * K), d_out(static_cast<size_t>(M) * N),
        d_Wf(static_cast<size_t>(N) * K), d_bias(N);
    GpuBufU16 d_Wb(static_cast<size_t>(N) * K);
    d_in.Upload(in.data());
    d_Wf.Upload(W.data());
    d_bias.Upload(bias.data());
    gemm::F32ToBf16(d_Wf.ptr, d_Wb.ptr, static_cast<long>(N) * K);
    cudaDeviceSynchronize();

    // The dot product is independent of the activation; compute the
    // pre-activation reference (acc + bias) once, then apply each act cheaply.
    std::vector<float> pre(static_cast<size_t>(M) * N);
    for (int m = 0; m < M; ++m) {
      const float* a = &inq[static_cast<size_t>(m) * K];
      for (int n = 0; n < N; ++n) {
        const float* w = &Wq[static_cast<size_t>(n) * K];
        double acc = 0.0;
        for (int k = 0; k < K; ++k) acc += static_cast<double>(a[k]) * w[k];
        pre[static_cast<size_t>(m) * N + n] = static_cast<float>(acc) + bias[n];
      }
    }

    for (int act : {0, 1, 2}) {
      gemm::Linear(d_in.ptr, d_Wb.ptr, d_bias.ptr, d_out.ptr, M, K, N, act);
      cudaDeviceSynchronize();
      d_out.Download(out.data());

      double max_rel = 0.0, sum_rel = 0.0;
      for (size_t i = 0; i < pre.size(); ++i) {
        float v = pre[i];
        if (act == 1)
          v = 0.5f * v * (1.0f + std::erf(v * 0.70710678118654752440f));
        else if (act == 2)
          v = std::fmax(v, 0.0f);
        const float rel = std::abs(out[i] - v) / std::max(1e-3f, std::abs(v));
        max_rel = std::max(max_rel, static_cast<double>(rel));
        sum_rel += rel;
      }
      worst = std::max(worst, max_rel);
      printf("    [%-14s K=%5d N=%5d M=%4d act=%d] max_rel=%.2e mean_rel=%.2e\n",
             s.tag, K, N, M, act, max_rel, sum_rel / pre.size());
      // FP32-accumulate vs f64 reference over K up to 7680 stays well within 2e-2.
      if (max_rel > 2e-2) {
        printf("  FAIL %s:%d: shape %s act=%d max_rel=%.3e exceeds 2e-2\n",
               __FILE__, __LINE__, s.tag, act, max_rel);
        ++g_failures;
      }
    }
  }
  printf("    worst max_rel across all shapes/acts = %.2e\n", worst);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main() {
  printf("[test_asr_gemm] CUDA GEMM unit tests\n");

  int device_count = 0;
  cudaError_t ce = cudaGetDeviceCount(&device_count);
  if (ce != cudaSuccess || device_count < 1) {
    printf("  SKIP: no CUDA device available (cudaGetDeviceCount=%d, count=%d)\n",
           static_cast<int>(ce), device_count);
    printf("Results: 0 / 0 passed (skipped)\n");
    return 0;
  }

  TestF32Bf16Roundtrip();
  TestF32Bf16Empty();
  TestLinearIdentity();
  TestLinearWithBias();
  TestLinearReLU();
  TestLinearPre();
  TestLinearVsF64Reference();

  // Clean up global cuBLAS handle and scratch buffer
  gemm::Shutdown();

  printf("\n");
  printf("Results: %d / %d passed", g_tests - g_failures, g_tests);
  if (g_failures > 0) printf(", %d FAILED", g_failures);
  printf("\n");

  return g_failures > 0 ? 1 : 0;
}
