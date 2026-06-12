# Project State — Orator

A point-in-time record of where the project stands. Updated at meaningful
checkpoints. Authoritative engineering rules live in
[.specify/memory/constitution.md](../.specify/memory/constitution.md); active
work is specified under [specs/](.).

- **Last updated**: 2026-06-12 (Spec 001 implemented)
- **Branch**: `master`
- **Constitution**: v1.1.0

---

## 1. What Orator is

A real-time, edge-deployed (Jetson Orin) auditory pipeline, **pure C++/CUDA with
zero runtime third-party dependencies**. It ingests a live mono-audio stream over
WebSocket and produces a comprehensive timeline that carries both **speaker
separation** and **ASR transcript** content, one track per pipeline, on one
absolute time base.

## 2. Current phase

**Spec 001 implemented: threaded streaming dual pipeline live through WebSocket.**
Diarization and ASR run on independent worker threads behind a controller, fed
by a shared audio buffer, producing one comprehensive timeline (one track per
pipeline). Validated end-to-end by streaming `test.mp3` through the real
WebSocket faster than real time, with the full timeline written to JSON. GPU
access across the two threads is serialized by a process-wide GPU mutex (one
physical device; required for correctness on the Jetson unified-memory
platform).

## 3. Component status

| Component | Status | Notes |
|---|---|---|
| Streaming diarization (Sortformer) | ✅ Verified, industrial-grade | Incremental, O(n), persistent identity; matched vs NeMo (forward <5e-3, streaming <1e-2, incremental <1e-4). |
| Native Qwen3-ASR engine | ✅ Verified vs PyTorch oracle | mel 3.9e-3, encoder 1.3e-3, decoder argmax-match; transcript matches gold. Pure bf16 compute. |
| WhisperMel / BPE tokenizer / sharded safetensors loader | ✅ Verified | Unit-tested. |
| Decoupling (interfaces + registry) | ✅ In place | `IDiarizer`, `IAsr`, `ITimelineMerger`; registry-constructed. |
| WebSocket server (from-scratch POSIX) | ✅ Working | RFC6455 handshake + frame codec, no deps. |
| ASR + WS integration | Done; threaded, independent | `AuditoryStream` is a controller owning a `SharedAudioBuffer`, two worker threads (`DiarizationWorker`, `AsrWorker`), and a mutex-guarded `StreamTimeline`. Sends incremental `asr` messages and a comprehensive timeline. GPU work serialized by `gpu::DeviceLock()`. |
| Streaming validation | ✅ Through real WebSocket | `tools/ws_stream_client.py` (stdlib, reader thread) streams PCM through the socket at an accelerated rate and exports the full event log + timeline to JSON. |
| Test suite | ✅ 19/19 ctests pass | Clean build under `-Wall -Wextra`; `test_shared_buffer` exercises the threaded buffer (deterministic across runs). |

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

- [.specify/memory/constitution.md](../.specify/memory/constitution.md) — v1.1.0
- [specs/001-streaming-pipeline/spec.md](001-streaming-pipeline/spec.md) — implemented
- [specs/001-streaming-pipeline/plan.md](001-streaming-pipeline/plan.md) — implemented
- [specs/001-streaming-pipeline/tasks.md](001-streaming-pipeline/tasks.md) — implemented
- [specs/002-gpu-scheduling/spec.md](002-gpu-scheduling/spec.md) — draft (awaiting review)
- [specs/002-gpu-scheduling/plan.md](002-gpu-scheduling/plan.md) — draft
- [specs/002-gpu-scheduling/tasks.md](002-gpu-scheduling/tasks.md) — draft

## 7. Immediate next step

Spec 001 is complete (including the comprehensive speaker-turn view). The next
feature is **Spec 002 — GPU Scheduling**: replace the single global GPU mutex
with per-pipeline CUDA streams and stream priorities, remove the unsafe host
access to device-managed memory, and reduce the total wall time. Spec 002 is
measurement-gated: the baseline (GPU-busy fraction and per-pipeline timing) is
recorded before any engine change. Awaiting owner review of Spec 002 (T000),
then Phase 1 measurement.

Other known follow-ups (not in Spec 002): ASR streaming throughput (~2.6x, NG1).
