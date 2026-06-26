#include "pipeline/pipeline_audio_cache.h"

#include <utility>

namespace orator {
namespace pipeline {

PipelineAudioCache::PipelineAudioCache(int sample_rate)
    : PipelineAudioCache(sample_rate, Config()) {}

PipelineAudioCache::PipelineAudioCache(int sample_rate, const Config& config)
    : sample_rate_(sample_rate), config_(config) {}

PipelineAudioCache::~PipelineAudioCache() = default;

void PipelineAudioCache::Append(const float* samples, int n) {
  if (n <= 0 || samples == nullptr) return;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    buf_.insert(buf_.end(), samples, samples + n);
    total_ += n;
  }
  cv_.notify_one();
}

void PipelineAudioCache::Close() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    closed_ = true;
  }
  cv_.notify_one();
}

bool PipelineAudioCache::WaitAndRead(std::vector<float>* out,
                                     long* span_start_abs) {
  std::unique_lock<std::mutex> lock(mutex_);
  cv_.wait(lock, [this] { return !buf_.empty() || closed_; });
  if (buf_.empty()) return false;  // closed and fully drained

  if (span_start_abs) *span_start_abs = read_pos_;

  // Hand the whole unread backlog to the consumer and release it locally. The
  // move leaves buf_ empty; reassigning a fresh vector frees the old capacity
  // so retained memory tracks only the NEW backlog accumulated during the next
  // read cycle.
  *out = std::move(buf_);
  buf_ = std::vector<float>();
  read_pos_ = total_;  // consumed everything currently appended
  return true;
}

void PipelineAudioCache::Reset() {
  std::lock_guard<std::mutex> lock(mutex_);
  buf_.clear();
  buf_.shrink_to_fit();
  read_pos_ = 0;
  total_ = 0;
  closed_ = false;
}

long PipelineAudioCache::total_samples() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return total_;
}

long PipelineAudioCache::read_position() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return read_pos_;
}

long PipelineAudioCache::pending_samples() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return total_ - read_pos_;
}

}  // namespace pipeline
}  // namespace orator
