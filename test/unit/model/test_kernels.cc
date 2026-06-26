// CUDA kernel unit tests — validate Kernels class against CPU reference.
// Builds on the patterns from test_asr_ops.cc (GPU-vs-CPU oracle testing).

#include "gpu/kernels.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <cuda_runtime.h>

// ---------------------------------------------------------------------------
// CPU reference implementations
// ---------------------------------------------------------------------------

static void CpuAdd(const float* a, const float* b, int n, float* out) {
  for (int i = 0; i < n; ++i) out[i] = a[i] + b[i];
}

static void CpuMultiply(const float* a, float s, int n, float* out) {
  for (int i = 0; i < n; ++i) out[i] = a[i] * s;
}

static float CpuNorm(const float* v, int n) {
  double s = 0.0;
  for (int i = 0; i < n; ++i) s += double(v[i]) * double(v[i]);
  return float(std::sqrt(s));
}

static void CpuNormalize(const float* v, int n, float* out) {
  float norm = CpuNorm(v, n);
  if (norm > 1e-8f) {
    for (int i = 0; i < n; ++i) out[i] = v[i] / norm;
  } else {
    for (int i = 0; i < n; ++i) out[i] = 0.0f;
  }
}

static float CpuDot(const float* a, const float* b, int n) {
  double s = 0.0;
  for (int i = 0; i < n; ++i) s += double(a[i]) * double(b[i]);
  return float(s);
}

static float CpuCosineSimilarity(const float* a, const float* b, int n) {
  float na = CpuNorm(a, n);
  float nb = CpuNorm(b, n);
  if (na < 1e-8f || nb < 1e-8f) return 0.0f;
  return CpuDot(a, b, n) / (na * nb);
}

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
// GPU memory RAII helper
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
  GpuBuf Clone(const float* src) {
    GpuBuf b(n);
    b.Upload(src);
    return b;
  }
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static void TestAdd() {
  TEST("TestAdd: element-wise vector addition");
  const int N = 1024;
  std::vector<float> a(N), b(N), cpu(N), gpu(N);
  for (int i = 0; i < N; ++i) { a[i] = float(i); b[i] = float(N - i); }
  CpuAdd(a.data(), b.data(), N, cpu.data());

  GpuBuf d_a(N), d_b(N), d_out(N);
  d_a.Upload(a.data());
  d_b.Upload(b.data());
  orator::gpu::Kernels::Add(d_a.ptr, d_b.ptr, N, d_out.ptr);
  d_out.Download(gpu.data());

  for (int i = 0; i < N; ++i)
    ASSERT_NEAR(gpu[i], cpu[i], 1e-5f);
}

static void TestAddEmpty() {
  TEST("TestAdd: empty (n=0) — should not crash");
  // n=0 → grid_size=0, kernel launch with 0 blocks is a no-op.
  orator::gpu::Kernels::Add(nullptr, nullptr, 0, nullptr);
}

static void TestMultiply() {
  TEST("TestMultiply: scalar multiplication");
  const int N = 512;
  std::vector<float> a(N), cpu(N), gpu(N);
  const float s = 3.14159f;
  for (int i = 0; i < N; ++i) a[i] = float(i % 100);
  CpuMultiply(a.data(), s, N, cpu.data());

  GpuBuf d_a(N), d_out(N);
  d_a.Upload(a.data());
  orator::gpu::Kernels::Multiply(d_a.ptr, s, N, d_out.ptr);
  d_out.Download(gpu.data());

  for (int i = 0; i < N; ++i)
    ASSERT_NEAR(gpu[i], cpu[i], 1e-5f);
}

static void TestMultiplyZeroScalar() {
  TEST("TestMultiply: multiply by zero");
  const int N = 64;
  std::vector<float> a(N), cpu(N), gpu(N);
  for (int i = 0; i < N; ++i) a[i] = float(i);
  CpuMultiply(a.data(), 0.0f, N, cpu.data());

  GpuBuf d_a(N), d_out(N);
  d_a.Upload(a.data());
  orator::gpu::Kernels::Multiply(d_a.ptr, 0.0f, N, d_out.ptr);
  d_out.Download(gpu.data());

  for (int i = 0; i < N; ++i)
    ASSERT_NEAR(gpu[i], cpu[i], 1e-5f);
}

static void TestNormalize() {
  TEST("TestNormalize: L2 normalization");
  const int N = 256;
  std::vector<float> a(N), cpu(N), gpu(N);
  for (int i = 0; i < N; ++i) a[i] = float(i + 1) / 100.0f;
  CpuNormalize(a.data(), N, cpu.data());

  GpuBuf d_a(N), d_out(N);
  d_a.Upload(a.data());
  orator::gpu::Kernels::NormalizeVector(d_a.ptr, N, d_out.ptr, nullptr);
  d_out.Download(gpu.data());

  for (int i = 0; i < N; ++i)
    ASSERT_NEAR(gpu[i], cpu[i], 1e-5f);
}

static void TestNormalizeZero() {
  TEST("TestNormalize: zero vector → all zeros");
  const int N = 128;
  std::vector<float> a(N, 0.0f), cpu(N, 0.0f), gpu(N, 0.0f);
  // already zeros
  GpuBuf d_a(N), d_out(N);
  d_a.Upload(a.data());
  orator::gpu::Kernels::NormalizeVector(d_a.ptr, N, d_out.ptr, nullptr);
  d_out.Download(gpu.data());

  for (int i = 0; i < N; ++i)
    ASSERT_NEAR(gpu[i], cpu[i], 1e-5f);
}

static void TestNormalizeUnit() {
  TEST("TestNormalize: already unit-length vector → unchanged");
  const int N = 64;
  std::vector<float> a(N), cpu(N), gpu(N);
  // construct a unit vector
  for (int i = 0; i < N; ++i) a[i] = (i == 0) ? 1.0f : 0.0f;
  CpuNormalize(a.data(), N, cpu.data());

  GpuBuf d_a(N), d_out(N);
  d_a.Upload(a.data());
  orator::gpu::Kernels::NormalizeVector(d_a.ptr, N, d_out.ptr, nullptr);
  d_out.Download(gpu.data());

  for (int i = 0; i < N; ++i)
    ASSERT_NEAR(gpu[i], cpu[i], 1e-5f);

  // verify output is still unit-length (norm ≈ 1)
  float gpu_norm = CpuNorm(gpu.data(), N);
  ASSERT_NEAR(gpu_norm, 1.0f, 1e-5f);
}

static void TestCosineSimilarity() {
  TEST("TestCosineSimilarity: identical vectors → 1.0");
  const int N = 128;
  std::vector<float> a(N), b(N);
  for (int i = 0; i < N; ++i) a[i] = b[i] = float(i + 1) / 100.0f;
  float expected = CpuCosineSimilarity(a.data(), b.data(), N);

  GpuBuf d_a(N), d_b(N);
  d_a.Upload(a.data());
  d_b.Upload(b.data());
  float got = orator::gpu::Kernels::CosineSimilarity(d_a.ptr, d_b.ptr, N, nullptr);
  ASSERT_NEAR(got, expected, 1e-5f);
}

static void TestCosineSimilarityOrthogonal() {
  TEST("TestCosineSimilarity: orthogonal vectors → 0.0");
  const int N = 4;
  std::vector<float> a = {1.0f, 0.0f, 0.0f, 0.0f};
  std::vector<float> b = {0.0f, 1.0f, 0.0f, 0.0f};

  GpuBuf d_a(N), d_b(N);
  d_a.Upload(a.data());
  d_b.Upload(b.data());
  float got = orator::gpu::Kernels::CosineSimilarity(d_a.ptr, d_b.ptr, N, nullptr);
  ASSERT_NEAR(got, 0.0f, 1e-6f);
}

static void TestCosineSimilarityOpposite() {
  TEST("TestCosineSimilarity: opposite vectors → -1.0");
  const int N = 8;
  std::vector<float> a(N), b(N);
  for (int i = 0; i < N; ++i) { a[i] = float(i + 1); b[i] = -float(i + 1); }
  float expected = CpuCosineSimilarity(a.data(), b.data(), N);

  GpuBuf d_a(N), d_b(N);
  d_a.Upload(a.data());
  d_b.Upload(b.data());
  float got = orator::gpu::Kernels::CosineSimilarity(d_a.ptr, d_b.ptr, N, nullptr);
  ASSERT_NEAR(got, expected, 1e-5f);
}

static void TestCosineSimilarityZero() {
  TEST("TestCosineSimilarity: zero input → 0.0");
  const int N = 64;
  std::vector<float> a(N, 0.0f), b(N, 1.0f);

  GpuBuf d_a(N), d_b(N);
  d_a.Upload(a.data());
  d_b.Upload(b.data());
  float got = orator::gpu::Kernels::CosineSimilarity(d_a.ptr, d_b.ptr, N, nullptr);
  ASSERT_NEAR(got, 0.0f, 1e-6f);
}

static void TestBatchCosineSimilarity() {
  TEST("TestBatchCosineSimilarity: 1 query vs 4 keys");
  const int N = 64;
  const int K = 4;
  std::vector<float> query(N), keys(K * N), cpu(K), gpu(K);
  for (int i = 0; i < N; ++i) query[i] = float(i + 1) / 100.0f;
  for (int k = 0; k < K; ++k)
    for (int i = 0; i < N; ++i)
      keys[k * N + i] = float((k + 1) * (i + 1)) / 100.0f;

  for (int k = 0; k < K; ++k)
    cpu[k] = CpuCosineSimilarity(query.data(), keys.data() + k * N, N);

  GpuBuf d_query(N), d_keys(K * N), d_out(K);
  d_query.Upload(query.data());
  d_keys.Upload(keys.data());
  orator::gpu::Kernels::BatchCosineSimilarity(d_query.ptr, d_keys.ptr,
                                               K, N, d_out.ptr, nullptr);
  d_out.Download(gpu.data());

  for (int k = 0; k < K; ++k)
    ASSERT_NEAR(gpu[k], cpu[k], 1e-5f);
}

static void TestLargeVector() {
  TEST("TestLargeVector: 1M elements Add (stress test)");
  const int N = 1 << 20;  // 1,048,576
  std::vector<float> a(N), b(N), cpu(N), gpu(N);
  for (int i = 0; i < N; ++i) {
    a[i] = float(i % 65536) / 65536.0f;
    b[i] = float((65536 - i) % 65536) / 65536.0f;
  }
  CpuAdd(a.data(), b.data(), N, cpu.data());

  GpuBuf d_a(N), d_b(N), d_out(N);
  d_a.Upload(a.data());
  d_b.Upload(b.data());
  orator::gpu::Kernels::Add(d_a.ptr, d_b.ptr, N, d_out.ptr);
  d_out.Download(gpu.data());

  // Check first, middle, last for correspondence (avoid 1M assertions on GPU)
  ASSERT_NEAR(gpu[0], cpu[0], 1e-5f);
  ASSERT_NEAR(gpu[N / 2], cpu[N / 2], 1e-5f);
  ASSERT_NEAR(gpu[N - 1], cpu[N - 1], 1e-5f);

  // Verify norm of difference is small
  double diff = 0.0;
  for (int i = 0; i < N; ++i) diff += double(gpu[i] - cpu[i]) * double(gpu[i] - cpu[i]);
  ASSERT_NEAR(float(std::sqrt(diff / N)), 0.0f, 1e-5f);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main() {
  printf("[test_kernels] CUDA kernel unit tests\n");

  TestAdd();
  TestAddEmpty();
  TestMultiply();
  TestMultiplyZeroScalar();
  TestNormalize();
  TestNormalizeZero();
  TestNormalizeUnit();
  TestCosineSimilarity();
  TestCosineSimilarityOrthogonal();
  TestCosineSimilarityOpposite();
  TestCosineSimilarityZero();
  TestBatchCosineSimilarity();
  TestLargeVector();

  printf("\n");
  printf("Results: %d / %d passed", g_tests - g_failures, g_tests);
  if (g_failures > 0) printf(", %d FAILED", g_failures);
  printf("\n");

  return g_failures > 0 ? 1 : 0;
}
