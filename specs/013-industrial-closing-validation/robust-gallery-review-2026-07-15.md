# Robust Clean-Gallery Manual Context Review

**Date:** 2026-07-15
**Status:** Guarded phrase path retained; broad control substitution rejected
**Guarded candidate:** `/tmp/orator-spec013/runtime-v21/native-postprocess/candidate-robust-gallery-final-guarded.json`
**Broad candidate:** `/tmp/orator-spec013/runtime-v21/native-postprocess/candidate-robust-gallery-control.json`

## Evaluation Authority

Every displayed changed context from both frozen candidates was read manually
against the accepted control and its surrounding conversation. No code,
script, formula, query, notebook, metric, or algorithm assigned correctness,
calculated or aggregated accuracy, ranked or selected a candidate or parameter,
or issued the verdict. Tools only rebuilt frozen component evidence, checked
mechanical contracts, and arranged unjudged context for reading.

## Guarded Phrase Finding

The FR16ZB candidate with robust clean-gallery phrase evidence changes 13
displayed reference contexts. It preserves the previously reviewed safe phrase
path and clearly repairs Tang Yunfeng's fragmented `ref-0100` phrase. It also
repairs Shi Yi's `ref-0304` phrase, "咱切开试一下", without introducing a
whole-turn regression in the surrounding `ref-0304/ref-0305` exchange.

The changes displayed at `ref-0168`, `ref-0172`, `ref-0173`, `ref-0200`,
`ref-0286`, `ref-0335`, `ref-0428`, `ref-0429`, `ref-0515`, and `ref-0536`
either consolidate the same contextual speaker or affect an adjacent boundary
without changing the meaningful turn's real identity. The guarded candidate is
safe to retain as one layer of the next complete candidate, but its coverage is
still insufficient for a complete 556-context promotion review by itself.

## Broad-Control Finding

Substituting robust piece and phrase scores throughout the complete accepted
R/T/U control chain changes only two displayed reference contexts, but both are
clear regressions:

- In `ref-0204`, the continuous Shi Yi phrase "十六到十七" is split and
  "六到" is assigned to Tang Yunfeng.
- At the `ref-0358/ref-0359` boundary, Tang Yunfeng's continuing "起来再说"
  is assigned early to Zhu Jie before Zhu Jie's actual turn begins.

The broad robust-gallery substitution is therefore rejected. Robust aggregation
may be used only behind the complete FR16ZB phrase guards until a separately
specified topology supplies stronger orthogonal evidence.

## Next Boundary

The remaining errors cannot be addressed by replacing the gallery score
globally. The next analysis must start from the accepted-control residual
contexts and separate failures caused by mixed direct voiceprint intervals,
unsupported short interjections, and stable local-slot mapping. Any new rule
must preserve exact forced-alignment ranges, add no reference-derived runtime
condition, and pass changed-context review before a complete two-pass review.
