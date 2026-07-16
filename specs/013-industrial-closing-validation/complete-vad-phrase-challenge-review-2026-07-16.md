# Complete VAD Phrase Challenge Review (2026-07-16)

## Governance

Every displayed changed context was read manually against the surrounding
conversation and complete reference transcript. Tools only ran frozen models,
verified source/time/config/hash contracts, and arranged unjudged evidence. No
code, script, formula, query, notebook, metric, or algorithm assigned product
correctness, aggregated accuracy, ranked or selected a candidate or parameter,
or issued the verdict.

## Guard Derivation

The initial FR16AAF candidate exposed three complete short-VAD phrase
challenges. Manual contextual reading rejected two changes whose outer VAD
voiceprints had low absolute similarity and retained one real speaker handoff.
The final policy therefore requires both outer VAD registry views to meet the
already configured regular-score floor. This tightens an existing gate; it does
not introduce or fit a new threshold.

## Changed-Context Review

The guarded candidate contains one change. Near `3472.54-3473.34`, Zhu Jie asks
`不是海外架构嘛？` after Tang Yunfeng's preceding contribution. The VAD session,
VAD robust, phrase session, and phrase robust views all select Zhu Jie, both
outer views meet the inherited regular floor, and the raw local mapping differs.
Manual contextual reading accepts the change as a complete handoff repair.

No changed context regresses. FR16AAF is retained. This review makes no updated
full-session accuracy claim; the candidate still requires complete chronological
and reverse-block contextual semantic review.

## Frozen Artifacts

- Policy: `speaker-v21-complete-vad-phrase-challenge.toml`
- Candidate A: `/tmp/orator-spec013/runtime-v21/native-postprocess/candidate-complete-vad-phrase-challenge-guarded-a.json`
- Candidate B: `/tmp/orator-spec013/runtime-v21/native-postprocess/candidate-complete-vad-phrase-challenge-guarded-b.json`
- SHA-256: `a17bf9c5771c5288f42f1b9717c9049b33115fdad5eb9815509a1219411ed53e`

Candidate metadata records every immutable source path and hash.
