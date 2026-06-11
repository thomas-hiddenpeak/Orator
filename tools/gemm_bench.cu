// Microbenchmark + correctness check for the shared SGEMM (model/gemm.cuh).
// Times out = in[M,K] * W[N,K]^T at the sizes that dominate the diarizer, and
// validates against a naive reference.
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <cuda_runtime.h>

#include "model/gemm.cuh"

static void fill(std::vector<float>& v) {
  for (auto& x : v) x = (float(rand()) / RAND_MAX - 0.5f) * 0.2f;
}

static double bench(int M, int K, int N, int iters, double* gflops,
                    float* maxerr) {
  std::vector<float> hin(size_t(M) * K), hW(size_t(N) * K), hb(N), hout(size_t(M) * N);
  fill(hin); fill(hW); fill(hb);
  float *din, *dW, *db, *dout;
  cudaMalloc(&din, hin.size() * 4); cudaMalloc(&dW, hW.size() * 4);
  cudaMalloc(&db, hb.size() * 4); cudaMalloc(&dout, hout.size() * 4);
  cudaMemcpy(din, hin.data(), hin.size() * 4, cudaMemcpyHostToDevice);
  cudaMemcpy(dW, hW.data(), hW.size() * 4, cudaMemcpyHostToDevice);
  cudaMemcpy(db, hb.data(), hb.size() * 4, cudaMemcpyHostToDevice);

  // warmup + correctness
  orator::gemm::LaunchSgemm(din, dW, db, dout, M, K, N, 0);
  cudaDeviceSynchronize();
  cudaMemcpy(hout.data(), dout, hout.size() * 4, cudaMemcpyDeviceToHost);
  *maxerr = 0.0f;
  // spot-check a few outputs
  for (int s = 0; s < 64; ++s) {
    int m = rand() % M, n = rand() % N;
    double acc = hb[n];
    for (int k = 0; k < K; ++k) acc += double(hin[size_t(m) * K + k]) * hW[size_t(n) * K + k];
    float e = std::fabs(float(acc) - hout[size_t(m) * N + n]);
    if (e > *maxerr) *maxerr = e;
  }

  cudaEvent_t a, b; cudaEventCreate(&a); cudaEventCreate(&b);
  cudaEventRecord(a);
  for (int i = 0; i < iters; ++i)
    orator::gemm::LaunchSgemm(din, dW, db, dout, M, K, N, 0);
  cudaEventRecord(b); cudaEventSynchronize(b);
  float ms = 0; cudaEventElapsedTime(&ms, a, b);
  double sec = ms / 1e3 / iters;
  *gflops = 2.0 * M * K * N / sec / 1e9;
  cudaFree(din); cudaFree(dW); cudaFree(db); cudaFree(dout);
  return sec * 1e6;  // us per call
}

int main() {
  struct S { int M, K, N; const char* name; };
  S sizes[] = {
    {376, 512, 512, "QKV/out 512x512"},
    {376, 512, 2048, "FFN linear1 ->2048"},
    {376, 2048, 512, "FFN linear2 2048->"},
    {376, 512, 1024, "conv pw1 ->1024"},
    {376, 4096, 512, "preenc flatten 4096->512"},
    {376, 192, 192, "decoder qkv 192"},
    {376, 512, 192, "encoder_proj 512->192"},
  };
  printf("%-28s %8s %10s %10s\n", "case", "us/call", "GFLOP/s", "maxerr");
  double tot = 0;
  for (auto& s : sizes) {
    double gf; float err;
    double us = bench(s.M, s.K, s.N, 200, &gf, &err);
    printf("%-28s %8.1f %10.1f %10.2e\n", s.name, us, gf, err);
    tot += us;
  }
  printf("sum us/iter (one of each): %.1f\n", tot);
  return 0;
}
