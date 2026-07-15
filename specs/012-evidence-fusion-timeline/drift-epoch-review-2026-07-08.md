# Spec 012 Drift-Epoch and Runtime Fusion Review - 2026-07-08

> **Evaluation governance:** Under Constitution 1.7.0, no code or executable
> automation may assign correctness, calculate accuracy, rank/select a
> candidate, or issue a verdict. Automated values below are mechanical evidence
> only; product results require complete contextual semantic review and manual
> result verification.

## Test Summary

| Item | Content |
|---|---|
| Test type | Full-length real WebSocket after speaker drift and runtime comprehensive-view fixes |
| Input audio | `test/data/audio/test.mp3`, 3615.0 s, pushed at 1.0x |
| Reference text | `test/data/reference/test.txt` |
| Output package | `/tmp/orator_timelinefusion_full_20260708.json` |
| Fusion business-turn output | `/tmp/orator_timelinefusion_full_20260708_fusion_bt_timeline.json` |
| Wall time | 3618.74 s |
| Stream RTF | 0.999x |
| Diarization | 773 segments, RTF 98.426x |
| ASR | 288 final segments, RTF 1.83x |
| Forced alignment | 288/288 ASR segments aligned |
| Business-turn fusion | 728 entries, unknown 171.860 s (4.75%), no mechanical audit issues |
| Subjective conclusion | Major known speaker-business regressions are materially recovered in the reviewed windows, but the result is not a perfect scalar-accuracy closeout. Residual short-boundary artifacts and conservative unknown spans remain. |

## Root Causes

1. A diarizer-local speaker slot can drift during the full 60-minute session.
   Treating the local slot as one global speaker for the whole run rewrote later
   competing-speaker evidence onto earlier intervals.
2. The runtime comprehensive timeline serialized `speaker_id` from the latest
   local-speaker map. When a local slot later mapped to a different global
   identity, historical comprehensive entries could be emitted with the wrong
   global id even when the diarization track itself had the correct per-segment
   `speaker_id`.
3. The first align-first fusion candidate assigned a whole alignment run by
   largest overlap. That preserved phrase timing but could swallow a real turn
   change inside a long ASR segment. Tightening the pause threshold alone
   over-split rapid backchannels, so the boundary rule needed both pause and
   diarization evidence.

## Runtime Fixes

- Added TOML-gated local drift and competing-identity split/backfill parameters
  under `[speaker]`.
- Tightened the speaker embedding clean-span selection so reference embeddings
  come from the center of the clean available audio, not from a noisy retained
  window edge.
- Stored resolved `speaker_id` per `ComprehensiveTimeline::Entry` and made
  snapshot equality, coalescing, gap fill, JSON serialization, and WebSocket
  serialization respect that interval-level id.
- Added `[timeline].align_snap_pause_sec = 0.25` and
  `[timeline].align_boundary_split_tolerance_sec = 0.08`. The comprehensive
  view keeps normal alignment runs intact, but splits at a diarization boundary
  when that boundary is near the forced-alignment unit gap.
- Updated `fusion_audit.py` and `speaker_business_review_packet.py` so frozen
  evidence can be reviewed as business turns without re-running the models.

## Validation Evidence

Commands:

```bash
python3 -m py_compile tools/verify/py/fusion_audit.py tools/verify/py/speaker_business_review_packet.py
cmake --build build -j
cd build && ctest --output-on-failure
cmake --build build -j 2>&1 | grep -E "warning:|error:" || true
ORATOR_CONFIG=orator.toml ./build/orator_ws
PYTHONUNBUFFERED=1 python3 tools/verify/py/ws_unified_test.py \
  --duration 3615 --rate 1 --port 8765 \
  --out /tmp/orator_timelinefusion_full_20260708.json \
  --timeline-timeout 300 --max-total-time 4800
python3 tools/verify/py/fusion_audit.py \
  --input /tmp/orator_timelinefusion_full_20260708.json \
  --out /tmp/orator_timelinefusion_full_20260708_fusion_bt.json \
  --timeline-out /tmp/orator_timelinefusion_full_20260708_fusion_bt_timeline.json \
  --timeline-view business_turns \
  --short-flip-sec 0.35 \
  --short-flip-chars 2 \
  --fill-unknown-sec 0.35 \
  --business-turn-gap-sec 1.0 \
  --print-summary
```

Results:

- `ctest`: 47/47 passed.
- Warning/error grep: no output.
- Full WebSocket run completed: audio 3615.00 s, wall 3618.74 s, stream RTF
  0.999x.
- Capture JSON and fusion timeline JSON are valid JSON.
- Fusion audit: ASR=288, diar=773, align=288, current=968,
  candidate=930, business_turns=728, align missing=0, extra=0, issues=none.

## Context-Aware Speaker Business Review

The following windows were reviewed against `test/data/reference/test.txt` using
`speaker-business-method.md`. Historical code counts below are mechanical records
only; acceptance is based on context-aware reading of who said what.

| Window | Reference context | Final-view observation | Judgment |
|---|---|---|---|
| 1800-1870 s | Long 朱杰 narrative with a short interjection. | 1803.12-1805.76 is a short `spk_1` interjection; 1805.76-1870.56 stays on `spk_0` for the long narrative. | Recovered for business attribution. |
| 2133-2289 s | 唐云峰 asks about LP possibility, 成都公司估值, and收购 framing. | 2133.36-2150.08, 2223.12-2264.32, and 2266.88-2289.04 are all attributed to `spk_1`, preserving the same speaker's business position. | Recovered for the known regression. |
| 3108-3133 s | 朱杰 says拆分灵活度更大; 石一 gives a short response; 朱杰 continues the explanation. | 3108.56-3112.56 and 3113.76-3133.28 are `spk_0`; 3112.56-3113.76 is a short `spk_3` interjection. | Materially recovered, with a tiny boundary artifact. |

## Residual Risk

- The 3108 s boundary still leaves the single character "其" before the
  `spk_0` turn because the forced-alignment unit timestamp is before the
  diarization boundary. This is intentionally not fixed with lexical heuristics.
- The business-turn fusion keeps 4.75% of the audio as `unknown` rather than
  borrowing unsupported speakers.
- This review does not claim a universal scalar accuracy. It confirms that the
  major known full-length speaker-business regressions are corrected under the
  current TOML profile and identifies the remaining boundary/short-turn risks.
