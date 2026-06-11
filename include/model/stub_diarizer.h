#pragma once

// Lightweight diarizer used for MVP wiring and tests.
//
// Produces a deterministic frame-level activity track from short-time energy:
// the active speaker slot rotates over time, demonstrating multi-speaker output
// without any model weights. Implements the same core::IDiarizer contract as
// SortformerDiarizer, so the pipeline cannot tell them apart.

#include <string>

#include "core/stages.h"

namespace orator {
namespace model {

class StubDiarizer final : public core::IDiarizer {
 public:
  void Initialize(const core::DiarizationConfig& config) override;
  void LoadWeights(const std::string& path) override;
  void Reset() override;
  core::DiarizationFrames ProcessChunk(const core::AudioChunk& chunk) override;

  int max_speakers() const override { return max_speakers_; }
  double frame_period_sec() const override { return frame_period_sec_; }
  std::string name() const override { return "stub"; }

 private:
  int sample_rate_ = 16000;
  int max_speakers_ = 4;
  float threshold_ = 0.5f;
  double frame_period_sec_ = 0.08;  // matches sortformer hop*subsampling
  int hop_samples_ = 160;
  int frames_per_speaker_ = 12;  // rotate speakers roughly every ~1s
  double stream_time_sec_ = 0.0;
  int64_t global_frame_ = 0;
};

}  // namespace model
}  // namespace orator
