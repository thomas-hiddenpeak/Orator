# Bounded Local-Run Voiceprint Review (2026-07-15)

## Governance

Every displayed changed context was read manually with its surrounding
conversation. Tools only generated frozen evidence, checked source/time and
TOML contracts, and arranged unjudged context. No code, script, formula,
query, notebook, metric, or algorithm assigned correctness, aggregated
accuracy, ranked or selected a candidate or parameter, or issued the verdict.

## Unguarded result

The first FR16ZO candidate admitted four complete phrases. Full manual review
found two partial repairs and two regressions. At `ref-0147`, a continuous Shi
Yi question was assigned to Zhu Jie. At `ref-0426`, part of Shi Yi's B/C
grouping statement was assigned to Tang Yunfeng. In both regressions the
TitaNet-selected identity had no corresponding active native Sortformer channel
inside the query.

The successor guard requires the TitaNet-selected identity's mapped native
channel to be active in the query for the unchanged FR16J `0.4 s` sustained-run
floor. This is orthogonal channel confirmation, not a new result-derived score
or duration threshold.

## Guarded result

The guarded candidate admits only `这个你说了算，` in `ref-0258`. The session
registry and robust clean gallery both select Tang Yunfeng, while native
local-0 support overlaps the query for more than the inherited sustained floor.
The main semantic statement returns from Shi Yi to Tang Yunfeng and joins the
already-correct following `这这这我我不插话`. One repeated `说了算` fragment of
approximately `0.4 s` remains on the old Shi Yi identity. The meaningful
speaker contribution is therefore repaired with this boundary residual
explicitly retained.

The guarded FR16ZO candidate is retained as the next composition baseline. No
other assignment changes.

## Frozen artifacts

- Policy: `speaker-v21-bounded-local-run-voiceprint.toml`
- Metadata: `/tmp/orator-spec013/runtime-v21/native-postprocess/bounded-local-run-voiceprint-metadata.json`
- Guarded candidate: `/tmp/orator-spec013/runtime-v21/native-postprocess/candidate-bounded-local-run-voiceprint-guarded.json`
- Review: `/tmp/orator-spec013/runtime-v21/native-postprocess/review-bounded-local-run-voiceprint-guarded.md`

The frozen metadata records all source paths and hashes.
