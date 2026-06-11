#include "model/stub_diarizer.h"

#include <algorithm>
#include <cmath>

namespace orator {
namespace model {

void StubDiarizer::Initialize(const core::DiarizationConfig& config) {
  sample_rate_ = config.sample_rate > 0 ? config.sample_rate : 16000;
  max_speakers_ = config.max_speakers > 0 ? config.max_speakers : 4;
  threshold_ = config.activity_threshold;
  hop_samples_ = std::max(1, static_cast<int>(sample_rate_ * 0.01));
  frame_period_sec_ = 0.01 * 8;  // hop * subsampling
  Reset();
}

void StubDiarizer::LoadWeights(const std::string& /*path*/) {
  // Stub: no weights.
}

void StubDiarizer::Reset() {
  stream_time_sec_ = 0.0;
  global_frame_ = 0;
}

core::DiarizationFrames StubDiarizer::ProcessChunk(const core::AudioChunk& chunk) {
  const int samples_per_diar_frame = hop_samples_ * 8;
  const int diar_frames =
      std::max(1, chunk.num_samples / std::max(1, samples_per_diar_frame));

  core::DiarizationFrames out;
  out.num_frames = diar_frames;
  out.num_speakers = max_speakers_;
  out.frame_period_sec = frame_period_sec_;
  out.t_start_sec = stream_time_sec_;
  out.probs.assign(static_cast<size_t>(diar_frames) * max_speakers_, 0.0f);

  for (int f = 0; f < diar_frames; ++f) {
    // Short-time energy gate over the frame's samples.
    const int begin = f * samples_per_diar_frame;
    const int end = std::min(chunk.num_samples, begin + samples_per_diar_frame);
    double energy = 0.0;
    for (int s = begin; s < end; ++s) {
      const double v = chunk.samples[s];
      energy += v * v;
    }
    const int count = std::max(1, end - begin);
    const double rms = std::sqrt(energy / count);

    const int active_speaker =
        static_cast<int>((global_frame_ / std::max(1, frames_per_speaker_)) %
                         max_speakers_);
    // Active when there is signal energy; silence stays all-zero.
    const float prob = rms > 1e-4 ? 1.0f : 0.0f;
    out.probs[static_cast<size_t>(f) * max_speakers_ + active_speaker] = prob;
    ++global_frame_;
  }

  stream_time_sec_ += diar_frames * frame_period_sec_;
  return out;
}

}  // namespace model
}  // namespace orator
