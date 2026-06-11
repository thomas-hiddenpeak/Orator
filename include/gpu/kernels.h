#pragma once

namespace orator {
namespace gpu {

// Basic vector operations kernels
class Kernels {
 public:
  // Normalize vector (L2 norm)
  // Input: input[n], Output: output[n] = input[n] / ||input||
  static void NormalizeVector(const float* input, int n, float* output);

  // Compute cosine similarity between two vectors
  // Returns: dot(a, b) / (||a|| * ||b||)
  static float CosineSimilarity(const float* a, const float* b, int n);

  // Batch cosine similarity: compute similarity between one query and multiple keys
  // query[n], keys[m*n], output[m]
  static void BatchCosineSimilarity(const float* query, const float* keys,
                                    int num_keys, int vec_dim,
                                    float* output);

  // Element-wise operations
  static void Add(const float* a, const float* b, int n, float* output);
  static void Multiply(const float* a, float scalar, int n, float* output);
};

}  // namespace gpu
}  // namespace orator
