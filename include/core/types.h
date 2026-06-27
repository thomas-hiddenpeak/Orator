#pragma once

// Core data model shared across all pipeline stages.
//
// These types form the stable contract between decoupled modules. Stage
// interfaces (see core/stages.h) exchange only these types, so any concrete
// model implementation can be swapped without touching its consumers.

#include <cstdint>
#include <string>
#include <vector>

namespace orator {
namespace core {

// A contiguous block of mono PCM audio with absolute stream timing.
struct AudioChunk {
  const float* samples = nullptr;  // non-owning, length == num_samples
  int num_samples = 0;
  int sample_rate = 16000;
  double t_start_sec = 0.0;  // absolute start time within the stream

  double DurationSec() const {
    return sample_rate > 0 ? static_cast<double>(num_samples) / sample_rate
                           : 0.0;
  }
};

// Frame-level speaker activity probabilities produced by a diarizer.
// probs is row-major [num_frames * num_speakers], values in [0, 1].
struct DiarizationFrames {
  std::vector<float> probs;
  int num_frames = 0;
  int num_speakers = 0;
  double t_start_sec = 0.0;       // absolute time of the first frame
  double frame_period_sec = 0.0;  // seconds advanced per frame

  float At(int frame, int speaker) const {
    return probs[static_cast<size_t>(frame) * num_speakers + speaker];
  }
};

// A continuous interval during which one (local) speaker is active.
struct DiarSegment {
  double start_sec = 0.0;
  double end_sec = 0.0;
  int local_speaker = -1;   // diarizer-local speaker slot index
  std::string speaker_id;   // resolved registry id (may be empty)
  float confidence = 0.0f;  // mean activity probability over the span
};

// A single ASR token/word with timing (the upstream ASR result).
struct AsrToken {
  double start_sec = 0.0;
  double end_sec = 0.0;
  std::string text;
};

// Full ASR transcript for a session/utterance.
struct Transcript {
  std::vector<AsrToken> tokens;
};

// A speaker-attributed span of transcript text. This is the unit the
// downstream LLM consumer reads.
struct TimelineSegment {
  double start_sec = 0.0;
  double end_sec = 0.0;
  std::string speaker_id;
  std::string text;
};

// Ordered, speaker-attributed transcript ready for the LLM consumer.
struct Timeline {
  std::vector<TimelineSegment> segments;
};

}  // namespace core
}  // namespace orator
