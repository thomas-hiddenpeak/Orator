#pragma once

// Deterministic speaker embedder for MVP wiring and tests.
//
// Computes a small fixed-dimension descriptor from simple signal statistics.
// Not a real speaker model, but satisfies ISpeakerEmbedder so the registry /
// matching path is exercised end-to-end.

#include <string>
#include <vector>

#include "core/stages.h"

namespace orator {
namespace model {

class StubEmbedder final : public core::ISpeakerEmbedder {
 public:
  explicit StubEmbedder(int dim = 64) : dim_(dim) {}

  void LoadWeights(const std::string& /*path*/) override {}
  int dim() const override { return dim_; }
  std::vector<float> Embed(const core::AudioChunk& chunk) override;
  std::string name() const override { return "stub_embedder"; }

 private:
  int dim_;
};

}  // namespace model
}  // namespace orator
