#include "pipeline/diarization_worker.h"

#include <chrono>
#include <mutex>

#include "gpu/gpu_lock.h"

namespace orator {
namespace pipeline {

namespace {
using Clock = std::chrono::steady_clock;
double Secs(Clock::time_point a, Clock::time_point b) {
  return std::chrono::duration<double>(b - a).count();
}
}  // namespace

DiarizationWorker::DiarizationWorker(model::SortformerDiarizer* diarizer,
                                     StreamTimeline* timeline)
    : diarizer_(diarizer), timeline_(timeline) {}

void DiarizationWorker::ProcessSpan(const float* samples, int n) {
  if (samples == nullptr || n <= 0) return;
  const auto t0 = Clock::now();
  core::DiarizationFrames part;
  {
    // Serialize GPU access against the ASR worker (one shared device).
    std::lock_guard<std::mutex> gpu(gpu::DeviceLock());
    part = diarizer_->StreamAudio(samples, n, false);
  }
  compute_sec_ += Secs(t0, Clock::now());
  timeline_->AppendDiarFrames(part);
  processed_samples_.fetch_add(n);
}

void DiarizationWorker::Finalize() {
  const auto t0 = Clock::now();
  core::DiarizationFrames tail;
  {
    std::lock_guard<std::mutex> gpu(gpu::DeviceLock());
    tail = diarizer_->StreamAudio(nullptr, 0, true);
  }
  compute_sec_ += Secs(t0, Clock::now());
  timeline_->AppendDiarFrames(tail);
}

void DiarizationWorker::Reset() {
  diarizer_->Reset();
  processed_samples_.store(0);
  compute_sec_ = 0.0;
}

}  // namespace pipeline
}  // namespace orator
