#include "pipeline/shared_audio_buffer.h"

#include <algorithm>

namespace orator {
namespace pipeline {

SharedAudioBuffer::SharedAudioBuffer(int sample_rate)
    : sample_rate_(sample_rate), config_() {}

SharedAudioBuffer::SharedAudioBuffer(int sample_rate, const Config& config)
    : sample_rate_(sample_rate), config_(config) {}

SharedAudioBuffer::~SharedAudioBuffer() = default;

int SharedAudioBuffer::AddConsumer() {
  std::lock_guard<std::mutex> lock(mutex_);
  cursors_.push_back(base_sample_);
  return static_cast<int>(cursors_.size()) - 1;
}

void SharedAudioBuffer::Append(const float* samples, int n) {
  if (samples == nullptr || n <= 0) return;

  {
    std::lock_guard<std::mutex> lock(mutex_);
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
                                    long* span_start_abs,
                                    long max_batch_samples) {
  std::unique_lock<std::mutex> lock(mutex_);
  cv_.wait(lock, [&] { return cursors_[cursor] < total_samples_ || closed_; });

  if (cursors_[cursor] >= total_samples_)
    return false;  // all samples read and the stream is closed

  // The cursor's absolute position before this read is the absolute index of
  // the first returned sample (on the common clock).
  if (span_start_abs != nullptr) *span_start_abs = cursors_[cursor];

  const long requested_start = cursors_[cursor];
  long count = total_samples_ - requested_start;
  // Cap the batch so a consumer pulls a flooded backlog in fixed-size pieces at
  // its own max speed (rate-independent per-batch behaviour). 0 = no cap.
  if (max_batch_samples > 0 && count > max_batch_samples)
    count = max_batch_samples;

  // Copy under the lock: Append (insert -> possible reallocation) and other
  // consumers' RemovePassedPrefix (erase) mutate memory_buffer_, so reading its
  // iterators without the lock would be a data race / use-after-free.
  // requested_start >= min_cursor >= memory_start_sample_, so the slice is
  // always fully inside memory_buffer_.
  const long from_in_buffer = requested_start - memory_start_sample_;
  out->assign(memory_buffer_.begin() + from_in_buffer,
              memory_buffer_.begin() + from_in_buffer + count);

  cursors_[cursor] =
      requested_start + count;  // advance only by what we returned
  RemovePassedPrefix();
  return true;
}

void SharedAudioBuffer::RemovePassedPrefix() {
  if (cursors_.empty()) return;
  const long min_cursor = *std::min_element(cursors_.begin(), cursors_.end());

  // Update base_sample to min_cursor
  base_sample_ = min_cursor;

  // If min_cursor has advanced past memory_start_sample_, clean up memory
  // buffer
  if (min_cursor >= memory_start_sample_) {
    long samples_to_remove_from_memory =
        std::min(static_cast<long>(memory_buffer_.size()),
                 min_cursor - memory_start_sample_);
    if (samples_to_remove_from_memory > 0) {
      memory_buffer_.erase(
          memory_buffer_.begin(),
          memory_buffer_.begin() +
              static_cast<size_t>(samples_to_remove_from_memory));
      memory_start_sample_ += samples_to_remove_from_memory;
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
