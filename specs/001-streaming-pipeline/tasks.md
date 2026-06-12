# Tasks 001 — Real-Time Streaming Dual Pipeline

- **Feature**: `001-streaming-pipeline`
- **Spec**: [spec.md](spec.md) · **Plan**: [plan.md](plan.md)
- **Status**: Implemented & validated (T000 approved by owner)
- **Constitution**: v1.0.0

> Ordered, independently verifiable steps. Each task states its outcome and how
> it is verified. `[P]` = parallelizable with its siblings.

---

## Phase 0 — Review gate
- [x] **T000** Owner approved proceeding ("可以先开动起来"); implementation begun.

## Phase 1 — Shared buffer foundation
- [x] **T010** `SharedAudioBuffer` (thread-safe append + per-cursor read,
  `mutex_`/`cv_`, `eos_`, low-water-mark prefix drop). Clean build.
- [x] **T011** `test/test_shared_buffer.cc`: producer + two consumers with
  distinct cursors; each sees every sample once, in order; memory bounded to the
  lagging cursor (peak ~8k of 200k). Deterministic across runs.

## Phase 2 — Worker extraction
- [x] **T020** `DiarizationWorker` consumes a buffer cursor, accumulates frames.
- [x] **T021** `AsrWorker` consumes a buffer cursor, endpoints + transcribes,
  emits incremental utterance events.
- [x] **T022** `StreamTimeline` is mutex-guarded; both workers append through it.

## Phase 3 — Threaded controller
- [x] **T030** `AuditoryStream` is the controller: owns the buffer, spawns diar +
  ASR threads, `PushAudio` producer side, `flush` (barrier) / `end`
  (eos+join+serialize), joins in the destructor.
- [x] **T031** `AuditoryWsHandler` drives the threaded controller; control
  messages map to thread coordination. **Fix**: a process-wide `gpu::DeviceLock()`
  serializes the two workers' GPU regions (one physical device; Tegra unified
  memory faults on host access to managed memory during another thread's kernel).
- [x] **T032** Concurrency verified: `test_shared_buffer` deterministic x3; the
  120s through-WS run completes without fault; GPU lock removes the data race.

## Phase 4 — Real streaming test client + JSON export
- [x] **T040** `tools/ws_stream_client.py` (stdlib): pushes PCM through the
  socket at a configurable accelerated rate; dedicated reader thread captures all
  frames (incremental `asr` events + final `timeline`).
- [x] **T041** JSON export: full event log + timeline + meta (rate, wall,
  per-pipeline RTF) written to file.
- [x] **T042** End-to-end 120s `test.mp3` through the WebSocket at max rate:
  25 diarization segments + 27 transcript utterances on one monotonic clock;
  transcript matches the verified engine. JSON at `/tmp/orator_stream_120.json`.

## Phase 5 — Test integration & cleanup
- [x] **T050** `test_shared_buffer` registered; full suite 19/19 passes clean.
- [ ] **T051** Retire the superseded inline path / old diarization-only handler
  if confirmed unused. *(Pending: old `DiarizationWsHandler` still present but
  no longer wired; remove after owner confirms.)*
- [x] **T052** `/memories/repo/` and `PROJECT_STATE.md` updated with verified
  streaming-path facts and honest metrics.

## Traceability (requirement → task)

| Requirement | Tasks |
|---|---|
| FR1 Ingest | T030, T031 |
| FR2 Independent consumption | T020, T021, T030 |
| FR3 Shared clock | T010, T020, T021 |
| FR4 Threads + controller | T030, T031, T032 |
| FR5 Incremental ASR output | T021, T040 |
| FR6 Unified timeline | T022, T030, T042 |
| FR7 Control protocol | T030, T031 |
| FR8 Accelerated streaming | T010, T040, T042 |
| FR9 JSON export | T041 |
| FR10 Honest metrics | T042, T052 |

| Acceptance | Tasks |
|---|---|
| AC1 timeline both modalities | T042 |
| AC2 real parallelism | T030, T032 |
| AC3 through-WS accelerated | T040, T042 |
| AC4 no dropped tail | T030 |
| AC5 JSON export valid | T041 |
| AC6 no quality regression | T042 |
| AC7 clean build/tests/race-free | T032, T050 |

## Definition of Done
All Phase 1–5 tasks complete; AC1–AC7 met; full suite green under
`-Wall -Wextra`; the exported JSON for `test.mp3` is captured for owner review;
memory updated. Constitution Check in spec/plan still holds.
