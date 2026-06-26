#include "pipeline/shared_audio_buffer.h"

#include <algorithm>

namespace orator {
namespace pipeline {

SharedAudioBuffer::SharedAudioBuffer(int sample_rate)
    : sample_rate_(sample_rate) {}

int SharedAudioBuffer::AddConsumer() {
  std::lock_guard<std::mutex> lock(mutex_);
  cursors_.push_back(base_sample_);
  return static_cast<int>(cursors_.size()) - 1;
}

void SharedAudioBuffer::Append(const float* samples, int n) {
  if (samples == nullptr || n <= 0) return;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    // Pre-allocate to avoid repeated reallocations
    if (samples_.size() + n > samples_.capacity()) {
      samples_.reserve(samples_.size() + n + 1024); // Add buffer for future growth
    }
    samples_.insert(samples_.end(), samples, samples + n);
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

  const long from = cursors_[cursor] - base_sample_;
  const long count = total_samples_ - cursors_[cursor];
  
  // Release lock before doing the copy to reduce lock contention
  lock.unlock();
  out->assign(samples_.begin() + from, samples_.begin() + from + count);
  
  // Re-acquire lock to update cursor and remove passed prefix
  lock.lock();
  cursors_[cursor] = total_samples_;

  RemovePassedPrefix();
  return true;
}

void SharedAudioBuffer::RemovePassedPrefix() {
  if (cursors_.empty()) return;
  const long min_cursor = *std::min_element(cursors_.begin(), cursors_.end());
  const long remove_count = min_cursor - base_sample_;
  if (remove_count <= 0) return;
  
  // Remove passed prefix and shrink to fit to release memory
  samples_.erase(samples_.begin(), samples_.begin() + remove_count);
  base_sample_ = min_cursor;
  
  // Shrink to fit to release memory if buffer is too large
  if (samples_.capacity() > 10000000) { // If capacity > 10M samples (~40MB)
    std::vector<float> temp(std::move(samples_));
    samples_.swap(temp);
  }
}

void SharedAudioBuffer::Reset() {
  std::lock_guard<std::mutex> lock(mutex_);
  samples_.clear();
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

}  // namespace pipeline
}  // namespace orator
