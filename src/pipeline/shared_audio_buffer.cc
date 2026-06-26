#include "pipeline/shared_audio_buffer.h"
#include "pipeline/disk_audio_storage.h"

#include <algorithm>

namespace orator {
namespace pipeline {

SharedAudioBuffer::SharedAudioBuffer(int sample_rate)
    : sample_rate_(sample_rate), config_(), disk_storage_(nullptr) {}

SharedAudioBuffer::SharedAudioBuffer(int sample_rate, const Config& config)
    : sample_rate_(sample_rate), config_(config), disk_storage_(nullptr) {
  // Initialize disk storage if configured
  // For now, we don't have a disk_storage_dir in Config, so we'll skip disk storage
  // until it's explicitly added to Config. But we keep the structure ready.
}

SharedAudioBuffer::~SharedAudioBuffer() {
  delete disk_storage_;
}

int SharedAudioBuffer::AddConsumer() {
  std::lock_guard<std::mutex> lock(mutex_);
  cursors_.push_back(base_sample_);
  return static_cast<int>(cursors_.size()) - 1;
}

void SharedAudioBuffer::Append(const float* samples, int n) {
  if (samples == nullptr || n <= 0) return;
  
  {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check in-memory buffer size limit
    if (config_.max_memory_samples > 0 && (memory_buffer_.size() + n > config_.max_memory_samples)) {
      // Memory buffer full, write overflow to disk
      const size_t overflow = (memory_buffer_.size() + n) - config_.max_memory_samples;
      
      // Calculate samples to write to disk
      long samples_to_disk_start = memory_start_sample_;
      int samples_to_disk_count = static_cast<int>(std::min(static_cast<size_t>(overflow), memory_buffer_.size()));
      
      // Write to disk if disk storage is available
      if (disk_storage_ && samples_to_disk_count > 0) {
        disk_storage_->Write(samples_to_disk_start, memory_buffer_.data(), samples_to_disk_count);
      }
      
      // Remove written samples from memory buffer
      if (samples_to_disk_count > 0) {
        memory_buffer_.erase(memory_buffer_.begin(), memory_buffer_.begin() + samples_to_disk_count);
        memory_start_sample_ += samples_to_disk_count;
      }
      
      // If memory buffer is now empty after writing to disk, reset base
      if (memory_buffer_.empty()) {
        memory_start_sample_ = total_samples_;
      }
    }
    
    // Pre-allocate to avoid repeated reallocations
    if (memory_buffer_.size() + n > memory_buffer_.capacity()) {
      memory_buffer_.reserve(memory_buffer_.size() + n + 1024); // Add buffer for future growth
    }
    
    // Append new samples to memory buffer
    memory_buffer_.insert(memory_buffer_.end(), samples, samples + n);
    total_samples_ += n;
  }
  cv_.notify_all();
}

void SharedAudioBuffer::Close() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    closed_ = true;
  }
  cv_.notify_all();
}

bool SharedAudioBuffer::WaitAndRead(int cursor, std::vector<float>* out,
                                    long* span_start_abs) {
  std::unique_lock<std::mutex> lock(mutex_);
  cv_.wait(lock, [&] { return cursors_[cursor] < total_samples_ || closed_; });

  if (cursors_[cursor] >= total_samples_)
    return false;  // all samples read and the stream is closed

  // The cursor's absolute position before this read is the absolute index of
  // the first returned sample (on the common clock).
  if (span_start_abs != nullptr) *span_start_abs = cursors_[cursor];

  const long requested_start = cursors_[cursor];
  const long requested_end = total_samples_;
  const long count = requested_end - requested_start;
  
  // Capture memory state while holding the lock
  const long current_memory_start_sample = memory_start_sample_;
  const size_t current_memory_buffer_size = memory_buffer_.size();
  
  // Release lock before doing the copy to reduce lock contention
  lock.unlock();
  
  // Read from memory or disk
  out->clear();
  if (requested_start >= current_memory_start_sample && (requested_start + count) <= (current_memory_start_sample + static_cast<long>(current_memory_buffer_size))) {
    // All requested data is in memory buffer
    long from_in_buffer = requested_start - current_memory_start_sample;
    out->assign(memory_buffer_.begin() + from_in_buffer, memory_buffer_.begin() + from_in_buffer + count);
  } else {
    // Data spans memory and disk, or is entirely on disk
    // For simplicity and correctness, read from disk if not fully in memory
    // Note: This is a simplified implementation. A full implementation would handle partial memory/disk splits.
    if (disk_storage_) {
      std::lock_guard<std::mutex> disk_lock(mutex_); // Re-acquire lock for disk access if needed, but DiskAudioStorage has its own lock
      disk_storage_->Read(requested_start, static_cast<int>(count), out);
    } else {
      // Fallback: try to read from memory buffer if possible
      std::lock_guard<std::mutex> mem_lock(mutex_); // Re-acquire to read memory_buffer safely
      long from_in_buffer = std::max(0L, requested_start - memory_start_sample_);
      long available_in_memory = static_cast<long>(memory_buffer_.size()) - from_in_buffer;
      if (available_in_memory > 0 && requested_start >= memory_start_sample_) {
        int read_count = static_cast<int>(std::min(static_cast<long>(count), available_in_memory));
        out->assign(memory_buffer_.begin() + from_in_buffer, memory_buffer_.begin() + from_in_buffer + read_count);
      }
    }
  }
  
  // Re-acquire lock to update cursor and remove passed prefix
  lock.lock();
  cursors_[cursor] = total_samples_;

  RemovePassedPrefix();
  return true;
}
  
  // Re-acquire lock to update cursor and remove passed prefix
  lock.lock();
  cursors_[cursor] = total_samples_;

  RemovePassedPrefix();
  return true;
}

void SharedAudioBuffer::RemovePassedPrefix() {
  if (cursors_.empty()) return;
  const long min_cursor = *std::min_element(cursors_.begin(), cursors_.end());
  
  // Update base_sample to min_cursor
  base_sample_ = min_cursor;
  
  // If min_cursor has advanced past memory_start_sample_, clean up memory buffer
  if (min_cursor >= memory_start_sample_) {
    long samples_to_remove_from_memory = std::min(static_cast<long>(memory_buffer_.size()), min_cursor - memory_start_sample_);
    if (samples_to_remove_from_memory > 0) {
      memory_buffer_.erase(memory_buffer_.begin(), memory_buffer_.begin() + static_cast<size_t>(samples_to_remove_from_memory));
      memory_start_sample_ += samples_to_remove_from_memory;
    }
    
    // If memory buffer is empty or min_cursor is far ahead, write remaining memory to disk and clear
    if (memory_buffer_.empty() || (config_.max_memory_samples > 0 && memory_buffer_.size() >= config_.max_memory_samples)) {
      // Write remaining memory buffer to disk if not empty
      if (!memory_buffer_.empty() && disk_storage_) {
        disk_storage_->Write(memory_start_sample_, memory_buffer_.data(), static_cast<int>(memory_buffer_.size()));
        memory_buffer_.clear();
        memory_start_sample_ = min_cursor;
      }
    }
  }
}

void SharedAudioBuffer::Reset() {
  std::lock_guard<std::mutex> lock(mutex_);
  memory_buffer_.clear();
  memory_start_sample_ = 0;
  base_sample_ = 0;
  total_samples_ = 0;
  closed_ = false;
  std::fill(cursors_.begin(), cursors_.end(), 0L);
}

long SharedAudioBuffer::total_samples() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return total_samples_;
}

long SharedAudioBuffer::base_sample() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return base_sample_;
}

long SharedAudioBuffer::cursor_position(int idx) const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (idx < 0 || idx >= static_cast<int>(cursors_.size())) return 0;
  return cursors_[idx];
}

SharedAudioBuffer::CursorProgress SharedAudioBuffer::GetCursorProgress() const {
  std::lock_guard<std::mutex> lock(mutex_);
  CursorProgress progress;
  progress.total_samples = total_samples_;
  progress.base_sample = base_sample_;
  progress.buffer_size = memory_buffer_.size();
  progress.cursors.reserve(cursors_.size());
  for (size_t i = 0; i < cursors_.size(); ++i) {
    progress.cursors.push_back({static_cast<int>(i), cursors_[i]});
  }
  return progress;
}

}  // namespace pipeline
}  // namespace orator
