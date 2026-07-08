# Tail Evidence Review - 2026-07-09

This review follows the project test-review protocol: script outputs below are
diagnostics only. Speaker-business acceptance remains the context-aware review
against `test/data/reference/test.txt`.

## Question

Investigate whether the severe 3240-3360 s speaker regression, especially
3270-3304 s, is caused by the latest comprehensive-timeline fusion changes or by
bottom diarization evidence.

Reference around the key span:

- `00:54:28` 朱杰: "谁能代持呢？你你不行你就先把法人挂着..."
- `00:54:59` 唐云峰: "他在这边占比大。"

The ASR/align text in the runtime package matches this region semantically, so
this is not a gross time-axis offset.

## New Probe

Added `tools/probes/diar_evidence_probe.cc`, an offline diagnostic target that:

- reads Sortformer parameters from `orator.toml` or `ORATOR_CONFIG`;
- runs the current Sortformer configuration over an audio file;
- writes frame-level `top1/top2/margin/active_count/spk0..spk3` CSV;
- writes the onset/offset diar segment view CSV;
- honors `[diarizer].reset_period_sec` by splitting output into independent
  sessions and offsetting `local_speaker` by session.

Build:

```bash
cmake --build build --target diar_evidence_probe -j
```

Baseline run:

```bash
./build/diar_evidence_probe /home/rm01/test/test.mp3 \
  /tmp/orator_diar_evidence_current_20260709
```

Result: 45189 frames, 759 diar segments, compute 26.82 s for 3615.12 s audio.

## Current Bottom Evidence

Current no-reset profile (`fifo_len=188`, `refresh=100`, `right=1`,
`reset_period_sec=0`) yields:

- 3240-3360 s: active 51.7%, mean top1 0.458, mean margin 0.401.
- 3270-3310 s: active 37.8%, repeated high-margin `spk3` hits.
- 3268-3299 s reference turn (朱杰) has mean probabilities
  `[0.057, 0.006, 0.010, 0.324]`, so its diagnostic top speaker is `spk3`.

Key generated segments:

| Span | Local speaker | Confidence | Mean margin |
|---|---:|---:|---:|
| 3270.80-3277.84 | 3 | 0.787 | 0.671 |
| 3284.08-3285.84 | 3 | 0.903 | 0.888 |
| 3293.20-3294.08 | 3 | 0.533 | 0.504 |
| 3296.24-3298.32 | 3 | 0.893 | 0.860 |
| 3301.20-3304.32 | 3 | 0.915 | 0.889 |

This means the comprehensive view is faithfully projecting wrong diar evidence;
it is not inventing the `spk3` ownership at the fusion layer.

## Historical Package Check

The same tail failure exists in older packages:

- `/tmp/orator_closing_full_3615s.json`: 3270.80-3304.40 is repeatedly
  `speaker=3`, `speaker_id=spk_17`.
- `/tmp/orator_full_async_default_20260706.json`: 3270.80-3304.40 is
  `speaker=3`, `speaker_id=spk_3`.
- `/tmp/orator_full_constitution_20260707.json`: same repeated `spk3` evidence.
- `/tmp/orator_timelinefusion_full_20260708.json`: same repeated `spk3`
  evidence, plus forced-alignment coverage.

Diagnostic 120 s windows:

| Package | 3000-3120 | 3120-3240 | 3240-3360 |
|---|---:|---:|---:|
| `orator_closing_full_3615s.json` | 58.9% | 48.5% | 40.2% |
| `orator_full_async_default_20260706.json` | 57.1% | 49.2% | 39.4% |
| `orator_timelinefusion_full_20260708_fusion_bt_timeline.json` | 66.4% | 71.3% | 39.7% |

The current fusion work materially improves earlier tail windows but does not
solve the 3240-3360 s hard spot.

## Rejected Low-Cost Bottom Experiments

All runs used temporary TOML files, keeping `orator.toml` unchanged.

| Experiment | Evidence | Conclusion |
|---|---|---|
| `reset_period_sec=600` | 7 sessions, 736 segments. Session 5 maps `gspk22` mostly to 朱杰, but 3270-3304 falls mostly on `gspk20/gspk23`, which are mainly 石一/唐云峰 in that session. | Not a fix. |
| `reset_period_sec=120` | 31 sessions, 741 segments. 3270-3304 still dominated by a mixed slot (`gspk108`). | Not a fix. |
| `reset_period_sec=60` | 61 sessions, 774 segments. 3270-3299 remains mixed; 3299+ crosses a reset boundary and fragments further. | Not a fix. |
| `use_silence_profile=true` | 730 segments. 3270-3304 remains repeated `spk3`; 3268-3299 still top `spk3`. | Not a fix. |

TitaNet group probes also did not provide a safe acoustic override:

- current no-reset `L3@3000-3600` remains highly similar to the same `L3`
  cluster across earlier buckets, so voiceprint would reinforce `spk3`;
- reset session-5 groups are not clean enough to recover 3270-3304.

## Rejected Frozen Fusion Experiments

All candidates used `/tmp/orator_timelinefusion_full_20260708.json`.

| Experiment | Mechanical result | Speaker-business result |
|---|---|---|
| `min_diar_coverage_ratio=0.45/0.50/0.60` | No audit issues; business unknown rises to 10.34-13.66%. | Does not materially fix 3240-3360; clears only part of 3270-3304 and lowers global diagnostic overlap. |
| `diar_gapfill_max_sec=0.35/1.0` | No audit issues; business unknown about 8.8%. | Cuts large diar gaps to unknown and improves 3240-3360 diagnostic from 39.7% to about 43%, but leaves several wrong high-confidence `spk3` islands. |
| `gapfill=0.35 + coverage=0.45/0.50` | Same as gapfill-only in the target span. | No accepted improvement. |

## Conclusion

The 3270-3304 s regression is not caused by the Web UI, serialization, forced
alignment, or the recent comprehensive-timeline per-entry `speaker_id` fix. It
is present in older closing packages and originates in sparse bottom diarization
evidence for a rapid tail exchange.

Within the current evidence set, an acoustic-only postprocessor cannot reliably
recover the correct speaker for 3270-3304 without creating comparable mistakes
elsewhere. The safe next direction is an uncertainty-aware comprehensive view:
surface diar support/gap metrics and mark weakly supported business ownership
as uncertain when appropriate, rather than silently asserting a speaker. A true
accuracy recovery for this span likely requires stronger model evidence or a
semantic speaker-business postprocessor, not another Sortformer TOML tweak.
