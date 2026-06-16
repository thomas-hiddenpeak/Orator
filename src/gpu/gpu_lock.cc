#include "gpu/gpu_lock.h"

#include <cstdlib>

namespace orator {
namespace gpu {

std::mutex& DeviceLock() {
  static std::mutex lock;
  return lock;
}

bool ConcurrentGpuEnabled() {
  // Read the env once. ORATOR_GPU_CONCURRENT in {1,t,T,y,Y} enables the
  // concurrent path; anything else (including unset) keeps serialized mode.
  static const bool enabled = [] {
    const char* v = std::getenv("ORATOR_GPU_CONCURRENT");
    return v != nullptr &&
           (v[0] == '1' || v[0] == 't' || v[0] == 'T' || v[0] == 'y' ||
            v[0] == 'Y');
  }();
  return enabled;
}

DeviceGuard::DeviceGuard(bool own_stream) : locked_(false) {
  // EMPIRICAL R1 RESULT (Spec 002, 2026-06-17): the lock-free own-stream path is
  // gated OFF. This Orin reports concurrentManagedAccess == 0 (verified by
  // device query), so the host may not touch ANY cudaMallocManaged
  // (`UnifiedBuffer`) memory while a kernel runs on any stream — it faults via
  // page migration. Both engines host-touch managed memory pervasively during
  // streaming (the diarizer's whole SortformerState: spkcache/fifo/spk_perm and
  // its streaming logic; the ASR decoder's PrefillAt/Forward residual + position
  // staging). The ASR-side host-touch sites were de-coupled (device + pinned
  // staging), but the diarizer's managed host access is large and lives in the
  // NeMo-verified engine. Safe concurrency additionally requires routing the
  // diarizer/VAD off the default stream (cudaStreamAttachMemAsync cannot attach
  // managed memory to stream 0 — verified "invalid argument"), so each pipeline
  // owns a non-default stream its managed memory is single-attached to. Until
  // that verified-engine change is made and gated, DeviceGuard ALWAYS takes the
  // lock, so the env flag cannot enable the faulting path.
  constexpr bool kOwnStreamConcurrencySafe = false;
  const bool skip =
      own_stream && kOwnStreamConcurrencySafe && ConcurrentGpuEnabled();
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
