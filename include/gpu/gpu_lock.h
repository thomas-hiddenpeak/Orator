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

}  // namespace gpu
}  // namespace orator
