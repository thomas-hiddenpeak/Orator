# FR36 Partition-Invariant Regular Initial-Slot Review (2026-07-19)

## Scope and authority

This review evaluates FR36 only on the frozen T111 and T123 typed producer
tracks captured by T142's completed real-WebSocket promotion. FR36 changes the
final speaker-fusion policy; it does not rerun audio or alter diarization,
primary speaker, ASR, VAD, forced alignment, voiceprint evidence, TOML, or the
common time base.

`test/data/reference/test.txt` is the authoritative human-listened reference.
No executable mechanism assigned correctness, aggregated accuracy, ranked the
candidate, or issued the verdict. The production C++ replay probe executed the
projector. Mechanical tools checked hashes, immutable inputs, deterministic
output, source/time structure, and displayed the complete changed conversation.
The reviewer read that conversation chronologically and in reverse before
making the judgment below.

## Engineering and replay evidence

- Focused FR36 tests require the complete regular phrase, native same-slot
  activity/primary coverage, a different initial identity, the exact two-view
  gate pattern, and the unique VAD plus complete-source four-view reverse
  pattern. Independent tests preserve abstention for missing or mismatched
  identity, primary, activity, alignment, phrase rank or gates, VAD evidence,
  complete-source evidence, and protected current labels.
- A clean full build emits no `warning:` or `error:` line. Its log SHA-256 is
  `d912b3196c765e4058c777a8053dbabc962b6bb272a8a390bfe5db70eba22a92`.
- All `69/69` CTest entries pass. The CTest log SHA-256 is
  `d2cf39802954a2bb54f31a4b84bb6b37c94072047950f3eb7afae1edcefa489d`.
- Repeated T123 replay SHA-256:
  `6a34e272cd64c2513358c55f9c41b390bc17a4d048fd32ba1147c6b2766833cb`.
- Repeated T111 replay SHA-256:
  `646ea91b357cafaf8af82c4f45e5cc771c622a501e71b12f2d1aa1555fb055f2`.
  T111 is byte-identical to its FR35 replay.
- The production replay probe SHA-256 is
  `3c19f83635de79e7df7f6cac3ba1bde2d564f5927c4035032cb2d97f984b53da`;
  the checked-in TOML SHA-256 is
  `c3c1acbc723fb3f0d600625025023edb51750fb1b51731342bc893dd2d3273b1`;
  and the human reference SHA-256 is
  `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86`.
- Direct raw diff exposes one T123 change. The
  `2483.660-2485.500` `而且我觉得这个这个点儿听起来还不错。` entry moves
  from Tang Yunfeng `sole_diar_support` to Zhu Jie
  `voiceprint_phrase_partition_invariant_regular_initial_slot_override`. No
  other text, identity, reason, source boundary, or time range changes.

These are engineering and evidence-arrangement facts only. They do not assign
speaker correctness.

## Complete forward contextual review

The forward pass reads the complete displayed conversation from `40:45`
through `42:32`. Shi Yi explains the tax consequences if the company becomes
profitable. Tang Yunfeng challenges that framing and says genuine profitability
would be good. Zhu Jie agrees and says that this point sounds good. Tang asks
`哪个？`, Zhu answers that he means operating as an independent company, and
Tang explains that separation requires an independent company. Zhu agrees,
then the discussion moves to a possible Singapore entity, financing rounds,
and later restructuring.

FR36 assigns only Zhu Jie's first response to Zhu. It preserves Tang's
profitability question, Tang's immediate follow-up question, Zhu's explanation,
Tang's response, and every later contribution. The changed phrase is a complete
semantic response, not a boundary fragment from either Tang neighbour.

## Complete reverse contextual review

The reverse pass starts from the later adviser-cost and restructuring exchange,
returns through the Singapore-entity discussion, and reaches Tang's explanation
that separation requires an independent company. Before that, Zhu identifies
the independent-company point in response to Tang's `哪个？`; the earlier Zhu
statement introduces the point after Tang's profitability challenge. Reading
in this direction confirms the same Tang-to-Zhu-to-Tang-to-Zhu-to-Tang turn
sequence, and the FR36 write does not spill into either adjacent question.

## Decision and remaining gates

FR36 is retained on frozen evidence. `ref-0350` changes from confident-wrong to
accepted at the natural-contribution semantic level. It was a critical
business contribution. T111 and every other T123 contribution remain
unchanged.

Applying this one reviewed change to the already complete FR35 ledger yields
510 accepted and 46 incorrect contributions: 39 confident-wrong, six missing,
and one uncertain. Twenty-eight confident-wrong and two missing contributions
remain critical. The fixed blocks are `86/93`, `79/84`, `74/80`, `74/80`,
`117/129`, `77/87`, and `3/3`; the 2400-3000 block now passes, while the
3000-3600 block still fails. Speaker results are Zhu Jie `72/83`, Tang Yunfeng
`173/189`, Xu Zijing `67/73`, and Shi Yi `198/211`. The frozen natural-turn
result is `510/556`, about `91.73%`.

This is not a new real-WebSocket full result. Zhu Jie recall, the final full
600-second block, critical attribution, confident-wrong attribution,
speaker-time sign-off, and independent holdout evidence remain open. FR36 is
therefore a bounded transitional repair, not speaker-business closure.
