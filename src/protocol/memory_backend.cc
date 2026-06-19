#include "protocol/memory_backend.h"

#include <algorithm>
#include <cstring>

namespace orator {
namespace protocol {

MemoryBackend::MemoryBackend(size_t capacity)
    : capacity_(capacity), next_offset_(0) {}

uint64_t MemoryBackend::Write(const uint8_t* data, uint32_t size) {
  std::lock_guard<std::mutex> lock(mutex_);

  uint64_t offset = next_offset_++;

  // Evict oldest entries if adding this would exceed capacity.
  size_t total = 0;
  for (const auto& kv : store_) {
    total += kv.second.size();
  }
  if (total + size > capacity_) {
    // Evict oldest entries until we have room.
    while (total + size > capacity_ && !store_.empty()) {
      auto first = store_.begin();
      total -= first->second.size();
      store_.erase(first);
    }
  }

  std::vector<uint8_t> blob(data, data + size);
  store_[offset] = std::move(blob);

  return offset;
}

uint32_t MemoryBackend::Read(uint64_t offset, uint32_t size,
                             uint8_t* out) const {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = store_.find(offset);
  if (it == store_.end()) return 0;

  uint32_t available = static_cast<uint32_t>(it->second.size());
  uint32_t to_read = std::min(size, available);
  std::memcpy(out, it->second.data(), to_read);
  return to_read;
}

size_t MemoryBackend::used() const {
  std::lock_guard<std::mutex> lock(mutex_);
  size_t total = 0;
  for (const auto& kv : store_) {
    total += kv.second.size();
  }
  return total;
}

}  // namespace protocol
}  // namespace orator
