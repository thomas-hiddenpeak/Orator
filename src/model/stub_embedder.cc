#include "model/stub_embedder.h"

#include <cmath>

namespace orator {
namespace model {

std::vector<float> StubEmbedder::Embed(const core::AudioChunk& chunk) {
  std::vector<float> emb(static_cast<size_t>(dim_), 0.0f);
  if (chunk.samples == nullptr || chunk.num_samples <= 0) return emb;

  // Bucketed energy descriptor: spread samples across dim_ bins and accumulate
  // squared energy. Deterministic and stable for the same input.
  for (int i = 0; i < chunk.num_samples; ++i) {
    const int bin = (i * dim_) / chunk.num_samples;
    const float v = chunk.samples[i];
    emb[static_cast<size_t>(bin)] += v * v;
  }

  // L2 normalize for cosine similarity.
  double norm = 0.0;
  for (float v : emb) norm += static_cast<double>(v) * v;
  norm = std::sqrt(norm);
  if (norm > 1e-8) {
    for (float& v : emb) v = static_cast<float>(v / norm);
  }
  return emb;
}

}  // namespace model
}  // namespace orator
