#pragma once

#include <string>
#include <vector>

namespace orator {
namespace pipeline {

// ASR-only audio preprocessor.
//
// Runs AFTER ASR VAD segmentation and BEFORE ASR decode so it never affects
// the diarization pipeline features.
class AsrPreprocessor {
 public:
  struct Params {
    int sample_rate = 16000;
    std::string mode = "none";  // none|classical|frcrn|tfgridnet
    std::string frcrn_model_path = "models/asr_preproc/frcrn.safetensors";
    std::string tfgridnet_model_path = "models/asr_preproc/tfgridnet.safetensors";
  };

  explicit AsrPreprocessor(const Params& params);

  // Process one utterance. Output size equals input size.
  void Process(const float* samples, int n, std::vector<float>* out) const;

 private:
  enum class Mode {
    kNone,
    kClassical,
    kFrcrn,
    kTfGridNet,
  };

  Mode ParseMode(const std::string& mode) const;
  void ClassicalEnhance(const float* in, int n, std::vector<float>* out) const;
  bool RunFrcrnModelScope(const float* in, int n, std::vector<float>* out) const;
  void WarnOnce(const char* msg) const;

  Params params_;
  Mode mode_ = Mode::kNone;
  mutable bool warned_ = false;
};

}  // namespace pipeline
}  // namespace orator
