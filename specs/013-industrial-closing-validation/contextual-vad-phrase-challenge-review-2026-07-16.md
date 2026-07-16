# Contextual VAD Phrase Challenge Review (2026-07-16)

## Governance

All displayed changed contexts were read manually against the surrounding
conversation and complete reference transcript. Tools only ran frozen models,
verified source/time/config/hash contracts, and arranged unjudged evidence. No
code, script, formula, query, notebook, metric, or algorithm assigned product
correctness, aggregated accuracy, ranked or selected a candidate or parameter,
or issued the verdict.

## Changed-Context Review

Both full-session generations are byte-identical and expose three exact phrase
overlays across four displayed reference contexts:

- `十个工作日` is Tang Yunfeng's complete reply and is restored from Zhu Jie.
- The beginning of the accounting explanation near `3044.26-3046.42` belongs
  to Shi Yi. The overlay separates it from Tang Yunfeng's preceding closeout
  without changing that closeout; the adjacent reference row is displayed only
  because the ledger boundary overlaps the same ASR source.
- The phrase near `3245.44-3247.20` restores the first part of Shi Yi's longer
  contribution. Later text remains unchanged because its evidence conflicts,
  so this is a bounded partial repair rather than an unsupported expansion.

Manual contextual reading finds no changed-context regression. FR16AAG is
retained. This review makes no updated full-session accuracy claim; complete
chronological and reverse-block contextual semantic review remains mandatory.

## Frozen Artifacts

- Policy: `speaker-v21-contextual-vad-phrase-challenge.toml`
- Candidate A: `/tmp/orator-spec013/runtime-v21/native-postprocess/candidate-contextual-vad-phrase-challenge-a.json`
- Candidate B: `/tmp/orator-spec013/runtime-v21/native-postprocess/candidate-contextual-vad-phrase-challenge-b.json`
- SHA-256: `d993d7b2782a002986d2f4c874545d64886a93b0a5aeda0336ffdc237cd9f41a`

Candidate metadata records every immutable source path and hash.
