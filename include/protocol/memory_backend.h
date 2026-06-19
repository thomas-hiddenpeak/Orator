#pragma once

// MemoryBackend (Spec 004 Phase 10): in-memory message store.
//
// Stores serialized message bytes in an ordered map keyed by logical offset.
// When total stored size exceeds capacity, the oldest entries (by offset) are
// evicted until the total fits. Thread-safe via internal mutex.

#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <vector>

namespace orator {
namespace protocol {

class MemoryBackend {
 public:
  static constexpr size_t kDefaultCapacity = 128 * 1024 * 1024;  // 128 MB

  explicit MemoryBackend(size_t capacity = kDefaultCapacity);

  // Write data to the store. Returns logical offset for later read.
  // Evicts oldest entries if total size would exceed capacity.
  uint64_t Write(const uint8_t* data, uint32_t size);

  // Read data from a logical offset. Returns actual bytes read (0 if not found).
  uint32_t Read(uint64_t offset, uint32_t size, uint8_t* out) const;

  // Get current usage statistics.
  size_t capacity() const { return capacity_; }
  size_t used() const;

 private:
  // Evict oldest entries until total_bytes_ <= capacity_.
  void EvictIfNeeded();

  std::map<uint64_t, std::vector<uint8_t>> store_;
  size_t capacity_;
  uint64_t next_offset_ = 0;
  mutable std::mutex mutex_;
};

}  // namespace protocol
}  // namespace orator
