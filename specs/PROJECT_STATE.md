# Project State — Orator

A point-in-time record of where the project stands. Updated at meaningful
checkpoints. Authoritative engineering rules live in
[.specify/memory/constitution.md](../.specify/memory/constitution.md); active
work is specified under [specs/](.).

> **How this document stays truthful (Constitution Article VIII).** The code is
> authoritative; this file is subordinate to it. Every claim below names how to
> confirm it against the code (a symbol/file, a test, or a commit reference). If
> a claim and the code disagree, the code is correct and this file is the defect
> — fix it. Before acting on any claim here, verify it: a clean
> `cmake --build build -j` plus a full `cd build && ctest --output-on-failure`
> pass is the consistency proof. Status lines advance to `Implemented` in the
> same change that lands the code, with the commit reference.

- **Last updated**: 2026-06-17 (Spec 004 live ASR revision convergence hardened; interleaved convergence test + real-path WS revalidated, commit f3496ae)
- **Branch**: `master`
- **Constitution**: v1.2.1

---

## 1. What Orator is

A real-time, edge-deployed (Jetson Orin) auditory pipeline, **pure C++/CUDA with
zero runtime third-party dependencies**. It ingests a live mono-audio stream over
WebSocket and produces a comprehensive timeline that carries both **speaker
separation** and **ASR transcript** content, one track per pipeline, on one
absolute time base.

## 2. Current phase

**Specs 003 (incremental KV-cache ASR), 004 (revisable comprehensive timeline),
and 005 (reusable common time base) are implemented, verified, committed, and
pushed.** Spec 004's VAD pipeline was completed in Phase 5: the VAD detector now
runs BATCHED ON THE GPU (`GpuVad`) and its speech segments are a serialized
`vad` track in the timeline document (previously CPU-only and write-only).
The system runs **three independent active-producer pipelines** —
diarization (who/when), ASR (what/when), and VAD speech-activity detection — each
feeding one **native, revisable comprehensive timeline** on a single absolute
time base. The comprehensive layer is a **pure time-alignment layer**: it never
modifies, splits, infers, or back-fills any pipeline's content (Spec 004 §1a).
Validated end-to-end through the **real WebSocket** path (not just the test
harness): the `ready` message declares the common time base, and `asr`,
`asr_partial`, `vad`, `revision`, and `timeline` messages all flow; the
timebase reconciliation check is clean (no gap).

**Production default (2026-06-16):** the incremental KV-cache ASR path and the
independent VAD stream are the out-of-the-box `orator_ws` default
(`asr_incremental` and `vad_stream` default true). The legacy Silero-VAD
utterance path is deactivated by default (measured worse: 600 s CER 26.4% / 3.50x
vs 11.6% / 4.78x) and retained only for regression comparison via
`ORATOR_ASR_INCREMENTAL=0`. The `endpoint_reset` knob is **default off**: it runs
a CPU-only Silero pass synchronously in the ASR thread, which stalls the worker
(GPU idle, one CPU core busy) when ASR falls behind and drains a large backlog;
it is opt-in only (`ORATOR_ASR_ENDPOINT_RESET=1`).

**Full-length production validation (2026-06-16):** the full 1-hour `test.mp3`
(3615 s) was streamed through the real `orator_ws` WebSocket at the default
config. ASR covered the entire hour (0 → 3615.1 s, 151 segments, zero
discontinuities), the comprehensive `timeline` was produced, endpoints reached
3614 s (1454), reconciliation was clean, and CER vs gold was 16.2%
(edits 2162 / ref 13352). GPU stayed busy (GR3D busy fraction 59.8%, mean 41%,
longest idle stretch 25 s — no CPU-only stall).

**Milestone-gate full-hour revalidation on the stable-`text_id` ASR-revision
code (2026-06-17):** the full 1-hour `test.mp3` (3615.1 s) was re-streamed
through the real `orator_ws` WebSocket at the default config to certify the
ASR self-revision refactor (commit f3496ae) at full scale. Result (evidence in
`/tmp/fullhour_validate.py`, `/tmp/fullhour_timeline.json`): end-to-end 3.70x
(wall 975.9 s); ASR covered the entire hour (0 → 3615.12 s, 151 segments, zero
discontinuities); ASR self-revision fired in place for 149 of 151 `text_id`s
(same id revised across multiple states); every non-empty revised id's text
landed in the comprehensive view (151/151 present, 0 missing, 15974 chars);
diar track 1083, vad track 1454, comprehensive 1893 entries; timebase
reconciliation clean (no gap). GPU over the active window (0–976 s) busy
fraction 55.5%, mean load 34.1%, max 99%, longest idle 12 s (no CPU-only
stall; the 145 s idle in the raw log is post-completion trailing samples). CER
vs gold 16.2% (edits 2161 / ref 13352, hyp 13254) — unchanged from the prior
full-hour run, confirming the revision refactor did not regress accuracy.

### Pipeline responsibility boundaries (ratified, do not re-litigate)

- **ASR** outputs ONLY plain transcript text + its own time codes. It has **no**
  speaker awareness and never attributes speakers.
- **Diarization** outputs ONLY its own speaker identities + time codes. It never
  attributes text.
- **Comprehensive timeline** aligns the two purely by time overlap on the common
  base. A coarse ASR span overlapping several speakers aligning to one speaker is
  **by design, not a defect**. Finer attribution, if ever wanted, is an OPTIONAL
  enhancement owned by the ASR pipeline emitting finer-grained timed units — the
  comprehensive layer never synthesizes them. Do not track this as a pending bug.

## 3. Component status

| Component | Status | Notes |
|---|---|---|
| Streaming diarization (Sortformer) | ✅ Verified, industrial-grade | Incremental, O(n), persistent identity; matched vs NeMo (forward <5e-3, streaming <1e-2, incremental <1e-4). |
| Native Qwen3-ASR engine | ✅ Verified vs PyTorch oracle | mel 3.9e-3, encoder 1.3e-3, decoder argmax-match; transcript matches gold. Pure bf16 compute. |
| WhisperMel / BPE tokenizer / sharded safetensors loader | ✅ Verified | Unit-tested. |
| Decoupling (interfaces + registry) | ✅ In place | `IDiarizer`, `IAsr`; registry-constructed. Text↔speaker combination is the concrete `ComprehensiveTimeline` (pure time-alignment), not an interface. |
| `OverlapTimelineMerger` / `ITimelineMerger` | 🗑️ Removed | The old one-shot max-overlap merger and its orphaned interface were deleted (commit pending) — superseded by `ComprehensiveTimeline` (Spec 004). `orator_eval` now evaluates `ComprehensiveTimeline` on the real 4-speaker reference. |
| WebSocket server (from-scratch POSIX) | ✅ Working | RFC6455 handshake + frame codec, no deps. |
| ASR + WS integration | Done; threaded, three independent pipelines | `AuditoryStream` is a controller owning a `SharedAudioBuffer`, three worker threads (`DiarizationWorker`, `AsrWorker`, VAD detector), and a mutex-guarded `ComprehensiveTimeline`. Each pipeline is an active push producer (Spec 004). Sends incremental `diar`, `asr`/`asr_partial`, `vad`, `revision`, and a comprehensive `timeline`. ASR uses stable `text_id` for in-place segment revision. GPU work serialized by `gpu::DeviceLock()`. |
| Incremental KV-cache ASR streaming (Spec 003) | ✅ Implemented, verified, committed (8cc31ab) | Persistent KV cache + prefix caching + chunk-local windowed encoder; Silero endpoint reset. Full 1hr CER 16.1% / 6.22x; beats production Silero-VAD at every scale. |
| Revisable comprehensive timeline (Spec 004) | ✅ Implemented (core + VAD pipeline + WS conformance) | Native stateful PURE CONTAINER + diarization-driven VIEW. Three tracks (diarization, asr, vad) each carry data + `source` meta + time codes; every pipeline emits its own WS message (`diar`/`asr`/`vad`) and revisions are source-tagged. The comprehensive VIEW splits text at DIARIZATION boundaries (not ASR's coarse segmentation). VAD = batched GPU `GpuVad` publishing speech segments (`test_vad` gate 3.7e-8). Owner invariant: no overlap → "unknown", never borrowed. |
| Reusable common time base (Spec 005) | ✅ Implemented, committed (84fba90) | Header-only `core::TimeBase` value type shared by all pipelines; reconciliation check clean (zero gap) at 120s + 600s. |
| Streaming validation | ✅ Through real WebSocket | `tools/ws_stream_client.py` (stdlib, reader thread) streams PCM through the socket at an accelerated rate and exports the full event log + timeline to JSON. |
| Test suite | ✅ 19/19 ctests pass | Clean build under `-Wall -Wextra`, ZERO warnings (all dead kernels removed). `test_vad` gates the GPU VAD detector vs the CPU reference. NOTE: ctests are component/oracle gates; full production behavior is validated only through the real `orator_ws` WebSocket path (the `asr_stream_test` harness bypasses the socket). A green ctest run is necessary but not sufficient — the full-hour CPU-stall defect was invisible to ctest. |

## 4. Measured performance (GPU fixed at 1.3 GHz, power mode MaxN)

Measured through the **real WebSocket** at max push rate, 120 s of `test.mp3`
(`/tmp/orator_stream_120.json`):
- **Diarization**: ~9.6× real-time (compute 12.5 s).
- **ASR**: ~2.6× real-time (compute 46.4 s) — many small endpointed utterances,
  each paying fixed per-call cost. Throughput tuning is deferred by owner
  (Spec 001 NG1).
- **End-to-end stream**: ~2.26× real-time (wall 53 s). Because the two pipelines
  share ONE GPU, the GPU lock serializes device work, so wall ≈
  diar_compute + asr_compute. The threads still overlap their CPU-side work
  (buffering, endpointing, serialization); the wall is GPU-bound.
- 25 diarization segments + 27 transcript utterances on one time base; the
  comprehensive view groups them into 10 speaker turns; transcript matches the
  verified engine's output.

Clip-based ("whole buffer") numbers are **not** treated as streaming results,
per Constitution Art. IV.

### Spec 002 baseline (Phase 1, measured before any engine change)

Three configurations, 120 s of `test.mp3`, through the real WebSocket at max
push rate, GPU fixed at 1.3 GHz, power mode MaxN:

| Configuration | Wall time | GPU compute | GPU-busy fraction |
|---|---|---|---|
| Diarization only | 3.2 s (37.2×) | 3.0 s (39.9×) | 78.8% |
| ASR only | 38.4 s (3.13×) | 33.9 s (3.54×) | 72.8% |
| Both (current, global lock) | 53.3 s (2.26×) | — | ~63% |

Findings:
- The lower bound on total wall time is the larger single-pipeline compute time,
  which is ASR (~38 s). The current both-pipelines wall time is 53 s, so the
  global lock adds about 15 s of serialization.
- Diarization alone is about 3 s of GPU work, but under the global lock its
  measured time rises to 12.5 s because it waits behind ASR. The lock delays the
  latency-critical pipeline.
- ASR alone leaves the GPU idle about 27% of the time, so diarization's small
  GPU work can run during ASR's idle intervals.
- Realistic target (M3): reduce total wall time from 53 s toward the ASR-only
  floor (~38–40 s, about 3.0× real-time), a 25–28% reduction. The total cannot
  go below ASR-only without an ASR speedup (Spec 001 NG1, deferred).



## 5. Decisions on record

- **No quantization at this stage.** int8 was prototyped and **fully reverted**;
  decode is pure bf16. Any quantization is deferred to a separate, scheduled
  effort (Constitution II.3).
- **Two independent pipelines + threaded controller** is the agreed architecture
  (Spec 001). The main process owns and controls the worker threads.
- **Engineering quality is a ratified requirement** (Constitution Art. V):
  readability, organization, maintainability, extensibility, concurrency safety.

## 6. SDD artifacts

- [.specify/memory/constitution.md](../.specify/memory/constitution.md) — v1.2.1 (Article VIII: documentation–code consistency)
- [specs/001-streaming-pipeline/spec.md](001-streaming-pipeline/spec.md) — implemented
- [specs/001-streaming-pipeline/plan.md](001-streaming-pipeline/plan.md) — implemented
- [specs/001-streaming-pipeline/tasks.md](001-streaming-pipeline/tasks.md) — implemented
- [specs/002-gpu-scheduling/spec.md](002-gpu-scheduling/spec.md) — in progress (priority registry + gpu_telemetry done dbebf5f/854dc7e; lock removal blocked on diar gate)
- [specs/002-gpu-scheduling/plan.md](002-gpu-scheduling/plan.md) — in progress
- [specs/002-gpu-scheduling/tasks.md](002-gpu-scheduling/tasks.md) — in progress
- [specs/003-sliding-window-asr/spec.md](003-sliding-window-asr/spec.md) — implemented (8cc31ab)
- [specs/004-comprehensive-timeline/spec.md](004-comprehensive-timeline/spec.md) — implemented (core 3159b75, 673f95d; VAD pipeline Phase 5; live ASR revision + docs sync f3496ae)
- [specs/005-time-base/spec.md](005-time-base/spec.md) — implemented (84fba90)

## 7. Immediate next step

Specs 001, 003, 004, and 005 are complete, verified, committed, and pushed; the
full three-pipeline path is validated through the real WebSocket, including
in-place ASR self-revision (same `text_id` revised mid-segment, landing in the
comprehensive view; verified on the real WS path, commit f3496ae). No
comprehensive-layer work is pending — it is a finished pure time-alignment layer
by design.

Open, independently-scheduled items:
- **Spec 002 — GPU Scheduling** (IN PROGRESS): the GPU priority registry
  (`gpu::GpuScheduler`, `include/gpu/scheduler.h`) and the periodic
  `{"type":"gpu_telemetry"}` WS message are implemented, verified, and pushed
  (dbebf5f, 854dc7e). Each pipeline declares a priority index at registration
  (diar=0 fg, asr=1 fg, vad=2 bg); the ASR stream is sourced from the registry;
  telemetry was confirmed on the real WS path (4 snapshots over 120 s at a 2 s
  interval, env `ORATOR_GPU_TELEMETRY_SEC`). **R1 red-line tested and confirmed
  (2026-06-17):** an env-gated lock-free concurrent path (`ORATOR_GPU_CONCURRENT=1`,
  `gpu::DeviceGuard`, ASR lock-free on its own stream + stream-scoped diar syncs)
  produced an IMMEDIATE SIGSEGV on the first 120 s real-WS stream, while the
  serialized default completed correctly (40.6 s, 25 diar + 5 asr + 45 comp).
  Root cause = R1: both verified engines pervasively host-access
  `cudaMallocManaged` (`UnifiedBuffer`) memory during streaming, which faults on
  Tegra when the other pipeline's kernels run concurrently; fixing the one named
  case (`AsrTextDecoder::Embed`, already a plain-host shadow) was necessary but
  not sufficient. DECISION: `DeviceGuard` always takes the global lock (a
  compile-time constant disables the own-stream skip), so the env flag is a safe
  no-op (re-verified serialized, 40.7 s, no crash). The registry, telemetry, and
  stream-scoped diar syncs are retained as the foundation. Safe lock-free
  concurrency requires de-coupling BOTH engines from managed memory (device
  buffers + pinned host staging) — retained INSIDE Spec 002 as Phase 7
  (T070–T073, accuracy-gated per engine), not a new spec, since the requirement
  arose from this feature. The global lock stays the production default; no
  regression. A diarization numeric gate (`test_diar_stream`, d75da36) is in
  place for that engine work. **BREAKTHROUGH (2026-06-17, commit 3abea74): an
  ASR-only lock-free path WORKS and is measurably faster.** With the ASR engine's
  managed-memory host-touch sites de-coupled (device + pinned staging; 2c34d20,
  b4606d9) AND CUDA Graph capture auto-disabled under concurrency (capture is a
  process/stream-global state machine that the concurrently-issuing diar/VAD
  pipeline corrupts → "operation failed … during capture" abort — this was the
  second, non-memory blocker), the env-gated `ORATOR_GPU_CONCURRENT_ASR=1` runs
  ASR lock-free while diarization/VAD stay locked. Measured on 120 s real-WS
  (tegrastats): serial wall 40.2 s, GPU busy 55.4 %; concurrent wall 29.9 s
  (−25.6 %), GPU busy 65.0 %; diarization RTF rose 5.4× → 26× (it no longer waits
  behind ASR). 8+ consecutive runs, zero crashes, output identical to serial
  (asr 5 / diar 25 / comp 45). This hits the Spec 002 M3 target (25–28 %). It is
  an OPT-IN experiment for now (production default stays serial, lock on). **Full
  concurrency landed and measured (2026-06-17, commit 97dd2cc):**
  `ORATOR_GPU_CONCURRENT=1` drops the lock for diarization + VAD too. It was
  expected to need a large de-coupling of the diarizer's managed
  `SortformerState`, but a survey showed that is UNNECESSARY: the diarizer's
  streaming compute state is pure host `std::vector` (`HostStreamState`), the
  managed `SortformerState` is retained-but-INACTIVE (allocated + cleared, never
  read on the streaming path), `GpuVad` is already all-device (`cudaMalloc`), and
  the diarizer's result copies were already converted to device memory. So full
  concurrency runs stable (8/8 runs, zero crashes) and correct (asr 5 / diar 25 /
  comp 45). MEASURED (120 s real-WS, 3-run average): serial 39.9 s, ASR-only
  concurrent 31.4 s (−21.3 %), full concurrent 31.3 s (−21.6 %). KEY FINDING:
  ASR-only and full are essentially identical — the entire gain comes from ASR's
  own stream no longer blocking diarization; dropping the diar↔VAD lock adds
  nothing because they share the default stream (their kernels serialize on
  stream 0 regardless). Making diar and VAD truly GPU-parallel would require
  routing the diarizer's hundreds of kernel launches onto a dedicated non-default
  stream for no measurable benefit (diar is already ~26× RTF, VAD is tiny;
  neither is the bottleneck). Both concurrency modes remain OPT-IN; production
  default stays serial (lock on, no regression).
- **Full 1-hour real-WS revalidation on the current `text_id` revision code**:
  done (2026-06-17, commit c94c2fa) — see the milestone-gate section in §2.
- ASR streaming throughput (~2.6x, Spec 001 NG1) — deferred by owner.
