#include "gpu/gpu_lock.h"

namespace orator {
namespace gpu {

std::mutex& DeviceLock() {
  static std::mutex lock;
  return lock;
}

}  // namespace gpu
}  // namespace orator
