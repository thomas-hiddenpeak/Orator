#include "gpu/scheduler.h"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace orator {
namespace gpu {

namespace {
void CheckCuda(cudaError_t e, const char* what) {
  if (e != cudaSuccess) {
    throw std::runtime_error(std::string("GpuScheduler: ") + what + ": " +
                             cudaGetErrorString(e));
  }
}
}  // namespace

GpuScheduler::GpuScheduler() {
  // Query the device's supported stream priority range once. On platforms with
  // a single supported value, greatest_ == least_ and priorities collapse to
  // stream concurrency only (reported, not assumed).
  CheckCuda(cudaDeviceGetStreamPriorityRange(&least_, &greatest_),
            "cudaDeviceGetStreamPriorityRange");
}

GpuScheduler::~GpuScheduler() {
  for (auto& e : entries_) {
    if (e.stream_active && e.stream != nullptr) {
      CheckCuda(cudaStreamDestroy(e.stream), "cudaStreamDestroy");
    }
  }
}

int GpuScheduler::PriorityForIndex(int priority_index) const {
  // priority_index 0 maps to the highest priority (greatest_, numerically
  // smallest). Each step toward a larger index moves one step toward the lowest
  // priority (least_), clamped to the supported range.
  if (priority_index < 0) priority_index = 0;
  const int p = greatest_ + priority_index;
  // greatest_ <= least_ is not guaranteed by sign, but CUDA defines greatest as
  // the numerically smallest; clamp into [greatest_, least_].
  const int lo = std::min(greatest_, least_);
  const int hi = std::max(greatest_, least_);
  return std::max(lo, std::min(p, hi));
}

cudaStream_t GpuScheduler::Register(const std::string& name, int priority_index,
                                    bool background, bool create_stream) {
  std::lock_guard<std::mutex> lk(mu_);
  for (const auto& e : entries_) {
    if (e.name == name) return e.stream;  // idempotent by name
  }
  Entry e;
  e.name = name;
  e.priority_index = priority_index;
  e.background = background;
  e.cuda_priority = PriorityForIndex(priority_index);
  if (create_stream) {
    cudaStream_t s = nullptr;
    CheckCuda(cudaStreamCreateWithPriority(&s, cudaStreamNonBlocking,
                                           e.cuda_priority),
              "cudaStreamCreateWithPriority");
    e.stream = s;
    e.stream_active = true;
  } else {
    e.stream = nullptr;  // default stream (0)
    e.stream_active = false;
  }
  entries_.push_back(e);
  return e.stream;
}

std::vector<GpuScheduler::Entry> GpuScheduler::Snapshot() const {
  std::lock_guard<std::mutex> lk(mu_);
  return entries_;
}

void GpuScheduler::NoteForegroundSubmit() {
  std::lock_guard<std::mutex> lk(mu_);
  last_fg_submit_ = std::chrono::steady_clock::now();
  have_fg_submit_ = true;
}

bool GpuScheduler::ShouldBackgroundYield(double window_sec) const {
  std::lock_guard<std::mutex> lk(mu_);
  if (!have_fg_submit_) return false;
  const double dt = std::chrono::duration<double>(
                        std::chrono::steady_clock::now() - last_fg_submit_)
                        .count();
  return dt < window_sec;
}

}  // namespace gpu
}  // namespace orator
