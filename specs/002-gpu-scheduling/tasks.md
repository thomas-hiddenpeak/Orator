# Tasks 002 — GPU Scheduling for Concurrent Pipelines

- **Feature**: `002-gpu-scheduling`
- **Spec**: [spec.md](spec.md) · **Plan**: [plan.md](plan.md)
- **Status**: COMPLETED (2026-06-17). All phases done: registry + telemetry
  (dbebf5f, 854dc7e), managed-memory de-coupling (2c34d20, b4606d9, 3abea74),
  ASR-concurrent production default (d1a754e), full GPU concurrency unlocked
  (T040: diarization + conformer layer stream routing, T041: VAD stream routing,
  T050: default mode kFull, T051: 20/20 ctest green). All three pipelines
  (ASR, diarization, VAD) now run on dedicated CUDA streams without the global
  mutex. Build clean, 20/20 tests pass.
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
- [x] **T030** Thread `asr_stream_` through the ASR call path; replace default
  stream arguments and `cudaDeviceSynchronize()` with
  `cudaStreamSynchronize(asr_stream_)`. *(Done 3abea74/d1a754e path; ASR output
  unchanged on all gates and real-WS runs.)*
- [x] **T031** Remove the host read of device-managed memory in the decode path
  (`AsrTextDecoder::Embed`): gather the embedding on the device, or copy after a
  stream synchronize. *(Superseded/expanded by T072 de-coupling; `Embed` host
  shadow + decoder staging done, gates green.)*

## Phase 4 — Diarization engine on its stream
- [x] **T040** Route diarization kernels and copies onto its registered stream;
  replace `cudaDeviceSynchronize()` with `cudaStreamSynchronize(diar_stream)`.
  *(Done 2026-06-17: `sortformer_decoder.cu` LaunchLinear passes stream to
  `gemm::LaunchSgemm`; `conformer_preencode.cu` 2 GEMM calls pass stream;
  `ConformerLayer::Forward` receives `cudaStream_t stream` and threads it through
  all 17 kernel launches (LN, LaunchLinear, AddScaled, Gather, RelShift, Softmax,
  Scatter, GLU, MaskRows, DepthwiseConv, BatchNormSilu, batched SGEMM);
  `streaming_sortformer.cc` passes stream to `layer->Forward()`. Build clean,
  `test_diar_stream` gate green.)*
- [x] **T041** Route the VAD pipeline (`GpuVad`) onto its registered background
  stream and replace any device-wide synchronize with a per-stream synchronize;
  apply the bounded-yield so it never starves a foreground pipeline. *(Verify:
  `test_vad` gate unchanged; VAD segments unchanged on the same audio.)*
  *(Done: `gpu_vad.cu` kernel launches + `cudaMemcpyAsync` use `stream_`,
  `cudaStreamSynchronize(stream_)`; no `DeviceGuard` retained.)*

## Phase 5 — Remove the global lock; verify concurrency
- [x] **T050** Remove `gpu::DeviceLock()` from the worker GPU regions. If any
  shared GPU resource remains, give each pipeline its own; document any residual
  synchronization and the hazard it prevents. *(Done: `gpu_lock.cc` default mode
  changed from `kAsrOnly` to `kFull` — all three pipelines (ASR, diarization, VAD)
  now run lock-free by default. `DeviceGuard` skips the lock in `kFull` mode
  regardless of `own_stream`.)*
- [x] **T051** Stability: build clean + 20/20 ctest pass. *(Verify: build + ctest
  green after all Phase 4-5 changes. Full concurrency verified stable (8/8 runs,
  zero crashes) in prior Phase 7 T073 validation.)*

## Phase 6 — Post-change measurement, telemetry, and reporting
- [x] **T060** Re-run Phase 1 measurements; compare with baseline; report the
  total wall-time change on the fixed input. *(Verify: AC5; no wall-time
  regression.)*
  - *(measured on Jetson Orin)*: 120 s 3-way = serial 39.9 s, ASR-only 31.4 s, full 31.3 s; full-hour default gate 4.92x end-to-end.
  - *(measured on Jetson Thor, 2026-06-17)*: 120 s full 3-pipeline (ASR+Diar+VAD) avg 12.7s (9.44x RT), 5/5 stability runs pass.
- [x] **T061** Confirm AC4 (no quality regression) and AC6 (clean build, tests
  pass, race-free); update `/memories/repo/` and `PROJECT_STATE.md`. *(Verify:
  AC4, AC6.)*
  - *(measured on Jetson Orin)*: CER 16.2% on full-hour default, no quality regression; 20/20 ctest green.
  - *(measured on Jetson Thor)*: 20/20 ctest green, 5/5 stability runs pass (ASR 5 + Diar 25 + VAD 51 per run).
- [x] **T062** Emit the additive `{"type":"gpu_telemetry"}` WS message
  **periodically** at `gpu_telemetry_interval_sec` (documented default 1.0 s; 0
  disables; env `ORATOR_GPU_TELEMETRY_SEC`), built from the registry + per-worker
  compute/occupancy and sent through the existing serialized transport via a
  dedicated low-rate timer thread (no GPU worker on its hot path, no separate
  socket). Verified on the real WS path: a 120 s run at a 2 s interval delivered
  4 snapshots, each listing every pipeline's class + priority index + concrete
  CUDA priority + compute/RTF, with all existing messages unchanged. *(Done
  854dc7e; AC8.)*

## Phase 7 — Managed-memory de-coupling (R1 precondition, retained in Spec 002)
> The empirical R1 test (spec §7a; commits efa184d → 3b01bbb) proved that
> ASR-on-its-own-stream + lock-free is NOT safe: both engines host-access
> `cudaMallocManaged` (`gpu::UnifiedBuffer`) during streaming, which SIGSEGVs on
> Tegra under concurrent kernels. This phase removes that hazard so the lock-free
> path can be safely re-enabled. It stays inside Spec 002 (the requirement arose
> from this feature; attribution is cleanest here). Sequenced BEFORE re-enabling
> concurrency (the `kOwnStreamConcurrencySafe` constant) and BEFORE T050. Each
> task keeps its engine's numeric gate green; the global lock stays the default
> throughout.
- [x] **T070** Inventory every streaming host-visible `UnifiedBuffer` in both
  engines (host read/write while kernels may run), separating it from load-time
  managed use (safe). Record the list with file:line. *(Verify: the list matches
  the survey in spec §7a / plan §3.6; no engine change yet.)*
- [x] **T071** Diarization: convert the per-frame sigmoid result
  (`sortformer_decoder.cu` `predp`/`preds`), the pre-encode output
  (`conformer_preencode.cu`), and the mel output (`mel_spectrogram.cu`) from
  managed to `gpu::DeviceAllocator` device buffers + `gpu::PinnedAllocator` host
  staging, read only after `cudaStreamSynchronize(0)`. One buffer at a time.
  *(Done where required; additionally verified diarizer streaming state is pure
  host `HostStreamState`, while `SortformerState` is retained-but-inactive.)*
- [x] **T072** ASR: audit the decode-loop scalar reads (token id / argmax) and
  any other streaming host read; ensure each reads pinned or
  stream-synchronized memory on `asr_stream_`, never a managed page (the
  embedding is already a plain-host shadow). Convert any remaining managed host
  read. *(Verify after EACH: `test_asr_encoder` / `test_asr_decoder` stay
  green.)* *(Done: encoder + decoder staging de-coupled; gates green.)*
- [x] **T073** Re-enable the lock-free own-stream path
  (`kOwnStreamConcurrencySafe = true`) and re-run the concurrent stream
  (`ORATOR_GPU_CONCURRENT=1`): confirm NO SIGSEGV over a 120 s real-WS run.
  *(Done: full-concurrency stable and correct; no SIGSEGV.)*

## Traceability (requirement → task)

| Requirement | Tasks |
|---|---|
| FR1 per-pipeline streams by priority index | T020, T021, T030, T040, T041 |
| FR2 no host access to in-flight managed memory | T031, T070, T071, T072, T073 |
| FR3 remove global mutex where streams suffice | T050 |
| FR4 correctness preserved | T030, T031, T040, T041, T051, T071, T072 |
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
the lock-free path runs without R1 faults; the global mutex is removed from
stream-safe paths; five+ fault-free streaming runs; diarization and ASR outputs
unchanged within tolerance (numeric gates green); full suite green under
`-Wall -Wextra`; total wall-time change reported with no regression; full-hour
real-WS gate passed on the production-default configuration; memory and
PROJECT_STATE updated.

---

## Phase 2 tasks (re-opened 2026-06-27) — see spec §10

### P2.1 — In-project bf16 GEMM (keystone; removes cuBLAS)
- [x] **T100** Extend `test_asr_gemm`: add an f64 CPU reference (NOT cuBLAS) and
  the full production shape set (M 1..few-thousand; K,N in {1024,2048,3072,5000,
  6144,7680,vocab}); record max relative error. Gate: baseline (current cuBLAS)
  passes the new reference within ~3e-3 — establishes the oracle before the swap.
- [x] **T101** Add a bf16 GEMM to `orator::gemm` (gemm.cuh): bf16 in / FP32
  accumulate, row-major `out[M,N]=in[M,K]@W[N,K]^T`, tiled (extend the existing
  double-buffered SGEMM; add an mma/tensor-core path). Allocation-free,
  stream-explicit, no global handle.
- [x] **T102** Fused epilogue: bias + activation (0 none / 1 GELU exact-erf /
  2 ReLU) in the GEMM, matching `BiasActKernel` numerics.
- [x] **T103** Switch `asr_gemm::LinearPre`/`Linear` M>1 path to the in-project
  GEMM; remove `BiasActKernel` post-pass and the `ORATOR_ASR_CUBLAS_GEMV` hatch.
  Validate: `test_asr_gemm` (vs f64 ref) + `test_asr_encoder`/`_decoder`/
  `test_aligner_*` oracle gates unchanged-pass.
- [x] **T104** Remove `cublas` from `orator_core` link and `<cublas_v2.h>` from
  `asr_gemm.cu`; delete the cuBLAS handle (`Handle()`, `g_handle`, `Shutdown`).
  Build clean `-Wall -Wextra`, zero warnings.
- [ ] **T105** Real-WS gate (rate=0, 120s): transcript diff vs the cuBLAS build
  (byte-identical or within tolerance); `[asr-stream]` encode/decode timing
  no-regression (stream_rt >= 3.6x); diar/VAD unaffected; server stable. Update
  `PROJECT_STATE.md` (Art. VIII).

### P2.2 — Device memory pool (prereq for graph)  [outline]
- [ ] **T110** Per-context pre-allocated device pool (acquire/release); migrate
  `AsrAudioTower::Forward`'s per-Forward DeviceBuffers to the pool. No cudaMalloc
  on the hot path. Oracle gates unchanged-pass.

### P2.3 — Event-based fine-grained scheduling  [outline, high-risk]
- [ ] **T120** Replace the binary `DeviceLock` mode with per-pipeline stream +
  CUDA-event ordering; isolate a capture stream from concurrent issuance.

### P2.4 — CUDA Graph for decode + encoder  [outline, depends on P2.1-3]
- [ ] **T130** Capture the allocation-free / cuBLAS-free decode body and the
  fixed-shape streaming encoder Forward; replay under concurrency. Measure
  decode/encode speedup; oracle + real-WS gates.
