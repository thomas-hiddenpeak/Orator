#include "pipeline/disk_audio_storage.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

namespace orator {
namespace pipeline {

namespace {
// Helper to ensure directory exists
void EnsureDirectory(const std::string& dir) {
  mkdir(dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
}
}  // namespace

DiskAudioStorage::DiskAudioStorage(const std::string& storage_dir)
    : storage_dir_(storage_dir) {
  EnsureDirectory(storage_dir_);
  data_file_path_ = storage_dir_ + "/audio_data.bin";
  index_file_path_ = storage_dir_ + "/audio_index.dat";

  // Open or create data file
  int fd = open(data_file_path_.c_str(), O_RDWR | O_CREAT,
                S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
  if (fd >= 0) {
    // Get current file size to determine total samples written
    struct stat st;
    if (fstat(fd, &st) == 0) {
      total_disk_samples_written_ =
          static_cast<long>(st.st_size) / sizeof(float);
    }
    close(fd);
  }
}

DiskAudioStorage::~DiskAudioStorage() {
  // No active file descriptors held open continuously
}

size_t DiskAudioStorage::Write(long start_sample_offset, const float* samples,
                               int n) {
  if (samples == nullptr || n <= 0) return 0;

  std::lock_guard<std::mutex> lock(mutex_);

  int fd = open(data_file_path_.c_str(), O_RDWR | O_APPEND);
  if (fd < 0) return 0;

  // Seek to the correct offset
  off_t seek_pos = static_cast<off_t>(start_sample_offset) * sizeof(float);
  if (lseek(fd, seek_pos, SEEK_SET) < 0) {
    close(fd);
    return 0;
  }

  // Write data
  ssize_t bytes_to_write = static_cast<ssize_t>(n * sizeof(float));
  ssize_t written = write(fd, samples, bytes_to_write);
  close(fd);

  if (written > 0) {
    total_disk_samples_written_ =
        std::max(total_disk_samples_written_, start_sample_offset + n);
    return static_cast<size_t>(written);
  }
  return 0;
}

int DiskAudioStorage::Read(long start_sample_offset, int n,
                           std::vector<float>* out) const {
  if (n <= 0 || out == nullptr) return 0;

  std::lock_guard<std::mutex> lock(mutex_);

  int fd = open(data_file_path_.c_str(), O_RDONLY);
  if (fd < 0) return 0;

  off_t seek_pos = static_cast<off_t>(start_sample_offset) * sizeof(float);
  if (lseek(fd, seek_pos, SEEK_SET) < 0) {
    close(fd);
    return 0;
  }

  // Determine how many samples are available
  struct stat st;
  if (fstat(fd, &st) != 0) {
    close(fd);
    return 0;
  }

  long available_samples = static_cast<long>(st.st_size) / sizeof(float);
  long start_idx = start_sample_offset;
  if (start_idx < 0) start_idx = 0;

  long count_to_read =
      std::min(static_cast<long>(n), available_samples - start_idx);
  if (count_to_read <= 0) {
    close(fd);
    return 0;
  }

  // Resize output buffer
  out->resize(static_cast<size_t>(count_to_read));

  // Read data
  ssize_t bytes_to_read = static_cast<ssize_t>(count_to_read * sizeof(float));
  ssize_t read_bytes = read(fd, out->data(), bytes_to_read);
  close(fd);

  if (read_bytes > 0) {
    return static_cast<int>(read_bytes / sizeof(float));
  }
  return 0;
}

void DiskAudioStorage::CleanupBefore(long min_sample_offset) {
  // Optional: implement truncation or deletion of old disk data
  // For now, we keep all data to ensure no loss for replay purposes
  (void)min_sample_offset;
}

}  // namespace pipeline
}  // namespace orator
