#include "gpu/gpu_lock.h"

#include <cstdlib>

namespace orator {
namespace gpu {

namespace {
// Parse a boolean-ish env var (set + leading 1/t/T/y/Y => true).
bool EnvTrue(const char* name) {
  const char* v = std::getenv(name);
  return v != nullptr && (v[0] == '1' || v[0] == 't' || v[0] == 'T' ||
                          v[0] == 'y' || v[0] == 'Y');
}

// GPU concurrency mode (resolved once at first use). Spec 002:
//   kSerial  — every GPU region takes the global lock (legacy behavior).
//   kFull    — all pipelines (ASR, diarization, VAD) drop the lock. This is the
//              PRODUCTION DEFAULT: all three pipelines run lock-free by default
//              after stream routing is complete.
//   kAsrOnly — ONLY the ASR own-stream pipeline is lock-free; diarization + VAD
//              still hold the lock. Intermediate/legacy option.
// Resolution: ORATOR_GPU_SERIAL=1 forces kSerial; ORATOR_GPU_CONCURRENT=1 selects
// kFull; otherwise kFull (default).
enum class GpuMode { kSerial, kAsrOnly, kFull };

GpuMode ResolveMode() {
  if (EnvTrue("ORATOR_GPU_SERIAL")) return GpuMode::kSerial;
  if (EnvTrue("ORATOR_GPU_CONCURRENT")) return GpuMode::kFull;
  return GpuMode::kFull;
}

GpuMode Mode() {
  static const GpuMode m = ResolveMode();
  return m;
}
}  // namespace

std::mutex& DeviceLock() {
  static std::mutex lock;
  return lock;
}

bool ConcurrentGpuEnabled() {
  // True when full concurrency (all pipelines lock-free) is selected.
  return Mode() == GpuMode::kFull;
}

bool ConcurrentGpuActive() {
  // True if ANY lock-free concurrency mode is active (ASR-only default or full).
  // The ASR decoder uses this to disable CUDA Graph capture, which a
  // concurrently-issuing pipeline corrupts ("operation failed ... during
  // capture" abort).
  return Mode() != GpuMode::kSerial;
}

DeviceGuard::DeviceGuard(bool own_stream) : locked_(false) {
  // Spec 002 concurrency (validated 2026-06-17). This Orin reports
  // concurrentManagedAccess == 0, so the host must not touch managed memory
  // while any kernel runs. The ASR engine's streaming host-touch sites were
  // de-coupled to device + pinned memory, and CUDA Graph capture is auto-disabled
  // under concurrency, so the ASR own-stream pipeline is safe to run lock-free.
  // The diarizer's streaming compute state is pure host memory (HostStreamState)
  // and GpuVad is all-device, so the full mode is also safe; it is simply no
  // faster than ASR-only (diar/VAD share the default stream).
  //
  //   kSerial : everyone locks.
  //   kAsrOnly: ASR (own_stream) is lock-free; diar/VAD lock. PRODUCTION DEFAULT.
  //   kFull   : everyone is lock-free.
  const GpuMode mode = Mode();
  bool skip = false;
  switch (mode) {
    case GpuMode::kSerial: skip = false; break;
    case GpuMode::kAsrOnly: skip = own_stream; break;
    case GpuMode::kFull: skip = true; break;
  }
  if (!skip) {
    DeviceLock().lock();
    locked_ = true;
  }
}

DeviceGuard::~DeviceGuard() {
  if (locked_) DeviceLock().unlock();
}

}  // namespace gpu
}  // namespace orator
