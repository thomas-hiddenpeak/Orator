#pragma once

// SharedAudioBuffer: RETAINED BUT INACTIVE (Constitution Art. VIII).
//
// As of the per-pipeline cache refactor (2026-06-27) this class is NO LONGER on
// the production runtime path. `AuditoryStream` now gives each pipeline its own
// private `PipelineAudioCache` (single producer / single consumer, consumed
// prefix freed immediately) instead of one shared store with multiple cursors.
// This header is kept only as a reference implementation and for its
// concurrency test (`test/unit/core/test_shared_buffer.cc`); no `src/`, `net/`,
// or `pipeline/` code constructs it. Do not treat it as live behaviour.
//
// ----------------------------------------------------------------------------
// Original design (for reference): the single point of coupling between the
// ingest producer and the independent pipeline consumers (diarization, ASR).
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
  struct Config {
    size_t max_memory_samples;  // Fixed size of in-memory ring buffer (0 = no limit, defaults to 60s @ 16kHz = 960000)
    size_t shrink_threshold;    // 10M samples ~ 40MB
    
    Config() : max_memory_samples(960000), shrink_threshold(10000000) {}
    Config(size_t max_mem, size_t shrink_t) : max_memory_samples(max_mem), shrink_threshold(shrink_t) {}
  };

  explicit SharedAudioBuffer(int sample_rate = 16000);
  explicit SharedAudioBuffer(int sample_rate, const Config& config);
  ~SharedAudioBuffer();

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
  //
  // `max_batch_samples` caps how many samples a single read returns (0 = no cap,
  // return everything available). This lets a consumer pull the backlog in
  // fixed-size batches at its own (device-determined) maximum speed, so its
  // per-batch behaviour does not depend on how fast the producer floods the
  // buffer -- the consumer sees the same batch sequence whether audio arrives in
  // real time or all at once. The cursor advances only by what is returned, so
  // the next call continues from where this one stopped.
  bool WaitAndRead(int cursor, std::vector<float>* out,
                   long* span_start_abs = nullptr, long max_batch_samples = 0);

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

  // Get cursor progress info for telemetry. Thread-safe.
  struct CursorProgress {
    long total_samples;
    long base_sample;
    std::vector<std::pair<int, long>> cursors; // (cursor_id, position)
    size_t buffer_size; // current buffer size in samples
  };
  CursorProgress GetCursorProgress() const;

 private:
  // Remove the leading samples that every consumer has already read. The caller
  // holds `mutex_`.
  void RemovePassedPrefix();

  mutable std::mutex mutex_;
  std::condition_variable cv_;

  const int sample_rate_;
  Config config_;
  
  // Append-only sample store. The consumed prefix is erased once every cursor
  // has passed it (RemovePassedPrefix), so retained memory tracks the in-flight
  // window -- which stays small because every consumer runs faster than realtime.
  std::vector<float> memory_buffer_;
  long memory_start_sample_ = 0; // absolute sample index of memory_buffer_.front()

  long base_sample_ = 0;        // absolute index of the oldest retained sample
  long total_samples_ = 0;      // absolute count appended this session
  std::vector<long> cursors_;   // absolute read position per consumer
  bool closed_ = false;
};

}  // namespace pipeline
}  // namespace orator
