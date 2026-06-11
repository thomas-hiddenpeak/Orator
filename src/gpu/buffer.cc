#include "gpu/buffer.h"

#include "gpu/memory.h"
#include <cstring>

namespace orator {
namespace gpu {

AudioBuffer::AudioBuffer(size_t capacity)
    : capacity_(capacity), write_pos_(0), start_pos_(0),
      device_buffer_(capacity * sizeof(float), GpuMemory::unified_allocator()) {
  ptr_ = static_cast<float*>(device_buffer_.data());
}

size_t AudioBuffer::Write(const float* samples, size_t num_samples) {
  if (num_samples > capacity_) {
    throw std::invalid_argument("Cannot write more samples than buffer capacity");
  }

  size_t write_offset = write_pos_;
  size_t remaining = num_samples;

  // Handle wrap-around at buffer end
  if (write_pos_ + remaining > capacity_) {
    size_t first_chunk = capacity_ - write_pos_;
    std::memcpy(ptr_ + write_pos_, samples, first_chunk * sizeof(float));

    size_t second_chunk = remaining - first_chunk;
    std::memcpy(ptr_, samples + first_chunk, second_chunk * sizeof(float));
    write_pos_ = second_chunk;
  } else {
    std::memcpy(ptr_ + write_pos_, samples, remaining * sizeof(float));
    write_pos_ += remaining;
  }

  if (write_pos_ >= capacity_) {
    write_pos_ = write_pos_ % capacity_;
  }

  return write_offset;
}

void AudioBuffer::Read(size_t offset, size_t num_samples, float* output) {
  // Validate that we're not trying to read more than capacity
  if (num_samples > capacity_) {
    throw std::invalid_argument("Cannot read more samples than buffer capacity");
  }

  // For ring buffer, handle wrap-around at the capacity boundary
  if (offset + num_samples <= capacity_) {
    // No wrap-around needed
    std::memcpy(output, ptr_ + offset, num_samples * sizeof(float));
  } else {
    // Wrap-around case: data spans the buffer boundary
    size_t first_chunk = capacity_ - offset;
    std::memcpy(output, ptr_ + offset, first_chunk * sizeof(float));

    size_t second_chunk = num_samples - first_chunk;
    std::memcpy(output + first_chunk, ptr_, second_chunk * sizeof(float));
  }
}

}  // namespace gpu
}  // namespace orator

