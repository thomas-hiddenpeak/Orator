# Exact Cross-Scale Primary-Return Review (2026-07-19)

## Scope and authority

This review evaluates FR32 on the frozen T111 and T123 typed packages after the
broader FR31 experiment was rejected. `test/data/reference/test.txt` remains
the human-listened authority. No executable mechanism assigned correctness,
aggregated accuracy, ranked the candidate, or issued the verdict. Tools only
executed the production C++ projector, verified hashes and source contracts,
located changed source IDs, and displayed unjudged evidence.

FR32 permits a broader phrase or complete-source write to preserve a native B
character only when one exact primary-run `business_interval` independently
selects B in both TitaNet galleries under the existing TOML gates. The primary
run must be a short A-B-A return, activity B must cover it completely, and any
third activity identity vetoes the rule. No producer evidence, common-clock
value, TOML value, transcript value, speaker name, or known timestamp changes.

## Engineering evidence

- Focused `test_business_speaker_pipeline` passes the positive contract and
  abstention for partial or third-party activity, wrong topology, missing
  alignment, regular duration, non-exact or duplicate intervals, missing
  embedding, incomplete robust gallery, gallery disagreement, and gate failure.
- The complete build emits no warning or error line.
- All 69 configured CTest entries pass. These are engineering facts only.
- Repeated T123 replay SHA-256:
  `8fb70821df483cf40b28c701b88d38713404472dcb179a2bad10c14d4fd72ef2`.
- Repeated T111 replay SHA-256:
  `646ea91b357cafaf8af82c4f45e5cc771c622a501e71b12f2d1aa1555fb055f2`.
- Mechanical evidence arrangement finds one changed T123 source (`text_id=84`)
  and no changed T111 source. This statement is not a correctness result.

## Complete contextual review

The reviewer read the complete `ref-0150` through `ref-0158` conversation in
chronological order and then reread the same evidence from `ref-0158` back to
`ref-0150`.

Shi Yi asks whether the three rounds include the current round at `ref-0153`.
Tang Yunfeng answers `不含` at `ref-0154` and continues into the current-round
discussion at `ref-0155` and `ref-0156`. Before FR32, the broad phrase write
assigns `不含哦` to Shi Yi. FR32 preserves only the aligned `不含` with Tang
Yunfeng. The preceding Tang explanation, Shi question, following `哦`, later
round discussion, and the Shi turn beginning at `ref-0157` are byte-for-byte
unchanged in the product view.

The reverse pass reaches the same judgment: the changed fragment is exactly the
Tang answer between the Shi question and Tang continuation, with no candidate
change in either neighboring contribution.

## Decision

FR32 is retained for real-WebSocket promotion. It repairs the only changed
context and introduces no changed-context regression in either frozen package.
This is a bounded frozen-development result, not full real-path acceptance and
not speaker-business closure. T142 must still pass the silence, repeated
120-second, complete 600-second, and full empty/frozen-registry WebSocket ladder
with contextual semantic review at every product gate.
