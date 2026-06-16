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
  // Skip the lock only for an own-stream pipeline in concurrent mode; otherwise
  // serialize via the global lock (the safe default and the diar/VAD path).
  const bool skip = own_stream && ConcurrentGpuEnabled();
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
