# Cross-View Handoff Projection Review

**Date:** 2026-07-18
**Status:** Frozen projection retained; real-WebSocket promotion remains T128
**Scope:** FR29 projection only; no producer track or TOML value changed

## Evaluation Authority

`test/data/reference/test.txt` is the human-listened reference. The export,
replay, hashing, and review-packet tools captured immutable evidence, checked
source reconstruction and deterministic bytes, and displayed affected
contexts. They did not assign correctness, calculate or estimate accuracy,
rank a candidate, or issue a promotion verdict. The product judgment below was
made by reading every displayed change in its complete surrounding conversation
chronologically and again in reverse order.

## T126 Finding

The corrected 600-second real-WebSocket artifact passes its mechanical
contracts. Complete forward and reverse review of all 93 in-scope reference
contributions restores Tang Yunfeng's substantive `ref-0037` sentence, but
`ref-0073` remains wrong: Shi Yi's response is rendered as Tang/Shi/Tang.

The producer tracks retain useful evidence at the second failure:

- Sortformer activity exposes Shi Yi through `496.720 s` and Tang Yunfeng from
  `496.880 s`.
- Primary top-1 exposes Shi Yi through `496.480 s` and Tang Yunfeng from
  `496.880 s`.
- One forced-alignment unit starts at `495.868 s` and is stretched through
  `497.148 s`; the next source units collapse onto that endpoint.
- The base projection therefore joins `对，四十五。` to the following Tang
  run, and a containing business-interval voiceprint writes its Tang majority
  across both native identities.

The accepted T111 evidence places the same substantive response before the
handoff. The regression is therefore in projection robustness, not a missing
diarization speaker transition.

## FR29 Contract

`BusinessSpeakerPipeline` now derives one shared handoff object only when:

- activity and primary agree on both the preceding and following identities;
- their following onsets agree within the existing TOML alignment tolerance;
- both following runs meet the existing minimum evidence duration;
- neither view contains a competing identity after the onset;
- at least one view exposes the existing minimum pause before the onset; and
- one alignment unit longer than that pause straddles the onset.

The base projector ends the preceding source run after that crossing unit. A
zero-duration follower remains with it only when it closes at an existing TOML
punctuation boundary before the next aligned unit. The fusion policy reuses the
same handoff object and prevents a containing `business_interval` majority from
overwriting the competing base run when both native views cover its aligned
content for the existing minimum duration. Missing or ambiguous evidence keeps
the prior behavior. No producer record, timestamp, model value, or TOML value
is modified.

## Frozen Evidence

| Evidence | Value |
|---|---|
| T126 source timeline SHA-256 | `3b97659a77277587c68ad2248e107181ba3f9a3755f6a4fef6a519fe24fcb7b7` |
| Typed diar / primary records | `98 / 166` |
| Typed ASR / align units | `52 / 2051` |
| Typed voiceprint records | `2472` |
| Candidate business records | `196` |
| Repeated candidate SHA-256 | `8d6b75b435bc263e184dd5138870c1e0d7b4633f527daf667f0564e38ea13f15` |
| Production replay probe SHA-256 | `a3072299c4d4f5a450062221c821a894151442ceecd2fb1d0a3cfc5021364ab0` |
| Frozen TOML SHA-256 | `c3c1acbc723fb3f0d600625025023edb51750fb1b51731342bc893dd2d3273b1` |

Three independently written candidate files are byte-identical. These counts
and hashes are mechanical evidence only.

## Complete Changed-Context Review

The exact-boundary display lists five reference contexts. `ref-0019`,
`ref-0078`, and `ref-0079` retain the same speaker sequence and conversational
ownership; only replay precision or audit provenance is displayed as changed.

The two speaker-sequence changes form one continuous exchange:

- `ref-0073`: the candidate retains Shi Yi for `否决了，对，四十五。` instead
  of assigning `对，四十五` to Tang Yunfeng. The very short aligned onset
  remains attached to Tang, as in the accepted T111 boundary, but the complete
  substantive response is again Shi Yi.
- `ref-0074`: the clause-closing punctuation stays with Shi Yi, then Tang
  Yunfeng begins with `嗯，如果说我和你加起来也能否决我`. The candidate
  does not carry Shi Yi into Tang's following contribution.

Reading `ref-0067` through `ref-0082` in order and then from the later response
back to the earlier question reaches the same judgment. Reading the complete
`ref-0018` through `ref-0023` context confirms that the other displayed range
is unchanged. No other frozen 600-second conversation context changes.

## Engineering Verification

- The focused C++ matrix covers the retained topology plus one-view,
  identity-disagreement, subminimum-following, normal-unit, insufficient-native-
  coverage, unaligned-source, and existing dual-short-gap abstentions.
- The full build completes with no warning or error; GCC emits ABI notes only.
- All `69/69` registered CTest entries pass in `53.02 s` after the final full
  relink.
- `git diff --check` passes.

## Result

The frozen FR29 projection is retained for real-path promotion because complete
forward and reverse contextual review restores the failed contribution without
introducing a new changed-context speaker error. This does not advance the
accepted T111 full-session baseline and does not close a 600-second or full
gate. T128 must recapture two independent 120-second production WebSocket runs
and one clean 600-second run before T123 can start.
