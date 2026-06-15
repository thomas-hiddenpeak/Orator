# Plan 004 — Common Time Base + Revisable Comprehensive Timeline

- **Feature**: `004-comprehensive-timeline`
- **Spec**: [spec.md](spec.md)
- **Status**: Draft
- **Constitution**: v1.1.0

> HOW to satisfy [spec.md](spec.md). Standard terminology.

---

## 1. Data model (native, stateful)

A new `ComprehensiveTimeline` (pipeline layer) owns the comprehensive view as a
living structure on the common time base (seconds; origin = absolute sample 0).

State:
- `speakers_`: time-ordered speaker segments `{start, end, speaker, conf}`
  (who/when), from diarization. Overlaps allowed (E2E diarization can overlap).
- `texts_`: text segments `{id, start, end, text}` (what/when), from ASR, keyed
  by a stable `id` so a later update to the same utterance revises in place.
- `endpoints_`: sorted endpoint times (markers; informational).
- `view_`: the derived comprehensive entries `{start, end, speaker, text}`,
  ordered by time — the projection kept in sync with the inputs.
- `committed_until_`: time up to which `view_` has been emitted to the client
  (so revisions only cover changed regions).

## 2. Attribution by time alignment (G4, FR3)

Diarization gives who/when; ASR gives what/when; correspondence is time overlap.

`Project(text_seg)`:
- Find the speaker segment with the greatest time overlap with `text_seg`.
- Emit one comprehensive entry `{text_seg.start, text_seg.end, that_speaker,
  text_seg.text}`. If no overlap, speaker = "unknown" (nearest by midpoint, as
  today) until diarization covers it (then a revision fixes it).
- Consecutive entries with the same speaker are coalesced for the snapshot view.

Rationale: ASR text segments are produced at speech endpoints (Spec 003), so they
are mostly single-speaker; attribution is by time and is **revisable**, so an
early attribution against incomplete diarization is corrected when diarization
covers the span. No per-word timestamps are needed (NG1). A text segment that
genuinely spans multiple speakers is attributed to its max-overlap speaker
(default, R1); a future time-split projection can replace this without changing
the contract.

## 3. Incremental update + revision (G2, G3, FR2, FR4)

Each upsert returns the set of revisions to push.

`UpsertSpeaker(start,end,spk,conf)`:
1. Insert/merge into `speakers_`.
2. Affected range = `[start,end)`. Re-project only the `texts_` overlapping it.
3. For each re-projected text entry whose attributed speaker changed vs the
   stored `view_` entry, update `view_` and add it to the revision set.

`UpsertText(id,start,end,text)`:
1. Insert or replace the `texts_` entry with this `id`.
2. Re-project that one segment; if its `view_` entry is new or changed, add it to
   the revision set.

`MarkEndpoint(t)`: append to `endpoints_` (no view change by itself).

A revision is emitted only when the **attributed result** changes (not on every
raw update), bounding revision volume (R2). Out-of-order updates converge because
each upsert re-projects the affected region from current state (R3).

## 4. Wiring (controller)

`AuditoryStream`:
- Replace the one-shot `OverlapTimelineMerger` call in `Serialize()` with a
  snapshot of `ComprehensiveTimeline::view_`. Keep the `tracks` array as today
  (raw per-pipeline entries) plus the now-native `comprehensive`.
- Diarization worker → on each finalized speaker segment, call `UpsertSpeaker`
  and push returned revisions via the existing `EmitLocked`.
- ASR worker → on each committed text segment, call `UpsertText(id=...)` and push
  revisions. (The worker already emits `{"type":"asr",...}`; the comprehensive
  revision is additional.)
- **Endpoint pipeline (G6, FR6)**: a third `buffer_.AddConsumer()` + thread runs
  `AsrSileroVad` purely as an endpoint detector (continuous audio, no trim),
  calling `MarkEndpoint` and emitting `{"type":"endpoint","time":...}`. The ASR
  worker keeps its own reset for now (R4: the shared endpoint stream is
  informational; a later step can drive ASR reset from it to remove duplication).

## 5. WS protocol additions (G1, G5, FR1, FR5)

Additive only (schema stays compatible, AC7):
- A session-open metadata field `{"type":"meta","sample_rate":R,"time_base":
  "absolute_samples","origin_sample":0}` so a consumer knows the common base.
- `{"type":"endpoint","time":T}` markers.
- `{"type":"revision","dirty_start":S,"dirty_end":E,"entries":[{start,end,speaker,
  text}...]}` — replace the consumer's comprehensive entries in `[S,E)` with
  `entries`.
- All existing messages already carry `start`/`end` on the common base (FR1 met
  for them); confirm and document.

## 6. Validation

- **Unit (AC2, AC3)**: `test_comprehensive_timeline` — drive `UpsertSpeaker` /
  `UpsertText` out of order; assert the projection is correct, an update touches
  only the affected region, and a changed attribution yields one revision with
  the right dirty range; an unchanged update yields none.
- **Multi-speaker (AC1)**: a scripted case where a text segment overlaps two
  speaker segments asserts attribution to the max-overlap speaker, and a
  later-arriving speaker update flips it with a revision.
- **WS (AC4)**: stream `test.mp3`; collect revisions; a client applying them in
  order reproduces the final comprehensive view.
- **Endpoint (AC5)**: confirm the third thread emits endpoint markers and the
  other two pipelines complete if it is disabled.
- **Build/tests (AC7)**: `ctest` green, `-Wall -Wextra` clean, race-free.

## 7. Risks and Mitigations

- **R1 multi-speaker projection** → max-overlap + revisable default; time-split
  deferred (needs no word ts to swap in later).
- **R2 revision volume** → emit only when attributed result changes.
- **R3 out-of-order** → re-project affected region from current state each upsert.
- **R4 endpoint vs ASR reset** → endpoint stream informational first; unify later.

## 8. Out of Scope

Per-word timestamps, engine numerics, GPU scheduling, cross-session persistence.
