# Tasks 004 — Common Time Base + Revisable Comprehensive Timeline

- **Feature**: `004-comprehensive-timeline`
- **Spec**: [spec.md](spec.md) · **Plan**: [plan.md](plan.md)
- **Status**: Partially implemented. Phases 1–4 done + committed (3159b75 step 1,
  673f95d step 2). Phase 3 T031 (endpoint pipeline) is only PARTLY done: the third
  thread runs and emits `{"type":"endpoint"}`, but its detector is CPU-only and
  `MarkEndpoint` writes a vector that is never read/serialized (FR6/FR7 unmet).
  Phase 5 completes the endpoint pipeline (GPU detector + serialized track) and
  folds in the related dead-code cleanup.
- **Constitution**: v1.2.1

> Ordered, independently verifiable steps. Each phase builds + tests before the
> next. Schema changes are additive only.

---

## Phase 1 — Native comprehensive timeline core (the foundation)
- [x] **T010** Add `ComprehensiveTimeline` (`include/pipeline/comprehensive_timeline.h`,
  `src/pipeline/comprehensive_timeline.cc`): state per plan §1; `UpsertSpeaker`,
  `UpsertText`, `MarkEndpoint`, `Snapshot`, returning a `Revision` set. Time-
  alignment attribution (plan §2), incremental re-projection of only the affected
  region (plan §3). No WS / no threads yet. *(Done 3159b75; compiles clean.)*
- [x] **T011** `test_comprehensive_timeline`: out-of-order upserts project
  correctly; an update touches only the affected region; a changed attribution
  yields one revision with the right dirty range; an unchanged update yields none;
  a multi-speaker text segment attributes to max-overlap and flips on a later
  speaker update. *(Done 3159b75; ctest green.)*

## Phase 2 — Controller wiring (replace one-shot merger)
- [x] **T020** `AuditoryStream` owns a `ComprehensiveTimeline`; diarization
  finalized segments call `UpsertSpeaker`, ASR committed segments call
  `UpsertText(id)`. `Serialize()` reads the native `view_` for the
  `comprehensive` array. *(Done 673f95d. The one-shot `OverlapTimelineMerger` was
  later fully removed in 2414128.)*
- [x] **T021** Push revisions: returned revisions are emitted via `EmitLocked` as
  `{"type":"revision",...}`. *(Done 673f95d; revisions appear on the WS path.)*

## Phase 3 — Common time base metadata + endpoint pipeline
- [x] **T030** WS meta: on session open send the common-base declaration
  (`sample_rate`, `time_base`, `origin_sample`) in the `ready` message; all
  messages carry common-base start/end. *(Done 673f95d; AC6.)*
- [~] **T031** Endpoint pipeline: a third `buffer_.AddConsumer()` + thread runs
  `AsrSileroVad` as an endpoint detector (continuous, no trim), calls
  `MarkEndpoint`, emits `{"type":"endpoint","time"}`. ASR worker unchanged.
  *(PARTIAL: third thread + emission done (673f95d) and disabling it does not
  stall others (AC5 met). NOT done: the detector is CPU-only (violates FR6 GPU
  compute; caused the full-hour single-CPU-core stall), and `MarkEndpoint` writes
  a vector that is never serialized (FR7 unmet — endpoints are write-only). Completed in Phase 5.)*

## Phase 4 — Validation
- [x] **T040** Multi-speaker attribution on test.mp3 (real diarizer + incremental
  ASR): a speaker-change span is attributed correctly, not wholly to one speaker.
  *(Done; AC1 end-to-end — head "unknown", spk0/1/2 present.)*
- [x] **T041** WS revision replay: stream test.mp3, collect revisions, apply in
  order, reproduce the final comprehensive view. *(Done; AC4 via ws_raw_capture.)*
- [x] **T050** Full build + `ctest` green under `-Wall -Wextra`; threaded path
  race-checked; schema additive-only. *(Done; AC7.)*
- [x] **T051** Update `/memories/repo/` + `PROJECT_STATE.md`; commit. *(Done.)*

## Phase 5 — Complete the endpoint pipeline (GPU detector + serialized track) + cleanup
> Closes the FR6/FR7 gap left by T031 and folds in FR8/FR9. Each task builds +
> tests before the next; the final gate is the real-WS full-hour run.
- [ ] **T060** PyTorch silero-vad oracle: a `tools/` dump of per-window speech
  probabilities over a fixed audio slice (and the prior CPU implementation's
  probabilities), saved under `models/reference/` for a numeric gate. *(Verify:
  oracle file exists; CPU vs oracle within tolerance recorded — establishes the
  baseline the GPU port must match.)* (FR8)
- [ ] **T061** GPU endpoint detector: port the per-window compute (STFT via the
  fixed conv basis, the 4-layer conv encoder + ReLU, the LSTM cell step, the
  linear + sigmoid) to CUDA, driven in BATCHES — one buffered read is processed as
  a single batched GPU pass over all ready windows; the LSTM recurrence is one
  scan over the batch. Runs under `gpu::DeviceLock()`. *(Verify: `test_vad` numeric
  gate — GPU per-window probability matches oracle + CPU within tolerance; AC10.)*
  (FR6, FR8)
- [ ] **T062** Wire the GPU detector into the endpoint thread, replacing the
  CPU detector; remove the CPU per-window path from the streaming thread. *(Verify:
  endpoint markers still emitted on the common base; thread independent.)* (FR6)
- [ ] **T063** Serialize endpoints into the timeline document: add an additive
  endpoint marker track to `Snapshot()`/`Serialize()` (pure marker track — it does
  NOT alter the ASR/diar tracks). Replace the write-only `endpoints_` accessor.
  *(Verify: final `{"type":"timeline"}` carries endpoints; AC9; schema additive.)*
  (FR7)
- [ ] **T064** Dead-code cleanup (FR9): remove `StubAsr`/`StubDiarizer`/
  `StubEmbedder` + their registrations; remove never-launched kernels
  (`RelAttnKernel`, `FlattenLinearKernel`, `GeluKernel`/`Conv2dKernel` in
  `asr_audio_tower.cu`); drop the unused eager Silero load in `AsrWorker`; fix the
  stale `asr_worker.h` "energy VAD" comment and any others found. *(Verify: build
  has no `#177-D` for the listed kernels; ctest green; AC11.)*
- [ ] **T065** Real-WS full-hour validation: stream the full 3615 s `test.mp3`
  through the production `orator_ws` path at the default config; confirm ASR
  covers the whole hour with NO multi-minute single-CPU-core stall, endpoints are
  in the final timeline, reconcile clean, CER unchanged. *(Verify: AC8, AC9
  end-to-end on the real transport — not the bypass harness.)*
- [ ] **T066** Update `tasks.md` checkboxes, spec status → Implemented, `/memories/`
  and `PROJECT_STATE.md` with commit refs. *(Verify: Constitution Art. VIII.)*

## Traceability

| Requirement | Tasks |
|---|---|
| FR1 common-base metadata | T030 |
| FR2 native timeline | T010, T020 |
| FR3 time-alignment attribution | T010, T040 |
| FR4 revision events | T010, T021 |
| FR5 WS revision push | T021, T041 |
| FR6 endpoint pipeline | T031, T061, T062 |
| FR7 final timeline | T020, T063 |
| FR8 endpoint numeric gate | T060, T061 |
| FR9 dead-code removal | T064 |

| Acceptance | Tasks |
|---|---|
| AC1 multi-speaker correct | T011, T040 |
| AC2 incremental | T011 |
| AC3 revision on change | T011, T021 |
| AC4 WS replay | T041 |
| AC5 endpoint pipeline | T031 |
| AC6 common-base meta | T030 |
| AC7 build/tests/schema | T050 |
| AC8 GPU detector, no stall | T061, T065 |
| AC9 endpoints in final timeline | T063, T065 |
| AC10 GPU vs oracle numeric gate | T060, T061 |
| AC11 dead code removed | T064 |

## Definition of Done
Native stateful comprehensive timeline with time-alignment attribution and
in-place revision; revisions pushed to WS; common-time-base metadata exposed;
endpoint detector as an independent third pipeline; multi-speaker attribution
correct on test.mp3; build + tests green; schema additive; docs updated; commit.
