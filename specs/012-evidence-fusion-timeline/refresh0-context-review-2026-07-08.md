# Spec 012 Refresh-Rate and Context-Fusion Review - 2026-07-08

This review follows `.specify/test-review-protocol.md` and
`speaker-business-method.md`. Script output below is diagnostic only; the
acceptance object is the context-aware speaker-business view against
`test/data/reference/test.txt`.

## Evidence Packages

| Item | Path / value |
|---|---|
| Current baseline package | `/tmp/orator_timelinefusion_full_20260708.json` |
| Current baseline business view | `/tmp/orator_timelinefusion_full_20260708_fusion_bt_timeline.json` |
| Histctx candidate package | `/tmp/orator_histctx_300_40_5_20260708.json` |
| Refresh-0 candidate package | `/tmp/orator_refresh0_20260708.json` |
| Refresh-0 business view | `/tmp/orator_refresh0_20260708_fusion_bt_timeline.json` |
| Context low-support experiment | `/tmp/orator_refresh0_context_low_support_timeline.json` |
| Input audio | `test/data/audio/test.mp3`, 3615 s |
| Reference | `test/data/reference/test.txt` |

## Runtime Validation

The refresh-0 candidate used a temporary TOML copied from `orator.toml` with
only:

- `[speaker].registry_path = "/tmp/orator/speakers_refresh0_20260708.json"`
- `[diarizer].spkcache_refresh_rate = 0`

Full WebSocket run:

- audio 3615.00 s, wall 3618.74 s, stream RTF 0.999x;
- diar 773 segments, ASR 288 utterances, VAD 972 segments, align 288/288;
- telemetry 5931 samples, device series 3611 samples;
- client/server error scan was empty.

Fusion audit for refresh-0:

- ASR=288, diar=773, align=288, current=962, candidate=926,
  business_turns=722;
- align missing=0, extra=0;
- business unknown 175.640 s (4.86%);
- mechanical issues: none.

## Candidate Results

| Candidate | Diagnostic global | 3000-3300 | 3240-3360 | Context judgment |
|---|---:|---:|---:|---|
| Current baseline | 80.9% | 56.2% | 39.7% | Known windows recovered; tail 3270-3304 remains wrong. |
| Histctx 300/40/5 | 77.8% | 53.3% | 37.3% | Rejected: worse global and tail, no tail fix. |
| Refresh-0 | 80.9% | 56.6% | 39.7% | Rejected: no material improvement over baseline. |
| Context low-support | 81.0% | 66.4% | 64.2% | Rejected: fixes 3270-3304 but badly regresses 1200-1320. |

## Context Review

### Preserved Windows

- `1805.760-1870.560`: `spk_0` preserves the long 朱杰 narrative.
- `2133.360-2150.080`, `2223.120-2264.320`,
  `2266.880-2289.040`: `spk_1` preserves 唐云峰's LP / valuation /
  acquisition framing.
- `3108.560-3133.280`: `spk_0` preserves 朱杰's split-flexibility
  explanation, with only a short `spk_3` interjection.

### Remaining Tail Failure

Reference:

- `00:54:28-00:54:59` 朱杰: asks who can hold shares, says the others can go
  first, and tells the target speaker to leave after finding a suitable holder.

Refresh-0 business view:

- `3270.800-3304.400` `spk_3`: the same business statement is still attributed
  to `spk_3`.

The bottom diarization track in this region contains sparse but repeated
`spk_3` segments (`3270.80-3274.16`, `3275.92-3277.68`,
`3284.08-3285.84`, `3293.20-3294.08`, `3296.24-3298.32`,
`3299.76-3304.40`). Forced alignment supplies useful text timing but does not
provide speaker identity evidence, so the comprehensive view faithfully
projects the wrong diar evidence.

### Rejected Context Low-Support Rule

A frozen-view experiment reassigned low-diar-coverage long business turns to
the previous speaker when the same previous speaker reappeared within 60 s. It
changed only two runs:

- `1255.920-1280.080`: `spk_3 -> spk_1`, coverage 0.315.
- `3270.800-3304.400`: `spk_3 -> spk_0`, coverage 0.428.

This improved the tail diagnostic, but the first change is wrong in context:
`1255.920-1280.080` is 石一 answering 唐云峰's question, not 唐云峰
continuing. The 120 s diagnostic window `1200-1320` dropped from 82.0% to
59.7%, so this rule is rejected.

### Rejected Voiceprint Override

A custom TitaNet probe compared key spans:

- `/tmp/orator_key_spans_probe.txt`
- target `3270.80-3299.80` did not score closer to Zhu anchors than to Shi /
  adjacent target evidence;
- target `1255.92-1280.08` likewise showed that a simple acoustic override
  cannot safely distinguish the false-positive context rule.

Therefore, a voiceprint override for this tail span is not supported by the
available frozen evidence.

## Conclusion

The current formal profile remains the best validated operating point from this
round. Do not adopt histctx, refresh-0, or the context low-support heuristic.

The residual regression is not caused by Web UI rendering or business-turn
serialization. It originates in bottom diarization evidence for a sparse tail
region. A durable fix should target diarization/identity evidence quality or a
new uncertainty-aware speaker-business view that can expose ambiguous support
without inventing speaker ownership.
