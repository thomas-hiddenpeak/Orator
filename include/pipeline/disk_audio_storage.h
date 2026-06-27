#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace orator {
namespace pipeline {

// DiskAudioStorage: stores audio samples (float) to SSD based on absolute
// sample offsets. Data is written in append-only fashion and read by
// start_sample offset.
class DiskAudioStorage {
 public:
  explicit DiskAudioStorage(const std::string& storage_dir);
  ~DiskAudioStorage();

  // Disable copy and move
  DiskAudioStorage(const DiskAudioStorage&) = delete;
  DiskAudioStorage& operator=(const DiskAudioStorage&) = delete;

  // Write samples starting at start_sample_offset to disk.
  // Returns the number of bytes written.
  size_t Write(long start_sample_offset, const float* samples, int n);

  // Read n samples starting at start_sample_offset from disk into out buffer.
  // Returns the number of samples actually read.
  int Read(long start_sample_offset, int n, std::vector<float>* out) const;

  // Cleanup old disk data before min_sample_offset (optional optimization)
  void CleanupBefore(long min_sample_offset);

 private:
  std::string storage_dir_;
  std::string data_file_path_;
  std::string index_file_path_;

  mutable std::mutex mutex_;
  long total_disk_samples_written_ = 0;
};

}  // namespace pipeline
}  // namespace orator
