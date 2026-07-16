# Bracketed Local-Churn Review (2026-07-15)

## Governance

Every displayed changed context was read manually with the surrounding
conversation. Tools only executed models, verified numerical/source/time-base
contracts, and arranged unjudged evidence. No code, script, formula, query,
notebook, metric, or algorithm assigned correctness, aggregated accuracy,
ranked or selected a candidate or parameter, or issued the verdict.

## Unrestricted candidate

The first frozen FR16ZV candidate accepts 22 query regions and changes 23
displayed reference contexts. Complete contextual reading rejects it. A longer
query can be dominated by the outer speaker and absorb a real short
contribution, including the transitions around `ref-0005`, `ref-0058`,
`ref-0163`, `ref-0221`, `ref-0261`, `ref-0278`, `ref-0382`, and `ref-0550`.
Long-query dual-gallery agreement alone is therefore not sufficient evidence
for contribution consolidation.

## Guarded candidate

The final contract adds three categorical protections without changing a
score threshold:

- exact phrase projection requires one uniform conflicting baseline identity
  and both phrase galleries must rank the outer identity first;
- complete-clause expansion requires every inner run to have lower mean native
  top-1 margin than both outer runs, with no phrase-level dual agreement on a
  different top-ranked identity; and
- every inner frame must contain exactly one active native channel, preserving
  overlap as a cannot-link boundary.

The guarded candidate changes three contexts. Complete manual reading confirms:

- `ref-0441`: the full middle clause group `不，就是，对吧？就是这个。...`
  is unified with Shi Yi's surrounding contribution;
- `ref-0452`: `五六万够，` is reassigned consistently to Tang Yunfeng; and
- `ref-0554`: the complete question and reply tail are unified with Shi Yi.

No reviewed context regresses. FR16ZV is retained as the next composition
baseline. The review does not claim a new full-session accuracy result; that
requires a complete two-pass contextual review of the retained full candidate.

## Frozen artifacts

- Policy: `speaker-v21-bracketed-local-churn.toml`
- Metadata: `/tmp/orator-spec013/runtime-v21/native-postprocess/bracketed-local-churn-metadata.json`
- Query embeddings: `/tmp/orator-spec013/runtime-v21/native-postprocess/bracketed-local-churn-query-embeddings.tsv`
- Session evidence: `/tmp/orator-spec013/runtime-v21/native-postprocess/bracketed-local-churn-session-evidence.tsv`
- Robust evidence: `/tmp/orator-spec013/runtime-v21/native-postprocess/bracketed-local-churn-robust-evidence.tsv`
- Candidate: `/tmp/orator-spec013/runtime-v21/native-postprocess/candidate-bracketed-local-churn-final.json`
- Review: `/tmp/orator-spec013/runtime-v21/native-postprocess/review-bracketed-local-churn-final.md`

The metadata and candidate record source paths and hashes.
