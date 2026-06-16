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
| Test suite | ✅ 19/19 ctests pass | Clean build under `-Wall -Wextra`, ZERO warnings (all dead kernels removed). `test_vad` gates the GPU endpoint detector vs the CPU reference. NOTE: ctests are component/oracle gates; full production behavior is validated only through the real `orator_ws` WebSocket path (the `asr_stream_test` harness bypasses the socket). A green ctest run is necessary but not sufficient — the full-hour CPU-stall defect was invisible to ctest. |

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
- [specs/002-gpu-scheduling/spec.md](002-gpu-scheduling/spec.md) — draft (awaiting review)
- [specs/002-gpu-scheduling/plan.md](002-gpu-scheduling/plan.md) — draft
- [specs/002-gpu-scheduling/tasks.md](002-gpu-scheduling/tasks.md) — draft
- [specs/003-sliding-window-asr/spec.md](003-sliding-window-asr/spec.md) — implemented (8cc31ab)
- [specs/004-comprehensive-timeline/spec.md](004-comprehensive-timeline/spec.md) — implemented (core 3159b75, 673f95d; endpoint pipeline Phase 5)
- [specs/005-time-base/spec.md](005-time-base/spec.md) — implemented (84fba90)

## 7. Immediate next step

Specs 003, 004, and 005 are complete, verified, committed, and pushed; the full
three-pipeline path is validated through the real WebSocket. The real-path env
wiring gap in `orator_ws` (it was not reading the Spec 003/004 streaming knobs)
is fixed (a823eb9, local). Open, independently-scheduled items: **Spec 002 — GPU
Scheduling** (replace the global GPU mutex with per-pipeline CUDA streams +
priorities) and an optional full 1hr real-path stress run. No comprehensive-layer
work is pending — it is a finished pure time-alignment layer by design.

Other known follow-ups (not in Spec 002): ASR streaming throughput (~2.6x, NG1).
