# Tasks 002 — GPU Scheduling for Concurrent Pipelines

- **Feature**: `002-gpu-scheduling`
- **Spec**: [spec.md](spec.md) · **Plan**: [plan.md](plan.md)
- **Status**: Draft (awaiting review) — implementation begins after sign-off
- **Constitution**: v1.1.0

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
- [ ] **T020** In the controller, create `diar_stream_` and `asr_stream_` with
  priorities from `cudaDeviceGetStreamPriorityRange` (diarization higher). Report
  the queried priority range at startup. *(Verify: AC2; startup log shows the
  range and assignment.)*

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
- [ ] **T040** Route diarization kernels and copies onto `diar_stream_`; replace
  `cudaDeviceSynchronize()` with `cudaStreamSynchronize(diar_stream_)`. *(Verify:
  diarization verified against the NeMo reference within recorded tolerance.)*

## Phase 5 — Remove the global lock; verify concurrency
- [ ] **T050** Remove `gpu::DeviceLock()` from the worker GPU regions. If any
  shared GPU resource remains, give each pipeline its own; document any residual
  synchronization and the hazard it prevents. *(Verify: no shared mutable GPU
  state between pipelines.)*
- [ ] **T051** Stability: five consecutive 120 s streaming runs through the
  WebSocket with no segmentation fault; deterministic output. Run
  `compute-sanitizer --tool racecheck` where available. *(Verify: AC3.)*

## Phase 6 — Post-change measurement and reporting
- [ ] **T060** Re-run Phase 1 measurements; compare with baseline; report the
  total wall-time change on the fixed input. *(Verify: AC5; no wall-time
  regression.)*
- [ ] **T061** Confirm AC4 (no quality regression) and AC6 (clean build, tests
  pass, race-free); update `/memories/repo/` and `PROJECT_STATE.md`. *(Verify:
  AC4, AC6.)*

## Traceability (requirement → task)

| Requirement | Tasks |
|---|---|
| FR1 per-pipeline streams + priority | T020, T030, T040 |
| FR2 no host access to in-flight managed memory | T031 |
| FR3 remove global mutex where streams suffice | T050 |
| FR4 correctness preserved | T030, T031, T040, T051 |
| FR5 measurement | T010–T012, T060 |
| M1 GPU-busy fraction | T011 |
| M2 per-pipeline + total timing | T010, T060 |
| M3 realistic target | T012, T060 |

| Acceptance | Tasks |
|---|---|
| AC1 baseline recorded first | T010–T012 |
| AC2 streams with priority | T020 |
| AC3 no fault, deterministic | T051 |
| AC4 no quality regression | T030, T031, T040, T061 |
| AC5 post-change measured | T060 |
| AC6 clean build/tests/race-free | T061 |

## Definition of Done
M1–M3 recorded before and after; each pipeline on its own prioritized stream;
no host access to in-flight device-managed memory; the global mutex removed from
stream-safe paths; five fault-free streaming runs; diarization and ASR outputs
unchanged within tolerance; full suite green under `-Wall -Wextra`; total
wall-time change reported with no regression; memory and PROJECT_STATE updated.
