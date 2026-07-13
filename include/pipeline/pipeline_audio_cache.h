#pragma once

// PipelineAudioCache: one pipeline's PRIVATE audio backlog.
//
// Architecture (Constitution Art. III -- independent pipelines on a common time
// base): every registered pipeline (diarization, ASR, VAD) gets its own cache.
// The ingest producer fans each decoded mono PCM frame out to ALL caches, so
// every cache holds the same audio indexed by the same absolute sample position
// (sample 0 is the start of the stream). The common time base therefore stays
// valid across pipelines by construction -- absolute position N is the same
// audio in every cache.
//
// Each cache is single-producer / single-consumer. Unlike the shared buffer
// (one store, many cursors, prefix freed only behind the SLOWEST cursor), here
// the consumed samples are released immediately after each read because there
// is exactly one consumer. Retained memory therefore tracks only THIS
// pipeline's unread backlog: a slow consumer never pins another pipeline's
// audio, and each pipeline drains its backlog at its own (device-determined)
// maximum speed.
//
// Thread-safety: all mutable state is guarded by `mutex_`; `cv_` signals that
// samples were appended or the stream was closed. The consumer blocks in
// WaitAndRead until one of those occurs rather than polling. This class owns no
// threads; the controller that starts the pipeline worker owns its lifecycle.

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

#include "core/time_base.h"

namespace orator {
namespace pipeline {

class PipelineAudioCache {
 public:
  struct Config {
    // Cap on the unread in-memory backlog, in samples (0 = unbounded). Today
    // the cache is memory-only; this is enforced once SSD spill-over lands.
    size_t max_memory_samples = 0;

    // ── SSD spill-over (INTERFACE PLACEHOLDER -- NOT YET IMPLEMENTED) ──────
    // When the unread backlog exceeds `offload_threshold_samples` and
    // `offload_dir` is non-empty, the oldest unread samples will be paged to
    // disk and read back on demand, so a far-behind consumer cannot exhaust
    // memory. The fields are wired through the constructor so callers and the
    // config schema are stable; the implementation is deferred (the main body
    // -- the per-pipeline memory cache -- ships first).
    std::string offload_dir;
    size_t offload_threshold_samples = 0;
  };

  explicit PipelineAudioCache(core::TimeBase time_base);
  PipelineAudioCache(core::TimeBase time_base, const Config& config);
  ~PipelineAudioCache();

  PipelineAudioCache(const PipelineAudioCache&) = delete;
  PipelineAudioCache& operator=(const PipelineAudioCache&) = delete;

  // Producer: append `n` mono float samples at the stream head and wake the
  // consumer. n <= 0 is a no-op. Thread-safe.
  void Append(const float* samples, int n);

  // Producer: declare that no more audio will arrive. Wakes the consumer so it
  // can drain its remaining tail and then exit. Idempotent.
  void Close();

  // Consumer: block until samples beyond the read cursor are available or the
  // stream is closed. On return with `true`, `out` is overwritten with
  // everything available since the last read (the consumer pulls its whole
  // backlog at once, so its batching does not depend on how fast the producer
  // fed the cache) and the cursor advances past it. Returns `false` only when
  // the stream is closed AND fully drained -- the signal for the consumer loop
  // to exit. If `span_start_abs` is non-null it receives the ABSOLUTE sample
  // index (on the common clock) of out->front(), so the consumer can anchor its
  // local time codes onto the common time base.
  bool WaitAndRead(std::vector<float>* out, long* span_start_abs = nullptr);

  // Clear all state for a fresh session. The consumer MUST be stopped (joined)
  // first.
  void Reset();

  int sample_rate() const { return time_base_.sample_rate(); }

  // The canonical session time base received from the audio-ingest owner.
  const core::TimeBase& time_base() const { return time_base_; }

  // Total samples appended this session (absolute clock head). Thread-safe.
  long total_samples() const;

  // Absolute index the consumer has read up to (== absolute index of the next
  // unread sample). Thread-safe.
  long read_position() const;

  // Unread backlog held in memory (total_samples - read_position). Thread-safe.
  long pending_samples() const;

 private:
  mutable std::mutex mutex_;
  std::condition_variable cv_;

  const core::TimeBase time_base_;
  Config config_;

  // Unread samples only: buf_ holds [read_pos_, total_), so buf_.front() is the
  // sample at absolute index read_pos_. After each read the consumed samples
  // are moved out and buf_ is reset to empty (single consumer -> immediate
  // release).
  std::vector<float> buf_;
  long read_pos_ = 0;  // absolute index of buf_.front() / next unread sample
  long total_ = 0;     // absolute appended count (clock head)
  bool closed_ = false;
};

}  // namespace pipeline
}  // namespace orator
