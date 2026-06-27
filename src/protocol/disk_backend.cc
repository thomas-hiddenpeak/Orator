#include "protocol/disk_backend.h"

#include <algorithm>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace orator {
namespace protocol {

DiskBackend::DiskBackend(const std::string& path,
                         const std::string& session_id) {
  // Create directory if it doesn't exist.
  mkdir(path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

  file_path_ = path + "/" + session_id + ".dat";

  unlink(file_path_.c_str());

  fd_ = open(file_path_.c_str(), O_RDWR | O_CREAT,
             S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
}

DiskBackend::~DiskBackend() {
  if (fd_ >= 0) {
    close(fd_);
    fd_ = -1;
  }
}

uint64_t DiskBackend::Write(const uint8_t* data, uint32_t size) {
  std::lock_guard<std::mutex> lock(mutex_);

  // Seek to end to append.
  off_t offset = lseek(fd_, 0, SEEK_END);
  if (offset < 0) return 0;

  // Write data.
  ssize_t written = write(fd_, data, size);
  if (written < 0) return 0;

  total_bytes_ += static_cast<uint64_t>(written);
  return static_cast<uint64_t>(offset);
}

uint32_t DiskBackend::Read(uint64_t offset, uint32_t size, uint8_t* out) const {
  std::lock_guard<std::mutex> lock(mutex_);

  if (lseek(fd_, static_cast<off_t>(offset), SEEK_SET) < 0) return 0;

  ssize_t bytes_read = read(fd_, out, size);
  return static_cast<uint32_t>(bytes_read > 0 ? bytes_read : 0);
}

}  // namespace protocol
}  // namespace orator
