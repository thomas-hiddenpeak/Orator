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
- 25 diarization segments + 27 transcript utterances on one monotonic clock;
  transcript matches the verified engine's output.

Clip-based ("whole buffer") numbers are **not** treated as streaming results,
per Constitution Art. IV.

## 5. Decisions on record

- **No quantization at this stage.** int8 was prototyped and **fully reverted**;
  decode is pure bf16. Any quantization is deferred to a separate, scheduled
  effort (Constitution II.3).
- **Two independent pipelines + threaded controller** is the agreed architecture
  (Spec 001). The main process owns and controls the worker threads.
- **Engineering quality is a ratified requirement** (Constitution Art. V):
  readability, organization, maintainability, extensibility, concurrency safety.

## 6. SDD artifacts

- [.specify/memory/constitution.md](../.specify/memory/constitution.md) — v1.0.0
- [specs/001-streaming-pipeline/spec.md](001-streaming-pipeline/spec.md)
- [specs/001-streaming-pipeline/plan.md](001-streaming-pipeline/plan.md)
- [specs/001-streaming-pipeline/tasks.md](001-streaming-pipeline/tasks.md)

## 7. Immediate next step

Spec 001 is functionally complete and validated. Known follow-ups to discuss
with the owner (noted, not yet acted on):
- **ASR streaming throughput** (~2.6×): the dominant end-to-end cost. Options
  include larger utterances, reduced per-call fixed cost, or batching (NG1).
- **GPU sharing**: device work is serialized by one lock; if higher end-to-end
  throughput is needed, options include CUDA streams/priorities or time-slicing
  — to be weighed against accuracy and complexity.
- Potential issues the owner flagged for later review remain open by design.
