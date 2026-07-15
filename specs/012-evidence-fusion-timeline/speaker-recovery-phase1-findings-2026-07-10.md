# Spec 012 Speaker-Recovery Phase 1 Findings - 2026-07-10

> **Evaluation governance:** Under Constitution 1.7.0, no code or executable
> automation may assign correctness, calculate accuracy, rank/select a
> candidate, or issue a verdict. Automated values below are mechanical evidence
> only; product results require complete contextual semantic review and manual
> result verification.

## Scope

This is the first execution of
`speaker-recovery-validation-plan-2026-07-10.md`. It uses the already captured
full-length real WebSocket package:

- `/tmp/orator_support_diag_full_20260710.json`
- `/home/rm01/test/test.txt`

The purpose is layer localization, not final accuracy acceptance.

## Commands

Mechanical fusion audit:

```bash
python3 tools/verify/py/fusion_audit.py \
  --input /tmp/orator_support_diag_full_20260710.json \
  --out /tmp/orator_support_diag_fusion_audit_20260710.json \
  --timeline-out /tmp/orator_support_diag_fusion_business_20260710.json \
  --timeline-view business_turns \
  --print-summary
```

Mandatory-window review packet:

```bash
python3 tools/verify/py/speaker_business_review_packet.py \
  --timeline /tmp/orator_support_diag_full_20260710.json \
  --reference /home/rm01/test/test.txt \
  --windows 0-600,1200-1320,1800-2400,2400-3000,3000-3068,3270-3304,3350-3615 \
  --max-chars 560 \
  --out /tmp/orator_support_diag_mandatory_review_packet_20260710.md
```

Business-turn candidate packet:

```bash
python3 tools/verify/py/speaker_business_review_packet.py \
  --timeline /tmp/orator_support_diag_fusion_business_20260710.json \
  --reference /home/rm01/test/test.txt \
  --windows 3000-3068,3270-3304,3350-3615 \
  --max-chars 560 \
  --out /tmp/orator_support_diag_fusion_business_tail_packet_20260710.md
```

The historical overlap script was run to display changed windows. Its
percentages are mechanical records only and may not evaluate, compare, rank, or
select a result.

## Mechanical Results

Fusion audit result:

- audio: 3615.000 s;
- tracks: ASR 288, diarization 773, align 288, current comprehensive 962;
- align coverage: 288 aligned, 0 missing, 0 extra;
- diagnostic business-turn candidate: 773 entries;
- candidate unknown speaker duration: 168.880 s (4.67%);
- business-turn unknown speaker duration: 173.800 s (4.81%);
- mechanical issues: none.

Diagnostic localization again shows late-session degradation:

| Window | Diagnostic attribution |
|---|---:|
| 0-600 s | 89.6% |
| 600-1200 s | 86.7% |
| 1200-1800 s | 84.2% |
| 1800-2400 s | 84.8% |
| 2400-3000 s | 74.1% |
| 3000-3600 s | 64.7% |

Finer 120 s diagnostic windows show the late degradation is concentrated in
several blocks rather than uniformly distributed:

| Window | Diar local ceiling | Comprehensive attribution diagnostic |
|---|---:|---:|
| 2400-2520 s | 78.2% | 73.2% |
| 2640-2760 s | 61.7% | 69.8% |
| 2880-3000 s | 61.5% | 63.5% |
| 3000-3120 s | 57.2% | 66.3% |
| 3120-3240 s | 78.1% | 71.7% |
| 3240-3360 s | 57.5% | 39.5% |
| 3360-3480 s | 60.6% | 65.0% |
| 3480-3600 s | 78.7% | 80.6% |

This diagnostic pattern supports a diar-local evidence problem in multiple
late-session windows. It also shows that 3120-3240 s may include a
post-diarization mapping or comprehensive-view effect, because the local
ceiling is higher than the final attribution diagnostic.

The business-turn candidate did not improve the diagnostic result:

- full-session diagnostic stayed at 80.5%;
- 3000-3600 s changed from 64.7% to 64.5%.

Therefore simple business-turn coalescing is rejected as a recovery path.

## Layer Findings

### 3000-3068 s

Reference context:

- Xu Zijing raises the wholly owned subsidiary concern.
- Shi Yi and Tang Yunfeng alternate through symbolic pricing, loss balancing,
  capital supplementation, consolidated reporting, and dividend taxation.

Pipeline evidence:

- ASR spans and forced-alignment groups cover the business content and preserve
  the overall topic order.
- Diarization produces many short, high-confidence local segments with
  overlapping local speakers.
- Most comprehensive entries in this range have `speaker_support = "strong"`
  with coverage 1.0 and gap 0.0.

Finding:

- The support diagnostics do not detect the issue because the selected speaker
  is acoustically well supported in the raw diar track.
- The first contradiction is at the diar local / speaker identity evidence
  layer, not at the ASR/align timing layer.
- A support-threshold-only policy cannot fix this window.

### 3270-3304 s

Reference context:

- 3270-3299 s is mainly Zhu Jie discussing who can hold the legal
  representative role and whether the group should proceed first.
- 3299-3306 s switches to Tang Yunfeng on the Hangzhou ownership share.

Runtime comprehensive entries:

```text
3270.800-3279.500 spk_3 weak  coverage=0.589 gap=1.82 islands=2
3279.600-3287.300 spk_3 weak  coverage=0.229 gap=4.48 islands=1
3287.400-3288.800 spk_3 none  coverage=0.000 gap=1.40 islands=0
3288.900-3299.800 spk_3 weak  coverage=0.275 gap=4.30 islands=3
3299.900-3304.400 spk_3 strong coverage=1.000 gap=0.00 islands=1
```

Raw diarization entries:

```text
3270.800-3274.160 local=3 speaker_id=spk_3 confidence=0.879
3275.920-3277.680 local=3 speaker_id=spk_3 confidence=0.694
3284.080-3285.840 local=3 speaker_id=spk_3 confidence=0.901
3293.200-3294.080 local=3 speaker_id=spk_3 confidence=0.576
3296.240-3298.320 local=3 speaker_id=spk_3 confidence=0.914
3299.760-3304.400 local=3 speaker_id=spk_3 confidence=0.799
```

Finding:

- The comprehensive layer is projecting raw diar evidence; it is not inventing
  the wrong speaker.
- The support diagnostics correctly expose the sparse/fragmented part of this
  hard failure as weak or none.
- The final 3299.9-3304.4 s interval is still `strong` because raw diar evidence
  is continuous, even though the business context says the speaker changes to
  Tang Yunfeng.
- A weak-support-only uncertainty policy improves safety for most of the window
  but cannot fully recover speaker ownership.

### 3350-3615 s

Reference context:

- The discussion moves through US company structure, disclosure, operating
  shells, private treasury, and future outbound money flow.

Finding:

- The topic order is broadly preserved by ASR/align.
- Speaker attribution remains unstable between `spk_1`, `spk_3`, and occasional
  `spk_0`.
- Business-turn coalescing does not improve this region; it can make a wrong
  long segment more visually authoritative.

## Bottom Diar Evidence Probe

The current TOML configuration was also run through the existing
`diar_evidence_probe` target:

```bash
cmake --build build --target diar_evidence_probe -j
./build/diar_evidence_probe /home/rm01/test/test.mp3 \
  /tmp/orator_diar_phase1_20260710
```

Probe metadata:

- config: `orator.toml`;
- audio duration: 3615.12 s;
- frames: 45189;
- sessions: 1;
- diar segments: 759;
- compute: 26.8487 s;
- RTF: 0.00742677.

Frame-level summaries for late diagnostic windows:

| Window | Active frames | Mean top1 | Mean margin | Top speaker frame counts |
|---|---:|---:|---:|---|
| 2400-2520 s | 76.4% | 0.693 | 0.593 | spk0=499, spk1=75, spk2=286, spk3=640 |
| 2640-2760 s | 91.5% | 0.804 | 0.642 | spk0=618, spk1=0, spk2=255, spk3=627 |
| 2880-3000 s | 80.0% | 0.705 | 0.560 | spk0=333, spk1=283, spk2=126, spk3=758 |
| 3000-3120 s | 88.8% | 0.805 | 0.663 | spk0=382, spk1=180, spk2=15, spk3=923 |
| 3240-3360 s | 51.7% | 0.458 | 0.401 | spk0=424, spk1=144, spk2=256, spk3=676 |
| 3360-3480 s | 66.7% | 0.574 | 0.460 | spk0=639, spk1=87, spk2=214, spk3=560 |

Segment-level summaries:

| Window | Segments | Diar duration | Mean confidence | Mean margin | Duration by local speaker |
|---|---:|---:|---:|---:|---|
| 2400-2520 s | 35 | 99.4 s | 0.865 | 0.696 | spk0=33.1, spk1=4.5, spk2=17.8, spk3=44.0 |
| 2640-2760 s | 37 | 129.8 s | 0.805 | 0.616 | spk0=53.0, spk1=0.0, spk2=24.4, spk3=52.4 |
| 2880-3000 s | 36 | 114.2 s | 0.786 | 0.612 | spk0=31.9, spk1=18.3, spk2=6.5, spk3=57.5 |
| 3000-3120 s | 24 | 127.7 s | 0.852 | 0.661 | spk0=37.2, spk1=13.3, spk2=2.2, spk3=75.0 |
| 3240-3360 s | 22 | 65.5 s | 0.809 | 0.688 | spk0=13.4, spk1=3.8, spk2=14.0, spk3=34.2 |
| 3360-3480 s | 35 | 92.5 s | 0.779 | 0.610 | spk0=36.0, spk1=8.8, spk2=13.4, spk3=34.2 |

This separates the late failures into two evidence modes:

- 3000-3120 s: high active-frame coverage and high top1/margin, but the local
  evidence is concentrated on the wrong speaker for important business turns.
- 3240-3360 s: low active-frame coverage and weaker frame probabilities, plus
  sparse `spk_3` segments that the comprehensive view projects into the known
  hard failure.

These modes require different handling. A weak-support uncertainty policy is
appropriate for sparse evidence. It cannot detect high-confidence local diar
misclassification.

## Current Conclusion

The evidence now separates two failure classes:

1. **Sparse wrong-evidence failure**: 3270-3299 s. Support diagnostics can expose
   this class as weak/none and should be used for business-view uncertainty.
2. **Confident wrong-evidence failure**: 3000-3068 s and parts of 3350-3615 s.
   Raw diar or speaker identity evidence is strong but inconsistent with the
   reference context. Support diagnostics alone cannot detect this class.

The next candidate should therefore be split:

- short-term safety: add a business-view uncertainty state for weak/none
  ownership so sparse wrong evidence is not presented as reliable;
- accuracy recovery: investigate diar local / speaker identity behavior in late
  windows, because confident wrong-evidence failures require better underlying
  speaker evidence or a separately validated semantic speaker-business policy.

## Rejected Direction

Business-turn-only coalescing is rejected. It preserves the same speaker errors
and can merge the known 3270-3304 s failure into one longer wrong speaker turn.
