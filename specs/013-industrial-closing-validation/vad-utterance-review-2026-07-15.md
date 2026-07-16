# VAD Utterance Manual Context Review (2026-07-15)

## Governance

This is a manual contextual semantic review. Tools only executed the frozen
models, verified mechanical contracts, and arranged the comparison context.
No code, script, formula, query, notebook, metric, or algorithm assigned
correctness, aggregated accuracy, ranked or selected a candidate or parameter,
or issued the verdict below.

## Frozen evidence

- Baseline:
  `/tmp/orator-spec013/runtime-v21/native-postprocess/candidate-robust-gallery-final-guarded.json`
- Candidate:
  `/tmp/orator-spec013/runtime-v21/native-postprocess/candidate-vad-utterance.json`
- Padded production VAD evidence:
  `/tmp/orator-spec013/runtime-v21/native-postprocess/vad-padded-segments.tsv`
- Changed-context worksheet:
  `/tmp/orator-spec013/runtime-v21/native-postprocess/review-vad-utterance-changed.md`
- Policy:
  `speaker-v21-vad-utterance.toml`

The candidate contains three accepted projected pieces in two displayed
reference contexts. Both displayed contexts were read in full against the
timestamped reference and both candidate views.

## Manual findings

- `ref-0107`: regression. The contribution is Tang Yunfeng's continuous
  statement. Reassigning the isolated aligned characters `来` and `次` to Shi
  Yi breaks the statement and creates a false speaker interruption.
- `ref-0165`: directionally correct but incomplete. The reference contribution
  belongs to Shi Yi, so changing `不多` from Xu Zijing to Shi Yi is locally
  supported. The surrounding `差不多，差不多。嗯，` remains split across Xu
  Zijing and Shi Yi, so this does not repair the complete contribution.

## Verdict

The unrestricted FR16ZG candidate is rejected. A complete short padded VAD
interval with one native top-1 channel and dual-gallery TitaNet agreement is
not sufficient when forced alignment projects only disconnected characters
from a longer contribution. The safe next rule must require complete business
contribution evidence or abstain; it must not introduce a character-count gate
derived from these reviewed rows.
