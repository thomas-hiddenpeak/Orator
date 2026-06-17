#pragma once

// SharedAudioBuffer: the single point of coupling between the ingest producer
// and the independent pipeline consumers (diarization, ASR).
//
// Per the architecture (Spec 001 / plan.md §2.1), the input audio is the only
// data the two pipelines share. The producer appends decoded mono PCM indexed
// by absolute sample position (sample 0 is the start of the stream). Each
// consumer holds its own read cursor and reads the samples appended since its
// last read, at its own rate; consumers do not read or wait on each other. The
// buffer removes a prefix only after every consumer's cursor has advanced past
// it (the minimum of all cursors), so no consumer loses unread audio when one
// consumer is further ahead than another.
//
// Thread-safety: all mutable state is guarded by `mutex_`; `cv_` signals that
// samples were appended or the stream was closed. A consumer blocks in
// WaitAndRead until one of those occurs, rather than polling. This class owns
// no threads; the controller that starts the pipeline workers owns their
// lifecycle.

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <vector>

#include "core/time_base.h"

namespace orator {
namespace pipeline {

class SharedAudioBuffer {
 public:
  explicit SharedAudioBuffer(int sample_rate = 16000);

  SharedAudioBuffer(const SharedAudioBuffer&) = delete;
  SharedAudioBuffer& operator=(const SharedAudioBuffer&) = delete;

  // Register a consumer before streaming starts; returns its cursor handle.
  // Handles are dense indices [0, num_consumers). Not thread-safe against
  // concurrent Append/WaitAndRead -- call during setup only.
  int AddConsumer();

  // Producer: append `n` mono float samples at the current stream head and wake
  // any waiting consumers. n <= 0 is a no-op.
  void Append(const float* samples, int n);

  // Producer: declare that no more audio will arrive. Wakes all consumers so
  // each can drain its remaining tail and then exit. Idempotent.
  void Close();

  // Consumer: block until samples beyond `cursor` are available or the stream
  // is closed. On return with `true`, `out` is overwritten with the newly
  // available span and the cursor is advanced past it. Returns `false` only
  // when the stream is closed AND this cursor has consumed everything -- the
  // signal for the consumer loop to exit. If `span_start_abs` is non-null it
  // receives the ABSOLUTE sample index (on the common clock) of out->front() --
  // i.e. the cursor's position before this read -- so a consumer can anchor its
  // local time codes onto the common time base.
  bool WaitAndRead(int cursor, std::vector<float>* out,
                   long* span_start_abs = nullptr);

  // Clear all state for a fresh session. Consumers MUST be stopped (joined)
  // first; this resets cursors and re-initializes the buffer.
  void Reset();

  int sample_rate() const { return sample_rate_; }

  // The session's common time base (origin = stream start, sample 0). Every
  // consumer derives its time codes from this so all pipelines align by
  // construction rather than by each counting from 0 independently.
  core::TimeBase time_base() const { return core::TimeBase(sample_rate_, 0); }

  // Total samples appended this session (absolute clock head). Thread-safe.
  long total_samples() const;

  // Absolute index of the oldest sample still retained.
  long base_sample() const;

  // Returns the absolute read position of cursor `idx`. Thread-safe.
  long cursor_position(int idx) const;

 private:
  // Remove the leading samples that every consumer has already read. The caller
  // holds `mutex_`.
  void RemovePassedPrefix();

  mutable std::mutex mutex_;
  std::condition_variable cv_;

  const int sample_rate_;
  std::vector<float> samples_;  // retained window; samples_[0] == base_sample_
  long base_sample_ = 0;        // absolute index of samples_.front()
  long total_samples_ = 0;      // absolute count appended this session
  std::vector<long> cursors_;   // absolute read position per consumer
  bool closed_ = false;
};

}  // namespace pipeline
}  // namespace orator
