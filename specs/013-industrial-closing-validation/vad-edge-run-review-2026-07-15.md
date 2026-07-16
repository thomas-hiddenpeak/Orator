# VAD Edge-Run Manual Context Review (2026-07-15)

## Governance

This is a manual contextual semantic review of every displayed changed
context. Tools only executed frozen models, verified mechanical contracts, and
arranged unjudged comparisons. No code, script, formula, query, notebook,
metric, or algorithm assigned correctness, aggregated accuracy, ranked or
selected a candidate or parameter, or issued the verdict below.

## Frozen evidence

- Baseline:
  `/tmp/orator-spec013/runtime-v21/native-postprocess/candidate-robust-gallery-final-guarded.json`
- Candidate:
  `/tmp/orator-spec013/runtime-v21/native-postprocess/candidate-vad-edge-run.json`
- Changed-context worksheet:
  `/tmp/orator-spec013/runtime-v21/native-postprocess/review-vad-edge-run-changed.md`
- Policy: `speaker-v21-vad-edge-run.toml`

Sixteen accepted projected pieces produce nine displayed reference contexts.
All nine were read in full against the timestamped reference and both business
views.

## Manual findings

- `ref-0146`: complete repair. The padded VAD begins with a sustained native
  channel 2 run, mapped to Xu Zijing, before switching to channel 3 for Shi
  Yi. The projected `四层` is the ASR rendering of Xu Zijing's `43啊` turn.
- `ref-0133` through `ref-0136`: trade, not a safe repair. Moving `我就` to
  Tang Yunfeng improves `ref-0134`, but extending the write through `对吧`
  regresses Shi Yi's adjacent `ref-0135` contribution.
- `ref-0052`: the changed `么看` boundary fragment still does not match Tang
  Yunfeng's preceding contribution and does not repair the row.
- `ref-0154`, `ref-0404`, and `ref-0551`: only isolated characters move toward
  the contextual speaker. The complete contributions remain split and
  incorrect.
- The remaining accepted pieces do not change a displayed speaker sequence and
  do not establish an additional complete contribution repair.

## Evidence distinction and verdict

The unrestricted FR16ZH candidate is rejected. The repaired `ref-0146` edge
run is continuously top-1 for one local channel while every frame remains
below the existing `0.5` activity threshold. Every other accepted edge run has
at least one frame above that already-frozen threshold. This distinction is a
model-contract property, not a reference identity, timestamp, transcript, or
new numerical gate. FR16ZI may therefore retain only edge runs with no active
frame under the unchanged FR16J threshold, followed by another complete
changed-context manual review.

## FR16ZI guarded result

The low-activity guard leaves four evidence pieces. Dual TitaNet and local-map
agreement accept one piece, producing exactly one displayed changed context:
`ref-0146`. Manual contextual review confirms the complete Xu Zijing
contribution is repaired, with no other candidate assignment change. The
guarded candidate is retained as the next composition baseline:

`/tmp/orator-spec013/runtime-v21/native-postprocess/candidate-vad-edge-run-low-activity.json`

This retention verdict is manual. The evidence tools did not assign
correctness, aggregate accuracy, rank candidates or parameters, or promote the
candidate.
