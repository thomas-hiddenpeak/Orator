# Spec 002 — GPU Scheduling for Concurrent Pipelines

- **Feature**: `002-gpu-scheduling`
- **Status**: Implemented (2026-06-17) — all 17 tasks complete, build clean, 20/20 tests pass
- **Created**: 2026-06-12
- **Owner**: project owner
- **Constitution**: v1.2.1

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
  that the pipelines' kernels are not forced to run one-at-a-time when the GPU
  has idle capacity.
- **G2** Assign each pipeline a GPU stream priority from a **priority index it
  declares when it registers** (not a hard-coded pairwise rule), so the relative
  ordering generalizes to any number of pipelines. The diarization pipeline
  registers as latency-critical (foreground), ASR as throughput (foreground,
  lower than diarization), and the VAD pipeline as background.
- **G3** Eliminate the host access to device-managed memory that occurs while
  another thread may have a kernel executing, so that concurrent GPU use is
  correct without the global mutex.
- **G4** Preserve correctness: no segmentation fault, no data race, and no
  change to diarization or ASR output beyond floating-point run-to-run variation
  already present.
- **G5** Measure and report the change in per-pipeline real-time factors and in
  total wall time on the streaming path, before and after.
- **G6** Publish a GPU-scheduling **telemetry snapshot over the WebSocket**,
  pushed **periodically** at a bounded interval, so a client can drive a live
  scheduling dashboard. Each snapshot reports per-pipeline scheduling state
  (registered priority class, stream, and a compute/occupancy summary). The
  output contract is extended additively (a new message type), consistent with
  the existing `ready` / `timeline` / `revision` messages.

## 4. Non-Goals

- **NG1** Changing model numerics (quantization, precision reduction, kernel
  fusion that alters results). Deferred (Constitution II.3).
- **NG2** ASR endpointing or throughput-parameter changes (Spec 001 NG1).
- **NG3** Multi-GPU or multi-process execution. One GPU, one process.
- **NG4** Changing the output contract destructively. The timeline document is
  unchanged; the only protocol change is the ADDITIVE `gpu_telemetry` message
  (FR7). No existing message is altered or removed.

## 5. Functional Requirements

- **FR1 — Per-pipeline streams by declared priority index**: Each pipeline SHALL
  issue its GPU work on its own CUDA stream. Each pipeline SHALL declare a
  **priority index** (a small integer priority class) when it registers with the
  controller; the scheduler SHALL map that index onto the device's supported CUDA
  stream-priority range (`cudaDeviceGetStreamPriorityRange`). A lower-latency
  class maps to a higher CUDA priority. The mapping SHALL be the single point
  that converts a declared index to a concrete stream priority, so adding a
  pipeline is a registration change, not a scheduler edit (Constitution Art. V.4).
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
- **FR6 — Priority registry with three classes**: the controller SHALL keep a
  registry mapping each registered pipeline to its declared priority class.
  The three current pipelines SHALL register as: diarization = foreground
  (latency-critical, highest), ASR = foreground (lower than diarization), VAD =
  background (lowest). The registry SHALL be the single source of truth for the
  priority-to-stream mapping (FR1) and for the telemetry snapshot (FR7). A
  background-class pipeline SHALL yield within a bounded window so a
  foreground pipeline always makes progress.
- **FR7 — Periodic telemetry over WebSocket**: the server SHALL emit an additive
  `{"type":"gpu_telemetry",...}` message **periodically** at a bounded interval
  (a configurable `gpu_telemetry_interval_sec` with a documented default, e.g.
  1.0 s; a value of 0 disables it). Each message SHALL carry, per registered
  pipeline, its declared priority class, its concrete stream priority, and a
  compute/occupancy summary (for example `compute_sec` and real-time factor
  already tracked per worker). The message SHALL carry the common time base like
  the other messages and SHALL be additive (existing messages unchanged,
  Constitution Art. III.4). Emission SHALL go through the same serialized
  transport send as the other messages (no separate socket, no unsynchronized
  write) and SHALL NOT block a pipeline worker (a dedicated low-rate timer or the
  controller emits it, never a GPU worker thread on its hot path).

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
- **AC2** Each pipeline issues GPU work on its own stream; each stream's concrete
  priority is derived from the pipeline's declared priority index, with
  diarization (foreground) above ASR (foreground) above VAD (background),
  verified by inspection and by the priority values reported at startup. (FR1,
  FR6)
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
- **AC7** Adding a hypothetical fourth pipeline requires only a registration with
  a declared priority index (no edit to the priority-to-stream mapping or the
  scheduler), demonstrated by inspection of the registry call site. (FR1, FR6)
- **AC8** Streaming `test.mp3` through the real WebSocket emits the additive
  `{"type":"gpu_telemetry"}` message **repeatedly at the configured interval**
  (more than one snapshot over a multi-second run), each listing every registered
  pipeline with its priority class, concrete stream priority, and
  compute/occupancy summary; setting the interval to 0 suppresses it; a client
  can read it without any change to the existing messages. (FR7)

## 7a. Empirical R1 finding (2026-06-17) — resolved for deployment modes

The R1 risk (a host access to in-flight device-managed memory faulting when the
two pipelines' GPU work overlaps) was tested directly and **confirmed**:

- **Method**: the env-gated concurrent path (`ORATOR_GPU_CONCURRENT=1`,
  `gpu::DeviceGuard`) made ASR lock-free on its own non-default CUDA stream while
  diarization ran concurrently on the default stream; diar's hot-path device
  synchronizations were scoped to the default stream. A 120 s real-WS stream was
  run.
- **Result**: an **immediate `SIGSEGV` (core dumped)** on the first stream — the
  server crashed before completing one run. The serialized default
  (`ORATOR_GPU_CONCURRENT` unset) completed the same input in 40.6 s with correct
  output (25 diarization + 5 ASR + 45 comprehensive entries).
- **Root cause**: this Orin reports `concurrentManagedAccess == 0` (confirmed by
  `cudaDeviceGetAttribute`). On such a device the host may not access ANY
  `cudaMallocManaged` page while a kernel runs on ANY stream — it faults via page
  migration. Both verified engines host-access managed memory pervasively during
  streaming (the diarizer's whole `SortformerState` — spkcache/fifo/spk_perm — and
  its streaming logic; the ASR decoder's `PrefillAt`/`Forward` residual and
  position staging). A `gdb` backtrace pinned the first crash to the ASR thread's
  `PrefillAt` host `memcpy` into a managed buffer while a diarization kernel ran.
  Fixing the single named `Embed` case (already a plain-host shadow) was necessary
  but far from sufficient.
- **The pooling / stream-attach option (evaluated)**: the documented mechanism for
  multi-stream + managed memory on such a device is
  `cudaStreamAttachMemAsync(stream, ptr, cudaMemAttachSingle)`, which makes a
  managed allocation coherent with ONE stream so the host may touch it after
  synchronizing only that stream, regardless of other streams' kernels (logical
  per-stream ownership; the pooling idea). An empirical test on this device
  confirmed: single-attach to a NON-default stream succeeds, but single-attach to
  the DEFAULT stream (0) returns `invalid argument`. The diarization and VAD
  engines run on the default stream and take no stream parameter, so the attach
  approach still requires routing the entire diarizer onto a dedicated non-default
  stream first — the same large change to the verified engine as de-coupling its
  managed buffers. The attach mechanism does not avoid touching the verified
  engine; it relocates the work.
- **Impact / decision (validated by practice)**: the issue was resolved in two
  steps and both were required. (1) ASR-side managed host-touch sites were
  de-coupled to device + pinned staging (`PrefillAt`/`Forward` decoder staging,
  encoder reshape path; commits 2c34d20, b4606d9). (2) CUDA Graph capture is
  auto-disabled whenever any lock-free concurrency mode is active, because a
  concurrently-issuing pipeline corrupts capture and aborts with
  "operation failed ... during capture" (commit 3abea74). With both fixes,
  lock-free concurrency is stable and correct:
  - ASR-only lock-free (`ORATOR_GPU_CONCURRENT_ASR=1`): 8+ runs, no faults,
    deterministic output, ~21% wall reduction on 120 s.
  - Full lock-free override (`ORATOR_GPU_CONCURRENT=1`): 8+ runs, no faults,
    deterministic output.
  The production default is now ASR-only lock-free (commit d1a754e), with
  `ORATOR_GPU_SERIAL=1` as explicit opt-out and `ORATOR_GPU_CONCURRENT=1` as full
  override. A full-hour real-WS gate on the default configuration passed
  (C1/C2/C3 PASS, CER 16.2%, 4.92x end-to-end — measured on Jetson Orin). The measured gain comes from
  ASR no longer blocking diarization; making diar/VAD lock-free too adds ~0 wall
  benefit because they share the default stream and serialize on stream 0.

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
