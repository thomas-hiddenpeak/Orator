#pragma once

// AsrPreprocessor: audio preprocessing pipeline for ASR input. Applies
// noise reduction (spectral subtraction), optional DC offset removal, and
// gain normalization to raw PCM before feeding the ASR model.

#include <string>
#include <vector>

namespace orator {
namespace pipeline {
// Runs AFTER ASR VAD segmentation and BEFORE ASR decode so it never affects
// the diarization pipeline features.
class AsrPreprocessor {
 public:
  struct Params {
    int sample_rate = 16000;
    std::string mode = "none";  // none|classical|frcrn|tfgridnet
    std::string frcrn_model_path = "models/asr_preproc/frcrn.safetensors";
    std::string tfgridnet_model_path =
        "models/asr_preproc/tfgridnet.safetensors";
  };

  explicit AsrPreprocessor(const Params& params);

  // Apply noise reduction and normalization to one utterance.
  // Input and output have identical sample count.
  void Process(const float* samples, int n, std::vector<float>* out) const;

 private:
  enum class Mode {
    kNone,
    kClassical,
    kFrcrn,
    kTfGridNet,
  };

  Mode ParseMode(const std::string& mode) const;
  // Classical DSP pipeline: spectral subtraction + DC removal + gain.
  void ClassicalEnhance(const float* in, int n, std::vector<float>* out) const;
  // FRCRN neural enhancement (not yet wired — returns false).
  bool RunFrcrnModelScope(const float* in, int n,
                          std::vector<float>* out) const;
  // Issue a warning at most once per instance lifetime.
  void WarnOnce(const char* msg) const;

  Params params_;
  Mode mode_ = Mode::kNone;
  mutable bool warned_ = false;
};

}  // namespace pipeline
}  // namespace orator
