# Spec 010 Local Diarization Review - 2026-07-06

This review follows `.specify/test-review-protocol.md`. Script/JQ summaries below
are structural diagnostics only; the accuracy conclusion is from context-aware
reading against `test/data/reference/test.txt`.

## Test Summary

| Item | Content |
|---|---|
| Test type | Full-length real WebSocket, Phase H conservative speaker-id candidate |
| Input audio | `test/data/audio/test.mp3`, 3615 s |
| Reference text | `test/data/reference/test.txt` |
| Output | `/tmp/orator_phaseh_full.json` |
| Run result | Success, final timeline returned |
| Wall time | 3618.185 s |
| Stream RTF | 0.999x |
| ASR RTF | 1.858x |
| Diar RTF | 100.761x |
| Device metrics | 3611 `tegrastats` samples in `device_series` |
| Subjective conclusion | Stable runtime, but local diarization quality is not restored. |

## Segmented Review

| Time span | Reference summary | System output summary | ASR semantic | Speaker eval | Issues |
|---|---|---|---|---|---|
| 00:00-10:00 | Four speakers discuss equity ratios; clear long turns plus short interjections. | Four global ids present and balanced in the first window. Main long turns are mostly usable; short interjections are still noisy. | Mostly accurate | Mostly accurate | Short rapid turns around 01:05-01:30 and 07:00-08:30 still fragment. |
| 10:00-20:00 | Discussion continues with multiple speakers but less stable turn lengths. | Phase H leaves many later-session slots local-only and only two global ids dominate (`spk_1`, `spk_3`). | Mostly accurate | Major confusion | Missing global speakers; local-only labels replace some wrong ids but do not provide usable attribution. |
| 20:00-30:00 | Agreement/terms discussion with recurring short confirmations. | Same pattern as 10:00-20:00: local-only spans plus global dominance by one or two ids. | Mostly accurate | Major confusion | Speaker count is incomplete in the global view; this is not a threshold-only problem. |
| 30:00-40:00 | Company structure, Hangzhou/Chengdu relationship, technical transfer. | Long 1802-1870 s turn is stable (`spk_0`), but rapid Q/A after 1870 s switches among `spk_0/1/2/3` and unknown. | Partly distorted | Minor to major confusion | Main long speaker is usable; short replies and overlaps are fragmented. ASR also repeats "对" around 1927-1944 s. |
| 40:00-50:00 | Legal/company structure and market/investor strategy. | `spk_1` and `spk_3` dominate with local-only spans. Some speaker distinctions are visible but not reliable enough for attribution. | Mostly accurate | Major confusion | Better than stable wrong new ids, but still insufficient speaker separation. |
| 50:00-60:15 | Tail discussion on subsidiary accounting, overseas company, accounts and disclosure. | Four global ids appear, but local speaker turns switch every 1-3 s in dense dialogue. Tail 3600-3615 s remains local-only. | Mostly accurate | Major confusion | Fragmentation remains the main failure; Phase H avoided `spk_13/spk_14`-style late over-enrollment, but did not restore diar quality. |

## Diagnostic Observations

- Phase H full run produced 4 global ids plus 91 diar segments with empty
  `speaker_id`. Local-only labels are expected under the conservative policy.
- Window-level duration diagnostics show the problem shape:
  - 600-1200: local-only 87.3 s; `spk_3` 273.8 s dominates.
  - 1200-1800: local-only 78.6 s; `spk_3` 317.4 s dominates.
  - 3000-3615: all four ids appear, but rapid local-slot switching remains.
- Comparing `/tmp/orator_full_diar_baseline_20260705.json` with Phase H around
  3000-3060 s shows the same local diar fragmentation pattern. The baseline
  turns fragments into late globals (`spk_12/13/16/17`); Phase H maps some of the
  same uncertainty to canonical ids or local-only labels. The downstream
  voiceprint policy changes the label failure mode, not the diarizer's local
  separation.

## Conclusions

- **ASR semantic accuracy**: approximately 80-89% for this run. Main business
  topics and many numbers are preserved, but local phrase errors and one repeat
  burst remain.
- **Diarization accuracy**: approximately 60-69% for full-session attribution.
  Long isolated turns can be usable, but speaker count and short-turn attribution
  are not reliable in 600-1800 and 3000-3615.
- **Test result**: Fail for diarization accuracy recovery.
- **Next step**: Evaluate and improve local diarizer segmentation before global
  identity stitching. The next candidate must prove per-window local speaker
  separation first; speaker-id threshold tuning alone is insufficient.
