# Spec 004 — Common Time Base + Revisable Comprehensive Timeline

- **Feature**: `004-comprehensive-timeline`
- **Status**: Implemented. Comprehensive-timeline core + revisions + common time
  base (3159b75, 673f95d), the GPU VAD endpoint pipeline (Phase 5), and the
  Phase 6 conformance pass (uniform per-pipeline WS output with `source` meta;
  the VAD pipeline publishes speech SEGMENTS as a `vad` track; the comprehensive
  VIEW splits text at DIARIZATION boundaries, not by ASR's coarse segmentation)
  are done and validated on the real WS path.
- **Created**: 2026-06-15
- **Owner**: project owner
- **Constitution**: v1.2.1

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
  (diarization segment, ASR text, VAD segment) carries timestamps on one absolute
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
- **G6 — VAD as an independent pipeline**: the VAD detector (Silero) runs as a
  third independent consumer of the shared audio buffer, publishing speech
  segments onto the common time base; it does not sit inside
  the ASR worker.

## 4. Non-Goals

- **NG1** Per-word timestamps / forced alignment (model does not emit them).
- **NG2** Changing the diarization or ASR engines' numerics.
- **NG3** GPU scheduling (Spec 002).
- **NG4** Persisting the timeline across sessions.

## 5. Functional Requirements

- **FR1 — Common time base metadata**: every result message (`diar`, `asr`,
  `vad`, `timeline`, and revisions) SHALL carry absolute start/end times in
  seconds on the shared base, and the protocol SHALL state the base (sample rate
  + absolute sample origin) so a consumer can align any pipeline's output.
- **FR2 — Native comprehensive timeline**: the controller SHALL maintain one
  stateful comprehensive timeline updated by `ReplaceSpeakers(segs)`,
  `UpsertText(id,start,end,text)`, and `AddVad(start,end)`; it SHALL NOT rebuild
  the view from a full raw snapshot on each update.
- **FR3 — Diarization-driven view split**: the comprehensive view's boundaries
  SHALL come from the DIARIZATION track. Each ASR text segment SHALL be placed
  onto the diarization speaker turns it overlaps, and a text segment crossing a
  diarization boundary SHALL be SPLIT at that boundary (its characters allocated
  to each turn proportionally by time). The view SHALL NOT re-segment text by
  ASR's own coarse segmentation. Diarization SHALL NOT map text; where no
  diarization covers a span the speaker is honestly "unknown" (never borrowed).
- **FR4 — Revision events**: when an upsert changes a region already emitted, the
  controller SHALL emit a revision message identifying the changed time range and
  the new entries; committed-and-unchanged entries SHALL NOT be re-emitted.
- **FR5 — WS revision push**: revision messages SHALL be sent to the WS consumer
  as they occur (subject to the existing emit serialization).
- **FR6 — VAD pipeline**: the VAD detector SHALL be a third buffer consumer on
  its own thread, depositing speech segments onto the timeline; the ASR worker
  SHALL consume the continuous audio independently (its internal reset may use
  these shared VAD signals or its own). Its per-window detection compute
  (STFT, the convolutional encoder, the LSTM step) SHALL run on the GPU in
  batches, not on the CPU on the streaming thread; a buffered read SHALL be
  processed as one batched GPU pass over all ready windows, so the detector never
  spends a long single-CPU-core stretch with the GPU idle.
- **FR7 — Final timeline**: on flush/end the server SHALL still send one
  `{"type":"timeline",...}` document (tracks + comprehensive) consistent with the
  incremental revisions already sent. The VAD speech segments SHALL be a
  serialized part of this document (an additive `vad` track), not a write-only
  in-memory vector. The VAD track is a PURE DATA track: it does not modify,
  split, or re-segment the ASR or diarization tracks (it never drives their
  boundaries — that would be one pipeline doing another's job).
- **FR8 — VAD numeric gate**: the GPU detector's per-window speech
  probability SHALL be validated against the prior CPU implementation (the
  reference of record for the VAD detector; a PyTorch silero-vad hub dump is
  not reproducible in this offline environment) within a recorded tolerance,
  before it is considered done (Constitution Art. II — the detector previously had
  no numeric gate at all).
- **FR9 — Dead-code removal**: code made dead by this work or found dead in its
  vicinity SHALL be removed in the same change: the write-only
  `ComprehensiveTimeline::endpoints()` accessor (replaced by the serialized
  track), the unused `AsrWorker` eager Silero load when not used, the unused stub
  models (`StubAsr`/`StubDiarizer`/`StubEmbedder`) and their registrations, and
  the never-launched CUDA kernels (`RelAttnKernel`, `FlattenLinearKernel`,
  `GeluKernel`/`Conv2dKernel` in `asr_audio_tower.cu`). Stale doc comments
  (e.g. `asr_worker.h` "energy VAD") SHALL be corrected.
- **FR10 — Uniform per-pipeline WS output**: every registered pipeline SHALL
  publish its OWN output as a WS message carrying its type, `source` meta, and
  time codes: diarization `{type:diar, source, segments:[{start,end,speaker,
  confidence}]}`, ASR `{type:asr|asr_partial, source, start, end, text}`, VAD
  `{type:vad, source, start, end}`. Revision messages SHALL carry the `source`
  that triggered them and each entry's `text_id`.
- **FR11 — VAD pipeline (segments, not endpoints)**: the third pipeline is the
  VAD pipeline (named `vad`, not `endpoint`). It publishes SPEECH SEGMENTS
  `[start,end)` (voice-activity regions), its proper data, as a `vad` track in
  the timeline document with `source`/`compute_sec`/`real_time_factor` meta like
  the other tracks. It is a pure data track: it does not modify or re-segment the
  diar/asr tracks, nor drive the view's boundaries.
- **FR12 — Timeline vs view**: the comprehensive TIMELINE is a pure container of
  the three tracks (each pipeline's data + meta + time codes). The comprehensive
  VIEW is a DERIVED product of the timeline for human browsing (diar-driven
  split, FR3); it is the interactive form of the timeline and SHALL carry the
  same characteristics (common time base, no invented content).

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
- **AC5** The VAD detector runs as a third independent consumer/thread and
  emits `vad` speech segments on the common time base; removing it does not stall the
  other two pipelines. (G6, FR6)
- **AC6** Every result message carries common-time-base metadata sufficient to
  align it without reference to other messages. (G1, FR1)
- **AC7** Build clean under `-Wall -Wextra`; tests pass; threaded path race-free;
  the final timeline remains schema-compatible (additive only). (Constitution V)
- **AC8** The VAD detector's compute runs on the GPU in batches: streaming
  the full 1-hour `test.mp3` through the real WS path, ASR covers the whole hour
  with no multi-minute single-CPU-core stall, and the GPU-busy fraction shows no
  long idle stretch attributable to the detector. (FR6)
- **AC9** The final `{"type":"timeline"}` document carries the `vad` speech
  segments as a serialized track (not a write-only vector), consistent with the
  `{"type":"vad"}` messages already sent; a consumer can read VAD segments from
  the final document. (FR7)
- **AC10** The GPU detector's per-window probability matches the PyTorch
  silero-vad reference and the prior CPU implementation within the recorded
  tolerance; a `test_vad` numeric gate is added and passes. (FR8)
- **AC11** No dead code remains from FR9: the removed accessor, stubs, and unused
  kernels are gone; the build emits no `#177-D declared-but-never-referenced`
  warning for the listed kernels; stale comments are corrected. (FR9)
- **AC12** Every pipeline emits its own WS message with `source` meta + time
  codes: streaming `test.mp3` produces `diar`, `asr`, `vad`, and `revision`
  (source-tagged) messages; the timeline carries diarization, asr, and vad
  tracks each with `source`. (FR10, FR11)
- **AC13** The comprehensive view splits text at diarization boundaries: a text
  segment crossing a diarization speaker change appears as multiple view entries
  whose spans match the diarization turns and whose texts concatenate back to the
  original; it is not one coarse ASR-segment entry. (FR3, FR12)

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
