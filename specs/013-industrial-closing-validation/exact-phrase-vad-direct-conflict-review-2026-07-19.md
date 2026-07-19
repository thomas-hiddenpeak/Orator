# Exact Phrase/VAD Direct-Conflict Review (2026-07-19)

## Scope and authority

This review evaluates FR34 only on the frozen T111 and T123 typed producer
tracks already captured by T142's completed real-WebSocket promotion. FR34
changes the final speaker-fusion policy; it does not rerun audio or alter
diarization, primary speaker, ASR, VAD, forced alignment, voiceprint evidence,
TOML, or the common time base.

`test/data/reference/test.txt` is the authoritative human-listened reference.
No executable mechanism assigned correctness, aggregated accuracy, ranked the
candidate, or issued the verdict. The production C++ replay probe executed the
projector. Mechanical tools checked hashes, immutable inputs, deterministic
output, source/time structure, and displayed the changed conversation. The
reviewer read that complete conversation chronologically and in reverse before
making the judgment below.

## Engineering and replay evidence

- The FR34 focused case changes only the exact phrase when current-label,
  business-interval, phrase, VAD, activity, and primary conditions are all
  present. Its abstention cases cover missing/duplicate containment, each
  gallery failure or disagreement, missing embedding/gallery/alignment,
  incomplete activity, missing/ambiguous primary, non-distinct identities,
  extra activity, and a protected current label.
- A clean full build emits no `warning:` or `error:` line. Its log SHA-256 is
  `cb890415bf9fdd36834725881185f75d9893f13fe9308ea23a0ed623fe1df866`.
- All `69/69` CTest entries pass. The CTest log SHA-256 is
  `329632874da10efb3e0e96887afaa2e2fddd7ccadaa83e06d81a7006b01d5e04`.
- Repeated T123 replay SHA-256:
  `11d30935c940f155fb4b2089134b03b7ce08edb659cb1ae924122e46eea3919b`.
- Repeated T111 replay SHA-256:
  `646ea91b357cafaf8af82c4f45e5cc771c622a501e71b12f2d1aa1555fb055f2`.
  T111 is byte-identical to its FR33 replay.
- Removing only sequential replay IDs for display exposes one T123 difference:
  the former `2744.940-2746.060` Zhu Jie entry is split into the unchanged
  `2744.940-2745.100` `对，` and a new `2745.180-2746.060`
  `才十个工作日，` Tang Yunfeng entry. No other text, identity, reason, or
  time range changes.
- Forward reference/candidate display SHA-256 values are
  `bd2869fae48aad85bfac841c3a11fb9fe4b88a64ccbd600d71dc18315fd3cd93`
  and
  `23dab16da1b26f85dea994305f6aa70cdccef4a1c4c1446e711bf867c4f9df38`.
  Reverse display values are
  `86451d1bedbb00b25d4ecda0744a6f4a11510bb920409f7c0fd9745d0cdffdac`
  and
  `1b8533639bf656f661806fa11575dc3f7cdd6bc456e384e2aea2e27fd23f3407`.

These are engineering and evidence-arrangement facts only. They do not assign
speaker correctness.

## Complete forward contextual review

The forward pass reads every displayed contribution from `44:28` through
`46:57`. The discussion moves from independent investment offers and term
constraints into the proposed four-hundred-million valuation. Tang Yunfeng
states that funds arriving within ten working days would be acceptable. Zhu
Jie then asks whether the stated term was twelve working days. Tang answers
that it is ten working days, and Zhu immediately continues that the money then
arrives in ten working days. Shi Yi moves the discussion to parallel investor
outreach and the later company structure.

FR34 preserves Zhu Jie's complete question, assigns `才十个工作日，` to Tang
Yunfeng, preserves Zhu Jie's following continuation, and changes no earlier or
later contribution. The ASR token `才` differs from the reference wording but
retains the ten-working-day answer's meaning. The preceding `对，` remains
under Zhu Jie for `0.160 s`; this is a bounded source-time boundary residual.
It does not remove or reverse the substantive answer, but it remains explicit
for speaker-time review.

## Complete reverse contextual review

The reverse pass starts from the later PNP/DD and corporate-structure exchange,
returns through the decision to compare investor offers, then reaches Shi Yi's
parallel-outreach proposal. From that direction, Zhu Jie's statement that the
money would arrive in ten working days follows Tang Yunfeng's ten-working-day
answer, which itself follows Zhu Jie's twelve-working-day question. Continuing
backward reaches Tang's earlier ten-working-day condition and the valuation
discussion. The same three-speaker handoff remains coherent, and no FR34 write
spills into Zhu Jie's question or continuation.

## Decision and remaining gates

FR34 is retained on frozen evidence. `ref-0406` changes from confident-wrong to
accepted at the natural-contribution semantic level. The `0.160 s` `对，`
boundary residual is recorded rather than counted as a perfect speaker-time
boundary. T111 and every other T123 contribution remain unchanged.

Applying this one reviewed change to the already complete FR33 ledger yields
508 accepted and 48 incorrect contributions: 41 confident-wrong, six missing,
and one uncertain. Twenty-nine confident-wrong and two missing contributions
remain critical. The fixed blocks are `86/93`, `79/84`, `74/80`, `74/80`,
`115/129`, `77/87`, and `3/3`; the 2400-3000 and 3000-3600 blocks still fail.
Speaker results are Zhu Jie `70/83`, Tang Yunfeng `173/189`, Xu Zijing `67/73`,
and Shi Yi `198/211`. The frozen natural-turn result is `508/556`, about
`91.37%`.

This is not a new real-WebSocket full result. Zhu Jie recall, both late fixed
blocks, critical attribution, confident-wrong attribution, speaker-time
sign-off, and independent holdout evidence remain open. FR34 is therefore a
bounded transitional repair, not speaker-business closure.
