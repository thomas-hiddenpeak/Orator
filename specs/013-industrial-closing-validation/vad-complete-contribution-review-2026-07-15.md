# VAD-Complete Contribution Review (2026-07-15)

## Governance

Every displayed changed context was read manually with the surrounding
conversation. Tools only executed models, verified numerical/source/time-base
contracts, and arranged unjudged evidence. No code, script, formula, query,
notebook, metric, or algorithm assigned correctness, aggregated accuracy,
ranked or selected a candidate or parameter, or issued the verdict.

## Unrestricted VAD projection

The first FR16ZX candidate uses complete-VAD dual-gallery agreement to project
all complete aligned clauses. It accepts 39 VAD pieces and changes 49 displayed
contexts. Complete contextual reading rejects it: a long speaker dominates the
VAD query and absorbs real short replies, including the exchanges around
459--466, 505--507, 565--568, and 848--851 seconds.

## Clause and fragment guards

Requiring every complete clause to independently pass both TitaNet galleries
reduces the surface to eight pieces and six displayed changed contexts. Manual
reading still finds two regressions. The mixed clause
`六十六二十八点七，` contains contributions from two speakers, while `哦哦哦！`
is a real short reply; both are dominated by longer context.

The final contract requires an existing selected-identity anchor in the clause
and requires every exact conflicting baseline fragment to independently pass
both galleries with that identity. The frozen final generation contains 44
structural VAD pieces, 51 complete clauses, and 61 conflicting fragments, but
accepts no write. This is a mechanical no-op, not a product-quality judgment.
FR16ZX therefore preserves the retained FR16ZW baseline.

## Frozen artifacts

- Policy: `speaker-v21-vad-complete-contribution.toml`
- Metadata: `/tmp/orator-spec013/runtime-v21/native-postprocess/vad-complete-contribution-metadata.json`
- Combined embeddings: `/tmp/orator-spec013/runtime-v21/native-postprocess/vad-complete-contribution-query-embeddings.tsv`
- Session evidence: `/tmp/orator-spec013/runtime-v21/native-postprocess/vad-complete-contribution-session-evidence.tsv`
- Robust evidence: `/tmp/orator-spec013/runtime-v21/native-postprocess/vad-complete-contribution-robust-evidence.tsv`
- Final no-op candidate: `/tmp/orator-spec013/runtime-v21/native-postprocess/candidate-vad-complete-contribution-fragment-guarded.json`
- Clause-guard review: `/tmp/orator-spec013/runtime-v21/native-postprocess/review-vad-complete-contribution-clause-guarded.md`

The metadata and candidate record source paths and hashes.
