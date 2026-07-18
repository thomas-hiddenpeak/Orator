#include <chrono>
#include <cstdio>
#include <stdexcept>

#include "pipeline/periodic_deadline.h"

using orator::pipeline::PeriodicDeadline;

static int g_fail = 0;
#define CHECK(cond, msg)              \
  do {                                \
    if (!(cond)) {                    \
      std::printf("FAIL: %s\n", msg); \
      ++g_fail;                       \
    } else {                          \
      std::printf("PASS: %s\n", msg); \
    }                                 \
  } while (0)

int main() {
  using namespace std::chrono_literals;

  const PeriodicDeadline::TimePoint started_at{};
  PeriodicDeadline schedule(started_at, 100ms);
  CHECK(schedule.next() == started_at + 100ms,
        "first deadline is one interval after start");

  schedule.AdvancePast(started_at + 125ms);
  CHECK(schedule.next() == started_at + 200ms,
        "probe latency does not shift the original cadence");

  schedule.AdvancePast(started_at + 150ms);
  CHECK(schedule.next() == started_at + 200ms,
        "advance before the deadline preserves the pending slot");

  schedule.AdvancePast(started_at + 200ms);
  CHECK(schedule.next() == started_at + 300ms,
        "completion on a deadline advances to a strictly later slot");

  schedule.AdvancePast(started_at + 575ms);
  CHECK(schedule.next() == started_at + 600ms,
        "multi-period overrun skips expired slots without cadence drift");

  bool rejected_zero = false;
  try {
    PeriodicDeadline invalid(started_at, PeriodicDeadline::Duration::zero());
  } catch (const std::invalid_argument&) {
    rejected_zero = true;
  }
  CHECK(rejected_zero, "non-positive intervals are rejected");

  if (g_fail == 0) {
    std::printf("PeriodicDeadline test PASSED\n");
    return 0;
  }
  std::printf("PeriodicDeadline test FAILED (%d checks)\n", g_fail);
  return 1;
}
