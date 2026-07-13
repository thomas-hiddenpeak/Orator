#pragma once

// RetainedAudioBuffer: a bounded sliding window of recent audio, addressed by
// absolute sample index, for on-demand span reads.
//
// The forced-alignment pipeline does not drain audio sequentially like the
// per-pipeline PipelineAudioCache (single producer / single consumer, freed on
// read). Instead it must, when an ASR transcript segment [start, end] arrives
// (lagging its audio), read back exactly that audio span. This buffer retains
// the last `retain_sec` seconds keyed by absolute sample position and serves
// arbitrary spans; older audio is dropped to bound memory. Single producer
// (the ingest fan-out) + reads from the align worker; guarded by a mutex.

#include <cstddef>
#include <mutex>
#include <vector>

#include "core/time_base.h"

namespace orator {
namespace pipeline {

class RetainedAudioBuffer {
 public:
  RetainedAudioBuffer(core::TimeBase time_base, double retain_sec);

  // Append `n` mono float samples at the stream head. Drops the oldest samples
  // beyond the retention window. Thread-safe.
  void Append(const float* samples, int n);

  // Return the audio for absolute samples [start_sample, end_sample). If the
  // requested span is not fully within the retained window, returns empty.
  // Thread-safe.
  std::vector<float> ReadSpan(long start_sample, long end_sample) const;

  // Oldest retained absolute sample index.
  long base_sample() const;
  // Total samples appended this session (absolute clock head).
  long total_samples() const;

  const core::TimeBase& time_base() const { return time_base_; }

  void Reset();

 private:
  mutable std::mutex mutex_;
  const core::TimeBase time_base_;
  const long retain_samples_;
  std::vector<float> buf_;  // retained tail: buf_[0] is absolute sample base_
  long base_ = 0;
  long total_ = 0;
};

}  // namespace pipeline
}  // namespace orator
