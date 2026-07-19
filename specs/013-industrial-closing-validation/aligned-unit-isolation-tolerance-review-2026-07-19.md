# FR35 Aligned-Unit Isolation Tolerance Review (2026-07-19)

## Scope and authority

This review evaluates FR35 only on the frozen T111 and T123 typed producer
tracks captured by T142's completed real-WebSocket promotion. FR35 changes the
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

- Focused FR35 tests admit an isolated no-embedding aligned unit only when a
  neighbouring gap reaches the existing TOML isolation pause after adding at
  most the existing alignment boundary tolerance. They preserve abstention for
  an out-of-tolerance gap, zero tolerance, missing evidence, mismatched
  identities, weak or disagreeing VAD, and an independently embedded unit.
- A clean full build emits no `warning:` or `error:` line. Its log SHA-256 is
  `e105abe5df39ae6539660c68116435ec9f3a76a1b0ce8b2b768fb13990c62c3f`.
- All `69/69` CTest entries pass. The CTest log SHA-256 is
  `68095fa00254a39574a95aa4079b468fd3817ea60c0af14488d7897d35255e2c`.
- Repeated T123 replay SHA-256:
  `e068418430423db26970d674e841455133dd69bdd3c1c83d09dd23413985ad4f`.
- Repeated T111 replay SHA-256:
  `646ea91b357cafaf8af82c4f45e5cc771c622a501e71b12f2d1aa1555fb055f2`.
  T111 is byte-identical to its FR34 replay.
- The production replay probe SHA-256 is
  `0c8351fc2e944fa34dbcb8f67936c84c0dfcc32931501fd8a17be263b00bd9aa`;
  the checked-in TOML SHA-256 is
  `c3c1acbc723fb3f0d600625025023edb51750fb1b51731342bc893dd2d3273b1`;
  and the human reference SHA-256 is
  `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86`.
- Removing only sequential replay IDs for display exposes one T123 content
  change. The former `2809.436-2809.676` `嗯，` entry under Tang Yunfeng is
  replaced by `2809.436-2809.676` `嗯` under Zhu Jie with
  `voiceprint_aligned_unit_isolated_initial_slot_vad_override`; the remaining
  `2809.676-2809.916` punctuation-only source stays under Tang Yunfeng. No
  other normalized text, identity, reason, or time range changes.

These are engineering and evidence-arrangement facts only. They do not assign
speaker correctness.

## Complete forward contextual review

The forward pass reads the entire displayed conversation from `46:19` through
`47:53`. Tang Yunfeng directs the group to write the item according to
Hangzhou. Zhu Jie gives the isolated acknowledgement `嗯`. Shi Yi immediately
asks whether it should be written according to Hangzhou, and Zhu Jie explains
that the Hangzhou convention can be used because the subject is registered
there. The discussion then continues into financial and legal wording without
a speaker handoff caused by FR35.

FR35 restores only Zhu Jie's acknowledgement between Tang's instruction and
Shi's confirmation question. It preserves Tang's preceding instruction, Shi's
following question, Zhu's explanation, and every later contribution in the
displayed context. The punctuation-only residual has no spoken lexical content
and remains visible in the source-time view rather than being hidden.

## Complete reverse contextual review

The reverse pass starts from the later financial and legal wording, returns
through Zhu Jie's explanation of the Hangzhou registration, and then reaches
Shi Yi's confirmation question. Immediately before that question is Zhu Jie's
short acknowledgement; Tang Yunfeng's instruction precedes it. Reading in this
direction confirms the same Tang-to-Zhu-to-Shi-to-Zhu conversational sequence,
and the FR35 write does not spill into either neighbouring substantive turn.

## Decision and remaining gates

FR35 is retained on frozen evidence. `ref-0420` changes from confident-wrong to
accepted at the natural-contribution semantic level. T111 and every other T123
contribution remain unchanged.

Applying this one reviewed change to the already complete FR34 ledger yields
509 accepted and 47 incorrect contributions: 40 confident-wrong, six missing,
and one uncertain. Twenty-nine confident-wrong and two missing contributions
remain critical. The fixed blocks are `86/93`, `79/84`, `74/80`, `74/80`,
`116/129`, `77/87`, and `3/3`; the 2400-3000 and 3000-3600 blocks still fail.
Speaker results are Zhu Jie `71/83`, Tang Yunfeng `173/189`, Xu Zijing `67/73`,
and Shi Yi `198/211`. The frozen natural-turn result is `509/556`, about
`91.55%`.

This is not a new real-WebSocket full result. Zhu Jie recall, both late fixed
blocks, critical attribution, confident-wrong attribution, speaker-time
sign-off, and independent holdout evidence remain open. FR35 is therefore a
bounded transitional repair, not speaker-business closure.
