# FR40 Two-Unit Primary Handoff Frozen Replay Review (2026-07-19)

## Scope and claim boundary

This record completes T165 and T166 for FR40 on the frozen T123 and T111 typed
producer packages. It covers the production projection rule, focused
abstention matrix, warning-clean build, complete CTest suite, deterministic
replay, raw change scope, and complete forward/reverse contextual semantic
review against the human-listened `test.txt`.

No compiled code, test, script, query, formula, metric, or algorithm assigned
correctness, aggregated accuracy, selected FR40, or issued the retention
decision. Automation only executed the projector, checked engineering and
mechanical contracts, and displayed immutable evidence. The contextual reading
below is the sole product-evaluation authority.

FR40 is a frozen-evidence transitional candidate. It is not a new
real-WebSocket full result and does not close speaker attribution, T102, T084,
or Spec 013.

## Engineering and replay evidence

Focused C++ coverage includes both source partitions: T111's two reactions in
one source tail and T123's same reactions across adjacent sources. Independent
abstention cases remove direct-write provenance, no-embedding provenance,
one-character source shape, either punctuation boundary, VAD uniqueness,
strict containment, robust completeness, short duration, exact two-unit
membership, configured lower/upper gap handling, boundary tolerance, each
primary run, primary identity/order/uniqueness, activity coverage/uniqueness,
VAD top-two identities, score gate, margin gate, source adjacency, and current
pair membership. Positive cases prove that the later visible source text is
not repainted.

The clean build completed with no `warning:` or `error:` diagnostic. GCC emitted
only existing ABI `note:` lines. All 69 registered CTest entries passed.

| Evidence | SHA-256 |
|---|---|
| Clean build log | `01f286d84d65d243e3f5554c08f735a783d547782c14a1c5520408d1f1e45eef` |
| CTest log | `0874083f7900313b91a3c24762b7ffbd0e1eb5611238b36f7a23149dcc1ce1ce` |
| Clean replay probe | `47c598e861a5d8421df2d91080ed717edd32c75f5832f59c28d084c87ed4d66d` |
| Checked-in TOML | `c3c1acbc723fb3f0d600625025023edb51750fb1b51731342bc893dd2d3273b1` |
| Human reference | `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86` |

Repeated T123 projector outputs are byte-identical at SHA-256
`47b43e59afbc2e10776b7e7891712bd79437b97d8b19660de6738d5488509cc4`.
Repeated T111 outputs are byte-identical at SHA-256
`ad2abee782ab30ff67be1a86fa46f4ec0c16b5422ae18954107c398131157aa4`.

After omitting only mechanically shifted `turn_id` values, the complete raw
change display contains the following and nothing else:

- T123 changes `184.240-184.320` `嗯，` from `spk_0`/Zhu
  `voiceprint_direct_regular` to `spk_2`/Xu with the FR40 reason. The record
  count remains `1711`.
- T111 splits one `183.740-184.300` `啊？嗯。` record under `spk_2`/Xu into
  `183.740-183.900` `啊？` under `spk_0`/Zhu with the FR40 reason and the
  unchanged `184.220-184.300` `嗯。` under `spk_2`/Xu. The mechanical record
  count rises from `1751` to `1752`.

These counts and structural comparisons are not accuracy results.

## Complete contextual semantic review

The reviewer read the complete displayed `02:07-03:45` conversation in
chronological order and then reread the same T123 and T111 evidence from the
end back to the start. The human reference establishes the material sequence:

- Zhu Jie explains the equity sacrifice and asks Shi Yi about the calculated
  twenty-eight-percent share;
- Shi Yi and Tang Yunfeng work through the remaining percentages;
- Tang concludes that he can receive seven percent;
- Zhu reacts `啊？`;
- Xu Zijing gives the intervening `嗯？`;
- Zhu asks why the result is seven; and
- Tang explains the fifteen-percent premise before Zhu resumes his proposal.

T123 already preserves Zhu's first reaction, but assigns Xu's intervening
response to Zhu from the wider following interval. FR40 restores only that Xu
response. T111 instead merges both reactions and assigns the pair to Xu; FR40
restores only Zhu's first reaction and preserves Xu's second. Forward and
reverse readings therefore converge on the same Zhu-to-Xu-to-Zhu speaker
sequence in both ASR partitions.

No neighboring percentage statement, Zhu follow-up question, Tang answer, or
later proposal changes. ASR wording differs between T111 and T123, but this
review concerns speaker attribution only.

## Manual decision and ledger

FR40 is retained on frozen evidence. In the current T123-derived frozen
candidate, complete context changes only `ref-0025` from one noncritical
confident-wrong contribution to accepted. `ref-0024` was already accepted in
T123. The T111 `ref-0024` repair establishes partition consistency and is not
counted a second time in the current ledger.

Applying that one manually reviewed current-candidate change to the complete
FR39 ledger yields 514 accepted and 42 incorrect contributions: 35 confident-
wrong, six missing, and one uncertain. Twenty-five confident-wrong and two
missing contributions remain critical. The fixed blocks are `87/93`, `79/84`,
`74/80`, `74/80`, `117/129`, `80/87`, and `3/3`; every complete 600-second
natural-turn block passes its 90 percent floor. Speaker results are Zhu Jie
`74/83`, Tang Yunfeng `174/189`, Xu Zijing `68/73`, and Shi Yi `198/211`. The
manually reconciled frozen natural-turn result is `514/556`, about `92.45%`.

This is not speaker-business closure. Zhu Jie natural-turn recall remains below
90 percent, and critical attribution, confident-wrong attribution, speaker-time
and per-speaker-time sign-off, source-time-offset sign-off, independent full
real-WebSocket repeatability, and locked holdout evidence remain open.
