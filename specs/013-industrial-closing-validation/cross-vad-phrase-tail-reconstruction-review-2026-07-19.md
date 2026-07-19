# FR38 Cross-VAD Phrase-Tail Frozen Replay Review (2026-07-19)

## Scope and claim boundary

This record completes T159 and T160 for FR38 on the frozen T123 and T111
producer packages. It covers the production projection rule, focused
abstention matrix, warning-clean build, complete CTest suite, deterministic
replay, raw change scope, and complete forward/reverse contextual semantic
review against the human-listened `test.txt`.

No compiled code, test, script, query, formula, metric, or algorithm assigned
correctness, aggregated accuracy, selected FR38, or issued the retention
decision. Automation only executed the projector, checked engineering and
mechanical contracts, and displayed immutable evidence. The contextual reading
below is the sole product-evaluation authority.

FR38 is a frozen-evidence transitional candidate. It is not a new
real-WebSocket full result and does not close speaker attribution, T102, T084,
or Spec 013.

## Engineering and replay evidence

Focused C++ coverage includes the positive three-identity topology and
independent abstention for source shape, tail/following embeddings, following
identity, native local-slot agreement, competing activity, interval and phrase
rank/gate patterns, tail alignment, both VAD uniqueness/rank/gate/boundary
relationships, VAD isolation, and following-interval rank. The positive case
also proves that the separately embedded following clause remains unchanged.

The clean build completed with no `warning:` or `error:` diagnostic, and all
69 registered CTest entries passed.

| Evidence | SHA-256 |
|---|---|
| Clean build log | `185d2b7538d466774bcf00968f017e58771e77076a37d67a7886975dbd77b24b` |
| CTest log | `3573a37503687d21195d26f2f6082ddc37928da72629a4491c627fc9a058ddb1` |
| Replay probe | `976dc94c269c6650a92e21380066e8ee856b4906721800e98e37248fe1e072b4` |
| Checked-in TOML | `c3c1acbc723fb3f0d600625025023edb51750fb1b51731342bc893dd2d3273b1` |
| Human reference | `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86` |

Repeated T123 projector outputs are byte-identical at SHA-256
`090e2cb5b859e4f8df5ba4e2fb014a5c65180e9c945a4663883bf0759a08305a`.
Repeated T111 outputs are byte-identical at SHA-256
`646ea91b357cafaf8af82c4f45e5cc771c622a501e71b12f2d1aa1555fb055f2`
and remain byte-identical to FR37.

After omitting only the mechanically shifted `turn_id` sequence, the T123
display exposes one source split and no other content, identity, reason, or
time change:

- before FR38, `3301.164-3303.964` `着，对，我说他在杭州占比也不大，问题不大。`
  is one `spk_3` sole-support record;
- after FR38, only `3301.164-3301.244` `着，` becomes `spk_1` with reason
  `voiceprint_cross_vad_phrase_tail_reconstruction`; and
- the following `3301.404-3303.964` `对，我说他在杭州占比也不大，问题不大。`
  remains `spk_3` with its original `sole_diar_support` reason.

The additional boundary raises the mechanical record count from 1709 to 1710.
That count is not an accuracy result.

## Complete contextual semantic review

The reviewer read the complete displayed `53:49-55:45` conversation in
chronological order and then reread the same evidence from the end back to the
start. The human reference establishes the following sequence:

- Zhu Jie finishes the nominee-ownership proposal and says the other side's
  stake is small;
- Tang Yunfeng corrects him that the person's stake on this side is large and
  then continues with the Hangzhou-stake statement;
- Tang reacts and states that he is waiting for his `5.6` hundred million;
  and
- Shi Yi resumes with the domestic/overseas operating strategy.

Before FR38, T123 already assigns `就在这边站` to Tang but assigns the aligned
final `着，` to Shi together with the next clause. The user-facing phrase is
therefore split across unrelated known identities. FR38 restores only the
source-final `着，` to Tang, yielding the complete recognizable phrase
`就在这边站着，` under Tang. Reverse reading reaches the same boundary: the
following Hangzhou-stake clause remains separately assigned to Shi, so FR38
does not conceal or repair the existing `ref-0505` error.

The preceding sustained nominee proposal remains assigned predominantly to
Shi and therefore remains the existing `ref-0503` error. The later interjection,
`5.6` hundred million statement, and strategy/confirmation boundaries are also
unchanged. No neighbouring contribution gains a new wrong identity from FR38.

## Manual decision and ledger

FR38 is retained on frozen evidence. Complete context changes `ref-0504` from
one critical confident-wrong contribution to accepted. No other reference
contribution changes judgment.

Applying that one manually reviewed change to the complete FR37 ledger yields
512 accepted and 44 incorrect contributions: 37 confident-wrong, six missing,
and one uncertain. Twenty-six confident-wrong and two missing contributions
remain critical. The fixed blocks are `86/93`, `79/84`, `74/80`, `74/80`,
`117/129`, `79/87`, and `3/3`; every complete 600-second natural-turn block now
passes its 90 percent floor. Speaker results are Zhu Jie `73/83`, Tang Yunfeng
`174/189`, Xu Zijing `67/73`, and Shi Yi `198/211`. The manually reconciled
frozen natural-turn result is `512/556`, about `92.09%`.

This is not speaker-business closure. Zhu Jie natural-turn recall, critical
attribution, confident-wrong attribution, speaker-time and per-speaker-time
sign-off, source-time-offset sign-off, independent full real-WebSocket
repeatability, and locked holdout evidence remain open.
