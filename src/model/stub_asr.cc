#include "model/stub_asr.h"

#include <algorithm>
#include <cmath>

namespace orator {
namespace model {

void StubAsr::Initialize(const core::AsrConfig& config) {
  sample_rate_ = config.sample_rate > 0 ? config.sample_rate : 16000;
  Reset();
}

void StubAsr::LoadWeights(const std::string& /*path*/) {
  // Stub: no weights.
}

void StubAsr::Reset() {
  // Stub is stateless across calls.
}

core::Transcript StubAsr::Transcribe(const core::AudioChunk& audio) {
  core::Transcript out;
  if (audio.samples == nullptr || audio.num_samples <= 0) return out;

  const int sr = audio.sample_rate > 0 ? audio.sample_rate : sample_rate_;
  const int win = std::max(1, static_cast<int>(sr * window_sec_));
  const int num_windows = (audio.num_samples + win - 1) / win;

  // Run-length merge of voiced windows into contiguous tokens.
  bool in_token = false;
  double token_start = 0.0;
  double token_end = 0.0;

  auto flush = [&]() {
    if (!in_token) return;
    core::AsrToken tok;
    tok.start_sec = token_start;
    tok.end_sec = token_end;
    tok.text = "[speech]";
    out.tokens.push_back(std::move(tok));
    in_token = false;
  };

  for (int w = 0; w < num_windows; ++w) {
    const int begin = w * win;
    const int end = std::min(audio.num_samples, begin + win);
    double energy = 0.0;
    for (int s = begin; s < end; ++s) {
      const double v = audio.samples[s];
      energy += v * v;
    }
    const int count = std::max(1, end - begin);
    const float rms = static_cast<float>(std::sqrt(energy / count));

    const double t0 = audio.t_start_sec + static_cast<double>(begin) / sr;
    const double t1 = audio.t_start_sec + static_cast<double>(end) / sr;

    if (rms >= rms_threshold_) {
      if (!in_token) {
        in_token = true;
        token_start = t0;
      }
      token_end = t1;
    } else {
      flush();
    }
  }
  flush();

  return out;
}

}  // namespace model
}  // namespace orator
