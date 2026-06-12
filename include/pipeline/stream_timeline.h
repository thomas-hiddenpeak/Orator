#pragma once

// StreamTimeline: the mutex-guarded result store where the two independent
// pipeline workers (diarization, ASR) deposit their output on one shared clock.
//
// It is the single point of cross-thread sharing for results (Spec 001 plan.md
// §2.3): the diarization worker appends frame-level activity; the ASR worker
// appends timed tokens. Both happen under one lock. The controller snapshots
// the store under the same lock to serialize the unified timeline. The workers
// otherwise keep their own private state and never touch each other's data.

#include <mutex>
#include <vector>

#include "core/types.h"

namespace orator {
namespace pipeline {

class StreamTimeline {
 public:
  // Append a batch of newly produced diarization frames (probabilities are
  // concatenated in stream order; speaker count and frame period are captured
  // on the first non-empty batch). Thread-safe.
  void AppendDiarFrames(const core::DiarizationFrames& part);

  // Append one completed ASR utterance token. Thread-safe.
  void AppendToken(const core::AsrToken& token);

  // Snapshot the accumulated diarization frames as a single DiarizationFrames
  // (probs + meta + count) on the shared clock starting at t=0. Thread-safe.
  core::DiarizationFrames SnapshotDiarFrames() const;

  // Snapshot the accumulated transcript. Thread-safe.
  core::Transcript SnapshotTranscript() const;

  // Clear all accumulated results for a fresh session. Thread-safe.
  void Clear();

 private:
  mutable std::mutex mutex_;
  std::vector<float> diar_probs_;  // [frame * num_speakers + speaker]
  long diar_frames_ = 0;
  int diar_speakers_ = 0;
  double diar_frame_period_sec_ = 0.0;
  core::Transcript transcript_;
};

}  // namespace pipeline
}  // namespace orator
