# Raw-Authoritative Manual Context Review

**Date:** 2026-07-15
**Status:** Complete changed-context review; candidate rejected
**Candidate:** `/tmp/orator-spec013/runtime-v21/native-postprocess/candidate-raw-authoritative-sole.json`

## Evaluation Authority

The review was performed only by manually reading every displayed reference
context against the candidate and its accepted-control comparison. No code,
script, formula, query, notebook, metric, or algorithm assigned correctness,
calculated or aggregated accuracy, ranked or selected a candidate, selected a
parameter, or issued the verdict. The review-packet tool only arranged 52
unjudged changed contexts and mechanically displayed their evidence.

## Manual Verdict

The candidate is rejected before a complete 556-context review. Treating every
known `sole_diar_support` range as authoritative restores some useful local-run
structure, but it also reintroduces clear wrong identities across the session.
The broad categorical rule therefore cannot replace the accepted control.

Clear regressions include the meaningful speaker content in `ref-0001`,
`ref-0010`, `ref-0037`, `ref-0109`, `ref-0230`, `ref-0358`, `ref-0382`,
`ref-0405`, `ref-0407`, `ref-0437`, `ref-0464`, `ref-0511`, and `ref-0521`.
These are not reference-boundary-only differences: the candidate assigns
audible words from the cited turn to a different real speaker.

Useful changes exist at `ref-0120`, `ref-0132`, `ref-0164`, `ref-0173`,
`ref-0209`, `ref-0346`, `ref-0410`, and part of `ref-0440`, but they do not
justify the observed regressions. Other displayed changes affect adjacent
boundaries, preserve an already correct meaningful utterance, or replace one
wrong identity with another and therefore do not establish a safe promotion.

## Evidence Conclusion

The global v2.1 local-slot mapping remains coherent; the failure is local
speaker separation at rapid turns. A sole thresholded Sortformer interval is
not proof that its mapped identity owns the complete aligned phrase. The next
candidate must require independent identity evidence for the exact phrase and
must abstain on registry disagreement, rather than giving either raw diarization
or per-fragment voiceprint a universal overwrite right.
