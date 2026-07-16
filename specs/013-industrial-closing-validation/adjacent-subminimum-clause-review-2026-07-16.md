# Adjacent Subminimum-Clause Review (2026-07-16)

## Governance

All six changed contexts were read manually against the surrounding
conversation and complete reference transcript. Tools only enumerated the
reference-free clause topology, ran frozen models, verified numerical/source/
time/config/hash contracts, and arranged unjudged contexts. No code, script,
formula, query, notebook, metric, or algorithm assigned product correctness,
aggregated accuracy, ranked or selected a candidate or parameter, or issued the
verdict.

## Evidence Coverage Finding

The experiment confirms a real extraction gap: two adjacent punctuation
clauses can each fall below the existing `0.5 s` phrase minimum even though
their combined aligned span is embeddable. It exports 225 such envelopes.
However, adjacency in one ASR source does not imply one speaker. Rapid dialogue
frequently places the true handoff exactly between the two micro clauses.

The motivating contexts do not pass the frozen evidence gates. The
`对，独立公司，` envelope's session registry selects Xu Zijing while the robust
view misses the inherited margin. The `哦，没区别。` envelope misses the regular
absolute-score floor in both views and the two top identities disagree. Neither
can be promoted from this evidence.

## Complete Changed-Context Review

Both candidate generations are byte-identical and contain six overlays. Manual
reading rejects every one:

- `你是｜对` crosses from Shi Yi to Tang Yunfeng and cannot be assigned as one
  Tang contribution.
- Tang Yunfeng's following `你看` is reassigned to Shi Yi.
- Tang Yunfeng's `对吧？对` is reassigned to Shi Yi.
- Shi Yi's continuous `啊，可以` is reassigned to Zhu Jie.
- Shi Yi's `嗯，对` is reassigned to Xu Zijing.
- Tang Yunfeng's `嗯，你看` is reassigned to Shi Yi.

FR16AAI is rejected and the retained candidate remains FR16AAH. No complete
candidate review or updated full-session accuracy claim follows from this
rejected branch.

## Frozen Artifacts

- Policy: `speaker-v21-adjacent-subminimum-clause.toml`
- Metadata: `/tmp/orator-spec013/runtime-v21/native-postprocess/adjacent-subminimum-clause-metadata.json`
- Session evidence: `/tmp/orator-spec013/runtime-v21/native-postprocess/adjacent-subminimum-clause-session-titanet.tsv`
- Robust evidence: `/tmp/orator-spec013/runtime-v21/native-postprocess/adjacent-subminimum-clause-robust-titanet.tsv`
- Candidate A: `/tmp/orator-spec013/runtime-v21/native-postprocess/candidate-adjacent-subminimum-clause-a.json`
- Candidate B: `/tmp/orator-spec013/runtime-v21/native-postprocess/candidate-adjacent-subminimum-clause-b.json`
- SHA-256: `53acefc06b89506b762fcdd803dd40d0a9daab59d2ca604c0bd73f0b4ce89ccc`

Candidate metadata records every immutable source path and hash.
