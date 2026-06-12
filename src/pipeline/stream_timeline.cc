#include "pipeline/stream_timeline.h"

namespace orator {
namespace pipeline {

void StreamTimeline::AppendDiarFrames(const core::DiarizationFrames& part) {
  if (part.num_frames <= 0) return;
  std::lock_guard<std::mutex> lock(mutex_);
  diar_probs_.insert(diar_probs_.end(), part.probs.begin(), part.probs.end());
  diar_frames_ += part.num_frames;
  diar_speakers_ = part.num_speakers;
  diar_frame_period_sec_ = part.frame_period_sec;
}

void StreamTimeline::AppendToken(const core::AsrToken& token) {
  std::lock_guard<std::mutex> lock(mutex_);
  transcript_.tokens.push_back(token);
}

core::DiarizationFrames StreamTimeline::SnapshotDiarFrames() const {
  std::lock_guard<std::mutex> lock(mutex_);
  core::DiarizationFrames frames;
  frames.num_frames = static_cast<int>(diar_frames_);
  frames.num_speakers = diar_speakers_;
  frames.frame_period_sec = diar_frame_period_sec_;
  frames.t_start_sec = 0.0;
  frames.probs = diar_probs_;
  return frames;
}

core::Transcript StreamTimeline::SnapshotTranscript() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return transcript_;
}

void StreamTimeline::Clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  diar_probs_.clear();
  diar_frames_ = 0;
  diar_speakers_ = 0;
  diar_frame_period_sec_ = 0.0;
  transcript_.tokens.clear();
}

}  // namespace pipeline
}  // namespace orator
