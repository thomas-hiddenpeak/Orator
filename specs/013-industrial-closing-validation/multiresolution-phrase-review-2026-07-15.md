# Multiresolution Phrase Manual Context Review

**Date:** 2026-07-15
**Status:** Complete; candidate rejected from production integration
**Candidate:** `/tmp/orator-spec013/runtime-v21/native-postprocess/candidate-multiresolution-phrase.json`

## Evaluation Authority

This product result was evaluated only by manually reading the complete 556
reference contexts against the candidate business view, first chronologically
and then in reversed fixed time blocks. No code, script, formula, query,
notebook, metric, or algorithm assigned correctness, calculated or aggregated
accuracy, ranked or selected the candidate, selected a parameter, or issued the
verdict. Tools only executed the frozen candidate, checked mechanical
contracts, and arranged unjudged context views.

The review concerns the speaker business result contributed by all production
tracks. ASR wording differences and mechanical reference-boundary crossings do
not by themselves make a speaker assignment wrong when the complete utterance
and surrounding conversational turn retain the correct identity.

## Manual Verdict

- Correct: 475 reference contributions.
- Incorrect: 80 reference contributions.
- Ambiguous: `ref-0022` only.
- Manually reported full-length status: approximately 85.4 percent.
- Industrial closeout gate: not met.

The candidate repairs `ref-0035`, `ref-0037`, and `ref-0109` relative to the
posterior-bounded candidate. The complete two-direction contextual review found
no new whole-contribution speaker regression. Changes at `ref-0173`,
`ref-0209`, and `ref-0395` affect short boundary fragments while preserving the
speaker of the meaningful utterance. `ref-0214` and `ref-0298` change evidence
inside already-incorrect mixed contributions but remain incorrect.

## Complete Incorrect Set

The following list was written from the two-direction manual review. It was not
produced or checked by evaluation code.

- 00:00-10:00: `ref-0013`, `ref-0045`, `ref-0051`, `ref-0061`, `ref-0069`,
  `ref-0070`, `ref-0071`, `ref-0076`, `ref-0092`.
- 00:10-20:00: `ref-0100`, `ref-0102`, `ref-0113`, `ref-0127`, `ref-0139`,
  `ref-0146`, `ref-0154`, `ref-0155`, `ref-0160`, `ref-0171`.
- 00:20-30:00: `ref-0182`, `ref-0194`, `ref-0201`, `ref-0214`, `ref-0215`,
  `ref-0221`, `ref-0227`, `ref-0239`, `ref-0241`, `ref-0248`, `ref-0249`,
  `ref-0250`, `ref-0252`.
- 00:30-40:00: `ref-0258`, `ref-0263`, `ref-0280`, `ref-0290`, `ref-0292`,
  `ref-0294`, `ref-0296`, `ref-0298`, `ref-0302`, `ref-0304`, `ref-0308`,
  `ref-0331`, `ref-0333`.
- 00:40-50:00: `ref-0338`, `ref-0341`, `ref-0351`, `ref-0354`, `ref-0361`,
  `ref-0388`, `ref-0390`, `ref-0391`, `ref-0397`, `ref-0406`, `ref-0417`,
  `ref-0426`, `ref-0432`, `ref-0433`, `ref-0436`, `ref-0440`, `ref-0442`,
  `ref-0444`, `ref-0461`, `ref-0463`.
- 00:50-60:00: `ref-0471`, `ref-0472`, `ref-0499`, `ref-0500`, `ref-0501`,
  `ref-0503`, `ref-0505`, `ref-0506`, `ref-0509`, `ref-0512`, `ref-0521`,
  `ref-0532`, `ref-0533`, `ref-0534`, `ref-0537`.

There is no incorrect reference contribution after 60:00. The residuals remain
distributed through the session and are not a tail-only collapse.

## Evidence Conclusion

One-frame raw runs and dual voiceprint agreement recover three real short-turn
boundaries without creating a contextual regression. They do not repair most
remaining errors because the enclosing identity evidence is itself wrong or
mixed. Repeated TitaNet disagreement with otherwise coherent local runs also
shows that a single registry centroid can suppress useful within-speaker voice
variation or inherit contaminated enrollment windows. Further threshold
changes to this overlay would only reclassify the same weak evidence.

The next topology must therefore improve deployable identity evidence before
timeline fusion: build robust, auditable speaker prototypes from mutually
consistent clean spans, retain more than one valid voice mode where supported,
and allow a bounded current-audio query to choose among those prototypes. All
parameters remain TOML-owned, and product promotion still requires a new
complete manual contextual semantic review.
