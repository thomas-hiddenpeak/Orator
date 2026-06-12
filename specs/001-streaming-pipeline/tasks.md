# Tasks 001 — Real-Time Streaming Dual Pipeline

- **Feature**: `001-streaming-pipeline`
- **Spec**: [spec.md](spec.md) · **Plan**: [plan.md](plan.md)
- **Status**: Draft (awaiting review) — implementation begins after sign-off
- **Constitution**: v1.0.0

> Ordered, independently verifiable steps. Each task states its outcome and how
> it is verified. `[P]` = parallelizable with its siblings. Do not begin
> implementation tasks until the owner approves the spec + plan.

---

## Phase 0 — Review gate
- [ ] **T000** Owner reviews and approves `constitution.md`, `spec.md`,
  `plan.md`. No implementation task starts before this. *(Verify: explicit
  approval recorded.)*

## Phase 1 — Shared buffer foundation
- [ ] **T010** Add `SharedAudioBuffer` (`include/pipeline/shared_audio_buffer.h`,
  `src/pipeline/shared_audio_buffer.cc`): thread-safe append + per-cursor read,
  `mutex_`/`cv_`, `eos_`, low-water-mark prefix drop. *(Verify: builds clean;
  documented guard per field.)*
- [ ] **T011** Unit test `test/test_shared_buffer.cc`: producer thread + two
  consumer threads with distinct cursors; assert each consumer sees every sample
  exactly once, in order, and the buffer bounds memory to the lagging cursor.
  *(Verify: ctest passes; run under racecheck where available.)*

## Phase 2 — Worker extraction (no behavior change yet)
- [ ] **T020** Extract the diarization streaming loop into a `DiarizationWorker`
  that consumes a `SharedAudioBuffer` cursor and accumulates frames. *(Verify:
  produces identical frames to the current inline path on a fixed input.)*
- [ ] **T021** Extract the ASR endpoint+transcribe loop into an `AsrWorker` that
  consumes a `SharedAudioBuffer` cursor and emits utterances. *(Verify:
  identical utterances to the current inline path on a fixed input.)*
- [ ] **T022** Make `Timeline` accumulation mutex-guarded; both workers append
  through it. *(Verify: documented lock; no field written without it.)*

## Phase 3 — Threaded controller
- [ ] **T030** Refactor `AuditoryStream` into the controller: own the buffer,
  spawn diar + ASR worker threads on `Start()`, route `PushAudio` to the
  producer side, implement `flush` (barrier+drain+serialize) and `end`
  (eos+join+serialize), join all threads in the destructor. *(Verify: AC2 —
  wall < sum of pipeline computes; AC4 — no dropped tail.)*
- [ ] **T031** Update `AuditoryWsHandler` for the threaded controller
  (control-message → thread coordination). *(Verify: through-socket smoke test
  returns a unified timeline.)*
- [ ] **T032** Concurrency verification: stress run at high input rate; run
  under `compute-sanitizer --tool racecheck` / sanitizer where available;
  assert deterministic output across repeated runs. *(Verify: AC7 — no race.)*

## Phase 4 — Real streaming test client + JSON export
- [ ] **T040** Evolve `tools/ws_stream_client.py` (stdlib only): decode/accept
  PCM, push frames **through the socket** at a configurable accelerated rate,
  with a dedicated reader thread capturing ALL frames (fixes the dropped-event
  bug). *(Verify: captures incremental `asr` events + final `timeline`.)*
- [ ] **T041** JSON export (FR9): write the full event log + final timeline +
  meta (rate multiple, wall, per-pipeline RTF) to a file. *(Verify: AC5 — valid
  JSON, both modalities present.)*
- [ ] **T042** End-to-end validation run on `test.mp3` through the WebSocket at
  an accelerated multiple; capture the JSON and report honest metrics. *(Verify:
  AC1, AC3, AC6 — both arrays non-empty, monotonic clock, no quality
  regression; numbers from the streaming path only.)*

## Phase 5 — Test integration & cleanup
- [ ] **T050** Register the new tests in `test/CMakeLists.txt`; ensure the full
  suite passes. *(Verify: AC7 — clean `-Wall -Wextra`, all ctests pass.)*
- [ ] **T051** Remove or clearly retire the superseded inline path and the old
  diarization-only WS handler if no longer used; no dead code left behind.
  *(Verify: Constitution V.3 — no dead/commented-out code.)*
- [ ] **T052** Update `/memories/repo/` with the verified streaming-path facts
  and final honest metrics. *(Verify: memory matches reality.)*

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
