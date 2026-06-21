#pragma once

#include <chrono>

namespace orator {
namespace pipeline {
namespace worker {

using Clock = std::chrono::steady_clock;

inline double Secs(Clock::time_point a, Clock::time_point b) {
  return std::chrono::duration<double>(b - a).count();
}

}  // namespace worker
}  // namespace pipeline
}  // namespace orator
