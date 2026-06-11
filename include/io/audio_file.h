#pragma once

// Audio file ingestion: decode WAV or MP3 into mono float PCM at a target rate.
//
// This is an I/O utility for feeding real-world audio into the pipeline. The MP3
// path uses a vendored public-domain decoder (third_party/minimp3); the core
// runtime stays dependency-free.

#include <string>
#include <vector>

namespace orator {
namespace io {

struct AudioData {
  std::vector<float> samples;  // mono, normalized to [-1, 1]
  int sample_rate = 0;

  double DurationSec() const {
    return sample_rate > 0 ? static_cast<double>(samples.size()) / sample_rate
                           : 0.0;
  }
};

// Loads a .wav or .mp3 file, downmixes to mono, and resamples (linear) to
// target_rate. Throws std::runtime_error on failure.
AudioData LoadAudioMono(const std::string& path, int target_rate = 16000);

}  // namespace io
}  // namespace orator
