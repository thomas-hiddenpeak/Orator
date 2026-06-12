# Spec 001 — Real-Time Streaming Dual Pipeline (Diarization + ASR) on a Unified Timeline

- **Feature**: `001-streaming-pipeline`
- **Status**: Draft (awaiting review)
- **Created**: 2026-06-12
- **Owner**: project owner
- **Constitution**: v1.0.0

> This spec describes WHAT the system must do and WHY. It avoids implementation
> detail (thread/buffer design lives in `plan.md`).

---

## 1. Summary

Orator ingests a single real-time mono-audio stream over WebSocket and produces
a **unified timeline** that carries both **speaker-separation segments** and
**ASR transcript content** on one absolute time base. Speaker separation and ASR
run as **two independent pipelines** that share only the input audio and the
clock; neither blocks the other. The system is validated end-to-end by streaming
audio through the real WebSocket transport and asserting on the real terminal
output (the timeline JSON).

## 2. Background & Problem

The codebase already has:
- A verified, industrial-grade **streaming diarizer** (Sortformer, incremental,
  O(n), persistent speaker identity).
- A verified native **Qwen3-ASR engine** (mel → audio encoder → text decoder →
  BPE), accuracy-matched to a PyTorch oracle.

But they are **not yet a real streaming system together**:
- `orator_ws` historically served diarization only; ASR was bolted on as an
  inline, whole-buffer call.
- The two pipelines run **sequentially in one thread**, so wall time ≈ the sum,
  not the max — they are not truly independent.
- Streaming was "tested" by handing whole clips to a single offline call, which
  hid the real per-utterance cost and produced optimistic, unreal numbers.

This feature makes the streaming dual-pipeline real, independent, and honestly
testable.

## 3. Goals

- **G1** Audio received over WebSocket feeds BOTH pipelines from one shared
  buffer; the pipelines are independent (no cross-reads, no mutual blocking).
- **G2** The pipelines run on **independent threads**; a controller (the main
  process / connection handler) owns their lifecycle (start, stop, drain, join).
- **G3** Both pipelines write results onto **one unified timeline** keyed by the
  shared absolute clock. The terminal output always contains both modalities.
- **G4** Streaming is validated through the **real WebSocket transport**, by
  pushing an incremental stream (optionally at an accelerated real-time
  multiple); ASR works at its own maximum rate. No block-data shortcut counts as
  streaming validation.
- **G5** The full terminal output is **exported to a JSON file** for inspection
  and later context comparison.
- **G6** Diarization quality and ASR transcript quality are preserved (no
  regression vs the verified references).

## 4. Non-Goals

- **NG1** ASR throughput optimization (endpoint tuning, fixed-cost reduction,
  batching). Deferred by the owner; not in this feature.
- **NG2** Quantization or any numerical-fidelity reduction. Deferred until
  explicitly scheduled (Constitution II.3).
- **NG3** Speaker-to-text attribution/fusion logic (assigning each transcript
  span to a speaker). This feature emits both modalities on one timeline;
  cross-attribution is a separate future feature.
- **NG4** Multi-connection concurrency. One stream at a time matches the edge
  use case.

## 5. User Scenarios

### Scenario A — Live transcription with speaker separation
A client opens a WebSocket connection and streams microphone PCM. As speech
flows, the server emits incremental ASR utterances and continuously tracks
speaker activity. When the client requests a flush (or ends), it receives a
unified timeline with both speaker segments and transcript spans on one clock.

### Scenario B — Accelerated-rate validation
A test client streams a known recording (`test.mp3`, decoded to PCM) through the
WebSocket **faster than real time** (e.g. pushing frames back-to-back). The ASR
pipeline consumes from the shared buffer at its own maximum speed and catches up;
diarization does likewise. The final unified timeline is captured to a JSON file
and compared against expectations.

## 6. Functional Requirements

Each requirement is testable.

- **FR1 — Ingest**: The server SHALL accept binary WebSocket frames of mono PCM
  (int16-LE default; float32 selectable via a text control) and place the
  decoded samples into a single shared buffer tagged with the absolute stream
  position.
- **FR2 — Independent consumption**: Diarization and ASR SHALL each consume the
  buffered audio independently and concurrently. Neither SHALL read the other's
  output; neither SHALL wait on the other to make progress.
- **FR3 — Shared clock**: All emitted results (diarization segments and ASR
  tokens) SHALL carry timestamps in one absolute time base derived from the
  ingested sample count.
- **FR4 — Independent threads + controller**: The two pipelines SHALL run on
  separate worker threads. A single controller SHALL own their lifecycle:
  start on session open, signal stop and drain on flush/end, and join cleanly.
- **FR5 — Incremental ASR output**: As ASR completes an utterance, the server
  SHALL emit an incremental result event (`{"type":"asr", start, end, text}`).
- **FR6 — Unified timeline output**: On flush/end, the server SHALL emit one
  timeline document containing both a diarization array and a transcript array
  on the shared clock (`{"type":"timeline", ...}`; schema in §8).
- **FR7 — Control protocol**: The server SHALL honor text controls `reset`
  (drop all state), `flush` (emit timeline so far; streaming continues), and
  `end` (drain both pipelines, emit final timeline). `flush`/`end` SHALL emit
  the timeline only **after** in-flight work has drained, so no completed audio
  is missing from the result.
- **FR8 — Accelerated streaming**: The system SHALL behave correctly when frames
  arrive faster than real time; the shared buffer absorbs the burst and ASR
  drains it at its own maximum rate. (Buffer policy specified in `plan.md`.)
- **FR9 — JSON export**: The streaming test client SHALL write every received
  event (incremental `asr` events and the final `timeline`) to a JSON file on
  disk for inspection and later comparison.
- **FR10 — Honest metrics**: The test SHALL report per-pipeline real-time
  factors measured on the streaming path, plus the wall time to drain, under
  stated clock conditions.

## 7. Acceptance Criteria

- **AC1** With ASR enabled, streaming `test.mp3` through the real WebSocket
  yields a final `{"type":"timeline",...}` whose `diarization` array is
  non-empty AND whose `transcript` array is non-empty, all timestamps on one
  monotonic clock. (FR1–FR3, FR6)
- **AC2** Diarization and ASR run on separate threads under a controller;
  measured wall time to drain is materially less than the sum of the two
  pipelines' compute times (evidence of real parallelism), and is governed by
  the slower pipeline. (FR2, FR4)
- **AC3** The validation streams audio **through the WebSocket** at an
  accelerated multiple (frames pushed faster than real time); no part of the
  test hands a whole clip to a single offline transcription call. (FR4, FR8)
- **AC4** `flush` and `end` return a timeline that accounts for all audio
  ingested before the control (no dropped tail). (FR7)
- **AC5** The complete event stream and final timeline are written to a JSON
  file; the file is valid JSON and contains both modalities. (FR9)
- **AC6** Diarization output matches the verified streaming diarizer within its
  recorded tolerance; ASR transcript matches the verified engine's output for
  the same audio (no quality regression). (G6)
- **AC7** Build is clean under `-Wall -Wextra` with no new warnings; the full
  test suite passes; no data race is present in the threaded path (verified).
  (Constitution V)

## 8. Output Contract (JSON schemas)

Incremental ASR event (emitted as each utterance completes):
```json
{ "type": "asr", "start": 12.34, "end": 15.67, "text": "…" }
```

Unified timeline (emitted on flush/end):
```json
{
  "type": "timeline",
  "audio_sec": 120.0,
  "diar_compute_sec": 10.2,
  "asr_compute_sec": 36.3,
  "diarization": [
    { "start": 0.00, "end": 4.32, "speaker": 0, "confidence": 0.94 }
  ],
  "transcript": [
    { "start": 0.00, "end": 9.80, "text": "…" }
  ]
}
```

Schema rules:
- Times are seconds on the shared absolute clock; `end ≥ start`.
- `speaker` is a diarizer-local slot index (registry attribution is NG3).
- `text` is UTF-8, JSON-escaped.
- The document is the single terminal output and is the unit captured for
  inspection/comparison.

## 9. Constitution Check

- **Art. I (no deps)**: transport stays on the existing from-scratch POSIX
  WebSocket; threading uses `std::thread`/std stdlib. No new dependency. ✅
- **Art. II (accuracy)**: no fidelity changes; AC6 forbids quality regression;
  quantization explicitly NG2. ✅
- **Art. III (two pipelines, one timeline)**: the spec's core (G1–G3). ✅
- **Art. IV (real streaming tests)**: AC3 mandates WebSocket streaming, forbids
  block-data shortcuts; AC1/AC5 assert on real terminal output. ✅
- **Art. V (quality)**: AC7 enforces clean build, passing tests, race-freedom;
  threading discipline deferred to `plan.md`. ✅
- **Art. VI (SDD)**: this is the spec phase; plan/tasks follow. ✅

## 10. Open Questions / Risks

- **Q1 (resolved)** Block-data vs streaming: streaming through WebSocket is
  mandatory (AC3).
- **Q2 (resolved)** Threading: independent threads with a controller (FR4),
  per owner direction.
- **R1** Backpressure under accelerated input: ASR may lag far behind a fast
  producer, growing the buffer. The buffer policy (bounded vs unbounded
  catch-up) is decided in `plan.md`; for "ASR at its own maximum rate" the
  default is unbounded catch-up with drain measured.
- **R2** Clean drain on `end`: must join workers after the producer signals
  end-of-stream without losing the final partial utterance (AC4).
- **R3** Honest perf: numbers are reported from the streaming path only
  (Art. IV.4).
