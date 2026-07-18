# v2.1 Closing Baseline Context Review - 2026-07-15

## Scope and Judgment Boundary

This is the complete written-context speaker review of the clean v2.1 closing
baseline. The reviewer read all 556 reference contributions in chronological
order, then reviewed the same evidence again by fixed 600-second block in
reverse block order. Tools only captured and arranged time-overlapping evidence;
no compiled code, test, script, notebook, formula, query, metric, or algorithm
assigned correctness, resolved ambiguity, calculated or estimated accuracy,
ranked/selected a candidate, or issued the verdict. The reviewer manually
derived and checked every count and percentage after both context passes.

The review answers one question: whether the final `business_speaker` view
attributes each contextually complete contribution to the correct business
speaker. ASR wording is used only to locate the proposition and is not scored.
A minor boundary-word leak does not fail a row when the row's core proposition
remains assigned to the correct speaker. A complete or major proposition under
another speaker, a cross-speaker merge that makes the core identity unreliable,
or an unknown identity for a known proposition fails the row.

`test.txt` is the human-listened reference; no separate audible transcription is
required. This historical review did not manually derive speaker-time,
source-time-offset, overlap, or criticality breakdowns from its completed
contextual judgments, so those separate gates were not signed for this rejected
baseline. The source timestamps support only their recorded whole-second
precision.

## Frozen Evidence

| Item | Value |
|---|---|
| Source commit | `3b402453c7886e0e884f2eeb168f0f3405aa03fe` |
| Full producer package | `/tmp/orator-spec013/closing-v21-3b40245/full-02/orator-baseline-20260715T092158Z-3b402453c788-full-02.json` |
| Package SHA-256 | `66e1b23d8f0f35e4edc70cf9bd4b41a256062c6cf781fd3f0a5a482b26591665` |
| `test.mp3` SHA-256 | `b7c25d1c349b02d654b6a406bc29039749e4240a4109dda4fcc905285b14b18b` |
| `test.txt` SHA-256 | `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86` |
| Business / ASR / diar entries | 935 / 287 / 755 |
| Chronological display packet | `review/by-reference.md` |
| Reverse-block display packet | `review/reverse-blocks.md` |

The display packets cover all 556 rows and contain no correctness field. The
business identity mapping was stable in the reviewed artifact:

| Runtime identity | Business speaker |
|---|---|
| `spk_1` | 唐云峰 |
| `spk_2` | 徐子景 |
| `spk_3` | 石一 |
| `spk_4` | 朱杰 |

## Reconciled Result

The two manual passes reconcile to 443 correct, 112 incorrect, and one
ambiguous natural contribution. The written-context natural-turn result is
`443 / 556 = 79.6763%`.

| Fixed block | Correct | Incorrect | Ambiguous | Context result |
|---|---:|---:|---:|---:|
| 0-600 s | 74 | 18 | 1 | 79.5699% |
| 600-1200 s | 78 | 6 | 0 | 92.8571% |
| 1200-1800 s | 69 | 11 | 0 | 86.2500% |
| 1800-2400 s | 64 | 16 | 0 | 80.0000% |
| 2400-3000 s | 89 | 40 | 0 | 68.9922% |
| 3000-3600 s | 66 | 21 | 0 | 75.8621% |
| 3600-3615.12 s | 3 | 0 | 0 | 100.0000% |
| **Full session** | **443** | **112** | **1** | **79.6763%** |

The result fails the 90 percent full natural-turn gate. Five fixed 600-second
blocks also fail the per-block 90 percent gate. The 2400-3000-second block is
the largest concentration of errors, but the failures are distributed across
the session and are not a tail-only regression.

## Incorrect and Ambiguous Rows

The following labels, totals, percentages, and verdict are the reviewer's
manually reconciled semantic judgments, not generated classifications or
automated aggregation.

**0-600 s, 18 incorrect**

`ref-0001`, `ref-0005`, `ref-0008`, `ref-0009`, `ref-0013`, `ref-0025`,
`ref-0035`, `ref-0037`, `ref-0045`, `ref-0049`, `ref-0051`, `ref-0061`,
`ref-0071`, `ref-0073`, `ref-0076`, `ref-0079`, `ref-0084`, `ref-0089`.

**600-1200 s, 6 incorrect**

`ref-0096`, `ref-0109`, `ref-0135`, `ref-0139`, `ref-0154`, `ref-0171`.

**1200-1800 s, 11 incorrect**

`ref-0182`, `ref-0194`, `ref-0201`, `ref-0216`, `ref-0236`, `ref-0239`,
`ref-0241`, `ref-0245`, `ref-0248`, `ref-0250`, `ref-0252`.

**1800-2400 s, 16 incorrect**

`ref-0258`, `ref-0280`, `ref-0287`, `ref-0292`, `ref-0293`, `ref-0296`,
`ref-0298`, `ref-0299`, `ref-0304`, `ref-0306`, `ref-0308`, `ref-0312`,
`ref-0313`, `ref-0331`, `ref-0333`, `ref-0334`.

**2400-3000 s, 40 incorrect**

`ref-0338`, `ref-0341`, `ref-0343`, `ref-0350`, `ref-0352`, `ref-0354`,
`ref-0356`, `ref-0359`, `ref-0363`, `ref-0365`, `ref-0374`, `ref-0375`,
`ref-0379`, `ref-0382`, `ref-0384`, `ref-0385`, `ref-0388`, `ref-0390`,
`ref-0396`, `ref-0397`, `ref-0399`, `ref-0403`, `ref-0405`, `ref-0407`,
`ref-0409`, `ref-0417`, `ref-0420`, `ref-0422`, `ref-0429`, `ref-0432`,
`ref-0433`, `ref-0442`, `ref-0444`, `ref-0450`, `ref-0454`, `ref-0457`,
`ref-0459`, `ref-0461`, `ref-0463`, `ref-0464`.

**3000-3600 s, 21 incorrect**

`ref-0467`, `ref-0468`, `ref-0474`, `ref-0476`, `ref-0478`, `ref-0499`,
`ref-0500`, `ref-0503`, `ref-0504`, `ref-0505`, `ref-0506`, `ref-0507`,
`ref-0509`, `ref-0513`, `ref-0515`, `ref-0518`, `ref-0521`, `ref-0532`,
`ref-0533`, `ref-0535`, `ref-0537`.

**Ambiguous**

`ref-0022` is the only unresolved row. The duplicate timestamp and concurrent
numeric fragments do not permit an unambiguous speaker assignment from the
written conversation alone.

All other rows were manually accepted under the semantic-core rubric above.

## Context-Specific Reconciliation

`ref-0160` is marked as 石一 in the source text, but the surrounding exchange
identifies its speaker as 唐云峰: the contribution says that below ten percent
he would not join the board, and 石一 immediately answers by asking whether he
does not want to argue with them in the board office. The runtime assigns the
contribution to `spk_1`/唐云峰. This row is therefore contextually correct and
is retained as an explicit source-speaker/context conflict in the review record.

The second pass also rejected mechanical boundary reasoning in both directions.
Rows such as `ref-0351` and `ref-0425` remain correct because the relevant core
speaker identity is present even though the ASR wording or split is imperfect.
Rows `ref-0258`, `ref-0292`, `ref-0338`, `ref-0375`, `ref-0461`, `ref-0463`,
`ref-0467`, and `ref-0468` fail because mixed identities affect a core
proposition, not merely a boundary word.

## Error Structure and Decision

The observed business failures fall into three context-level classes:

1. A complete or major contribution is assigned to another known speaker.
2. A known contribution is left without a usable business identity.
3. A contribution is split or merged across identities so that its core
   speaker cannot be trusted.

The 2400-3000-second block contains repeated assignments of 朱杰 contributions
to `spk_1`/`spk_2` and collapsed short interjections. The 3000-3600-second block
contains mixed 唐云峰/石一 propositions around `ref-0467`-`ref-0468` and a severe
speaker collapse across `ref-0503`-`ref-0507`. The first block combines startup
unknown evidence with local-slot reuse on short turns. These clusters identify
business-view evidence problems for subsequent root-cause work; they do not
authorize a parameter choice or runtime change by themselves.

This clean 935-entry artifact supersedes the prior 936-entry `413 / 142 / 1`
cut-oriented written-context diagnostic as the current baseline's textual
semantic record. The difference comes from reviewing the exact clean artifact
and judging complete semantic contributions rather than mechanically penalizing
every imperfect split. It is not evidence that the model improved.

No candidate is promoted. The current baseline fails the natural-turn gate, so
no speaker-time accuracy or closing result is claimed and no further product
breakdowns are required for this rejected historical artifact.
