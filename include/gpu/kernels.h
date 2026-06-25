#pragma once

// CUDA kernel collection: basic vector operations (Add, Multiply,
// NormalizeVector, CosineSimilarity, BatchCosineSimilarity). Each kernel
// is validated against a CPU reference in test_kernels. Used by the GPU
// VAD detector and potentially by model inference layers.

#include <cuda_runtime.h>

namespace orator {
namespace gpu {
class Kernels {
 public:
  // Normalize vector (L2 norm)
  // Input: input[n], Output: output[n] = input[n] / ||input||
  static void NormalizeVector(const float* input, int n, float* output,
                              cudaStream_t stream = nullptr);

  // Compute cosine similarity between two vectors
  // Returns: dot(a, b) / (||a|| * ||b||)
  static float CosineSimilarity(const float* a, const float* b, int n,
                                cudaStream_t stream = nullptr);

  // Batch cosine similarity: compute similarity between one query and multiple keys
  // query[n], keys[m*n], output[m]
  static void BatchCosineSimilarity(const float* query, const float* keys,
                                    int num_keys, int vec_dim,
                                    float* output,
                                    cudaStream_t stream = nullptr);

  // Element-wise operations
  static void Add(const float* a, const float* b, int n, float* output,
                  cudaStream_t stream = nullptr);
  static void Multiply(const float* a, float scalar, int n, float* output,
                       cudaStream_t stream = nullptr);
};

}  // namespace gpu
}  // namespace orator
