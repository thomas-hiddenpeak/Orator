# Plan 001 — Real-Time Streaming Dual Pipeline

- **Feature**: `001-streaming-pipeline`
- **Spec**: [spec.md](spec.md)
- **Status**: Draft (awaiting review)
- **Constitution**: v1.0.0

> This plan describes HOW to satisfy [spec.md](spec.md). It is the design of
> record for the threaded WebSocket + dual-pipeline architecture.

---

## 1. Architecture Overview

```
                    WebSocket (POSIX, from scratch)
                              │  binary PCM frames
                              ▼
                    ┌───────────────────┐
                    │  Ingest (net)     │  decode int16/f32 → float, append
                    └─────────┬─────────┘
                              │ PushAudio(samples, n)
                              ▼
                    ┌───────────────────┐
                    │  SharedAudioBuffer│  thread-safe, monotonic abs clock
                    └────┬─────────┬────┘
              cursor A   │         │   cursor B   (each consumer has its own read cursor)
                         ▼         ▼
            ┌──────────────────┐ ┌──────────────────┐
            │ Diarization      │ │ ASR              │   ← independent worker THREADS
            │ worker thread    │ │ worker thread    │
            │ (Sortformer)     │ │ (endpoint+Qwen3) │
            └────────┬─────────┘ └────────┬─────────┘
                     │ results            │ results
                     ▼                    ▼
                    ┌───────────────────┐
                    │  Timeline (mutex)  │  diarization[] + transcript[]
                    └─────────┬─────────┘
                              │ flush/end → serialize
                              ▼
                    WebSocket text frames (asr events, timeline JSON)
```

A single **controller** (the connection handler, owned by the main process)
creates the buffer, spawns the two worker threads, routes control messages, and
performs ordered shutdown (signal → drain → join → emit).

## 2. Components

### 2.1 `SharedAudioBuffer` (new, `pipeline/`)
- Owns the session PCM on one absolute clock (sample 0 = stream start).
- Producer: ingest thread appends samples under a mutex; bumps `total_samples_`.
- Consumers: each pipeline holds its **own read cursor** (absolute sample
  index). A consumer asks for "samples from my cursor up to now"; the buffer
  returns a copy of that span and the consumer advances its cursor.
- Synchronization: `std::mutex` + `std::condition_variable`. Consumers wait on
  the CV for "new data or end-of-stream" instead of spinning.
- End-of-stream: producer sets an `eos_` flag and notifies; consumers drain
  remaining samples then exit.
- **Backpressure (R1)**: default is **unbounded catch-up** — the buffer retains
  unconsumed audio so ASR can run at its own maximum rate (per spec FR8). To
  bound memory, the buffer may drop a prefix only once **both** cursors have
  passed it (a low-water mark = min(cursor_A, cursor_B)). No consumer ever loses
  unread audio.

### 2.2 `StreamWorker` model (controller + two threads)
- The diarization worker loops: wait for data → pull new span → `StreamAudio()`
  incrementally → append frames to its local accumulator (persistent diarizer
  state lives in the worker). On eos: finalize tail.
- The ASR worker loops: wait for data → pull new span → append to its endpoint
  buffer → run energy endpointing → for each completed utterance, transcribe and
  publish an `asr` event + append a token to the timeline. On eos: flush the
  trailing utterance.
- Each worker measures its own compute time for honest per-pipeline RTF.

### 2.3 `Timeline` accumulator (mutex-guarded, in `AuditoryStream`)
- Two arrays guarded by one mutex: diarization segments (rebuilt from frames at
  serialize time) and transcript tokens (appended as utterances complete).
- Serialization (flush/end) snapshots both arrays under the lock and emits the
  unified document (spec §8).

### 2.4 `AuditoryStream` (refactor existing)
- Today it runs both pipelines **inline and sequentially** inside `PushAudio`.
- Refactor to the **controller** role: owns `SharedAudioBuffer`, spawns/join the
  two worker threads, exposes `PushAudio` (producer-side append), `EmitTimeline`,
  `Reset`. The streaming/endpoint logic moves into the workers.
- Keep the transport-agnostic `Emit` callback (handler sends to client; test
  captures to file). No sockets in this class.

### 2.5 `AuditoryWsHandler` (existing, minor change)
- Stays a thin adapter: decode frames → `stream_->PushAudio(...)`; route
  `reset`/`flush`/`end`; forward `Emit` JSON to the connection.
- Controls now imply thread coordination (flush = drain+serialize without
  stopping; end = eos+join+serialize).

## 3. Threading & Lifecycle (FR4)

- **Spawn**: controller starts diar + asr threads on session open.
- **Run**: ingest (the WebSocket read loop) is the producer; the two workers are
  consumers. They never touch each other's state.
- **flush**: producer marks a flush barrier (current `total_samples_`); workers
  drain up to that point; controller serializes the timeline. Streaming
  continues (persistent state retained).
- **end**: producer sets eos; workers drain fully and exit; controller joins
  both, serializes final timeline, then resets for any subsequent session.
- **Ownership**: the controller is the sole owner of thread handles; threads are
  always joined before the controller is destroyed (no detached threads).

## 4. Concurrency Safety (Constitution V.5)

- Each shared field documents its guard:
  - `SharedAudioBuffer`: `mutex_` guards `samples_`, `total_samples_`, `eos_`;
    `cv_` signals data/eos.
  - `Timeline`: `timeline_mutex_` guards `diar_*` and `transcript_`.
- Workers keep their **own** non-shared state (diarizer object, ASR endpoint
  buffer, read cursor) — no sharing, no lock needed for those.
- Pattern: copy-span-out under lock, then compute lock-free; take the timeline
  lock only to append/serialize. Locks are never held across CUDA work.
- Verification: run the streaming test under `compute-sanitizer --tool
  racecheck`/TSan-equivalent where available, plus a stress run at high input
  rate; assert deterministic output across runs.

## 5. Data Flow on the Shared Clock (FR3)

- Absolute sample index is the single source of truth. `t_sec = sample / rate`.
- Diarization frames carry `frame_period_sec`; segment times derive from frame
  index + period.
- ASR utterance times derive from the absolute sample offsets of the endpointed
  span. Both modalities therefore land on the same axis with no negotiation.

## 6. Test & Tooling Design (FR4, FR8–FR10)

### 6.1 Real WebSocket streaming client (`tools/ws_stream_client.py`, evolve existing)
- Stdlib-only (no deps), performs the RFC6455 handshake (already present).
- Decodes `test.mp3` to 16k mono PCM **outside** the timed path (or accepts a
  pre-decoded `.pcm`), then pushes int16-LE frames **through the socket** at an
  accelerated multiple (configurable; e.g. push N ms of audio every M ms, or
  back-to-back for "max rate").
- Reads ALL server frames concurrently (a reader thread), capturing every
  `asr` event and the final `timeline` — fixes the current client's "read one
  frame after flush" bug that drops streaming events.
- Sends `end`, waits for the final timeline, writes the **complete event log**
  to a JSON file (FR9): `{ "events": [ ...asr... ], "timeline": {...},
  "meta": { rate_multiple, wall_sec, ... } }`.

### 6.2 Honest metrics (FR10)
- Client reports wall time to drain and the server-reported
  `diar_compute_sec` / `asr_compute_sec`; per-pipeline RTF = audio_sec / compute.
- Numbers are labeled as streaming-path measurements under stated clock state
  (locked 1.3 GHz, MaxN).

### 6.3 Unit/integration coverage (Art. IV.2)
- An in-process integration test drives `AuditoryStream` via its public producer
  API with a synthetic stream and asserts the unified-timeline invariants
  (both arrays present, monotonic clock, drain completeness). The
  **authoritative streaming validation** remains the through-WebSocket client.

## 7. Constitution Check

- **I**: `std::thread` + stdlib + existing POSIX WS; no new dependency. ✅
- **II**: no fidelity change; pipelines unchanged numerically. ✅
- **III**: design centers on two independent threads + one timeline. ✅
- **IV**: validation is through the real socket at accelerated rate, asserting
  on real timeline output; JSON exported. ✅
- **V**: documented locks per field, RAII thread ownership, race verification,
  small single-purpose components. ✅

## 8. Risks & Mitigations

- **Drain race on `end`** → explicit barrier + join before serialize (FR7/AC4).
- **Unbounded buffer growth** under fast producer → low-water-mark prefix drop
  once both cursors pass; memory bounded by the lagging pipeline's backlog.
- **Lock held across GPU work** (perf/deadlock) → forbidden by design (copy out,
  then compute).
- **Client event loss** → dedicated reader thread captures all frames.

## 9. Out of Scope (per spec NG)

ASR throughput tuning, quantization, speaker↔text attribution, multi-connection.
