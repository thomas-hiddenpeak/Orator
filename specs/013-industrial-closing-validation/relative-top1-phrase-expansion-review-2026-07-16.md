# Relative Top-1 Phrase Expansion Review (2026-07-16)

## Governance

Every displayed changed context was read manually against the surrounding
conversation and the complete reference transcript. Tools only checked frozen
evidence, deterministic/source/time contracts, and arranged unjudged context.
No code, script, formula, query, notebook, metric, or algorithm assigned
correctness, aggregated accuracy, ranked or selected a candidate or parameter,
or issued the verdict.

## Identity And Boundary Contract

FR16AAE does not make a new identity decision. It reuses only an already
accepted FR16ZD relative-top-1 identity and asks both complete-phrase TitaNet
views whether expansion to the exact enclosing punctuation phrase must be
vetoed. Both views must top-rank the already selected identity under the frozen
margin floor. The ordinary absolute-score gate still applies to the original
accepted piece. Protected overlays abstain.

## Changed-Context Review

Both full-session generations produce the same business track and expose two
changed conversational contexts. Manual contextual reading accepts both:

- `他真盈利了，` is one Tang Yunfeng contribution in the discussion of whether
  a company is profitable. Expansion restores the leading `他` to the same
  complete contribution instead of leaving a boundary fragment on Shi Yi.
- `JP应该就直接叫全资子公司。` is one Shi Yi contribution. Expansion restores
  the complete phrase while leaving the following `嗯，那这事儿，我们俩就闭嘴`
  contribution with Tang Yunfeng.

No changed context regresses. FR16AAE is retained for the next full candidate.
This changed-context review makes no updated full-session accuracy claim. The
candidate must still pass complete chronological and reverse-block contextual
semantic review before any closing verdict.

## Frozen Artifacts

- Policy: `speaker-v21-relative-top1-phrase-expansion.toml`
- Candidate A: `/tmp/orator-spec013/runtime-v21/native-postprocess/candidate-relative-top1-phrase-expansion-a.json`
- Candidate B: `/tmp/orator-spec013/runtime-v21/native-postprocess/candidate-relative-top1-phrase-expansion-b.json`

The candidate metadata records the immutable source paths and hashes.
