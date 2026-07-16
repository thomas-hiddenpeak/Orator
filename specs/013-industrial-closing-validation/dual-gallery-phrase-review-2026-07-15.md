# Dual-Gallery Exact-Phrase Manual Context Review

**Date:** 2026-07-15
**Status:** Complete changed-context review; candidate rejected
**Candidate:** `/tmp/orator-spec013/runtime-v21/native-postprocess/candidate-dual-gallery-exact-phrase.json`

## Evaluation Authority

Every one of the 37 displayed changed reference contexts was read manually
against the accepted control and surrounding conversation. No code, script,
formula, query, notebook, metric, or algorithm assigned correctness, calculated
or aggregated accuracy, ranked or selected a candidate, selected a parameter,
or issued the verdict. The packet tool only arranged unjudged context.

## Manual Verdict

The unguarded candidate is rejected before a complete 556-context review. It
provides useful repairs at `ref-0100`, `ref-0168`, `ref-0200`, `ref-0206`,
`ref-0297`, `ref-0344`, `ref-0347`, `ref-0439`, and `ref-0511`, with additional
boundary improvements in several already-correct turns.

It also creates clear wrong-speaker changes at `ref-0021`, `ref-0023`,
`ref-0054`, `ref-0093`, `ref-0121`, `ref-0225`, `ref-0262`, `ref-0370`,
`ref-0426`, `ref-0531`, and `ref-0548`. In particular, one agreed phrase spans
Shi Yi's short `ref-0121` interjection and the following Tang Yunfeng turn,
proving that exact punctuation bounds and two TitaNet prototype sets still do
not establish a speaker-homogeneous phrase.

## Evidence Conclusion

Session-refreshed and clean multi-prototype galleries are separate registry
views of the same TitaNet model, not orthogonal model evidence. Their agreement
may challenge a fragmented baseline only when native Sortformer frames also
support the selected channel and do not expose a sustained competing-speaker
run inside the phrase. The next candidate adds those categorical guards using
only the already frozen FR16K activity and run parameters.
