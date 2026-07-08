# Plan: Evidence-First Comprehensive Timeline Fusion

## 1. Data flow

1. Enable `[align]` in `orator.toml` so the runtime emits forced-alignment units
   for every finalized ASR segment.
2. Run the unified WebSocket client against `test.mp3` and store the resulting
   JSON under `/tmp/`.
3. Treat the captured tracks as immutable evidence:
   - `timeline.tracks[kind=asr]`
   - `timeline.tracks[kind=diarization]`
   - `timeline.tracks[kind=vad]`
   - `timeline.tracks[kind=align]`
   - `timeline.comprehensive` as the current baseline view
4. Run an offline fusion/audit tool over the evidence package.
5. Review generated candidates against `test.txt` using
   `.specify/test-review-protocol.md` and
   `speaker-business-method.md`.

## 2. Fusion strategy

The accepted runtime candidate uses a diar-turn + align-run strategy:

- Reconstruct ASR `text_id` by ASR track order.
- For each `text_id`, use align units if available.
- Derive diarization speaker turns on the common time base, keeping the
  resolved per-interval `speaker_id` together with the diarizer-local speaker
  slot.
- Group adjacent align units into runs. A unit gap greater than
  `[timeline].align_snap_pause_sec` starts a new run.
- Also split an align run when a diarization speaker boundary falls within
  `[timeline].align_boundary_split_tolerance_sec` of the unit gap. This handles
  turn changes that are visible in diarization but have a sub-pause alignment
  gap.
- Attribute each align run by its midpoint into the derived diarization turn,
  so a long text segment is not owned by one speaker merely because the whole
  ASR span overlaps that speaker most.
- Keep "unknown" when no diarization overlaps; do not borrow neighboring
  speakers across unsupported gaps.
- Fall back to proportional diarization splitting for `text_id`s with no align
  units.

The runtime comprehensive entry stores its resolved `speaker_id` at the interval
level. Serialization must prefer that per-entry id over the diarizer-local
speaker label's latest map, because one local slot can legitimately map to
different global identities at different epochs.

This is intentionally conservative: it does not edit ASR text, does not infer
missing text, and does not rewrite pipeline tracks. It only derives a
business-facing comprehensive view from immutable track evidence.

## 3. Time-base checks

The offline tool shall report:

- track counts and time span coverage;
- align entries whose `text_id` has no ASR entry;
- align units outside their ASR segment;
- VAD/diar/comprehensive entries outside audio bounds;
- candidate comprehensive entries that move outside their source ASR segment.

## 4. Validation

- Smoke capture: short real WebSocket run with align enabled, confirming that
  `timeline.tracks` includes `align` and that align count matches ASR count.
- Full capture: 3615 s real WebSocket run using `test.mp3`.
- Tool validation: Python syntax check and a run over the smoke/full evidence
  package.
- Accuracy validation: manual context-aware review of the final candidate
  business view ("who said what in context"). Script summaries and diar-only
  percentages are diagnostics only.
- Runtime validation: `cmake --build build -j`,
  `cd build && ctest --output-on-failure`, warning/error grep, and a full
  3615 s real WebSocket run after the C++ comprehensive-view changes.
