#pragma once

#include <chrono>

namespace orator {
namespace pipeline {

class PeriodicDeadline {
 public:
  using Clock = std::chrono::steady_clock;
  using Duration = Clock::duration;
  using TimePoint = Clock::time_point;

  PeriodicDeadline(TimePoint started_at, Duration interval);

  TimePoint next() const { return next_; }
  void AdvancePast(TimePoint now);

 private:
  Duration interval_;
  TimePoint next_;
};

}  // namespace pipeline
}  // namespace orator
