# Tasks 002 — GPU Scheduling for Concurrent Pipelines

- **Feature**: `002-gpu-scheduling`
- **Spec**: [spec.md](spec.md) · **Plan**: [plan.md](plan.md)
- **Status**: In progress. Phase 2 (priority registry) + the periodic
  `gpu_telemetry` WS message are implemented and verified (dbebf5f, 854dc7e).
  The engine stream-routing + global-lock removal (T030/T031/T040/T041/T050) are
  BLOCKED pending a runnable diarization numeric gate (see the blocker note in
  Phase 2). The global lock is retained until then (no regression).
- **Constitution**: v1.2.1

> Ordered, independently verifiable steps. Measurement precedes any engine
> change. Do not begin Phase 2 until the owner approves the spec and plan and
> the baseline (Phase 1) is recorded.

---

## Phase 0 — Review gate
- [x] **T000** Owner approved proceeding ("请开始").

## Phase 1 — Baseline measurement (no engine change)
- [x] **T010** Per-pipeline + total timing on 120 s `test.mp3` at fixed clock,
  reproduced across two runs (both: 53.15 s / 53.37 s). Measurement harness:
  `/tmp/measure_config.sh`; the diarizer was made optional in `AuditoryStream`
  (empty weights path disables it) to allow ASR-only measurement.
- [x] **T011** `tegrastats` `GR3D_FREQ` GPU-busy fraction recorded for the three
  configurations: diarization only 78.8%, ASR only 72.8%, both ~63%.
- [x] **T012** Baseline (M1–M3) written to `/memories/repo/` and PROJECT_STATE.
  Realistic target: total wall 53 s toward the ASR-only floor ~38–40 s (~3.0×),
  about 25–28% reduction; the floor is ASR-only because ASR dominates.

## Phase 2 — Stream infrastructure
- [x] **T020** Add a priority registry in the `gpu/` layer
  (`include/gpu/scheduler.h`, `src/gpu/scheduler.cc`): a pipeline registers a
  name + a declared priority index (class) and receives a CUDA stream whose
  concrete priority is derived from the index via
  `cudaDeviceGetStreamPriorityRange`. Reports the queried range at startup. The
  ASR pipeline's stream is now sourced from the registry (replacing the ad-hoc
  creation). *(Done dbebf5f; AC2/AC7 at the registry level; build + 19/19 ctest
  green.)*
- [x] **T021** Register the three pipelines with their classes: diarization =
  foreground index 0, ASR = foreground index 1, VAD = background index 2. The
  registry is the single source of truth for the stream mapping and the
  telemetry. NOTE: only the ASR stream is currently CONSUMED (its engine already
  takes a stream); diar/vad register their CLASS but still execute on the
  default stream under the global lock (`create_stream=false`) until their engine
  kernels are stream-routed (T040/T041) — see the blocker below. *(Done dbebf5f.)*

> **PRECONDITION for T030/T031/T040/T041/T050 (engine stream-routing + lock**
> **removal) — now MET.** Removing the global `gpu::DeviceLock()` requires routing
> the diarization and ASR engines onto their own streams, replacing the
> device-wide `cudaDeviceSynchronize()` calls, and removing the host read of
> in-flight device-managed memory in the ASR decoder (R1). All three engines now
> have a runnable numeric gate: ASR `test_asr_encoder` / `test_asr_decoder`, VAD
> `test_vad`, and the new `test_diar_stream` (diarizer vs the NeMo streaming
> oracle, max_abs < 1e-2; commit d75da36). The remaining work is a large,
> multi-file refactor of two verified CUDA engines plus the lock removal and 5×
> stability runs; it is the next scheduled phase. Each step MUST keep its gate
> green; the global lock stays until T050 lands, so the current build is correct
> and serialized (no regression, no unvalidated change).

## Phase 3 — ASR engine on its stream, managed-memory safety
- [ ] **T030** Thread `asr_stream_` through the ASR call path; replace default
  stream arguments and `cudaDeviceSynchronize()` with
  `cudaStreamSynchronize(asr_stream_)`. *(Verify: ASR transcript unchanged vs
  baseline on the same audio.)*
- [ ] **T031** Remove the host read of device-managed memory in the decode path
  (`AsrTextDecoder::Embed`): gather the embedding on the device, or copy after a
  stream synchronize. *(Verify: decoder output matches the reference within
  tolerance; `test_asr_decoder` passes.)*

## Phase 4 — Diarization engine on its stream
- [ ] **T040** Route diarization kernels and copies onto its registered stream;
  replace `cudaDeviceSynchronize()` with `cudaStreamSynchronize(diar_stream)`.
  *(Verify: diarization verified against the NeMo reference within recorded
  tolerance.)*
- [ ] **T041** Route the VAD pipeline (`GpuVad`) onto its registered background
  stream and replace any device-wide synchronize with a per-stream synchronize;
  apply the bounded-yield so it never starves a foreground pipeline. *(Verify:
  `test_vad` gate unchanged; VAD segments unchanged on the same audio.)*

## Phase 5 — Remove the global lock; verify concurrency
- [ ] **T050** Remove `gpu::DeviceLock()` from the worker GPU regions. If any
  shared GPU resource remains, give each pipeline its own; document any residual
  synchronization and the hazard it prevents. *(Verify: no shared mutable GPU
  state between pipelines.)*
- [ ] **T051** Stability: five consecutive 120 s streaming runs through the
  WebSocket with no segmentation fault; deterministic output. Run
  `compute-sanitizer --tool racecheck` where available. *(Verify: AC3.)*

## Phase 6 — Post-change measurement, telemetry, and reporting
- [ ] **T060** Re-run Phase 1 measurements; compare with baseline; report the
  total wall-time change on the fixed input. *(Verify: AC5; no wall-time
  regression.)*
- [ ] **T061** Confirm AC4 (no quality regression) and AC6 (clean build, tests
  pass, race-free); update `/memories/repo/` and `PROJECT_STATE.md`. *(Verify:
  AC4, AC6.)*
- [x] **T062** Emit the additive `{"type":"gpu_telemetry"}` WS message
  **periodically** at `gpu_telemetry_interval_sec` (documented default 1.0 s; 0
  disables; env `ORATOR_GPU_TELEMETRY_SEC`), built from the registry + per-worker
  compute/occupancy and sent through the existing serialized transport via a
  dedicated low-rate timer thread (no GPU worker on its hot path, no separate
  socket). Verified on the real WS path: a 120 s run at a 2 s interval delivered
  4 snapshots, each listing every pipeline's class + priority index + concrete
  CUDA priority + compute/RTF, with all existing messages unchanged. *(Done
  854dc7e; AC8.)*

## Traceability (requirement → task)

| Requirement | Tasks |
|---|---|
| FR1 per-pipeline streams by priority index | T020, T021, T030, T040, T041 |
| FR2 no host access to in-flight managed memory | T031 |
| FR3 remove global mutex where streams suffice | T050 |
| FR4 correctness preserved | T030, T031, T040, T041, T051 |
| FR5 measurement | T010–T012, T060 |
| FR6 priority registry, three classes (VAD background) | T020, T021, T041 |
| FR7 telemetry over WebSocket | T062 |
| M1 GPU-busy fraction | T011 |
| M2 per-pipeline + total timing | T010, T060 |
| M3 realistic target | T012, T060 |

| Acceptance | Tasks |
|---|---|
| AC1 baseline recorded first | T010–T012 |
| AC2 streams with priority | T020, T021 |
| AC3 no fault, deterministic | T051 |
| AC4 no quality regression | T030, T031, T040, T041, T061 |
| AC5 post-change measured | T060 |
| AC6 clean build/tests/race-free | T061 |
| AC7 add-a-pipeline = registration only | T020, T021 |
| AC8 gpu_telemetry over WS | T062 |

## Definition of Done
M1–M3 recorded before and after; each pipeline on its own prioritized stream;
no host access to in-flight device-managed memory; the global mutex removed from
stream-safe paths; five fault-free streaming runs; diarization and ASR outputs
unchanged within tolerance; full suite green under `-Wall -Wextra`; total
wall-time change reported with no regression; memory and PROJECT_STATE updated.
