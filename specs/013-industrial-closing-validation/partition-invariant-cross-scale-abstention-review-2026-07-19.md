# Partition-Invariant Cross-Scale Abstention Review (2026-07-19)

## Scope and authority

This review evaluates FR33 with the frozen T111 and T123 typed packages after
T142 completed FR32's full real-WebSocket promotion. FR33 changes only the
final speaker-fusion policy. It does not rerun audio or modify diarization,
primary speaker, ASR, VAD, forced alignment, voiceprint evidence, TOML, or the
common time base.

`test/data/reference/test.txt` remains the authoritative human-listened
reference. No executable mechanism assigned correctness, aggregated accuracy,
ranked the candidate, or issued the verdict. The production C++ replay probe
executed the projector; mechanical tooling checked hashes and immutable source
contracts and displayed the changed evidence. The reviewer read the complete
surrounding conversation forward and reverse before deriving the decision.

## Engineering and replay evidence

- The focused `test_business_speaker_pipeline` positive case preserves a
  uniform native identity only for the complete FR33 topology.
- Focused abstention cases cover changed phrase rank, absent/duplicate or
  incomplete VAD, missing VAD reversal, absent/duplicate broad evidence,
  changed broad top pair or margin state, competing activity, and insufficient
  alignment.
- Repeated T123 replay SHA-256:
  `c1b3622c36daa34537984ed8036e45a40199a4612ba3a4590dc2f02a3d7e172e`.
- Repeated T111 replay SHA-256:
  `646ea91b357cafaf8af82c4f45e5cc771c622a501e71b12f2d1aa1555fb055f2`.
- T111 is byte-identical to the FR32 replay. The T123 source reconstruction,
  time order, input tracks, and entry count remain unchanged; only
  `text_id=289` has a changed speaker sequence.
- The changed-context forward and reverse worksheets each have SHA-256
  `1f87f3492112e5a326d46dab42c537636cd6e9d4c9d29092f15a21bb578c8a5f`
  and display `ref-0516` plus `ref-0517` because the changed phrase crosses
  their one-second reference boundary. This is evidence arrangement only.

These facts establish determinism and scope. They do not establish
correctness.

## Complete contextual review

The reviewer read `ref-0508` through `ref-0525` in chronological order. Shi Yi
finishes the company-location correction in `ref-0516`. The changed phrase
begins at `3378.604`, crosses the reference boundary at `3379`, and asks
`你没什么意见`. It continues with `美国公司怎么玩` under the same Tang Yunfeng
identity in `ref-0517`. FR32 had assigned the first phrase to Zhu Jie while
leaving the second with Tang. FR33 keeps both parts with Tang.

The reverse reading starts from Tang's later shell-company explanation, passes
Xu Zijing's restructuring comment, Shi Yi's deferral, Tang's `随哪边`, Zhu
Jie's `老师最有发言权`, and returns to the question. In that direction the first
phrase is still Tang's question between Shi Yi's correction and Zhu Jie's
response. No preceding Shi contribution, following Zhu response, or later Tang
contribution changes.

`ref-0516` appears in the mechanical changed worksheet only because its source
interval ends at `3379.000` while the runtime phrase begins at `3378.604`.
Semantic context keeps Shi Yi's complete contribution unchanged and assigns
the overlapping question to `ref-0517`. This is an accepted source-time offset,
not a new speaker error.

## Decision

FR33 is retained on frozen evidence. It repairs `ref-0517` and introduces no
changed-context regression in T123; T111 remains byte-identical. Applying this
single reviewed change to the already complete T142 ledger gives a frozen
candidate result of 507 accepted and 49 incorrect contributions, manually
reconciled as 42 confident-wrong, six missing, and one uncertain. Thirty
confident-wrong and two missing contributions remain critical.

The frozen block affected by FR33 moves from `76/87` to `77/87` in
3000-3600, and Tang Yunfeng moves from `171/189` to `172/189`. The full frozen
candidate is `507/556`, about `91.19%`. These manual results do not constitute a
new real-WebSocket full run. More importantly, 2400-3000 and 3000-3600 still
fail, Zhu Jie remains `70/83`, and the critical and confident-wrong gates still
fail. Speaker-business closure remains open.

The next step continues frozen evidence work rather than rerunning audio. A
separate FR34 candidate may test whether an exact phrase and its unique
containing VAD, when both galleries independently select the same identity,
may supersede an earlier coarse direct interval. It must be specified,
implemented, replayed, and completely reviewed independently from FR33 before
any combined real-stream promotion.
