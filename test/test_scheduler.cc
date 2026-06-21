// Unit tests for gpu/scheduler.h — GpuScheduler.
//
// GpuScheduler constructor calls cudaDeviceGetStreamPriorityRange, so a
// CUDA-capable GPU is required. We check cudaGetDeviceCount at the top and
// skip gracefully when no GPU is available.

#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#include <cuda_runtime.h>

#include "gpu/scheduler.h"

using orator::gpu::GpuScheduler;

static int g_fail = 0;
#define CHECK(cond, msg)              \
  do {                                \
    if (!(cond)) {                    \
      std::printf("FAIL: %s\n", msg); \
      ++g_fail;                       \
    }                                 \
  } while (0)

int main() {
  // Check GPU availability.
  int device_count = 0;
  cudaError_t ce = cudaGetDeviceCount(&device_count);
  if (ce != cudaSuccess || device_count < 1) {
    std::printf("SKIP: no CUDA-capable GPU found (cudaGetDeviceCount=%d, count=%d)\n",
                static_cast<int>(ce), device_count);
    return 0;
  }
  std::printf("Testing gpu::GpuScheduler...\n");

  // ------------------------------------------------------------------
  // 1. Constructor + PriorityRange query.
  // ------------------------------------------------------------------
  {
    std::printf("  Test 1: Constructor and PriorityRange\n");
    GpuScheduler sched;
    int greatest = 0, least = 0;
    sched.PriorityRange(&greatest, &least);
    // greatest <= least is the CUDA convention (greatest = numerically smallest
    // = highest priority). On some platforms they may be equal.
    CHECK(greatest <= least,
          "PriorityRange: greatest <= least (CUDA convention)");
    std::printf("    priority range: [%d, %d]\n", greatest, least);
    std::printf("  PASS\n");
  }

  // ------------------------------------------------------------------
  // 2. Register a pipeline with create_stream=true → non-null stream.
  // ------------------------------------------------------------------
  {
    std::printf("  Test 2: Register with create_stream=true\n");
    GpuScheduler sched;
    cudaStream_t s = sched.Register("diarization", 0, /*background=*/false,
                                    /*create_stream=*/true);
    CHECK(s != nullptr, "Register(priority=0, create=true) returns non-null stream");
    std::printf("  PASS\n");
  }

  // ------------------------------------------------------------------
  // 3. Register with create_stream=false → nullptr (default stream).
  // ------------------------------------------------------------------
  {
    std::printf("  Test 3: Register with create_stream=false\n");
    GpuScheduler sched;
    cudaStream_t s = sched.Register("vad", 2, /*background=*/true,
                                    /*create_stream=*/false);
    CHECK(s == nullptr,
          "Register(create_stream=false) returns nullptr (default stream)");
    std::printf("  PASS\n");
  }

  // ------------------------------------------------------------------
  // 4. Re-registering the same name returns the same stream (idempotent).
  // ------------------------------------------------------------------
  {
    std::printf("  Test 4: Re-register same name is idempotent\n");
    GpuScheduler sched;
    cudaStream_t s1 = sched.Register("asr", 1, /*background=*/false,
                                     /*create_stream=*/true);
    cudaStream_t s2 = sched.Register("asr", 1, /*background=*/false,
                                     /*create_stream=*/true);
    CHECK(s1 == s2, "re-register returns same stream pointer");
    std::printf("  PASS\n");
  }

  // ------------------------------------------------------------------
  // 5. PriorityForIndex: index 0 maps to greatest (highest priority),
  //    large index clamps to least (lowest priority).
  // ------------------------------------------------------------------
  {
    std::printf("  Test 5: PriorityForIndex mapping\n");
    GpuScheduler sched;
    int greatest = 0, least = 0;
    sched.PriorityRange(&greatest, &least);

    // Index 0 should map to greatest.
    int p0 = sched.PriorityForIndex(0);
    CHECK(p0 == greatest, "PriorityForIndex(0) == greatest");

    // A very large index should clamp to least.
    int p_big = sched.PriorityForIndex(999);
    CHECK(p_big == least, "PriorityForIndex(999) == least (clamped)");

    // Negative index is clamped to 0 → greatest.
    int p_neg = sched.PriorityForIndex(-5);
    CHECK(p_neg == greatest, "PriorityForIndex(-5) == greatest (clamped to 0)");

    std::printf("    greatest=%d, least=%d, p0=%d, p999=%d, p-5=%d\n",
                greatest, least, p0, p_big, p_neg);
    std::printf("  PASS\n");
  }

  // ------------------------------------------------------------------
  // 6. Snapshot returns all registered entries with correct metadata.
  // ------------------------------------------------------------------
  {
    std::printf("  Test 6: Snapshot\n");
    GpuScheduler sched;
    sched.Register("pipe_a", 0, false, true);
    sched.Register("pipe_b", 1, false, false);
    sched.Register("pipe_c", 2, true, true);

    auto snap = sched.Snapshot();
    CHECK(snap.size() == 3, "Snapshot has 3 entries");

    // Verify each entry's fields.
    for (const auto& e : snap) {
      if (e.name == "pipe_a") {
        CHECK(e.priority_index == 0, "pipe_a priority_index == 0");
        CHECK(!e.background, "pipe_a not background");
        CHECK(e.stream_active, "pipe_a stream_active == true");
        CHECK(e.stream != nullptr, "pipe_a stream != nullptr");
      } else if (e.name == "pipe_b") {
        CHECK(e.priority_index == 1, "pipe_b priority_index == 1");
        CHECK(!e.background, "pipe_b not background");
        CHECK(!e.stream_active, "pipe_b stream_active == false");
        CHECK(e.stream == nullptr, "pipe_b stream == nullptr");
      } else if (e.name == "pipe_c") {
        CHECK(e.priority_index == 2, "pipe_c priority_index == 2");
        CHECK(e.background, "pipe_c is background");
        CHECK(e.stream_active, "pipe_c stream_active == true");
        CHECK(e.stream != nullptr, "pipe_c stream != nullptr");
      } else {
        std::printf("FAIL: unexpected entry name '%s'\n", e.name.c_str());
        ++g_fail;
      }
    }
    std::printf("  PASS\n");
  }

  // ------------------------------------------------------------------
  // 7. NoteForegroundSubmit / ShouldBackgroundYield.
  // ------------------------------------------------------------------
  {
    std::printf("  Test 7: Foreground submit and background yield\n");
    GpuScheduler sched;

    // Before any submit, should not yield.
    CHECK(!sched.ShouldBackgroundYield(0.05),
          "ShouldBackgroundYield false before any submit");

    // After submit, should yield within the window.
    sched.NoteForegroundSubmit();
    CHECK(sched.ShouldBackgroundYield(0.05),
          "ShouldBackgroundYield true after foreground submit");

    // After the window expires, should not yield.
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    CHECK(!sched.ShouldBackgroundYield(0.05),
          "ShouldBackgroundYield false after window expires");

    std::printf("  PASS\n");
  }

  // ------------------------------------------------------------------
  // Summary
  // ------------------------------------------------------------------
  if (g_fail == 0) {
    std::printf("gpu::GpuScheduler test PASSED\n");
    return 0;
  }
  std::printf("gpu::GpuScheduler test FAILED (%d checks)\n", g_fail);
  return 1;
}
