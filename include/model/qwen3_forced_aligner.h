#pragma once

// Qwen3 Forced Aligner: end-to-end forced alignment.
//
// Given a mono 16 kHz audio span and its transcript, produces per-word
// timestamps in a single non-autoregressive forward pass. Wires the verified
// stages: WhisperMel -> AsrAudioTower (reused, output 1024) +
// multi_modal_projector -> tokenise + assemble input_ids -> AlignerLm (28-layer
// causal LM + score head) -> argmax timestamp labels -> _fix_timestamps decode.
//
// Pure C++20/CUDA runtime; weights load from the aligner checkpoint directory.

#include <string>
#include <vector>

#include "core/stages.h"
#include "io/bpe_tokenizer.h"
#include "feature/whisper_mel.h"
#include "model/asr_audio_tower.h"
#include "model/forced_align_decode.h"
#include "model/qwen3_aligner_lm.h"

namespace orator {
namespace model {

class Qwen3ForcedAligner : public core::IForcedAligner {
 public:
  Qwen3ForcedAligner();

  void set_profile(bool enabled) override {
    profile_ = enabled;
    tower_.set_profile(enabled);
  }

  // Load tokenizer + audio tower + projector + language model + score head from
  // the model directory (models/ForcedAligner).
  void LoadWeights(const std::string& model_dir) override;

  // Align `transcript` to `pcm` (mono 16 kHz, `n` samples, up to ~5 min).
  // `language` is a full name (e.g. "Chinese", "English") or empty. Returns the
  // per-word units with start/end seconds relative to the start of `pcm`.
  std::vector<core::AlignUnit> Align(
      const float* pcm, int n, const std::string& transcript,
      const std::string& language = "") const override;

  std::string name() const override { return "qwen3_forced_aligner"; }

 private:
  // Aligner special token ids (fixed by the checkpoint's tokenizer).
  static constexpr int kAudioStart = 151669;
  static constexpr int kAudioPad = 151676;
  static constexpr int kAudioEnd = 151670;
  static constexpr int kTimestamp = 151705;

  feature::WhisperMel mel_;
  AsrAudioTower tower_;
  AlignerLm lm_;
  io::BpeTokenizer tok_;
  bool loaded_ = false;
  bool profile_ = false;
};

}  // namespace model
}  // namespace orator
