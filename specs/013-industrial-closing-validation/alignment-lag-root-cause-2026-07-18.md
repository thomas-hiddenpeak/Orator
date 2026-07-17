# Delayed Alignment Root-Cause Record

**Date:** 2026-07-18
**Status:** Investigation complete; FR16ABN implemented and retained by frozen replay
**Scope:** Source-free common-clock evidence around the `ref-0090` residual

## Governance

Tools parsed and displayed typed production tracks. They did not compare a
candidate with the reference, assign correctness, aggregate accuracy, select a
parameter, or issue a verdict. The previously completed full contextual review
identified the business residual; this record traces only its raw evidence and
defines an implementation hypothesis. Audible boundary review remains open in
T102.

## Frozen Evidence

Both promoted full runs expose the same local producer topology. Run B is cited
below from:

`/tmp/orator-spec013/release-1a475e6-t106/full-b-frozen-registry-direct-end-ws.json`

- ASR `text_id=43` spans `557.10-581.10` on the common session clock.
- The preceding positive alignment unit ends at `567.42`; the short response's
  first positive unit starts at `569.26`, leaving a `1.84 s` internal alignment
  hole. Its positive units occupy only `569.26-569.42`, below the checked-in
  `0.40 s` embedding floor.
- Activity carries the surrounding `spk_3` through `567.68`, carries `spk_2`
  through `568.24`, then returns to `spk_3` at `569.20`.
- Primary independently gives `spk_2` a sustained `567.44-568.24` run inside
  the alignment hole, then returns to `spk_3` at `569.20` after local churn.
- Typed VAD ends at `568.124` and resumes at `568.388`, exposing a `0.264 s`
  speech boundary inside the same alignment hole.
- The delayed aligned response begins `0.06 s` after the activity and primary
  return to `spk_3`, within the checked-in `0.08 s` alignment tolerance.
- The delayed units have no independent embedding. The containing
  `568.388-579.612` VAD query and the `569.26-570.78` business query include the
  following sustained `spk_3` speech and therefore cannot isolate the short
  response.
- The terminal business view consequently assigns the complete short group at
  `569.26-569.42` to `spk_3`. This follows the evidence at its supplied time;
  it is not a session-wide identity permutation or common-clock drift.

Run A and Run B have the same activity, primary, VAD, alignment, and source
topology at this location. Registry state does not explain the defect.

## Root Cause

The forced aligner has displaced a subminimum clause group past an intervening
native speaker island and onto the return onset of the surrounding speaker.
The business projector then correctly applies time-local evidence to a
misplaced lexical range. Phrase-scale or VAD-scale TitaNet evidence cannot
repair the range because every embeddable query includes following speech.

FR16ABM is unrelated: it protects a handoff already present in the immutable
base source projection. Here the base projection never sees the intervening
identity at the delayed source position.

## Bounded Implementation Hypothesis

FR16ABN admits only a punctuation-delimited, source-contiguous, subminimum
aligned group in an A-B-A topology. The same incumbent must own the neighboring
source and the delayed return; one different identity must have sustained
overlapping activity/primary evidence inside the preceding alignment hole; and
one configured typed VAD gap must divide the native island from the delayed
return. Existing TOML punctuation, pause, tolerance, embedding-duration, and
aligned-unit-count values are reused unchanged.

The implemented candidate changes only speaker ownership for that exact source
group. It does not move forced-alignment times or mutate ASR, VAD, activity,
primary, or voiceprint tracks. Three byte-stable replays per promoted A/B track
change only that speaker sequence. Complete forward/reverse contextual review
retains the candidate for real-WebSocket promotion; see
`delayed-alignment-clause-review-2026-07-18.md`. This does not claim an audible
boundary correction or a new production-run result.
