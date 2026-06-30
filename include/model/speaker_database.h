#pragma once

// SpeakerDatabase: GPU-accelerated speaker embedding registry implementing
// ISpeakerRegistry. Stores enrolled speaker embeddings in device memory and
// provides 1:N cosine-similarity matching against query embeddings.
// Retained but inactive — not currently wired into any runtime pipeline.

#include <cstdint>
#include <mutex>
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
  // Like Match, but ignores any enrolled id in `exclude_ids` (used to honour the
  // diarizer's within-session separation: two local slots of the same session
  // are distinct speakers and must never resolve to the same global id).
  int MatchExcluding(const float* embedding, float threshold,
                     const std::vector<std::string>& exclude_ids,
                     float* out_score) const;
  std::string SpeakerIdAt(int index) const override;
  int EmbeddingDim() const override { return embedding_dim_; }
  int Size() const override { return size_; }

  // Additional management API.
  bool Update(const std::string& speaker_id, const float* embedding);
  // Remove an enrolled speaker (used by the identity stage's de-duplication: a
  // duplicate of an existing person is merged away so the registry holds one
  // entry per real speaker). O(1): the last row is swapped into the freed slot.
  bool Remove(const std::string& speaker_id);
  bool Contains(const std::string& speaker_id) const;
  int Capacity() const { return max_speakers_; }
  int IndexOf(const std::string& speaker_id) const;

  const float* Embeddings() const {
    return static_cast<const float*>(embeddings_.data());
  }
  float* Embeddings() { return static_cast<float*>(embeddings_.data()); }

  bool Save(const std::string& path) const;
  bool Load(const std::string& path);

  // Display-name hook (Spec 010 R6): associate a human-readable name with a
  // global voiceprint id. Names persist in a sidecar ("<path>.names") next to
  // the registry; there is no UI -- this is an interface for a future caller.
  void SetDisplayName(const std::string& speaker_id, const std::string& name);
  std::string DisplayName(const std::string& speaker_id) const;

 private:
  int max_speakers_;
  int embedding_dim_;
  int size_;

  gpu::UnifiedBuffer embeddings_;
  std::vector<std::string> speaker_ids_;
  std::unordered_map<std::string, int> speaker_to_index_;

  mutable std::mutex names_mutex_;
  std::unordered_map<std::string, std::string> names_;  // id -> display name

  void WriteEmbeddingAt(int index, const float* embedding);
};

}  // namespace model
}  // namespace orator
