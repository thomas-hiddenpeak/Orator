#include "pipeline/periodic_deadline.h"

#include <stdexcept>

namespace orator {
namespace pipeline {

PeriodicDeadline::PeriodicDeadline(TimePoint started_at, Duration interval)
    : interval_(interval), next_(started_at + interval) {
  if (interval_ <= Duration::zero()) {
    throw std::invalid_argument("periodic interval must be positive");
  }
}

void PeriodicDeadline::AdvancePast(TimePoint now) {
  if (next_ > now) return;

  const auto completed_periods = (now - next_) / interval_;
  next_ += interval_ * (completed_periods + 1);
}

}  // namespace pipeline
}  // namespace orator
