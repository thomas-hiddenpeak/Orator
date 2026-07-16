# Phrase-Led Outer-Abstention Review (2026-07-16)

## Governance

Every displayed changed context was read manually against the surrounding
conversation and complete reference transcript. Tools only consumed frozen
model evidence, verified source/time/config/hash contracts, and arranged
unjudged contexts. No code, script, formula, query, notebook, metric, or
algorithm assigned correctness, aggregated accuracy, ranked or selected a
candidate or parameter, or issued the verdict.

## Changed-Context Review

Both full-session generations are byte-identical and expose two exact phrase
overlays across three displayed reference rows:

- The `926.94-927.58` acknowledgment belongs to Tang Yunfeng. It begins Tang's
  reply before his continuation about taking an amount from the option pool;
  Shi Yi's preceding sentence has already ended.
- The `1769.04-1769.76` phrase `不一定啊` belongs to Tang Yunfeng. It answers Zhu
  Jie's immediately preceding question about whether to split. Shi Yi begins
  only with the following `这不是老板吗` contribution. The worksheet displays
  the latter reference row because the source timestamp boundary is coarse;
  contextual speaker order resolves the phrase itself.

Manual contextual reading finds no changed-context regression. FR16AAK is
retained. This review makes no updated full-session accuracy claim; complete
chronological and reverse-block contextual semantic review remains mandatory.

## Frozen Artifacts

- Policy: `speaker-v21-phrase-led-outer-abstention.toml`
- Candidate A: `/tmp/orator-spec013/runtime-v21/native-postprocess/candidate-phrase-led-outer-abstention-a.json`
- Candidate B: `/tmp/orator-spec013/runtime-v21/native-postprocess/candidate-phrase-led-outer-abstention-b.json`
- SHA-256: `fb8a437a7a16a7e77f5b70b75a093620dc3191d261b9aa93c533c52be79659bf`

Candidate metadata records every immutable source path and hash.
