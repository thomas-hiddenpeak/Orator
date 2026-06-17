# Plan 002 — GPU Scheduling for Concurrent Pipelines

- **Feature**: `002-gpu-scheduling`
- **Spec**: [spec.md](spec.md)
- **Status**: Implemented (2026-06-17) — all phases complete
- **Constitution**: v1.2.1

> This plan describes HOW to satisfy [spec.md](spec.md). Terminology follows
> Constitution Article VI. "Stream" means a CUDA stream throughout.

---

## 1. Current GPU usage (measured from the code)

A survey of the GPU-using code establishes the starting point and the scope:

- **Diarization engine** (`conformer_layer.cu`, `conformer_preencode.cu`,
  `sortformer_decoder.cu`): launches kernels on the **default stream** and uses
  `cudaDeviceSynchronize()` and default-stream `cudaMemcpy` for ordering and for
  copying results back to the host.
- **ASR engine** (`asr_ops.cu`, `asr_gemm.cu`, `asr_audio_tower.cu`,
  `asr_text_decoder.cu`): the operator kernels already accept a `stream`
  parameter, but the engine drives them with the default stream (argument `0`)
  and synchronizes with `cudaDeviceSynchronize()`. The text decoder also reads
  device-managed memory on the host (the embedding table in
  `AsrTextDecoder::Embed`).
- **Coupling today**: both pipelines run on the default stream and both call
  `cudaDeviceSynchronize()`, which waits for **all** device work, including the
  other thread's. The global mutex (`gpu::DeviceLock()`) is held around each
  pipeline's whole GPU region, so in practice only one pipeline uses the GPU at
  a time.

Two facts follow:
1. `cudaDeviceSynchronize()` is device-wide. Even with separate streams, a
   device-wide synchronize in one thread would wait on the other thread's
   kernels. Per-pipeline isolation therefore requires replacing device-wide
   synchronization with per-stream synchronization on each pipeline's own stream.
2. The host read of device-managed memory (`Embed`) is the specific operation
   that faults when another thread has a kernel executing (Spec 002 R1). It must
   be removed or made safe before the mutex can be removed.

## 2. Target GPU usage

- A small **priority registry** in the `gpu/` layer (depends only on the CUDA
  runtime, never on `pipeline/` or `model/`): a pipeline registers a name + a
  declared **priority index** (priority class) and receives a CUDA stream whose
  concrete priority is derived from that index via
  `cudaDeviceGetStreamPriorityRange`. This single mapping is the only place that
  turns an index into a stream priority, so adding a pipeline is a registration
  change, not a scheduler edit (spec FR1, FR6; Constitution Art. V.4).
- The three current pipelines register as:
  - diarization — foreground, latency-critical (highest priority),
  - ASR — foreground, throughput (lower than diarization),
  - VAD — background (lowest priority); it yields within a bounded window so a
    foreground pipeline always makes progress.
- Each pipeline issues all of its kernels and copies on its own stream and
  synchronizes only that stream (`cudaStreamSynchronize(stream)`), never the
  whole device.
- No host read of device-managed memory occurs while a kernel that may write it
  is still in flight; such reads follow a `cudaStreamSynchronize` of the
  producing stream.
- The global mutex is removed from paths that streams order safely. If any
  residual cross-pipeline hazard remains (for example a shared scratch buffer),
  it is fixed by giving each pipeline its own buffer, not by re-introducing a
  device-wide lock.
- A **telemetry snapshot** (spec FR7) is read from the registry + the per-worker
  `compute_sec`/real-time-factor already tracked, serialized as an additive
  `{"type":"gpu_telemetry",...}` WebSocket message and pushed **periodically** at
  a bounded interval (`gpu_telemetry_interval_sec`, documented default 1.0 s; 0
  disables). A dedicated low-rate timer (or the controller) builds and emits the
  snapshot through the existing serialized transport send; no GPU worker thread
  emits it on its hot path, and no separate socket is opened. The registry is the
  single source of truth for both the stream mapping and the telemetry, so the
  two cannot drift.

## 3. Work breakdown

### 3.1 Measurement harness (precedes any engine change)
- A small instrumented run mode that streams a fixed input and records, per
  pipeline and combined: compute time, real-time factor, and the `tegrastats`
  `GR3D_FREQ` GPU-busy fraction. The baseline numbers are recorded before any
  engine change (spec M1–M3, AC1).

### 3.2 ASR engine: stream parameterization and managed-memory safety
- Thread the `asr_stream_` through the ASR call path so every kernel and copy
  uses it. The operator kernels already accept a `stream`; replace the default
  `0` arguments with the pipeline stream.
- Replace `cudaDeviceSynchronize()` in the ASR path with
  `cudaStreamSynchronize(asr_stream_)`.
- Remove the host read of device-managed memory in the decode path
  (`AsrTextDecoder::Embed`): either gather the embedding on the device with a
  kernel on `asr_stream_`, or copy the needed row to host pinned memory after a
  stream synchronize. The chosen method must keep the decoder's output identical
  within tolerance.

### 3.3 Diarization engine: stream parameterization
- Route the diarization kernels and copies onto `diar_stream_`.
- Replace `cudaDeviceSynchronize()` in the diarization path with
  `cudaStreamSynchronize(diar_stream_)`.
- This engine is verified against NeMo; the change is to ordering and stream
  assignment only, not to arithmetic. Re-run the diarization verification.

### 3.4 Controller: stream ownership and lock removal
- `AuditoryStream` creates the two prioritized streams at startup, passes each to
  its worker, and removes `gpu::DeviceLock()` from the worker GPU regions once
  §3.2 and §3.3 make concurrent use safe.
- Keep `gpu::DeviceLock()` (or delete it) based on whether any shared GPU
  resource remains after the per-pipeline buffers are separated. Document the
  decision and the reason.

### 3.5 Post-change measurement and comparison
- Re-run the §3.1 harness and record the same numbers. Compare with the baseline
  and report the total wall-time change (spec AC5).

### 3.6 Managed-memory de-coupling (the R1 precondition for safe concurrency)

The empirical R1 test (spec §7a) proved that routing ASR onto its own stream and
making it lock-free is NOT sufficient: both verified engines pervasively
host-access `cudaMallocManaged` memory (`gpu::UnifiedBuffer`) during steady-state
streaming, which faults on Tegra when the other pipeline's kernels run
concurrently. Safe concurrency therefore requires removing every host access to
in-flight managed memory in BOTH engines first. This is kept INSIDE Spec 002
(not a new spec): the requirement was discovered by this feature's own work, the
attribution is cleanest here, and the lock-removal decision can only be made
after this de-coupling is done and measured. It is sequenced AFTER §3.1–3.5 and
gated so it never regresses a verified engine.

Approach, applied per engine, one host-visible buffer at a time:
- Classify every `UnifiedBuffer` whose contents the host reads/writes during
  streaming (not at load time). Load-time managed use is safe (no concurrent
  kernels) and may stay.
- For each streaming host-visible result, replace the managed buffer with a
  device buffer (`gpu::DeviceAllocator`) plus an explicit copy into PINNED host
  memory (`gpu::PinnedAllocator`) issued on the owning stream, and read the host
  copy only after `cudaStreamSynchronize(owning_stream)`. Pinned host memory is
  not subject to the managed-page migration fault.
- Diarization host-visible streaming results to convert (from the survey): the
  decoder's per-frame sigmoid copy (`sortformer_decoder.cu` — `predp`/`preds`
  managed → device + pinned), the pre-encode output copy
  (`conformer_preencode.cu`), and the mel output copy (`mel_spectrogram.cu`). All
  diarization kernels already run on the default stream; once the host reads go
  through pinned staging after a `cudaStreamSynchronize(0)`, the diarizer no
  longer touches managed pages during a concurrent ASR kernel.
- ASR host-visible streaming reads to audit: the scalar reads in the decode loop
  (token id / argmax) — confirm each reads pinned or device-synchronized memory
  on `asr_stream_`, not managed pages. The named embedding read is already a
  plain-host shadow (`embed_host_`); verify no other managed host read remains on
  the hot path.
- After EACH buffer conversion, the engine's numeric gate MUST stay green
  (`test_diar_stream` for diarization; `test_asr_encoder`/`test_asr_decoder` for
  ASR) — the change is to WHERE memory lives and WHEN it is read, never to the
  math.

Only after both engines are managed-memory-clean on their streaming paths is the
lock-free path (`gpu::DeviceGuard` own-stream skip, currently disabled by the
`kOwnStreamConcurrencySafe` compile-time constant) re-enabled and re-tested for
the SIGSEGV; then the global lock is removed (§3.4) and the 5× stability +
post-change measurement (§3.5) decide whether concurrency is kept.

## 4. Concurrency Safety (Constitution V.5)

- Each pipeline owns its stream, its scratch buffers, and its model state. There
  is no shared mutable GPU state between the two pipelines after §3.4; if any is
  found, it is duplicated per pipeline.
- Host reads of device data occur only after the owning stream is synchronized.
- Verification: run the streaming test at least five times and confirm no fault
  and deterministic output (AC3); use `compute-sanitizer --tool racecheck` where
  available.

## 5. Validation

- **Correctness**: diarization verified against its NeMo reference within the
  recorded tolerance; ASR transcript compared against the pre-change output for
  the same audio (AC4). The existing `ctest` suite passes (AC6).
- **Stability**: five consecutive 120 s streaming runs through the WebSocket
  with no segmentation fault (AC3).
- **Performance**: baseline and post-change M1–M3 recorded; total wall time on
  120 s of `test.mp3` reported (AC5). A regression is a failure.

## 6. Constitution Check

- **I**: CUDA stream APIs from the toolkit; no new dependency.
- **II**: AC4 forbids quality regression; numerics unchanged (NG1).
- **III**: the pipelines stay independent; only GPU sharing changes.
- **IV**: measured on the real streaming path.
- **V**: per-pipeline ownership, host reads after stream sync, race verification.
- **VI**: standard terminology.
- **VII**: measurement (§3.1) gates the implementation.

## 7. Risks and Mitigations

- **Removing device-managed host access (R1)**: the highest-risk change, in the
  verified ASR decoder. Mitigation: change one access at a time, compare decoder
  output against the reference after each change, and keep the change limited to
  how the embedding is read (not to the math).
- **Device-wide synchronize defeating streams**: every `cudaDeviceSynchronize()`
  on a pipeline path is replaced with a per-stream synchronize; a missed one
  would serialize the pipelines and show up as no improvement in M2.
- **Small measured gain (R2)**: if M1 shows little GPU idle time, the wall-time
  reduction is small. The feature still removes the priority inversion and the
  unsafe memory access; the result is reported honestly without overstatement.
- **Stream priority range (R3)**: queried at runtime; if the range is a single
  value, priorities collapse and only stream concurrency (not priority) applies.
  Reported, not assumed.

## 8. Out of Scope (per spec Non-Goals)

Model numerics/quantization, ASR endpointing changes, multi-GPU or multi-process
execution, and any change to the output contract.
