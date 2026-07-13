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

enum class SchedulingMode { kAuto = 0, kSerial = 1, kConcurrent = 2 };

// Configure the process-wide mode before workers start. kAuto currently selects
// full concurrency. Runtime configuration is resolved by the server entry point
// and passed here explicitly; this layer never reads process environment.
void ConfigureSchedulingMode(SchedulingMode mode);
SchedulingMode CurrentSchedulingMode();

// Spec 002: GPU concurrency mode. The production default is full concurrency:
// all pipelines (ASR, diarization, VAD) run lock-free.
//
// ConcurrentGpuEnabled() is true only in full mode.
bool ConcurrentGpuEnabled();

// True if ANY lock-free concurrency mode is active (the ASR-only default or
// full). The ASR decoder uses this to disable CUDA Graph capture, which a
// concurrently-issuing pipeline corrupts ("operation failed ... during capture"
// abort). It is therefore true by default now.
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
