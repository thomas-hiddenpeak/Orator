# Complete Edge-Contribution Review (2026-07-15)

## Governance

Every displayed changed context was read manually with the surrounding
conversation. Tools only checked time/source/TOML contracts, reused frozen
acoustic evidence for exactly identical intervals, and arranged unjudged
context. No code, script, formula, query, notebook, metric, or algorithm
assigned correctness, aggregated accuracy, ranked or selected a candidate or
parameter, or issued the verdict.

## Initial complete-clause candidate

Replacing disconnected alignment units with complete punctuation clauses
reduces the active-edge surface to seven accepted clause groups. Manual review
finds all four terminal-edge changes contextually correct or attribution-
neutral. The three initial-edge changes are not safe: two are ambiguous
speaker-transition fillers and `啊` before `还有一个问题` belongs with the
following Shi Yi contribution rather than the outgoing Tang Yunfeng channel.

The failure is directional. An initial edge ends at the next speaker onset, so
forced-alignment lag can place that onset on the outgoing local channel. The
terminal edge begins after a handoff and contains the complete post-handoff
clause before VAD closure. The guarded candidate therefore admits terminal
edges only; this rule contains no transcript value, timestamp, speaker pair, or
reference-derived numerical threshold.

## Retained terminal-edge result

The guarded candidate has four accepted groups. One only strengthens an
already-correct Shi Yi `对，` attribution without changing the displayed
speaker sequence. Complete contextual review confirms three repaired business
contributions:

- `ref-0162`: Tang Yunfeng's reply `对，` is separated from Shi Yi's preceding
  question and assigned to Tang Yunfeng.
- `ref-0221`: Xu Zijing's complete `对啊，我们老是以为他，` contribution is
  separated from Shi Yi's preceding statement and assigned to Xu Zijing.
- `ref-0404`: Tang Yunfeng's `没事儿，` reply is separated from Zhu Jie's
  preceding question and assigned to Tang Yunfeng.

No manually reviewed context regresses. The terminal-edge FR16ZT candidate is
retained as the next composition baseline.

## Frozen artifacts

- Policy: `speaker-v21-complete-edge-contribution.toml`
- Metadata: `/tmp/orator-spec013/runtime-v21/native-postprocess/complete-edge-contribution-terminal-metadata.json`
- Candidate: `/tmp/orator-spec013/runtime-v21/native-postprocess/candidate-complete-edge-contribution-terminal.json`
- Review: `/tmp/orator-spec013/runtime-v21/native-postprocess/review-complete-edge-contribution-terminal.md`

The metadata and candidate record source paths and hashes.
