# Native Handoff Guard Context Review

**Date:** 2026-07-18  
**Status:** Frozen replay retained; real-WebSocket promotion pending  
**Scope:** FR16ABM generic punctuation-phrase abstention only

## Evaluation Authority

The replay tools read frozen typed diarization, primary-speaker, ASR, forced-
alignment, and voiceprint tracks. They verified input/output structure,
determinism, and source ordering, and displayed changed contexts. They did not
assign correctness, calculate accuracy, rank a rule, or issue a product
verdict. Every product judgment below was made by reading the complete affected
conversation in chronological order and then rereading the affected contexts in
reverse order for both Run A and Run B.

This review does not include audible boundary listening and does not close the
T102 ledger or any full real-WebSocket acceptance gate.

## Root Cause

`SplitTextByDiarBase` already retained two useful local handoffs:

- `492.34-493.38`: Tang Yunfeng's leading `有` followed by Shi Yi's numeric
  contribution `四十四十五`;
- `2223.34-2224.22`: Tang Yunfeng's substantive `我问你个事儿` followed by a
  short Shi Yi interjection at the phrase edge.

The later generic punctuation-phrase TitaNet pass treated each phrase as one
acoustic sample and repainted its complete source range with the majority
identity. This erased the more local activity/primary/alignment handoff.

The first broad guard protected every mixed base phrase. Frozen Run A exposed
clear contextual regressions immediately: short primary churn in `ref-0005`,
`ref-0008`, and `ref-0086` displaced correct phrase-scale attributions. That
prototype was rejected and was not promoted or assigned an aggregate result.

FR16ABM is deliberately narrower. The phrase-selected identity must remain one
of exactly two contiguous known base-identity runs. The other run must carry
primary tie-break/refinement provenance and at least the existing TOML
`speaker_fusion.min_embed_sec` of positive forced-alignment time. Unknowns,
third identities, return transitions, short churn, and identities absent from
the base phrase all abstain. No parameter or TOML value changed.

## Frozen Evidence

| Evidence | Run A | Run B |
|---|---|---|
| Source timeline SHA-256 | `15892a4d66e91f44e7f5de3c9f452ee43df0d741dad2be18dd1f5c55f000d4f0` | `75f74c8d5889efd6536314908b7c5fc3fbb20b1d778b1a48c2928f82241a5729` |
| Typed diar / primary records | `755 / 1348` | `755 / 1348` |
| Typed ASR / align records | `275 / 275` | `275 / 275` |
| Typed voiceprint records | `16072` | `16083` |
| Candidate business records | `1750` | `1760` |
| Repeated candidate SHA-256 | `707454f6c0c5d2def2fe76f965047c9fc64babff205d9405f379136aa98473ad` | `1406013fb0d27f97095d34be112806e65ec8692d102926e7715889066794eb86` |

Each repeated replay was byte-identical. Record counts and hashes are
mechanical evidence only.

## Complete Changed-Context Review

The final bounded rule changes only two conversation regions in either run.
The display tool lists four intersecting reference rows because three source
rows share the first region.

- `ref-0070` remains Tang Yunfeng: its leading `有` stays `spk_1`.
- `ref-0071` is repaired: the substantive `四十四十五` contribution is restored
  to Shi Yi / `spk_3` instead of being flattened into Tang Yunfeng's phrase.
- `ref-0072` remains Tang Yunfeng: `那你可以否决了` stays `spk_1`.
- `ref-0310` remains a Tang Yunfeng turn and is locally improved: the
  substantive `我问你个事儿` becomes `spk_1`; only the final discourse particle
  remains with Shi Yi before Tang continues the same valuation question.

The surrounding `ref-0065` through `ref-0077` and `ref-0305` through
`ref-0315` contexts preserve the natural exchange and the following handoffs.
The reverse read reached the same judgments. Run B exposes the same source
ranges and speaker sequences as Run A. No other reference context changes.

Relative to the previously signed frozen-replay natural-turn judgment, the
manual result would repair one previously incorrect contribution in each run
without introducing a new incorrect contribution. This is candidate evidence,
not a replacement for a new full real-WebSocket forward/reverse review.

## Engineering Verification

- Focused handoff and subminimum-churn C++ cases pass alongside all existing
  fusion-policy coverage.
- Complete build finished without warnings or errors.
- All `68/68` registered CTest entries passed.
- Candidate server binary SHA-256 before the documentation-only commit is
  `452e95d11aea4d748cc5b295cb33175f43ff462661f5d8ca73bba0ab063a61be`.
- Frozen TOML SHA-256 remains
  `d6056d75dc1ac72569a11b1d96b78edb6b8f7bf5fa310c9f6f07cbd49f439538`.

## Promotion Boundary

FR16ABM is retained for real-path validation. It is not yet an accepted
closeout result. T106 must run the new binary through 120-second and 600-second
incremental real-WebSocket fixtures, then recapture full direct-end A/B and
complete the full 556-contribution forward/reverse contextual semantic review.
T102 and T084 remain open independently.
