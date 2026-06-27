#include "gpu/kernels.h"
#include "gpu/memory.h"

#include <cuda_runtime.h>
#include <cmath>
#include <algorithm>

namespace orator {
namespace gpu {

__global__ void NormalizeKernel(const float* input, int n, float* output) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx >= n) return;

  // Compute norm
  float norm = 0.0f;
  for (int i = 0; i < n; ++i) {
    norm += input[i] * input[i];
  }
  norm = sqrtf(norm);

  if (norm > 1e-8f) {
    output[idx] = input[idx] / norm;
  } else {
    output[idx] = 0.0f;
  }
}

__global__ void CosineSimilarityKernel(const float* a, const float* b, int n,
                                       float* output) {
  int idx = threadIdx.x;
  if (idx >= n) return;

  // Simple implementation: accumulate in shared memory
  __shared__ float dot_prod;
  __shared__ float norm_a;
  __shared__ float norm_b;

  if (idx == 0) {
    dot_prod = 0.0f;
    norm_a = 0.0f;
    norm_b = 0.0f;
  }
  __syncthreads();

  // Compute in parallel
  float a_val = a[idx];
  float b_val = b[idx];

  atomicAdd(&dot_prod, a_val * b_val);
  atomicAdd(&norm_a, a_val * a_val);
  atomicAdd(&norm_b, b_val * b_val);

  __syncthreads();

  if (idx == 0) {
    norm_a = sqrtf(norm_a);
    norm_b = sqrtf(norm_b);

    if (norm_a > 1e-8f && norm_b > 1e-8f) {
      *output = dot_prod / (norm_a * norm_b);
    } else {
      *output = 0.0f;
    }
  }
}

__global__ void BatchCosineSimilarityKernel(const float* query,
                                            const float* keys, int num_keys,
                                            int vec_dim, float* output) {
  int key_idx = blockIdx.x;
  if (key_idx >= num_keys) return;

  int thread_idx = threadIdx.x;
  if (thread_idx >= vec_dim) return;

  // Compute similarity for this key
  const float* key = keys + key_idx * vec_dim;

  __shared__ float dot_prod;
  __shared__ float norm_query;
  __shared__ float norm_key;

  if (thread_idx == 0) {
    dot_prod = 0.0f;
    norm_query = 0.0f;
    norm_key = 0.0f;
  }
  __syncthreads();

  float q_val = query[thread_idx];
  float k_val = key[thread_idx];

  atomicAdd(&dot_prod, q_val * k_val);
  atomicAdd(&norm_query, q_val * q_val);
  atomicAdd(&norm_key, k_val * k_val);

  __syncthreads();

  if (thread_idx == 0) {
    norm_query = sqrtf(norm_query);
    norm_key = sqrtf(norm_key);

    if (norm_query > 1e-8f && norm_key > 1e-8f) {
      output[key_idx] = dot_prod / (norm_query * norm_key);
    } else {
      output[key_idx] = 0.0f;
    }
  }
}

__global__ void AddKernel(const float* a, const float* b, int n,
                          float* output) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx < n) {
    output[idx] = a[idx] + b[idx];
  }
}

__global__ void MultiplyKernel(const float* a, float scalar, int n,
                               float* output) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx < n) {
    output[idx] = a[idx] * scalar;
  }
}

void Kernels::NormalizeVector(const float* input, int n, float* output,
                              cudaStream_t stream) {
  if (n <= 0) return;
  int block_size = 256;
  int grid_size = (n + block_size - 1) / block_size;
  NormalizeKernel<<<grid_size, block_size, 0, stream>>>(input, n, output);
  CUDA_CHECK(cudaGetLastError());
  CUDA_CHECK(cudaStreamSynchronize(stream));
}

float Kernels::CosineSimilarity(const float* a, const float* b, int n,
                                cudaStream_t stream) {
  if (n <= 0) return 0.0f;
  float* d_out = nullptr;
  CUDA_CHECK(cudaMalloc(&d_out, sizeof(float)));
  CosineSimilarityKernel<<<1, n, 0, stream>>>(a, b, n, d_out);
  CUDA_CHECK(cudaGetLastError());
  float result = 0.0f;
  CUDA_CHECK(cudaMemcpy(&result, d_out, sizeof(float), cudaMemcpyDeviceToHost));
  CUDA_CHECK(cudaFree(d_out));
  return result;
}

void Kernels::BatchCosineSimilarity(const float* query, const float* keys,
                                    int num_keys, int vec_dim, float* output,
                                    cudaStream_t stream) {
  if (num_keys <= 0 || vec_dim <= 0) return;
  // One block per key, threads for vector dimension
  int block_size = std::min(vec_dim, 256);
  BatchCosineSimilarityKernel<<<num_keys, block_size, 0, stream>>>(
      query, keys, num_keys, vec_dim, output);
  CUDA_CHECK(cudaGetLastError());
  CUDA_CHECK(cudaStreamSynchronize(stream));
}

void Kernels::Add(const float* a, const float* b, int n, float* output,
                  cudaStream_t stream) {
  if (n <= 0) return;
  int block_size = 256;
  int grid_size = (n + block_size - 1) / block_size;
  AddKernel<<<grid_size, block_size, 0, stream>>>(a, b, n, output);
  CUDA_CHECK(cudaGetLastError());
  CUDA_CHECK(cudaStreamSynchronize(stream));
}

void Kernels::Multiply(const float* a, float scalar, int n, float* output,
                       cudaStream_t stream) {
  if (n <= 0) return;
  int block_size = 256;
  int grid_size = (n + block_size - 1) / block_size;
  MultiplyKernel<<<grid_size, block_size, 0, stream>>>(a, scalar, n, output);
  CUDA_CHECK(cudaGetLastError());
  CUDA_CHECK(cudaStreamSynchronize(stream));
}

}  // namespace gpu
}  // namespace orator
