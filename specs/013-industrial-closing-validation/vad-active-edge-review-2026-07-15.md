# Active VAD Edge Manual Context Review (2026-07-15)

## Governance

This is a manual contextual semantic review of every displayed changed
context. Tools only executed frozen models, verified mechanical contracts, and
arranged unjudged comparisons. No code, script, formula, query, notebook,
metric, or algorithm assigned correctness, aggregated accuracy, ranked or
selected a candidate or parameter, or issued the verdict below.

## Frozen evidence

- Baseline:
  `/tmp/orator-spec013/runtime-v21/native-postprocess/candidate-vad-edge-run-low-activity.json`
- Candidate:
  `/tmp/orator-spec013/runtime-v21/native-postprocess/candidate-vad-active-edge.json`
- Changed-context worksheet:
  `/tmp/orator-spec013/runtime-v21/native-postprocess/review-vad-active-edge-changed.md`
- Policy: `speaker-v21-vad-active-edge.toml`

Thirty-seven accepted projected pieces produce 24 displayed reference
contexts. All 24 contexts were read in full against the timestamped reference,
the retained baseline, and the candidate business view.

## Manual findings

- `ref-0221`: complete repair. The final active native channel-2 run restores
  Xu Zijing's complete `对啊，我们老是以为他` contribution before the
  following Shi Yi turn. Both TitaNet views abstain, so neither supplies
  positive contradictory identity evidence.
- `ref-0265` and `ref-0422` through `ref-0423`: the native handoff places a
  boundary filler or adjacent fragment more plausibly, but these changes do
  not establish enough complete contribution repairs to retain the rule.
- `ref-0005` through `ref-0006`, `ref-0068`, `ref-0299`, `ref-0358` through
  `ref-0359`, and `ref-0381` through `ref-0382`: regressions. The rule projects
  a valid native channel identity onto text before or after the corresponding
  semantic contribution boundary. In `ref-0358`, for example, the start run
  moves `后` and `起来再说` to Zhu Jie while the surrounding contribution is
  still Tang Yunfeng's.
- `ref-0052`, `ref-0133` through `ref-0136`, `ref-0154`, `ref-0310`,
  `ref-0404`, `ref-0428` through `ref-0429`, and `ref-0551`: partial or
  boundary-only changes. They do not repair a complete speaker contribution
  and several split otherwise continuous evidence into isolated characters.

## Verdict

The unrestricted FR16ZJ candidate is rejected. Sustained activity and a
stable local-slot mapping establish useful identity evidence, but they do not
establish that forced-alignment units at the native run edge belong to the same
semantic contribution. The dual-TitaNet agreed-different veto cannot protect
these cases because both views often abstain on short edge audio.

The retained composition baseline remains FR16ZI:

`/tmp/orator-spec013/runtime-v21/native-postprocess/candidate-vad-edge-run-low-activity.json`

`ref-0221` is evidence that an active handoff can be useful, not permission to
encode its identity, time, transcript, or a threshold derived from this row.
Any successor topology must prove a complete contribution boundary before it
may project the mapped local identity. This rejection and the successor
constraint are manual contextual judgments; no executable evaluator issued
them.
