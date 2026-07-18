# Cross-View Handoff Projection Review

**Date:** 2026-07-18
**Status:** Real-WebSocket 120/600 promotion passed; full T123 remains open
**Scope:** FR29 projection only; no upstream model track or TOML value changed

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

The frozen T111 evidence places the same substantive response before the
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
  remains attached to Tang, as in the retained T111 boundary, but the complete
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
introducing a new changed-context speaker error. This frozen result alone does
not advance the frozen T111 full-session comparison.

## T128 Real-WebSocket Gate

Clean commit `2ce4a12b7973381be68a22028e4b01b5d1e645a7` was built once and
used for two independent 120-second captures and one 600-second capture. Every
run used `test.mp3`, 100 ms incremental frames at 1.0x pacing, Sortformer v2.1,
an isolated empty registry and protocol store, direct `end`, early/transient/
late observers, runtime telemetry, and `tegrastats`. Each manifest records a
clean unchanged Git commit, unchanged TOML source, and unchanged server binary.
Only registry and storage paths differ from checked-in `orator.toml`.

| Evidence | 120 A | 120 B | 600 |
|---|---:|---:|---:|
| Audio | `120.000 s` | `120.000 s` | `600.000 s` |
| Total wall time | `121.21 s` | `121.21 s` | `603.33 s` |
| Direct terminal wait | `1.213 s` | `1.208 s` | `3.329 s` |
| Runtime / tegrastats samples | `119 / 120` | `119 / 120` | `599 / 601` |
| Common-clock final samples | `1,920,000` | `1,920,000` | `9,600,000` |
| Extent gaps | `0` | `0` | `0` |

The two 120-second seven-track entry bundles have the same SHA-256,
`e8613dfbdffbbb3394d5e80955eb73d30e7f200c4ffb4b3058df3a1b805928b8`.
Complete chronological and reverse reading of `ref-0001` through `ref-0018`
finds no new contextual speaker regression. Existing cold-start, micro-turn,
and boundary defects remain visible and are not relabeled as correct.

The 600-second artifact is
`/tmp/orator-spec013/release-2ce4a12-t128/run-600-ws.json`, SHA-256
`47567fa23ba58b49d4dde58ba1e46a1f0936765395122e115997a568a58aeeae`.
All observer terminal views converge, required telemetry coverage passes, and
all seven extents close on the common sample count. These are mechanical
contracts only.

## Derived Evidence Clarification

The real capture exposed one stale assumption in the frozen-review contract.
Diarization, primary-speaker, ASR, VAD, and alignment entries are byte-identical
to T126. Speaker voiceprint is not wholly upstream of the business projection:
`SpeakerEvidenceSnapshot` intentionally includes `business_speaker`, and
`SpeakerEvidenceStage::BuildVoiceprintQueries()` uses those entries after the
final primary-speaker deposit to form acoustic query ranges.

FR29 changes the base source partition at the corroborated handoff. The final
evidence stage consequently replaces the old containing
`business_interval:43:11` (`source 50-73`, `495.788-498.908 s`) with two derived
queries (`source 50-56`, `495.788-497.148 s`; and `source 56-73`,
`497.308-498.908 s`). Later business-interval ordinals for that text shift by
one; their acoustic spans and scores remain unchanged. This is the intended
one-pass base-projection-to-acoustic-evidence dependency, not an upstream model
mutation or a feedback cycle. The specification and code comments now state
that contract explicitly. A focused unit contract verifies that two base
business source runs generate two derived query ranges and that joining those
runs generates one range. The final full build emits no warning or error, and
all `69/69` CTest entries pass in `53.13 s`.

## Complete 600-Second Context Review

Every in-scope contribution, `ref-0001` through `ref-0093`, was read first in
chronological order and then from `ref-0093` back to `ref-0001`. The complete
conversation preserves the same known cold-start, missed micro-turn, and
cross-boundary defects seen in T126. The two speaker-sequence changes are one
continuous exchange:

- `ref-0073`: Shi Yi owns the substantive answer `否决了，对，四十五。`; the
  very short aligned onset remains attached to the preceding Tang Yunfeng run.
- `ref-0074`: Tang Yunfeng begins at `嗯，如果说我和你加起来...`; Shi Yi is
  not carried into the following contribution.

The reverse read confirms both handoff directions. `ref-0037` remains
substantively assigned to Tang Yunfeng. No other contextual speaker regression
is introduced. This manual semantic result passes T128's 600-second gate; it
does not calculate an accuracy percentage and does not advance the accepted
full-session T111 baseline. T123 remains the next full-session gate.
