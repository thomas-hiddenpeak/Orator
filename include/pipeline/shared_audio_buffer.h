#pragma once

// SharedAudioBuffer: the single coupling point between the ingest producer and
// the independent pipeline consumers (diarization, ASR).
//
// Per the architecture (Spec 001 / plan.md §2.1), audio entering the system is
// the only thing the two businesses share. The producer appends decoded mono
// PCM on one absolute clock (sample 0 == stream start). Each consumer holds its
// OWN read cursor and pulls "everything new since I last read" at its own pace;
// consumers never see or wait on each other. Memory is bounded by dropping only
// the prefix that EVERY consumer has already passed (a low-water mark), so no
// consumer can lose unread audio while a faster one races ahead.
//
// Thread-safety: all mutable state is guarded by `mutex_`; `cv_` signals
// "new data appended or stream closed". Consumers block in WaitAndRead instead
// of spinning. The class owns no threads -- the controller that spawns the
// pipeline workers owns their lifecycle.

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <vector>

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
  // signal for the consumer loop to exit.
  bool WaitAndRead(int cursor, std::vector<float>* out);

  // Clear all state for a fresh session. Consumers MUST be stopped (joined)
  // first; this resets cursors and re-arms the buffer.
  void Reset();

  int sample_rate() const { return sample_rate_; }

  // Total samples appended this session (absolute clock head). Thread-safe.
  long total_samples() const;

  // Absolute index of the oldest sample still retained (low-water mark).
  long base_sample() const;

 private:
  // Drop the prefix every consumer has already read. Caller holds `mutex_`.
  void TrimToLowWaterMark();

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
