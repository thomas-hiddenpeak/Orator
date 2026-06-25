#pragma once

// SpeakerDatabase: GPU-accelerated speaker embedding registry implementing
// ISpeakerRegistry. Stores enrolled speaker embeddings in device memory and
// provides 1:N cosine-similarity matching against query embeddings.
// Retained but inactive — not currently wired into any runtime pipeline.

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/stages.h"
#include "gpu/memory.h"

namespace orator {
namespace model {

// Unified-memory speaker registry with GPU-accelerated 1:N cosine matching.
// Implements core::ISpeakerRegistry so the pipeline depends only on the
// interface, not this concrete store.
class SpeakerDatabase final : public core::ISpeakerRegistry {
 public:
  SpeakerDatabase(int max_speakers, int embedding_dim);

  // ISpeakerRegistry
  bool Enroll(const std::string& speaker_id, const float* embedding) override;
  int Match(const float* embedding, float threshold,
            float* out_score) const override;
  std::string SpeakerIdAt(int index) const override;
  int EmbeddingDim() const override { return embedding_dim_; }
  int Size() const override { return size_; }

  // Additional management API.
  bool Update(const std::string& speaker_id, const float* embedding);
  bool Contains(const std::string& speaker_id) const;
  int Capacity() const { return max_speakers_; }
  int IndexOf(const std::string& speaker_id) const;

  const float* Embeddings() const {
    return static_cast<const float*>(embeddings_.data());
  }
  float* Embeddings() { return static_cast<float*>(embeddings_.data()); }

  bool Save(const std::string& path) const;
  bool Load(const std::string& path);

 private:
  int max_speakers_;
  int embedding_dim_;
  int size_;

  gpu::UnifiedBuffer embeddings_;
  std::vector<std::string> speaker_ids_;
  std::unordered_map<std::string, int> speaker_to_index_;

  void WriteEmbeddingAt(int index, const float* embedding);
};

}  // namespace model
}  // namespace orator
