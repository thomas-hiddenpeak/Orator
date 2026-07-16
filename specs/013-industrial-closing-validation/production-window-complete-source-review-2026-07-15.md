# Production-Window Complete-Source Review (2026-07-15)

## Governance

Every displayed changed context was read manually with its surrounding
conversation. Tools only executed frozen evidence, checked source/time/TOML
contracts, and arranged unjudged context. No code, script, formula, query,
notebook, metric, or algorithm assigned correctness, aggregated accuracy,
ranked or selected a candidate or parameter, or issued the verdict.

## FR16ZR production-window result

FR16ZR restores the committed `10.0 s` maximum TitaNet query window and keeps
all score, margin, VAD, local-run, identity-conflict, and projection contracts.
Its categorical alternative requires the selected identity's mapped native
channel to be top-2 on every query frame when the inherited sustained active-
channel floor is unavailable.

The candidate changes only `ref-0194`. The reference and conversation identify
the complete contribution `那老有点少，还有点少。` as Xu Zijing. FR16ZR moves
the first punctuation phrase to Xu Zijing, but leaves the short unindexed
suffix on Tang Yunfeng. This is directionally correct but remains a split
business contribution, so FR16ZR alone is not retained as the final layer.

## FR16ZS complete-source result

FR16ZS reuses the exact FR16ZR query and identity evidence. It admits a complete
ASR source only when the source has one indexed punctuation phrase, every
non-separator character has forced-alignment evidence wholly inside that query,
and the complete baseline source has one uniform known identity.

The only changed context remains `ref-0194`. Manual contextual review confirms
that the candidate assigns the complete `那老有点少，还有点少。` contribution
to Xu Zijing (`spk_2`), matching both the reference speaker and the surrounding
conversation. The prior half-sentence Tang Yunfeng residual is removed. No
other speaker sequence changes.

The FR16ZS candidate is retained as the next composition baseline.

## Frozen artifacts

- Policy: `speaker-v21-production-window-local-run.toml`
- FR16ZR metadata: `/tmp/orator-spec013/runtime-v21/native-postprocess/production-window-local-run-metadata.json`
- FR16ZR candidate: `/tmp/orator-spec013/runtime-v21/native-postprocess/candidate-production-window-local-run-top2.json`
- FR16ZS metadata: `/tmp/orator-spec013/runtime-v21/native-postprocess/complete-source-voiceprint-metadata.json`
- Retained candidate: `/tmp/orator-spec013/runtime-v21/native-postprocess/candidate-complete-source-voiceprint.json`
- Changed-context review: `/tmp/orator-spec013/runtime-v21/native-postprocess/review-complete-source-voiceprint.md`

The frozen metadata and candidates record their source paths and hashes.
