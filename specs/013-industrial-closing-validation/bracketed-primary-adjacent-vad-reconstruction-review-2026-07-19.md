# FR37 Bracketed-Primary Adjacent/VAD Reconstruction Review (2026-07-19)

## Scope and authority

This review evaluates FR37 only on the frozen T111 and T123 typed producer
tracks captured by T142's completed real-WebSocket promotion. FR37 changes the
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

- Focused FR37 tests require the exact interval top-two order, primary island
  and gapless brackets, two covering activity slots, distinct initial identity,
  adjacent phrase boundary and gate pattern, unique containing VAD, and
  complete alignment. Independent tests preserve abstention when any one of
  those contracts changes or disappears.
- A clean full build emits no `warning:` or `error:` line. Its log SHA-256 is
  `807dd9f0c6b911d2ae6c0bc150f8fca06fa49056f423d67440c87e2848f00d6d`.
- All `69/69` CTest entries pass. The CTest log SHA-256 is
  `dfe9ad2ae302cca09bb97dec9366469802640da1c42e4d75cd66566d9e8f89fc`.
- Repeated T123 replay SHA-256:
  `e4e0762d1d2324a555ad92ab268f423acb3c9ee468b2d691efd0f56e7c293e4b`.
- Repeated T111 replay SHA-256:
  `646ea91b357cafaf8af82c4f45e5cc771c622a501e71b12f2d1aa1555fb055f2`.
  T111 is byte-identical to its FR36 replay.
- The production replay probe SHA-256 is
  `51bc7e8a7db69c1d83b48cd69699fc699897742ba3da845bb4f1619754fe7c01`;
  the checked-in TOML SHA-256 is
  `c3c1acbc723fb3f0d600625025023edb51750fb1b51731342bc893dd2d3273b1`;
  and the human reference SHA-256 is
  `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86`.
- Direct raw diff exposes one T123 change. The
  `3075.096-3075.496` `我向国家交。` entry moves from Tang Yunfeng
  `voiceprint_direct_short` to Zhu Jie
  `voiceprint_interval_bracketed_primary_adjacent_vad_reconstruction`. No other
  text, identity, reason, source boundary, or time range changes.

These are engineering and evidence-arrangement facts only. They do not assign
speaker correctness.

## Complete forward contextual review

The forward pass reads the complete displayed conversation from `50:35`
through `52:28`. Shi Yi explains consolidation, dividends, and tax. Zhu Jie
asks whether the parent company pays the parent company. Shi answers that the
payment goes to the state, and Zhu repeats the conclusion. Shi then asks Zhu
whether he pays the state. FR37 restores Zhu's direct answer `我向国家交`.
Shi's following clarification remains under Shi, after which Shi continues the
long explanation of consolidated revenue and Zhu returns to the flexibility of
separating the companies.

The unchanged candidate still contains a short `国家？` boundary fragment under
Tang Yunfeng immediately before Zhu's answer. FR37 neither expands into nor
hides that residual. It restores only the identifiable material Zhu response,
matching the T111 conversation that already received complete contextual
acceptance.

## Complete reverse contextual review

The reverse pass starts from Zhu Jie's later explanation of domestic customers,
returns through the technology-transfer and consolidation discussion, and then
reaches the rapid tax exchange. In reverse, Shi's clarification follows Zhu's
`我向国家交`, which answers Shi's preceding `你向国家交` question. Continuing
backward reaches Zhu's earlier repetition and initial question. The same
Shi-to-Zhu handoff is coherent in both directions, and the FR37 write does not
spill into either Shi contribution.

## Decision and remaining gates

FR37 is retained on frozen evidence. `ref-0478` changes from confident-wrong to
accepted at the natural-contribution semantic level. It was a critical
business contribution. T111 and every other T123 contribution remain
unchanged.

Applying this one reviewed change to the already complete FR36 ledger yields
511 accepted and 45 incorrect contributions: 38 confident-wrong, six missing,
and one uncertain. Twenty-seven confident-wrong and two missing contributions
remain critical. The fixed blocks are `86/93`, `79/84`, `74/80`, `74/80`,
`117/129`, `78/87`, and `3/3`; the 3000-3600 block still fails. Speaker results
are Zhu Jie `73/83`, Tang Yunfeng `173/189`, Xu Zijing `67/73`, and Shi Yi
`198/211`. The frozen natural-turn result is `511/556`, about `91.91%`.

This is not a new real-WebSocket full result. Zhu Jie recall, the final full
600-second block, critical attribution, confident-wrong attribution,
speaker-time sign-off, and independent holdout evidence remain open. FR37 is
therefore a bounded transitional repair, not speaker-business closure.
