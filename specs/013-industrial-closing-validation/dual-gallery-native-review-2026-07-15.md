# Native-Guarded Dual-Gallery Manual Context Review

**Date:** 2026-07-15
**Status:** Complete changed-context review; candidate not promoted
**Candidate:** `/tmp/orator-spec013/runtime-v21/native-postprocess/candidate-dual-gallery-native-guarded.json`

## Evaluation Authority

All 20 displayed changed contexts were reviewed manually against the accepted
control and surrounding conversation. No code, script, formula, query,
notebook, metric, or algorithm assigned correctness, calculated or aggregated
accuracy, ranked or selected a candidate, selected a parameter, or issued the
verdict. Tools only generated the frozen candidate and arranged unjudged
context.

## Manual Finding

The native Sortformer guards remove the unguarded candidate's broad failures,
including the real speaker transition at `ref-0121`. They retain useful repairs
at `ref-0100`, `ref-0168`, `ref-0172`, `ref-0200`, `ref-0344`, `ref-0439`, and
several same-speaker boundary consolidations.

The candidate is not yet safe for complete promotion. The short `OK` spanning
`ref-0224/ref-0225` is a pure-unknown baseline range but is filled with a known
identity that conflicts with the conversational speaker. Around
`ref-0262/ref-0263`, a short direct anchor and both same-model galleries agree
on a locally fragmented identity, so the phrase expansion does not add
independent evidence. These findings require categorical evidence guards rather
than another score or duration change.

## Next Constraint

The next candidate acts only when it replaces a different known baseline
identity. Pure-unknown filling is excluded. If a direct anchor exists, an
agreeing regular-duration direct anchor is required; short-only direct support
cannot authorize phrase expansion. Native-channel and sustained-competing-run
guards remain unchanged.
