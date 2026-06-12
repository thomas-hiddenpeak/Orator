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

bool SharedAudioBuffer::WaitAndRead(int cursor, std::vector<float>* out) {
  std::unique_lock<std::mutex> lock(mutex_);
  cv_.wait(lock, [&] { return cursors_[cursor] < total_samples_ || closed_; });

  if (cursors_[cursor] >= total_samples_)
    return false;  // all samples read and the stream is closed

  const long from = cursors_[cursor] - base_sample_;
  const long count = total_samples_ - cursors_[cursor];
  out->assign(samples_.begin() + from, samples_.begin() + from + count);
  cursors_[cursor] = total_samples_;

  RemovePassedPrefix();
  return true;
}

void SharedAudioBuffer::RemovePassedPrefix() {
  if (cursors_.empty()) return;
  const long min_cursor = *std::min_element(cursors_.begin(), cursors_.end());
  const long remove_count = min_cursor - base_sample_;
  if (remove_count <= 0) return;
  samples_.erase(samples_.begin(), samples_.begin() + remove_count);
  base_sample_ = min_cursor;
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

}  // namespace pipeline
}  // namespace orator
