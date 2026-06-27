#pragma once

// DeviceScratch: a per-instance pool of grow-on-demand device buffers, indexed
// by slot, handing out stable device pointers that are reused across calls.
//
// Replaces per-call `cudaMalloc`/`cudaFree` on a pipeline's hot path. Each slot
// grows only when a call needs more bytes than ever before; once it reaches the
// steady-state maximum, no allocation happens. This both removes the per-call
// allocation overhead and makes the region capturable as a CUDA graph
// (`cudaMalloc` is illegal inside a capture region).
//
// Single thread of control per instance (one pipeline worker holds its own
// DeviceScratch). Pointers for a slot are stable until the next Get() for that
// SAME slot requests a larger size (which reallocates and may move it) -- so a
// buffer must not be aliased across a grow of its own slot.

#include <cstddef>
#include <memory>
#include <vector>

#include "gpu/memory.h"

namespace orator {
namespace gpu {

class DeviceScratch {
 public:
  // A device pointer to a buffer of at least `bytes` for `slot`, grown on
  // demand. Returns nullptr for bytes == 0.
  void* Get(int slot, size_t bytes) {
    if (bytes == 0) return nullptr;
    if (slot >= static_cast<int>(bufs_.size())) bufs_.resize(slot + 1);
    if (!bufs_[slot] || bufs_[slot]->size() < bytes) {
      bufs_[slot] = std::make_shared<DeviceBuffer>(bytes);
    }
    return bufs_[slot]->data();
  }

  // Typed convenience: `count` elements of T.
  template <class T>
  T* GetT(int slot, size_t count) {
    return static_cast<T*>(Get(slot, count * sizeof(T)));
  }

  // Drop all buffers (e.g. on session reset); they regrow on next use.
  void Clear() { bufs_.clear(); }

 private:
  std::vector<std::shared_ptr<DeviceBuffer>> bufs_;
};

}  // namespace gpu
}  // namespace orator
