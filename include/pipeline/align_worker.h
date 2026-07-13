#pragma once

// AlignWorker: the forced-alignment pipeline as an independent unit.
//
// It owns the forced aligner and aligns each finalized ASR transcript segment
// to its audio in a single non-autoregressive pass, producing precise per-unit
// (word/character) timestamps. It is a downstream consumer of the ASR
// pipeline's finalized typed transcript track: when ComprehensiveTimeline
// delivers an ASR final it reads back exactly that audio span from a
// retained-window buffer and aligns the known text against it.
//
// Per Constitution Art. III the pipeline never reads another pipeline's
// internal state or protocol serialization -- it consumes only the typed
// transcript (id, [start, end], text) and its retained audio view. Alignment
// runs on its own thread off the
// ASR hot path so it never blocks ingest or ASR; jobs are queued and processed
// in order, and the queue is drained on Stop().

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "core/stages.h"
#include "core/time_base.h"
#include "pipeline/retained_audio_buffer.h"

namespace orator {
namespace pipeline {

class AlignWorker {
 public:
  struct Params {
    int sample_rate = 16000;
    std::string language = "Chinese";
    double max_segment_sec = 300.0;  // skip absurdly long spans (safety cap)
  };

  // Delivers aligned units for one transcript segment. `seg_start`/`seg_end`
  // are the segment's bounds (common clock, sec); unit times are already mapped
  // onto the common clock.
  using UnitsSink =
      std::function<void(long id, double seg_start, double seg_end,
                         const std::vector<core::AlignUnit>& units)>;

  AlignWorker(core::IForcedAligner* aligner, RetainedAudioBuffer* audio,
              const Params& params, core::TimeBase tb);
  ~AlignWorker();

  AlignWorker(const AlignWorker&) = delete;
  AlignWorker& operator=(const AlignWorker&) = delete;

  void set_sink(UnitsSink sink) { sink_ = std::move(sink); }

  // Spawn the alignment thread.
  void Start();
  // Signal end-of-stream, drain the remaining queued jobs, and join the thread.
  void Stop();

  // Called by the session owner after Stop() has drained every finalized ASR
  // job. Advancing to the common final sample records that the align pipeline
  // has consumed all upstream evidence through end-of-stream; alignment
  // coverage is validated separately by exact ASR/alignment ID convergence.
  void FinalizeExtent(long total_samples);

  // Enqueue a finalized transcript segment for alignment. Non-blocking: returns
  // immediately after queuing. Empty text is ignored.
  void Enqueue(long id, double start, double end, const std::string& text);

  double compute_sec() const { return compute_sec_.load(); }
  long aligned_segments() const { return aligned_segments_.load(); }
  long processed_samples() const { return processed_samples_.load(); }

 private:
  struct Job {
    long id;
    double start;
    double end;
    std::string text;
  };

  void Loop();
  void Process(const Job& job);

  core::IForcedAligner* aligner_;
  RetainedAudioBuffer* audio_;
  Params params_;
  core::TimeBase tb_;
  UnitsSink sink_;

  std::thread thread_;
  std::mutex mutex_;
  std::condition_variable cv_;
  std::deque<Job> queue_;
  bool stop_ = false;
  bool running_ = false;

  std::atomic<double> compute_sec_{0.0};
  std::atomic<long> aligned_segments_{0};
  std::atomic<long> processed_samples_{0};
};

}  // namespace pipeline
}  // namespace orator
