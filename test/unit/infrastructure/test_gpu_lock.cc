// Unit tests for gpu/gpu_lock.h — DeviceGuard, DeviceLock,
// ConcurrentGpuEnabled/Active.
//
// These tests exercise the GPU access serialization logic. The mode is resolved
// once at first access via a static local; we set ORATOR_GPU_SERIAL=1 at the
// very start of main() so all tests run in kSerial mode, which is the only mode
// where DeviceGuard actually acquires the lock (kFull skips unconditionally).

#include <cstdio>
#include <cstdlib>
#include <thread>

#include "gpu/gpu_lock.h"

using orator::gpu::ConcurrentGpuActive;
using orator::gpu::ConcurrentGpuEnabled;
using orator::gpu::DeviceGuard;
using orator::gpu::DeviceLock;

static int g_fail = 0;
#define CHECK(cond, msg)              \
  do {                                \
    if (!(cond)) {                    \
      std::printf("FAIL: %s\n", msg); \
      ++g_fail;                       \
    }                                 \
  } while (0)

int main() {
  // Force serialized mode BEFORE any static-local mode resolution.
  ::setenv("ORATOR_GPU_SERIAL", "1", 1);

  std::printf("Testing gpu::DeviceGuard / DeviceLock / ConcurrentGpu...\n");

  // ------------------------------------------------------------------
  // 1. DeviceLock returns a usable mutex.
  // ------------------------------------------------------------------
  {
    std::printf("  Test 1: DeviceLock basic lock/unlock\n");
    auto& mtx = DeviceLock();
    mtx.lock();
    mtx.unlock();
    // Also verify mutual exclusion with a quick thread hand-off.
    bool entered = false;
    mtx.lock();
    std::thread t([&] {
      // Should block until main unlocks.
      mtx.lock();
      entered = true;
      mtx.unlock();
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    CHECK(!entered, "thread blocked while main holds DeviceLock");
    mtx.unlock();
    t.join();
    CHECK(entered, "thread acquired DeviceLock after main released");
    std::printf("  PASS\n");
  }

  // ------------------------------------------------------------------
  // 2. ConcurrentGpuEnabled() is false in serialized mode.
  // ------------------------------------------------------------------
  {
    std::printf(
        "  Test 2: ConcurrentGpuEnabled() == false under ORATOR_GPU_SERIAL\n");
    CHECK(!ConcurrentGpuEnabled(), "ConcurrentGpuEnabled false in serial mode");
    std::printf("  PASS\n");
  }

  // ------------------------------------------------------------------
  // 3. ConcurrentGpuActive() is false in serialized mode.
  // ------------------------------------------------------------------
  {
    std::printf(
        "  Test 3: ConcurrentGpuActive() == false under ORATOR_GPU_SERIAL\n");
    CHECK(!ConcurrentGpuActive(), "ConcurrentGpuActive false in serial mode");
    std::printf("  PASS\n");
  }

  // ------------------------------------------------------------------
  // 4. DeviceGuard(false) acquires the lock in serialized mode.
  //    Verify by checking that a concurrent thread cannot acquire
  //    DeviceLock while the guard is alive.
  // ------------------------------------------------------------------
  {
    std::printf("  Test 4: DeviceGuard(false) locks in serial mode\n");
    bool thread_entered = false;
    std::thread t;
    {
      DeviceGuard guard(false);
      // Spawn a thread that tries to lock DeviceLock.
      t = std::thread([&] {
        DeviceLock().lock();
        thread_entered = true;
        DeviceLock().unlock();
      });
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      CHECK(!thread_entered, "thread blocked while DeviceGuard holds lock");
    }
    // After guard destruction, the thread should acquire the lock.
    t.join();
    CHECK(thread_entered, "thread acquired lock after DeviceGuard destruction");
    std::printf("  PASS\n");
  }

  // ------------------------------------------------------------------
  // 5. DeviceGuard(true) also acquires the lock in serialized mode
  //    (own_stream is ignored when mode == kSerial).
  // ------------------------------------------------------------------
  {
    std::printf("  Test 5: DeviceGuard(true) locks in serial mode\n");
    bool thread_entered = false;
    std::thread t;
    {
      DeviceGuard guard(true);
      t = std::thread([&] {
        DeviceLock().lock();
        thread_entered = true;
        DeviceLock().unlock();
      });
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      CHECK(!thread_entered,
            "thread blocked while DeviceGuard(true) holds lock");
    }
    t.join();
    CHECK(thread_entered,
          "thread acquired lock after DeviceGuard(true) destruction");
    std::printf("  PASS\n");
  }

  // ------------------------------------------------------------------
  // Summary
  // ------------------------------------------------------------------
  if (g_fail == 0) {
    std::printf("gpu::DeviceGuard test PASSED\n");
    return 0;
  }
  std::printf("gpu::DeviceGuard test FAILED (%d checks)\n", g_fail);
  return 1;
}
