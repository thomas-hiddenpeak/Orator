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
  // gated OFF. A direct test (ORATOR_GPU_CONCURRENT=1, ASR lock-free on its own
  // stream while diarization ran concurrently on the default stream) produced an
  // IMMEDIATE SIGSEGV on the first 120 s stream. Root cause = R1: the verified
  // engines pervasively host-access cudaMallocManaged memory (`UnifiedBuffer`)
  // during steady-state streaming (result copies, scalar reads). On Tegra
  // unified memory, a host access to managed pages while another pipeline's
  // kernel executes faults via page migration. Fixing the one named case
  // (`AsrTextDecoder::Embed`, now reading a plain-host shadow) was necessary but
  // NOT sufficient; safe concurrency requires de-coupling BOTH engines from
  // managed memory (device buffers + pinned host staging), a separate gated
  // phase. Until then DeviceGuard ALWAYS takes the lock, so the env flag cannot
  // enable the faulting path; the scaffolding (priority registry, stream-scoped
  // diar syncs) remains for that future work.
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
