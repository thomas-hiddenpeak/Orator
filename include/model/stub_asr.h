#pragma once

// Lightweight ASR used for MVP wiring and tests.
//
// Produces a deterministic, timed placeholder transcript from short-time
// energy: each contiguous run of voiced (above-threshold) audio becomes one
// "[speech]" token with absolute timing. It carries no language model and no
// weights, yet satisfies the core::IAsr contract so the pipeline cannot tell
// it apart from a real engine (e.g. a future Qwen3-ASR). This lets the unified
// diarization+ASR timeline be exercised end-to-end without model assets.

#include <string>

#include "core/stages.h"

namespace orator {
namespace model {

class StubAsr final : public core::IAsr {
 public:
  void Initialize(const core::AsrConfig& config) override;
  void LoadWeights(const std::string& path) override;
  void Reset() override;
  core::Transcript Transcribe(const core::AudioChunk& audio) override;

  std::string name() const override { return "stub"; }

 private:
  int sample_rate_ = 16000;
  double window_sec_ = 0.5;   // granularity of emitted tokens
  float rms_threshold_ = 1e-3f;
};

}  // namespace model
}  // namespace orator
