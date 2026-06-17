#pragma once

// GpuLock: a process-wide mutex serializing GPU access across the independent
// pipeline threads.
//
// The diarization and ASR pipelines run on separate threads (Spec 001), but
// they share ONE physical GPU. On Tegra's unified memory in particular, a host
// read of managed memory (e.g. the decoder's embedding table) while another
// thread has a kernel in flight faults. The two pipelines are therefore
// independent on the CPU side (buffering, endpointing, serialization all
// overlap) but their GPU-touching regions are mutually exclusive: each worker
// holds this lock only around the span of work that issues kernels / copies or
// touches device-managed memory.
//
// This is a deliberate, honest acknowledgement of the single-GPU constraint --
// the threads do not pretend to run GPU math in parallel; they interleave on
// the device while overlapping their CPU work.

#include <mutex>

namespace orator {
namespace gpu {

// The single GPU access lock. Acquire via `std::lock_guard<std::mutex>
// guard(gpu::DeviceLock());` around a GPU-touching region.
std::mutex& DeviceLock();

// Spec 002: whether concurrent GPU mode is enabled (env ORATOR_GPU_CONCURRENT,
// read once). In concurrent mode a pipeline that runs on its OWN non-default
// CUDA stream (currently only ASR) does not take the global lock: its stream and
// stream-scoped synchronization order its work, so it can overlap another
// pipeline's GPU work. Pipelines that share the default stream (diarization and
// VAD) ALWAYS take the lock so their kernels never interleave on stream 0.
// Default (unset) is serialized mode: every GPU region takes the lock, exactly
// as before (the safe, validated production behavior).
bool ConcurrentGpuEnabled();

// Spec 002: whether ANY lock-free concurrency mode is active (full
// ORATOR_GPU_CONCURRENT or the ASR-only experiment ORATOR_GPU_CONCURRENT_ASR).
// When true, the ASR pipeline's GPU work runs WITHOUT the global lock while the
// diarization/VAD pipelines may run kernels concurrently. CUDA Graph capture is
// unsafe in this state (capture is a process/stream-global state machine that a
// concurrently-issuing pipeline corrupts -> "operation failed ... during
// capture" abort), so the ASR decoder disables graph capture when this is true.
bool ConcurrentGpuActive();

// RAII GPU-region guard (Spec 002). In serialized mode it always holds
// DeviceLock() for the region's lifetime. In concurrent mode it skips the lock
// ONLY when `own_stream` is true (the pipeline runs on its own non-default
// stream); a default-stream pipeline still locks so its kernels stay ordered.
class DeviceGuard {
 public:
  // own_stream=false (default): shares the default stream -> always lock.
  // own_stream=true: has a dedicated non-default stream -> lock-free in
  // concurrent mode.
  explicit DeviceGuard(bool own_stream = false);
  ~DeviceGuard();

  DeviceGuard(const DeviceGuard&) = delete;
  DeviceGuard& operator=(const DeviceGuard&) = delete;

 private:
  bool locked_;
};

}  // namespace gpu
}  // namespace orator
