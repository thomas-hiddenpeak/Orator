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
- [ ] **T000** Owner reviews and approves `spec.md` and `plan.md`.

## Phase 1 — Baseline measurement (no engine change)
- [ ] **T010** Add a measurement mode to the streaming test that records, on a
  fixed 120 s input at the fixed clock (1.3 GHz, MaxN): `diar_compute_sec`,
  `asr_compute_sec`, total wall time, and per-pipeline real-time factors.
  *(Verify: numbers reproduced across two runs.)*
- [ ] **T011** Record the `tegrastats` `GR3D_FREQ` GPU-busy fraction for three
  configurations: diarization only, ASR only, both pipelines. *(Verify: M1 table
  recorded.)*
- [ ] **T012** Write the baseline (M1–M3) to `/memories/repo/` and a measurement
  note; state the maximum achievable wall-time reduction and the target.
  *(Verify: AC1.)*

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
