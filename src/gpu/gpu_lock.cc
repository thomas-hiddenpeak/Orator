#include "gpu/gpu_lock.h"

#include <cstdlib>

namespace orator {
namespace gpu {

std::mutex& DeviceLock() {
  static std::mutex lock;
  return lock;
}

bool ConcurrentGpuEnabled() {
  // Read the env once. ORATOR_GPU_CONCURRENT in {1,t,T,y,Y} enables the
  // concurrent path; anything else (including unset) keeps serialized mode.
  static const bool enabled = [] {
    const char* v = std::getenv("ORATOR_GPU_CONCURRENT");
    return v != nullptr &&
           (v[0] == '1' || v[0] == 't' || v[0] == 'T' || v[0] == 'y' ||
            v[0] == 'Y');
  }();
  return enabled;
}

bool ConcurrentGpuActive() {
  // True if any lock-free concurrency mode is on: the full path (currently still
  // gated off at the DeviceGuard) OR the ASR-only experiment knob. Used by the
  // ASR decoder to disable CUDA Graph capture, which a concurrently-issuing
  // pipeline corrupts.
  static const bool active = [] {
    const char* a = std::getenv("ORATOR_GPU_CONCURRENT_ASR");
    const bool asr_only = a != nullptr && (a[0] == '1' || a[0] == 't' ||
                                           a[0] == 'T' || a[0] == 'y' ||
                                           a[0] == 'Y');
    return asr_only || ConcurrentGpuEnabled();
  }();
  return active;
}

DeviceGuard::DeviceGuard(bool own_stream) : locked_(false) {
  // EMPIRICAL R1 RESULT (Spec 002, 2026-06-17): the lock-free own-stream path is
  // gated OFF BY DEFAULT. This Orin reports concurrentManagedAccess == 0
  // (verified by device query), so the host may not touch ANY cudaMallocManaged
  // (`UnifiedBuffer`) memory while a kernel runs on any stream — it faults via
  // page migration. The ASR-side host-touch sites WERE de-coupled (device +
  // pinned staging); the diarizer's managed host access (the whole streaming
  // SortformerState) is large and lives in the NeMo-verified engine and is NOT
  // yet de-coupled.
  //
  // CONTROLLED EXPERIMENT KNOB (ORATOR_GPU_CONCURRENT_ASR): when set, ONLY the
  // ASR own-stream pipeline skips the lock, while diarization and VAD continue
  // to hold it (they still serialize against each other on the default stream).
  // This isolates whether the completed ASR-side de-coupling is sufficient for
  // ASR to run concurrently with the (locked) diar/VAD work. It is an experiment
  // gate, not a production default; ORATOR_GPU_CONCURRENT (full) stays OFF until
  // the diarizer is also de-coupled.
  static const bool asr_only_concurrent = [] {
    const char* v = std::getenv("ORATOR_GPU_CONCURRENT_ASR");
    return v != nullptr && (v[0] == '1' || v[0] == 't' || v[0] == 'T' ||
                            v[0] == 'y' || v[0] == 'Y');
  }();
  // FULL concurrency experiment (ORATOR_GPU_CONCURRENT): ALL pipelines skip the
  // lock — ASR on its own stream, diarization + VAD sharing the default stream
  // (their kernels still serialize on stream 0, but the host-side lock is
  // dropped). This is gated to a controlled experiment because it requires every
  // pipeline's host-touched memory to be R1-safe (device/pinned): GpuVad is
  // already all-device (cudaMalloc); the diarizer's streaming compute uses pure
  // host vectors (HostStreamState) and its result copies were converted to
  // device; the managed SortformerState is retained-but-inactive (never read on
  // the streaming path). Validated empirically before any production use.
  constexpr bool kOwnStreamConcurrencySafe = false;
  const bool full_skip = own_stream && kOwnStreamConcurrencySafe &&
                         ConcurrentGpuEnabled();
  const bool full_all_skip = ConcurrentGpuEnabled();  // full mode: skip for all
  const bool asr_skip = own_stream && asr_only_concurrent;
  const bool skip = full_skip || full_all_skip || asr_skip;
  if (!skip) {
    DeviceLock().lock();
    locked_ = true;
  }
}

DeviceGuard::~DeviceGuard() {
  if (locked_) DeviceLock().unlock();
}

}  // namespace gpu
}  // namespace orator
