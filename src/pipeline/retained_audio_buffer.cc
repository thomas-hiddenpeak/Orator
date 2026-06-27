#include "pipeline/retained_audio_buffer.h"

#include <algorithm>

namespace orator {
namespace pipeline {

RetainedAudioBuffer::RetainedAudioBuffer(int sample_rate, double retain_sec)
    : sample_rate_(sample_rate),
      retain_samples_(static_cast<long>(retain_sec * sample_rate + 0.5)) {}

void RetainedAudioBuffer::Append(const float* samples, int n) {
  if (n <= 0) return;
  std::lock_guard<std::mutex> lock(mutex_);
  buf_.insert(buf_.end(), samples, samples + n);
  total_ += n;
  if (static_cast<long>(buf_.size()) > retain_samples_) {
    const long drop = static_cast<long>(buf_.size()) - retain_samples_;
    buf_.erase(buf_.begin(), buf_.begin() + drop);
    base_ += drop;
  }
}

std::vector<float> RetainedAudioBuffer::ReadSpan(long start_sample,
                                                 long end_sample) const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (end_sample <= start_sample) return {};
  if (start_sample < base_ || end_sample > total_) return {};
  const long off = start_sample - base_;
  const long len = end_sample - start_sample;
  return std::vector<float>(buf_.begin() + off, buf_.begin() + off + len);
}

long RetainedAudioBuffer::base_sample() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return base_;
}

long RetainedAudioBuffer::total_samples() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return total_;
}

void RetainedAudioBuffer::Reset() {
  std::lock_guard<std::mutex> lock(mutex_);
  buf_.clear();
  base_ = 0;
  total_ = 0;
}

}  // namespace pipeline
}  // namespace orator
