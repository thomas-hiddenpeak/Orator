# Spec 004 — Common Time Base + Revisable Comprehensive Timeline

- **Feature**: `004-comprehensive-timeline`
- **Status**: Draft (owner-approved direction)
- **Created**: 2026-06-15
- **Owner**: project owner
- **Constitution**: v1.1.0

> WHAT to change and WHY. The data model and algorithms are in `plan.md`.

---

## 1. Summary

Replace the one-shot, recompute-on-flush comprehensive view with a **native,
stateful comprehensive timeline** that is built incrementally as each pipeline
deposits results and is **revised in place** when a pipeline updates a region it
already reported. Every pipeline places its data on one **common time base**
(absolute sample index), and the correspondence between *who is speaking*
(diarization) and *what is said* (ASR) is determined purely by **time alignment**
on that timeline — diarization only answers "who, when"; it does not attribute
text. Revisions are pushed to the WebSocket consumer so the client always holds
the current best view.

## 1a. Core invariant — the comprehensive layer is a pure time-alignment layer

The comprehensive timeline **never modifies, infers, substitutes, or back-fills
any pipeline's content**. Each pipeline is solely responsible for its own output
content AND its own accurate relative time on the common base. The comprehensive
layer's only job is to guarantee that whatever each pipeline emits can be aligned
in time. Consequences:

- It does **not** split a pipeline's content (e.g. it does not cut ASR text to
  match speaker boundaries — that would be the timeline doing diarization's job).
- It does **not** guess a missing attribution (e.g. if diarization has not
  covered a span, the speaker is honestly "unknown" — it is never borrowed from
  the nearest segment).
- Coverage gaps are represented faithfully on the time axis (the time code is
  continuous; missing pipeline data shows as absent/unknown, not filled in).
- Each pipeline owns its own time resolution: if finer-grained attribution is
  wanted, the pipeline emits finer-grained timed units; the comprehensive layer
  does not synthesize them.

## 2. Background and Problem

- The current comprehensive view ([`OverlapTimelineMerger`](../../src/pipeline/timeline_merger.cc))
  is computed once at flush/end from a full snapshot, attributing each ASR token
  whole to a single speaker (max time-overlap). With coarse ASR segments and
  E2E diarization showing frequent speaker switches, a segment that spans several
  speakers is mis-attributed entirely to one. The defect is the one-shot,
  bake-at-emit attribution — not diarization, which is correct.
- Diarization (E2E sortformer) provides an accurate **who/when** speaker timeline
  independent of ASR. ASR (Spec 003 incremental) provides accurate **what/when**
  text on the same absolute clock. Their correspondence is a **time-alignment**
  problem, solvable on a shared timeline without per-word timestamps.
- The view is produced only at flush/end and never revised, so an early ASR
  result aligned against incomplete diarization is never corrected.

## 3. Goals

- **G1 — Common time base as a first-class contract**: every emitted datum
  (diarization segment, ASR text, endpoint) carries timestamps on one absolute
  time base derived from the cumulative ingested sample count, and the WS
  protocol exposes this explicitly as metadata.
- **G2 — Native stateful comprehensive timeline**: a single in-memory structure
  is updated incrementally as pipelines deposit results; it is not recomputed
  from a full raw snapshot on each emit.
- **G3 — Revision in place**: when a pipeline updates a region it already
  reported (diarization refines a span, ASR revises text), the affected
  comprehensive entries are recomputed and emitted as revisions.
- **G4 — Time-alignment attribution**: "who said what" is derived from time
  overlap between speaker segments and text segments on the common time base;
  diarization does not attribute text. Precondition: accurate per-pipeline
  timing on the common base.
- **G5 — Revisions pushed to WS**: the consumer receives revision messages so it
  can update or overwrite previously shown entries for usability and debugging.
- **G6 — Endpoint as an independent pipeline**: the speech-endpoint detector
  (Silero) runs as a third independent consumer of the shared audio buffer,
  publishing endpoint markers onto the common time base; it does not sit inside
  the ASR worker.

## 4. Non-Goals

- **NG1** Per-word timestamps / forced alignment (model does not emit them).
- **NG2** Changing the diarization or ASR engines' numerics.
- **NG3** GPU scheduling (Spec 002).
- **NG4** Persisting the timeline across sessions.

## 5. Functional Requirements

- **FR1 — Common time base metadata**: every result message (`diar`, `asr`,
  `endpoint`, `timeline`, and revisions) SHALL carry absolute start/end times in
  seconds on the shared base, and the protocol SHALL state the base (sample rate
  + absolute sample origin) so a consumer can align any pipeline's output.
- **FR2 — Native comprehensive timeline**: the controller SHALL maintain one
  stateful comprehensive timeline updated by `UpsertSpeaker(start,end,speaker,
  conf)`, `UpsertText(id,start,end,text)`, and `MarkEndpoint(time)`; it SHALL NOT
  rebuild the view from a full raw snapshot on each update.
- **FR3 — Time-alignment attribution**: the comprehensive view SHALL attribute
  text to speakers by time overlap on the common base; diarization SHALL NOT map
  text. A text segment overlapping multiple speaker segments SHALL be presented
  per the plan's projection rule (no forced single-speaker bake).
- **FR4 — Revision events**: when an upsert changes a region already emitted, the
  controller SHALL emit a revision message identifying the changed time range and
  the new entries; committed-and-unchanged entries SHALL NOT be re-emitted.
- **FR5 — WS revision push**: revision messages SHALL be sent to the WS consumer
  as they occur (subject to the existing emit serialization).
- **FR6 — Endpoint pipeline**: the endpoint detector SHALL be a third buffer
  consumer on its own thread, depositing endpoint markers onto the timeline; the
  ASR worker SHALL consume the continuous audio independently (its internal reset
  may use these shared endpoints or its own).
- **FR7 — Final timeline**: on flush/end the server SHALL still send one
  `{"type":"timeline",...}` document (tracks + comprehensive) consistent with the
  incremental revisions already sent.

## 6. Acceptance Criteria

- **AC1** Streaming `test.mp3` produces a final timeline whose comprehensive view,
  on a multi-speaker span, attributes text to the time-overlapping speaker(s)
  correctly (a span where diarization shows a speaker change is not wholly
  mis-attributed to one speaker as before). (G4, FR3)
- **AC2** The comprehensive timeline is built incrementally: with instrumentation,
  an update touches only the affected region, not a full rebuild. (G2, FR2)
- **AC3** A diarization or ASR update to an already-reported region produces a
  revision message with the changed time range; unchanged entries are not
  re-emitted. (G3, FR4)
- **AC4** Revision messages are delivered over the real WS path and a client can
  reconstruct the same final comprehensive view by applying them in order. (G5,
  FR5, FR7)
- **AC5** The endpoint detector runs as a third independent consumer/thread and
  emits endpoint markers on the common time base; removing it does not stall the
  other two pipelines. (G6, FR6)
- **AC6** Every result message carries common-time-base metadata sufficient to
  align it without reference to other messages. (G1, FR1)
- **AC7** Build clean under `-Wall -Wextra`; tests pass; threaded path race-free;
  the final timeline remains schema-compatible (additive only). (Constitution V)

## 7. Constitution Check

- **Art. I (no deps)**: existing CUDA + std lib only.
- **Art. II (accuracy)**: ASR/diarization numerics unchanged; this is a view layer.
- **Art. III (independent pipelines on a comprehensive timeline)**: strengthened —
  three independent pipelines, one revisable comprehensive timeline.
- **Art. IV (streaming validation)**: validated through the real WS path.
- **Art. V (quality)**: native structure, documented state, race-free.
- **Art. VI (terminology)**: standard terms.
- **Art. VII (SDD)**: spec → plan → tasks before implementation.

## 8. Open Questions and Risks

- **R1** Projection of a text segment overlapping multiple speakers: present
  whole under the max-overlap speaker (revisable) vs. split by time. Decided in
  `plan.md`; no per-word times, so a time split is approximate. Default: attribute
  to max-overlap speaker, revisable as diarization refines (endpoint-based text
  is mostly single-speaker).
- **R2** Revision volume: frequent diarization refinement could cause many
  revisions. Mitigate by emitting a revision only when the *attributed result*
  changes, not on every raw update.
- **R3** Ordering: diarization usually leads ASR; the timeline must accept
  out-of-order updates and converge. Verified by AC4.
- **R4** Endpoint pipeline vs. ASR internal reset: the shared endpoint stream and
  the ASR worker's own reset must not double-segment. Decided in `plan.md`.
