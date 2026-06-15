# Tasks 004 — Common Time Base + Revisable Comprehensive Timeline

- **Feature**: `004-comprehensive-timeline`
- **Spec**: [spec.md](spec.md) · **Plan**: [plan.md](plan.md)
- **Status**: In progress (owner-approved direction)
- **Constitution**: v1.1.0

> Ordered, independently verifiable steps. Each phase builds + tests before the
> next. Schema changes are additive only.

---

## Phase 1 — Native comprehensive timeline core (the foundation)
- [ ] **T010** Add `ComprehensiveTimeline` (`include/pipeline/comprehensive_timeline.h`,
  `src/pipeline/comprehensive_timeline.cc`): state per plan §1; `UpsertSpeaker`,
  `UpsertText`, `MarkEndpoint`, `Snapshot`, returning a `Revision` set. Time-
  alignment attribution (plan §2), incremental re-projection of only the affected
  region (plan §3). No WS / no threads yet. *(Verify: compiles clean.)*
- [ ] **T011** `test_comprehensive_timeline`: out-of-order upserts project
  correctly; an update touches only the affected region; a changed attribution
  yields one revision with the right dirty range; an unchanged update yields none;
  a multi-speaker text segment attributes to max-overlap and flips on a later
  speaker update. *(Verify: AC1, AC2, AC3 at unit level; ctest green.)*

## Phase 2 — Controller wiring (replace one-shot merger)
- [ ] **T020** `AuditoryStream` owns a `ComprehensiveTimeline`; diarization
  finalized segments call `UpsertSpeaker`, ASR committed segments call
  `UpsertText(id)`. `Serialize()` reads the native `view_` for the
  `comprehensive` array (drop the one-shot `OverlapTimelineMerger` call there;
  keep the merger class for now). *(Verify: final timeline unchanged-or-better on
  test.mp3; tracks intact.)*
- [ ] **T021** Push revisions: returned revisions are emitted via `EmitLocked` as
  `{"type":"revision",...}`. *(Verify: revisions appear on the WS path.)*

## Phase 3 — Common time base metadata + endpoint pipeline
- [ ] **T030** WS meta: on session open send `{"type":"meta","sample_rate",
  "time_base","origin_sample"}`; document that all messages carry common-base
  start/end. *(Verify: AC6; meta present; existing messages carry times.)*
- [ ] **T031** Endpoint pipeline: a third `buffer_.AddConsumer()` + thread runs
  `AsrSileroVad` as an endpoint detector (continuous, no trim), calls
  `MarkEndpoint`, emits `{"type":"endpoint","time"}`. ASR worker unchanged.
  *(Verify: AC5; third thread emits markers; disabling it does not stall others.)*

## Phase 4 — Validation
- [ ] **T040** Multi-speaker attribution on test.mp3 (real diarizer + incremental
  ASR): a speaker-change span is attributed correctly, not wholly to one speaker.
  *(Verify: AC1 end-to-end.)*
- [ ] **T041** WS revision replay: stream test.mp3, collect revisions, apply in
  order, reproduce the final comprehensive view. *(Verify: AC4.)*
- [ ] **T050** Full build + `ctest` green under `-Wall -Wextra`; threaded path
  race-checked; schema additive-only. *(Verify: AC7.)*
- [ ] **T051** Update `/memories/repo/` + `PROJECT_STATE.md`; commit.

## Traceability

| Requirement | Tasks |
|---|---|
| FR1 common-base metadata | T030 |
| FR2 native timeline | T010, T020 |
| FR3 time-alignment attribution | T010, T040 |
| FR4 revision events | T010, T021 |
| FR5 WS revision push | T021, T041 |
| FR6 endpoint pipeline | T031 |
| FR7 final timeline | T020 |

| Acceptance | Tasks |
|---|---|
| AC1 multi-speaker correct | T011, T040 |
| AC2 incremental | T011 |
| AC3 revision on change | T011, T021 |
| AC4 WS replay | T041 |
| AC5 endpoint pipeline | T031 |
| AC6 common-base meta | T030 |
| AC7 build/tests/schema | T050 |

## Definition of Done
Native stateful comprehensive timeline with time-alignment attribution and
in-place revision; revisions pushed to WS; common-time-base metadata exposed;
endpoint detector as an independent third pipeline; multi-speaker attribution
correct on test.mp3; build + tests green; schema additive; docs updated; commit.
