#pragma once

// GpuScheduler: a per-pipeline GPU stream priority registry (Spec 002).
//
// Each pipeline declares a PRIORITY INDEX when it registers (0 = most
// latency-critical; a larger index is lower priority). The scheduler maps that
// index onto the device's supported CUDA stream-priority range
// (cudaDeviceGetStreamPriorityRange) and, when asked, creates a CUDA stream at
// the derived priority. This single mapping is the only place that converts a
// declared index into a concrete stream priority, so adding a pipeline is a
// registration change, not a scheduler edit (Constitution Art. V.4; spec FR1,
// FR6).
//
// The registry is also the single source of truth for the GPU-scheduling
// telemetry snapshot (spec FR7): Snapshot() returns every registration's class
// and assigned priority, which the controller serializes alongside each
// pipeline's compute/occupancy summary.
//
// Layering: this type lives in the gpu/ layer and depends only on the CUDA
// runtime. It never includes pipeline/ or model/ headers; consumers (the
// controller) own an instance and pass the returned streams to their workers.
//
// A "background" pipeline is one that must yield to foreground pipelines so the
// latency-critical work always makes progress. The class records the flag and
// exposes a bounded-yield consultation (NoteForegroundSubmit / ShouldYield) for
// the future lock-free path; recording it here keeps the priority policy in one
// place.

#include <cuda_runtime.h>

#include <chrono>
#include <mutex>
#include <string>
#include <vector>

namespace orator {
namespace gpu {

class GpuScheduler {
 public:
  // One pipeline's scheduling state.
  struct Entry {
    std::string name;
    int priority_index = 0;     // 0 = most latency-critical; larger = lower
    bool background = false;    // background pipelines yield to foreground
    bool stream_active = false; // a dedicated prioritized stream was created
    int cuda_priority = 0;      // concrete CUDA stream priority assigned
    cudaStream_t stream = nullptr;  // owned by the scheduler when stream_active
  };

  GpuScheduler();
  ~GpuScheduler();

  GpuScheduler(const GpuScheduler&) = delete;
  GpuScheduler& operator=(const GpuScheduler&) = delete;

  // Register a pipeline with its declared priority index and background flag.
  // When create_stream is true, a CUDA stream at the derived priority is created
  // and returned (owned by the scheduler); otherwise the default stream (0) is
  // returned and only the class metadata is recorded (its engine is not yet
  // stream-routed). Re-registering a name returns its existing stream.
  cudaStream_t Register(const std::string& name, int priority_index,
                        bool background, bool create_stream);

  // Snapshot of all registrations (for telemetry). Thread-safe.
  std::vector<Entry> Snapshot() const;

  // The supported CUDA stream priority range. greatest is the numerically
  // smallest value (highest priority); least is the lowest priority.
  void PriorityRange(int* greatest, int* least) const {
    *greatest = greatest_;
    *least = least_;
  }

  // Bounded-yield consultation for a future lock-free background path: record a
  // foreground submission; a background pipeline should yield while a foreground
  // submission occurred within the recent window.
  void NoteForegroundSubmit();
  bool ShouldBackgroundYield(double window_sec = 0.05) const;

  // Map a declared priority index to a concrete CUDA stream priority within the
  // device's supported range. Exposed for telemetry of not-yet-streamed classes.
  int PriorityForIndex(int priority_index) const;

 private:
  mutable std::mutex mu_;
  std::vector<Entry> entries_;
  int greatest_ = 0;  // highest priority (numerically smallest)
  int least_ = 0;     // lowest priority
  std::chrono::steady_clock::time_point last_fg_submit_{};
  bool have_fg_submit_ = false;
};

}  // namespace gpu
}  // namespace orator
