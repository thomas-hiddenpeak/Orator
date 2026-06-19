#pragma once

// DiskBackend (Spec 004 Phase 10): file-backed message store.
//
// Writes serialized message bytes to a POSIX file opened with O_APPEND.
// Reads use lseek to the stored offset followed by read().
// Thread-safe via internal mutex.

#include <cstdint>
#include <mutex>
#include <string>

namespace orator {
namespace protocol {

class DiskBackend {
 public:
  // path: directory for storage files. Creates directory if needed.
  // session_id: unique session identifier for file naming.
  explicit DiskBackend(const std::string& path,
                       const std::string& session_id = "default");
  ~DiskBackend();

  DiskBackend(const DiskBackend&) = delete;
  DiskBackend& operator=(const DiskBackend&) = delete;

  // Write data to the file. Returns file offset for later read.
  uint64_t Write(const uint8_t* data, uint32_t size);

  // Read data from a file offset. Returns actual bytes read.
  uint32_t Read(uint64_t offset, uint32_t size, uint8_t* out) const;

  // Get total bytes written.
  uint64_t total_bytes() const { return total_bytes_; }

 private:
  std::string file_path_;
  int fd_ = -1;
  uint64_t total_bytes_ = 0;
  mutable std::mutex mutex_;
};

}  // namespace protocol
}  // namespace orator
