# Spec 010 Local Diarization Default-188 Review - 2026-07-06

This review follows `.specify/test-review-protocol.md`. Script/JQ summaries are
diagnostic only; acceptance is based on context-aware reading against
`test/data/reference/test.txt`.

## Test Summary

| Item | Content |
|---|---|
| Test type | Full-length real WebSocket, async/no-reset local-diar restore |
| Input audio | `test/data/audio/test.mp3`, 3615 s |
| Reference text | `test/data/reference/test.txt` |
| Config focus | `[diarizer] spkcache_update_period=188`, `chunk_right_context=1`, `spkcache_sil_frames=3`, `fifo_len=188`, `reset_period_sec=0` |
| Output | `/tmp/orator_full_async_default_20260706.json` |
| Run result | Success, final timeline returned |
| Wall time | 3618.487 s |
| Stream RTF | 0.999x |
| ASR RTF | 1.847x |
| Diar RTF | 98.842x |
| Tracks | diar 773, ASR 288, VAD 972 |
| Device metrics | 3611 `tegrastats` samples in `device_series` |
| Subjective conclusion | Stable 4-id operating profile restored; tail rapid-turn diar remains imperfect. |

Note: a direct compile-time `SortformerConfig` default change was attempted and
rejected because it broke `test_diar_stream` NeMo oracle equivalence. The runtime
operating profile is therefore held in `orator.toml`, per the project
configuration rule.

## Segmented Review

| Time span | Reference summary | System output summary | ASR semantic | Speaker eval | Issues |
|---|---|---|---|---|---|
| 00:00-10:00 | Four speakers discuss equity ratios, 51/28/15/6 proposals, and short objections. | Four global ids appear immediately and remain balanced. Main turns are usable; short confirmations are still fragmented. | Mostly accurate | Mostly accurate | Short interjections around 01:05-01:30 and 07:00-08:30 remain noisy. |
| 10:00-20:00 | Dense back-and-forth on voting control, dilution, option pool, and valuation. | No local-only gaps. One dominant speaker is expected from the reference, but other speakers still appear through short turns. | Mostly accurate | Mostly accurate | Rapid confirmations are often assigned to adjacent speakers; this is still not precise enough for every short utterance. |
| 20:00-30:00 | Continued valuation/option discussion with repeated short replies. | Four ids continue across the window. The global view no longer has Phase H's local-only holes, but short turns are still coarse. | Mostly accurate | Minor confusion | Dense short-turn regions are usable as rough attribution, not exact turn-level ground truth. |
| 30:00-40:00 | Company structure, Hangzhou/Chengdu relationship, technical transfer. | Long 1803-1870 s Zhu Jie narrative is stable as one id; short replies after 1871 s switch among the other ids. | Partly distorted | Mostly accurate | ASR repeats "对" heavily at 1927-1944 s; diar remains plausible but ASR text quality degrades there. |
| 40:00-50:00 | Legal/company structure, patents, markets, investor strategy. | Four ids continue without local-only gaps. Long turns are usable; short legal/finance clarifications are still interleaved. | Mostly accurate | Minor confusion | Proper nouns and finance terms have ASR errors; speaker attribution is acceptable for main turns. |
| 50:00-60:15 | Tail discussion on subsidiary accounting, overseas company, accounts and disclosure. | Four ids remain stable, but 3000-3615 s has real rapid speaker exchange. The 3000-3068 s segment follows the reference's 石一/唐云峰/朱杰/徐子景 pattern only roughly; 3350-3615 s alternates mainly between two ids with short fragments. | Mostly accurate | Minor to major confusion | Tail attribution is better than Phase H local-only output but still fragmented; not a full diar accuracy fix. |

## Diagnostic Observations

- Speaker-id coverage is restored: diar track counts are `spk_3` 280,
  `spk_0` 234, `spk_1` 140, `spk_2` 119; there are no empty `speaker_id`
  diar entries.
- The recorded row-level comprehensive count was affected by final snapshot
  aggregation that merged adjacent same-speaker ASR finals. That aggregation is
  presentation-level behavior and should not be used as the accuracy object;
  the accuracy view must preserve ASR `text_id` boundaries while applying
  diarization ownership.
- Window-level duration diagnostics show stable 4-id coverage:
  - 0-600: all four ids present (`spk_0` 141.9 s, `spk_1` 113.4 s,
    `spk_2` 96.8 s, `spk_3` 100.4 s).
  - 600-1200 and 1200-1800: `spk_3` dominates, consistent with the reference's
    long 石一 turns, while other speakers still appear.
  - 3000-3615: all four ids remain present, but rapid exchanges produce many
    short fragments.
- The conservative Phase H candidate is rejected as a replacement because it
  creates local-only gaps. This default-188 profile is the better operating
  profile, but it is not a complete diarization quality fix.

## Conclusions

- **ASR semantic accuracy**: approximately 80-89%. Main business topics,
  ratios, and company-structure discussion are preserved; local word errors,
  proper-noun errors, and the 1927-1944 s repeat burst remain.
- **Diarization accuracy**: approximately 70-79% for full-session attribution.
  The run restores four stable global identities and main-turn usability, but
  rapid tail exchanges and short interjections are still not precise enough for
  high-confidence per-utterance attribution.
- **Test result**: Conditional pass for restoring the current operating profile;
  fail as a complete diar quality closure.
- **Next step**: keep `188/1/3` as the default profile. Further improvement must
  target overlap/short-turn handling or ASR repeat suppression, not global
  speaker-id threshold tuning.
