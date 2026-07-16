# Posterior-Bounded Phrase Manual Context Review

**Date:** 2026-07-15
**Status:** Complete; candidate rejected from production integration
**Candidate:** `/tmp/orator-spec013/runtime-v21/native-postprocess/candidate-posterior-bounded-phrase.json`

## Evaluation Authority

This product result was evaluated only by manually reading all 556 reference
contexts against the candidate business view, first chronologically and then in
reversed fixed time blocks. No code, script, formula, query, notebook, metric,
or algorithm assigned correctness, calculated or aggregated accuracy, ranked or
selected the candidate, selected a parameter, or issued the verdict. Tools only
executed the pipeline, checked mechanical contracts, and arranged unjudged
context views.

The review concerns the speaker business result contributed by all production
tracks. ASR wording differences and mechanical reference-boundary crossings do
not by themselves make a speaker assignment wrong when the complete utterance
and surrounding conversational turn retain the correct identity.

## Manual Verdict

- Correct: 472 reference contributions.
- Incorrect: 83 reference contributions.
- Ambiguous: `ref-0022` only.
- Manually reported full-length status: approximately 84.9 percent.
- Industrial closeout gate: not met.

The candidate repairs `ref-0033`, `ref-0156`, and `ref-0459` relative to the
duration-calibrated candidate. It introduces no contextual speaker regression.
`ref-0096` keeps the meaningful "绝对通" portion with Tang Yunfeng while the
last character crosses into Shi Yi's following response. `ref-0186` keeps both
"嗯" and "怎么着" with Shi Yi but forced alignment places them just outside
the mechanical reference window. Both remain correct under full-context
semantic review.

## Complete Incorrect Set

The following list was written from the two-direction manual review. It was not
produced or checked by evaluation code.

- 00:00-10:00: `ref-0013`, `ref-0035`, `ref-0037`, `ref-0045`, `ref-0051`,
  `ref-0061`, `ref-0069`, `ref-0070`, `ref-0071`, `ref-0076`, `ref-0092`.
- 00:10-20:00: `ref-0100`, `ref-0102`, `ref-0109`, `ref-0113`, `ref-0127`,
  `ref-0139`, `ref-0146`, `ref-0154`, `ref-0155`, `ref-0160`, `ref-0171`.
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

There is no incorrect reference contribution after 60:00. The residuals are
distributed across the session rather than being only a tail collapse.

## Evidence Conclusion

The posterior boundary prevents the anchored punctuation topology from
propagating Tang Yunfeng across Shi Yi's `817.70-818.42` interjection and makes
three genuine repairs. It does not resolve most mixed direct intervals because
regular direct voiceprint anchors are still immutable. In several residual
contexts, forced alignment and punctuation expose a semantic subphrase while
the enclosing regular anchor spans speech from more than one person. The next
candidate therefore changes the evidence topology: a bounded subphrase may
challenge only an enclosing mixed anchor, never an exact-bound anchor, and only
with independent current-audio identity agreement. It must retain the same
TOML-frozen gates and pass the same complete manual contextual review.
