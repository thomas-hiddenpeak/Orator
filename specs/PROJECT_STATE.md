# Project State — Orator

A point-in-time record of where the project stands. Updated at meaningful
checkpoints. Authoritative engineering rules live in
[.specify/memory/constitution.md](../.specify/memory/constitution.md); active
work is specified under [specs/](.).

- **Last updated**: 2026-06-12
- **Branch**: `master`
- **Constitution**: v1.0.0

---

## 1. What Orator is

A real-time, edge-deployed (Jetson Orin) auditory pipeline, **pure C++/CUDA with
zero runtime third-party dependencies**. It ingests a live mono-audio stream over
WebSocket and produces a unified timeline carrying both **speaker separation**
and **ASR transcript** content on one absolute clock.

## 2. Current phase

**SDD established; threaded streaming architecture specified, not yet
implemented.** The two engines exist and are verified individually; wiring them
into a truly independent, threaded, honestly-tested streaming system is the next
implementation effort (Spec 001).

## 3. Component status

| Component | Status | Notes |
|---|---|---|
| Streaming diarization (Sortformer) | ✅ Verified, industrial-grade | Incremental, O(n), persistent identity; matched vs NeMo (forward <5e-3, streaming <1e-2, incremental <1e-4). |
| Native Qwen3-ASR engine | ✅ Verified vs PyTorch oracle | mel 3.9e-3, encoder 1.3e-3, decoder argmax-match; transcript matches gold. Pure bf16 compute. |
| WhisperMel / BPE tokenizer / sharded safetensors loader | ✅ Verified | Unit-tested. |
| Decoupling (interfaces + registry) | ✅ In place | `IDiarizer`, `IAsr`, `ITimelineMerger`; registry-constructed. |
| WebSocket server (from-scratch POSIX) | ✅ Working | RFC6455 handshake + frame codec, no deps. |
| ASR ↔ WS integration | ⚠️ Functional, single-threaded | `AuditoryStream` runs diarization + ASR **sequentially in one thread**; emits unified timeline JSON. Independent threads NOT yet implemented (Spec 001). |
| Streaming validation | ⚠️ Partial | A frame-by-frame tool exists but bypasses the real WS; the through-socket, accelerated-rate, JSON-exporting test is specified in Spec 001, not built. |
| Test suite | ✅ 18/18 ctests pass | Clean build under `-Wall -Wextra`. |

## 4. Honest performance snapshot (locked 1.3 GHz, MaxN)

Measured on the streaming path (frame-by-frame), 120 s of `test.mp3`:
- **Diarization**: ~11.8× real-time — meets target.
- **ASR**: ~3.3× real-time — limited by per-utterance fixed cost when streaming
  endpointing yields many small utterances. (Throughput tuning is deferred by
  owner; see Spec 001 Non-Goals.)
- Combined wall reflects **sequential** execution today; independent threads
  (Spec 001) will make wall track the slower pipeline, not the sum.

Clip-based ("whole buffer") numbers from earlier exploration are **not** treated
as streaming results, per Constitution Art. IV.

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

Owner reviews/approves the SDD (Task T000). On approval, implement Spec 001
Phases 1–5: shared buffer → worker extraction → threaded controller → real
through-WebSocket accelerated streaming test with JSON export → cleanup.
