# Spec 002 — GPU Scheduling for Concurrent Pipelines

- **Feature**: `002-gpu-scheduling`
- **Status**: Draft (awaiting review)
- **Created**: 2026-06-12
- **Owner**: project owner
- **Constitution**: v1.1.0

> This spec describes WHAT to change and WHY. The design (CUDA streams, stream
> priorities, removal of the global lock, memory-access rules) is in `plan.md`.

---

## 1. Summary

The two pipelines (diarization, ASR) currently share the GPU through a single
process-wide mutex (`gpu::DeviceLock()`) that makes every GPU-using region
mutually exclusive. This serializes all device work: the two pipelines cannot
run kernels at the same time, there is no priority between them, and the total
processing time is approximately the sum of the two pipelines' GPU compute
times. This feature replaces the single global mutex with per-pipeline CUDA
streams and stream priorities, removes the mutex from paths that streams already
order correctly, and eliminates the unsafe host access to device-managed memory
that the mutex currently prevents. The goal is to let one pipeline's kernels use
the GPU while the other pipeline is not using it, and to give the latency-
critical pipeline (diarization) priority over the throughput pipeline (ASR).

## 2. Background and Problem

### 2.1 Current state (measured)
- Spec 001 runs diarization and ASR on independent threads. Both call CUDA on
  one physical GPU.
- A single global mutex serializes all GPU regions because of a specific
  failure: on the Jetson unified-memory platform, a host read of device-managed
  memory while another thread has a kernel executing causes a segmentation
  fault. The mutex was added to prevent that fault (commit history; Spec 001
  §4).
- Measured on the streaming path (120 s of `test.mp3`, GPU fixed at 1.3 GHz,
  power mode MaxN): diarization compute 12.5 s (9.5x real-time), ASR compute
  46.4 s (2.6x), total wall time 53 s (2.26x). The total is approximately the
  sum of the two compute times, which is the signature of fully serialized GPU
  use.

### 2.2 Why there is room to improve
- Earlier profiling of the ASR decode step showed the GPU is idle for a large
  fraction of the time (the per-token step is limited by kernel-launch and host
  overhead, not by sustained arithmetic). Idle intervals in one pipeline can be
  filled by the other pipeline's kernels.
- The two pipelines have different timing requirements: diarization must keep up
  with the input stream (latency-critical), while ASR may lag and catch up
  (throughput-oriented). A priority relationship between them is meaningful.

### 2.3 The constraint to respect
- There is one physical GPU. If both pipelines were each saturating the GPU's
  arithmetic units, concurrent streams could not increase total throughput. The
  opportunity exists only to the extent that the GPU is currently idle during
  one pipeline's execution. The achievable improvement is therefore measured,
  not assumed (see §6, Measurement).

## 3. Goals

- **G1** Replace the single global GPU mutex with per-pipeline CUDA streams so
  that the two pipelines' kernels are not forced to run one-at-a-time when the
  GPU has idle capacity.
- **G2** Give the diarization pipeline higher GPU stream priority than ASR, so
  diarization latency is protected when both pipelines have work.
- **G3** Eliminate the host access to device-managed memory that occurs while
  another thread may have a kernel executing, so that concurrent GPU use is
  correct without the global mutex.
- **G4** Preserve correctness: no segmentation fault, no data race, and no
  change to diarization or ASR output beyond floating-point run-to-run variation
  already present.
- **G5** Measure and report the change in per-pipeline real-time factors and in
  total wall time on the streaming path, before and after.

## 4. Non-Goals

- **NG1** Changing model numerics (quantization, precision reduction, kernel
  fusion that alters results). Deferred (Constitution II.3).
- **NG2** ASR endpointing or throughput-parameter changes (Spec 001 NG1).
- **NG3** Multi-GPU or multi-process execution. One GPU, one process.
- **NG4** Changing the output contract (the timeline document is unchanged).

## 5. Functional Requirements

- **FR1 — Per-pipeline streams**: Each pipeline SHALL issue its GPU work on its
  own CUDA stream. The diarization stream SHALL have higher priority than the
  ASR stream (using the device's supported priority range).
- **FR2 — No host access to in-flight managed memory**: A pipeline SHALL NOT
  read device-managed memory on the host while that memory may be written by a
  kernel that has not completed. Any host read SHALL follow a synchronization of
  the stream that produced the data.
- **FR3 — Remove the global GPU mutex where streams suffice**: The single global
  mutex SHALL be removed from execution paths whose ordering and safety are
  guaranteed by stream semantics and §FR2. Any remaining synchronization SHALL
  be documented with the specific hazard it prevents.
- **FR4 — Correctness preserved**: With the change in place, streaming `test.mp3`
  through the WebSocket SHALL complete without fault, and the diarization and ASR
  outputs SHALL match the pre-change outputs within the tolerance recorded for
  each engine.
- **FR5 — Measurement**: A baseline measurement SHALL be recorded before the
  change and the same measurement after, both on the streaming path under stated
  clock conditions: per-pipeline compute time and real-time factor, GPU-busy
  fraction per pipeline, and total wall time.

## 6. Measurement (required before and after)

The first task of this feature is a baseline measurement; no engine code is
changed until the baseline exists.

- **M1 — GPU-busy fraction**: Using `tegrastats` (the `GR3D_FREQ` field) during
  a streaming run, record the fraction of samples for which the GPU load is
  greater than zero, for: (a) diarization only, (b) ASR only, (c) both pipelines
  running. This quantifies the idle capacity available to overlap.
- **M2 — Per-pipeline and total timing**: Record `diar_compute_sec`,
  `asr_compute_sec`, and total wall time on a fixed input (120 s of `test.mp3`)
  at the fixed clock (1.3 GHz, MaxN).
- **M3 — Realistic target**: From M1 and M2, state the maximum achievable
  reduction in total wall time (it cannot be less than the larger single-pipeline
  compute time) and set the target for this feature accordingly. No numeric
  target is asserted before M1/M2 are measured.

## 7. Acceptance Criteria

- **AC1** Baseline M1–M3 are recorded and written to `/memories/repo/` and to a
  measurement note before any engine change. (M1–M3)
- **AC2** Each pipeline issues GPU work on its own stream; diarization uses a
  higher-priority stream than ASR (verified by inspection and by the stream
  priority values reported at startup). (FR1)
- **AC3** Streaming `test.mp3` through the WebSocket completes without
  segmentation fault across at least five consecutive runs, and the output is
  deterministic within recorded tolerance. (FR2, FR4)
- **AC4** The diarization and ASR transcripts match the pre-change outputs for
  the same audio within recorded tolerance (no quality regression). (FR4,
  Constitution II)
- **AC5** Post-change M1–M3 are recorded and compared with the baseline; the
  total wall time on the fixed input is reported. A regression in total wall
  time is a failure. (FR5, M3)
- **AC6** Build produces no new warnings under `-Wall -Wextra`; the full test
  suite passes; no data race is present in the threaded path (verified). (
  Constitution V)

## 8. Constitution Check

- **Art. I (no dependencies)**: uses CUDA stream APIs already in the toolkit and
  `std::thread`; no new dependency.
- **Art. II (accuracy)**: AC4 requires no quality regression; numerics are NG1.
- **Art. III (independent pipelines)**: the pipelines remain independent; this
  feature changes only how they share the GPU.
- **Art. IV (streaming validation)**: AC3/AC5 measure on the real streaming path.
- **Art. V (quality)**: AC6 requires a clean build, passing tests, and a
  race-free path; the concurrency design is in `plan.md`.
- **Art. VI (terminology)**: standard terms; the term "stream" means a CUDA
  stream throughout.
- **Art. VII (SDD)**: spec now; plan and tasks follow; measurement gates the
  implementation.

## 9. Open Questions and Risks

- **R1** The global mutex prevents a real segmentation fault, not a hypothetical
  one. Removing it requires first removing every host access to in-flight
  device-managed memory (FR2). This touches the verified ASR decoder's memory
  model and is the primary risk to accuracy and stability.
- **R2** The achievable improvement depends on the measured GPU-idle fraction
  (M1). If both pipelines are closer to GPU-saturated than expected, the gain is
  small; the feature still removes the priority inversion and the unsafe memory
  access, but the wall-time reduction may be modest. This is reported honestly.
- **R3** Stream priority on this platform has a limited range; the actual range
  is queried at runtime and reported, rather than assumed.
- **Q1 (resolved)** Direction approved by the owner: pursue per-pipeline streams
  and priority instead of the single global mutex.
