#pragma once

// RingBuffer<T>: a GPU-accessible circular buffer for streaming audio in
// unified memory. Supports lock-free single-producer single-consumer access
// via head/tail indices. Used by SharedAudioBuffer for the audio ingestion
// ring buffer shared across all pipeline consumers.

#include <cstddef>
#include <vector>

#include "gpu/memory.h"

namespace orator {
namespace gpu {

// Ring buffer for streaming audio data in unified memory
class AudioBuffer {
 public:
  // Constructor: capacity in number of float samples
  explicit AudioBuffer(size_t capacity);

  // Write audio samples from host to unified buffer
  // Returns the offset where samples were written
  size_t Write(const float* samples, size_t num_samples);

  // Read audio samples from unified buffer to host
  void Read(size_t offset, size_t num_samples, float* output);

  // Get a pointer to buffer data (accessible from GPU)
  float* GetPtr() { return ptr_; }

  // Get current write position
  size_t GetWritePos() const { return write_pos_; }

  // Get buffer size in samples
  size_t GetCapacity() const { return capacity_; }

  // Get number of available samples (from start to write_pos)
  size_t GetAvailableSamples() const {
    return (write_pos_ >= start_pos_) ? (write_pos_ - start_pos_)
                                      : (capacity_ - start_pos_ + write_pos_);
  }

  // Reset buffer
  void Reset() {
    write_pos_ = 0;
    start_pos_ = 0;
  }

 private:
  size_t capacity_;
  size_t write_pos_;  // Where new data will be written
  size_t start_pos_;  // Where readable data starts (for ring buffer semantics)
  UnifiedBuffer device_buffer_;
  float* ptr_;  // Cached pointer for GPU access
};

}  // namespace gpu
}  // namespace orator
