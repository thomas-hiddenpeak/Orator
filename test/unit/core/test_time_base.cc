#include <cassert>
#include <cmath>
#include <cstdio>
#include <initializer_list>

#include "core/time_base.h"

using orator::core::TimeBase;

static int g_fail = 0;
#define CHECK(cond, msg)              \
  do {                                \
    if (!(cond)) {                    \
      std::printf("FAIL: %s\n", msg); \
      ++g_fail;                       \
    }                                 \
  } while (0)

static bool Near(double a, double b) { return std::fabs(a - b) < 1e-9; }

int main() {
  std::printf("Testing core::TimeBase (Spec 005 T011)...\n");

  // Default-constructed is invalid.
  {
    TimeBase t;
    CHECK(!t.valid(), "default base is invalid");
    CHECK(Near(t.SecondsAt(16000), 0.0), "invalid base yields 0 seconds");
  }

  // Basic conversion at origin 0.
  {
    TimeBase t(16000);
    CHECK(t.valid(), "rate>0 is valid");
    CHECK(Near(t.SecondsAt(0), 0.0), "sample 0 -> 0s");
    CHECK(Near(t.SecondsAt(16000), 1.0), "16000 -> 1.0s");
    CHECK(Near(t.SecondsAt(8000), 0.5), "8000 -> 0.5s");
    CHECK(Near(t.Duration(48000), 3.0), "duration 48000 -> 3.0s");
    CHECK(t.SampleAt(1.0) == 16000, "1.0s -> 16000");
    CHECK(t.SampleAt(0.5) == 8000, "0.5s -> 8000");
    // Round trip.
    for (long s : {0L, 1L, 15999L, 16000L, 1234567L})
      CHECK(t.SampleAt(t.SecondsAt(s)) == s, "round trip sample->sec->sample");
  }

  // Non-zero origin: t=0 shifts to origin_sample.
  {
    TimeBase t(16000, /*origin=*/16000);
    CHECK(Near(t.SecondsAt(16000), 0.0), "origin sample -> 0s");
    CHECK(Near(t.SecondsAt(32000), 1.0), "origin+16000 -> 1.0s");
  }

  // Derive + LocalSeconds: a sub-stream that begins at an absolute anchor maps
  // its local sample index (from 0) onto the common clock.
  {
    TimeBase base(16000);
    const long anchor = 48000;  // sub-stream data begins at 3.0s
    TimeBase child = base.Derive(anchor);
    CHECK(child.anchor_sample() == anchor, "derive records anchor");
    CHECK(Near(child.LocalSeconds(0), 3.0), "local 0 -> 3.0s (anchor)");
    CHECK(Near(child.LocalSeconds(16000), 4.0), "local 16000 -> 4.0s");
    // Identity: LocalSeconds(i) == SecondsAt(anchor + i).
    for (long i : {0L, 1L, 16000L, 99999L})
      CHECK(Near(child.LocalSeconds(i), base.SecondsAt(anchor + i)),
            "LocalSeconds(i) == SecondsAt(anchor+i)");
  }

  // Reconciliation: signed gap of processed extent vs the common total.
  {
    CHECK(TimeBase::ReconcileExtent(16000, 16000) == 0, "aligned -> 0 gap");
    CHECK(TimeBase::ReconcileExtent(16000, 15000) == 1000, "ahead -> +gap");
    CHECK(TimeBase::ReconcileExtent(15000, 16000) == -1000, "behind -> -gap");
  }

  if (g_fail == 0) {
    std::printf("core::TimeBase test PASSED\n");
    return 0;
  }
  std::printf("core::TimeBase test FAILED (%d checks)\n", g_fail);
  return 1;
}
