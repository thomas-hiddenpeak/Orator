# Spec 012 Speaker-Support Full-Length Review - 2026-07-10

## Scope

This review evaluates the runtime speaker-support diagnostics change on the
full `test.mp3` session through the real WebSocket path. It follows the
speaker-business method: judge whether the comprehensive timeline answers
"who said what in context" across all pipeline evidence, not whether one
standalone diarization score improved.

The diagnostics-only change is intentionally conservative: it exposes weak
speaker evidence, but it does not rewrite speaker attribution.

## Commands

Server:

```bash
ORATOR_CONFIG=orator.toml ./build/orator_ws
```

Full-length capture:

```bash
PYTHONUNBUFFERED=1 python3 tools/verify/py/ws_unified_test.py \
  --pcm /home/rm01/test/test.mp3 \
  --duration 3615 --rate 1 --port 8765 \
  --out /tmp/orator_support_diag_full_20260710.json \
  --timeline-timeout 1200 --max-total-time 9000
```

Review packet:

```bash
python3 tools/verify/py/speaker_business_review_packet.py \
  --timeline /tmp/orator_support_diag_full_20260710.json \
  --reference /home/rm01/test/test.txt \
  --windows 0-600,600-1200,1200-1320,1800-2400,3000-3068,3270-3304,3350-3615 \
  --max-chars 420 \
  --out /tmp/orator_support_diag_review_packet_20260710.md
```

Diagnostic-only overlap check:

```bash
python3 tools/verify/py/speaker_attrib_eval.py \
  /tmp/orator_support_diag_full_20260710.json \
  --txt /home/rm01/test/test.txt --windows 600
```

The overlap check is not the acceptance metric. It is retained only as a
mechanical sanity probe before human context review.

## Real WebSocket result

- Audio duration: 3615.00 s.
- Wall time: 3619.31 s.
- Stream RT: 0.999x.
- Diarization track: 773 segments, reported RT 98.286x.
- ASR track: 288 utterances, reported RT 1.827x.
- VAD track: 972 segments.
- Align track: 288 groups.
- Comprehensive entries: 962.
- Every comprehensive entry carried the support diagnostics fields.

Support labels:

| Label | Entries | Duration | Share of labelled duration |
|---|---:|---:|---:|
| strong | 690 | 2029.28 s | 62.67% |
| weak | 155 | 1008.48 s | 31.14% |
| none | 117 | 200.46 s | 6.19% |

Weak-support duration by coarse window:

| Window | Weak duration |
|---|---:|
| 0-600 s | 230.56 s |
| 600-1200 s | 147.64 s |
| 1200-1800 s | 165.84 s |
| 1800-2400 s | 194.14 s |
| 2400-3000 s | 131.84 s |
| 3000-3600 s | 138.46 s |

## Mechanical diagnostic probe

The diagnostic attribution probe reported:

```text
diagnostic attribution overlap (dur-weighted) = 80.5% (2604/3236 s)
distinct gt names covered: 4/4

window | diar_local_ceiling | attrib_acc
   0- 600 | 87.8% | 89.6%
 600-1200 | 84.3% | 86.7%
1200-1800 | 81.6% | 84.2%
1800-2400 | 65.8% | 84.8%
2400-3000 | 68.7% | 74.1%
3000-3600 | 60.6% | 64.7%
3600-3615 | 89.4% | 92.0%
```

This indicates no material recovery versus the previously reviewed current
baseline. The important finding is the tail: 3000-3600 s remains far below the
closing-grade target, and the known 3270-3304 s failure remains visible.

## Context-aware speaker-business review

### 0-600 s

The main speaker roles are mostly usable for business reading. Short replies and
backchannels are still noisy, but the conversation context can generally recover
who is driving each topic. This window is not the current blocker.

### 1800-2400 s

The comprehensive business view is better than a local diarization-only reading
because global speaker identities and ASR/align boundaries preserve the topic
structure. It is still not perfect; short bursts and repeated acknowledgements
remain weak evidence. No new accepted recovery was found here.

### 3000-3068 s

The business content around subsidiary loss, capital supplementation, dividends,
and consolidated reporting is mostly readable. The key speakers are often
recoverable from context, but there are still short-turn attribution defects.
This is usable as a rough business transcript, not as precise speaker ownership.

### 3270-3304 s

Reference context:

- 3270-3299 s is mainly Zhu Jie discussing who can hold the legal
  representative role and whether to keep moving first.
- 3299-3306 s switches to Tang Yunfeng on the Hangzhou ownership share being
  small enough.

Candidate comprehensive entries attributed almost the whole 3270.8-3304.4 s
range to `spk_3`:

```text
3270.80-3279.50 spk_3 weak  coverage=0.589 gap=1.82 islands=2
3279.60-3287.30 spk_3 weak  coverage=0.229 gap=4.48 islands=1
3287.40-3288.80 spk_3 none  coverage=0.000 gap=1.40 islands=0
3288.90-3299.80 spk_3 weak  coverage=0.275 gap=4.30 islands=3
3299.90-3304.40 spk_3 strong coverage=1.000 gap=0.00 islands=1
```

The diagnostics correctly mark most of the bad range as weak or none, but they
do not recover the true speaker. This window must be treated as unreliable
speaker ownership in the comprehensive view.

### 3350-3615 s

The discussion about US company structure, operating shells, private treasury,
and disclosure is mostly readable at topic level. Speaker attribution still has
short-turn errors and alternating `spk_1`/`spk_3` confusion. It is acceptable for
rough review, not for closing-grade speaker accuracy.

## Verdict

The full-length evaluation accepts the support-diagnostics implementation as an
evidence-visibility feature:

- Real WebSocket full-length streaming is stable.
- The diagnostics fields propagate through every final comprehensive entry.
- Weak/none labels surface the known evidence gaps instead of silently
  presenting all speaker assignments as equally strong.

The evaluation rejects the change as an accepted speaker-accuracy recovery:

- The diagnostic overlap probe remains around the previous current baseline.
- Tail attribution from 3000-3600 s remains weak, with 3270-3304 s still a clear
  business-speaker failure.
- The new fields identify uncertainty; they do not supply a corrected speaker.

## Next handling

Keep the diagnostics and UI marker because they are useful. Do not use them to
claim restored accuracy.

For the next recovery attempt, the comprehensive timeline should use weak/none
support as an uncertainty signal at the view/export layer. Candidate directions:

- Preserve the raw selected speaker, but add an explicit `speaker_uncertain`
  business-view state for weak/none entries.
- Avoid context inheritance across weak evidence unless another independent
  source supports the same speaker.
- Validate any rewrite policy only against full-session context review, with
  3270-3304 s and 3000-3600 s as mandatory blocking windows.
