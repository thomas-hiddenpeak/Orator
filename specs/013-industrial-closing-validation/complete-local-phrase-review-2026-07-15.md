# Complete Local-Phrase Manual Context Review (2026-07-15)

## Governance

This is a manual contextual semantic review of every displayed changed
context. Tools only generated frozen evidence, checked mechanical contracts,
and arranged unjudged context. No code, script, formula, query, notebook,
metric, or algorithm assigned correctness, aggregated accuracy, ranked or
selected a candidate or parameter, or issued the verdict below.

## Frozen evidence

- Baseline:
  `/tmp/orator-spec013/runtime-v21/native-postprocess/candidate-vad-edge-run-low-activity.json`
- Guarded candidate:
  `/tmp/orator-spec013/runtime-v21/native-postprocess/candidate-complete-local-phrase-guarded.json`
- Changed-context worksheet:
  `/tmp/orator-spec013/runtime-v21/native-postprocess/review-complete-local-phrase-guarded.md`
- Policy: `speaker-v21-complete-local-phrase.toml`

Two complete phrases produce four displayed reference contexts. All four were
read in their surrounding exchange against both business views.

## Manual findings

- `ref-0069`: repair. The complete `我们俩多少？` phrase returns to Shi Yi,
  agreeing with the preceding `如果按二十八加十五` proposition and restoring
  the core question's speaker.
- `ref-0156`: safe strengthening. `二十个亿` returns to Tang Yunfeng and joins
  the surrounding Tang Yunfeng proposition. The context was already usable
  under the semantic-core rubric, so this is not claimed as a newly repaired
  reference contribution.
- `ref-0070` and `ref-0071`: display changes caused by removing the rewritten
  phrase from a longer ASR fragment. The remaining `有` and following numeric
  exchange keep their prior identities; no new semantic assignment is made.

## Verdict

The guarded FR16ZK candidate is retained as the next composition baseline. Its
raw-top-ranked-baseline veto removes the unsafe `那这事儿` rewrite and the
insufficiently supported `真的有意思` rewrite from the first candidate. The
retained path changes only complete punctuation phrases and produces no manual
contextual regression.

This retention is a manual semantic judgment. The evidence generator did not
score a reference row, aggregate a result, select the guard, or issue the
verdict.
