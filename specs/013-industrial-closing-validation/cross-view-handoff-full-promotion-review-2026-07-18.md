# FR29 Full Promotion Review (2026-07-18)

> **Reconciled 2026-07-19.** The complete cross-version audit applies the
> report's own material-fragment rule to `ref-0099`, which was split between
> 石一 and 朱杰. The corrected T123 result is `505/556`, not `506/556`; the
> tables below include the correction. See
> `speaker-baseline-reconciliation-2026-07-19.md`.

## Scope and authority

This report completes T123 for clean commit
`2ff9ce3655b2a12e90a5d0def25c0a30f171f2d9`. Run A streamed the complete
`test.mp3` through the production WebSocket path with an empty speaker
registry. Run B restarted the server and used the registry frozen at the end of
Run A. Both runs used the checked-in streaming Sortformer v2.1
`340/1/188/188` profile and direct `end` finalization.

`test/data/reference/test.txt` is the human-listened authority. Each run was
read independently across all 556 reference contributions in chronological
order and again in reverse 600-second block order. The second pass started at
the tail and ended at the beginning so that long-session drift could not be
hidden by the chronological reading order. ASR wording was used only to locate
the contribution; this review judges speaker business attribution.

No compiled code, script, notebook, formula, query, metric, or algorithm
assigned correctness, counted a result, calculated a percentage, compared a
gate, selected a candidate, or issued the verdict. Shell and review tools only
captured the runs, verified mechanical contracts, hashed immutable artifacts,
and displayed reference plus runtime evidence. Every result and gate decision
below was derived and cross-checked manually from complete conversational
context.

## Test summary

| Item | Run A | Run B |
|---|---:|---:|
| Registry | Empty | Run A registry, frozen before restart |
| Audio / duration | `test.mp3` / `3615.120 s` | `test.mp3` / `3615.120 s` |
| Wall time | `3631.741 s` | `3632.699 s` |
| Stream factor | `0.995x` | `0.995x` |
| Diar compute / reported factor | `47.777 s` / `75.667x` | `47.950 s` / `75.393x` |
| ASR compute / reported factor | `1658.833 s` / `2.179x` | `1664.621 s` / `2.172x` |
| Direct-end terminal wait | `16.540 s` | `17.499 s` |
| Runtime / tegrastats samples | `3615 / 3621` | `3615 / 3622` |
| Contract issues | None | None |
| Subjective speaker conclusion | Rejected for promotion | Rejected for promotion |

The rejection is not a runtime-stability result. Both runs completed normally,
remained real time, and met the 30-second terminal-latency contract. It follows
only from the complete contextual speaker review below.

## Frozen evidence

| Item | Value |
|---|---|
| Source commit | `2ff9ce3655b2a12e90a5d0def25c0a30f171f2d9` |
| Server binary SHA-256 | `841c7bdb08cbd6d3df1f4b40a15581cda193c29b6f7b461b98ecb641da46b9fd` |
| Sortformer v2.1 SHA-256 | `d036020b6b93977098929d417b1b106a952ec02cc38cafc9d3315ae0ec4d90b8` |
| Human reference SHA-256 | `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86` |
| Run A artifact SHA-256 | `61119ab2eb4f66ed08be85652df44619001227b4986fa6b770239a840b26a9f0` |
| Run A manifest SHA-256 | `f6d2ed09f4fa55041109ee2f3a85e99d6bfc1fa085394d3a89215581721ab914` |
| Run A TOML SHA-256 | `6a92c582a2cba7e26542f38a60516bb53929dc5d109ba331bbf4b5b614eb9b22` |
| Frozen registry SHA-256 | `66461a77755984a08231d06306da7ce9e1eeac07be1927e91f8a772fc54c7b3f` |
| Run B artifact SHA-256 | `fa3a0564547e4961e9d1050b54c934ddd066c2536f300f339b5647e44f56f972` |
| Run B manifest SHA-256 | `272cb014a7ebb9446a5dc148b274b2bec6a69bde5ab3124ea7029521a2997c6d` |
| Run B TOML SHA-256 | `37f527081f689bc997059a97fa31886fa7ce838d09192e7bae2775116e469f29` |
| Run A forward / reverse packets | `ebb49f090f5cb7271099efc5ed5a0f40dda464cb313002fa9b5143ee98d1927f` / `3c5a088be0d5aea90a53f0f8f00006803e13cfd600366f9ae09936be3942f58a` |
| Run B forward / reverse packets | `59d69f8d677579d51046d26484df2ccfda5fbf0ae19a911516c706afe8671090` / `f82f9d65d601ab826a67469c3181dbc42bbb9e2a50d20f6dacefd3bb6bb4854f` |
| Runtime identity map | `spk_0=朱杰`, `spk_1=唐云峰`, `spk_2=徐子景`, `spk_3=石一` |

Both artifacts contain 755 diarization, 1,348 primary-speaker, 308 ASR, 972
VAD, 308 forced-alignment, 16,103 voiceprint, and 1,707 final business entries.
All seven pipeline extents end at `57,841,920` samples with zero gap;
`timebase_ok`, `timebase_reconciled`, and `wall_clock_ok` are true. Producer,
early observer, and late observer terminal hashes agree within each run, and
all required telemetry fields have complete cadence coverage.

The A/B diarization, primary, ASR, alignment, voiceprint, and final business
exports are pairwise byte-identical after excluding run metadata. The complete
seven-track normalized entry bundle also has the same SHA-256 on both paths:
`98198e78373d5eb8128b4d70326645f61c0ef34bddc28e784d84b4c9bcecb216`.

After the review and state synchronization, a clean build completed with an
empty warning/error scan and all 69 configured CTest entries passed. These
checks verify engineering and mechanical contracts only; they do not contribute
to the contextual result or promotion decision.
Registry initialization therefore does not explain the result difference from
T111. This is a mechanical repeatability fact, not an accuracy judgment.

## Complete contextual reconciliation

`CW` means that the user-facing view materially assigns the recognizable
contribution, or a substantive part of it, to a known wrong speaker. `M` means
the contribution is missing rather than asserted by the wrong known speaker.
`U` means the contribution is explicitly uncertain. Contextual timing shifts
are accepted when the recognizable contribution appears under the correct
speaker in the adjacent conversation; ASR wording differences alone are not
speaker failures.

Run A and Run B have the same final error contexts, but each list was reviewed
and signed independently.

| Ref | Block | Speaker | Class | Critical | Contextual speaker finding |
|---|---|---|---|---|---|
| 0025 | 0-600 | 徐子景 | CW | No | Backchannel is assigned to 朱杰 |
| 0049 | 0-600 | 唐云峰 | CW | Fail | Endorsement of the 50-billion proposal is assigned to 石一 |
| 0058 | 0-600 | 唐云峰 | CW | Fail | `相差0.7` is assigned to 石一 |
| 0061 | 0-600 | 石一 | CW | No | Duplicate-time preface is merged into 唐云峰's following turn |
| 0063 | 0-600 | 石一 | M | No | Confirmation is absent |
| 0066 | 0-600 | 唐云峰 | CW | Fail | `你们俩可以` is absorbed into 石一's calculation |
| 0071 | 0-600 | 石一 | CW | Fail | `才44、45` is assigned to 唐云峰 |
| 0099 | 600-1200 | 石一 | CW | Fail | The first clause is assigned to 石一, but the substantive governance consequence is assigned to 朱杰 |
| 0102 | 600-1200 | 唐云峰 | CW | Fail | Governance conclusion is assigned to 石一 |
| 0118 | 600-1200 | 唐云峰 | CW | Fail | Fifteen-percent confirmation is predominantly assigned to 石一 |
| 0135 | 600-1200 | 石一 | CW | No | Calculation tag question is assigned to 唐云峰 |
| 0154 | 600-1200 | 唐云峰 | CW | Fail | The `不含` scope answer is assigned to 石一 |
| 0171 | 600-1200 | 石一 | CW | No | The short expectation lead-in is predominantly assigned to 唐云峰 |
| 0192 | 1200-1800 | 朱杰 | CW | Fail | `没有意见` is folded into 石一's response |
| 0194 | 1200-1800 | 徐子景 | CW | Fail | `老唐有点少` is almost entirely assigned to 唐云峰 |
| 0221 | 1200-1800 | 徐子景 | CW | No | Recognizable main fragment is assigned to 唐云峰 |
| 0239 | 1200-1800 | 徐子景 | CW | No | Backchannel is assigned to 唐云峰 |
| 0241 | 1200-1800 | 徐子景 | CW | No | Hat question is assigned to 唐云峰 |
| 0252 | 1200-1800 | 朱杰 | CW | Fail | Decision-ownership statement is split across 石一 and 唐云峰 |
| 0268 | 1800-2400 | 朱杰 | CW | No | Backchannel is assigned to 徐子景 |
| 0298 | 1800-2400 | 唐云峰 | CW | No | Preface is assigned to 徐子景 |
| 0313 | 1800-2400 | 石一 | CW | Fail | Equity-transfer confirmation is absent after 唐云峰's tag question |
| 0327 | 1800-2400 | 石一 | CW | Fail | Licensing confirmation is assigned to 唐云峰 |
| 0331 | 1800-2400 | 石一 | CW | Fail | Terminal confirmation is appended to 唐云峰's row |
| 0333 | 1800-2400 | 石一 | CW | Fail | Terminal confirmation is appended to 唐云峰's row |
| 0341 | 2400-3000 | 唐云峰 | M | No | Reaction is absent at the duplicate source time |
| 0350 | 2400-3000 | 朱杰 | CW | Fail | Complete independent-company endorsement is assigned to 唐云峰 |
| 0354 | 2400-3000 | 朱杰 | CW | Fail | Independent-company confirmation is assigned to 唐云峰 |
| 0375 | 2400-3000 | 徐子景 | CW | Fail | Approval is reduced to one syllable before 石一's following turn |
| 0390 | 2400-3000 | 唐云峰 | CW | Fail | Negotiation confirmation is assigned to 石一 |
| 0406 | 2400-3000 | 唐云峰 | CW | Fail | Ten-working-day confirmation is assigned to 朱杰 |
| 0409 | 2400-3000 | 朱杰 | M | No | `马上咱们也能吹一吹` is absent |
| 0417 | 2400-3000 | 唐云峰 | M | No | Clarification prompt is absent at the duplicate source time |
| 0420 | 2400-3000 | 朱杰 | CW | No | Backchannel is assigned to 唐云峰 |
| 0426 | 2400-3000 | 石一 | CW | Fail | B/C package statement is assigned across 唐云峰 and 徐子景 |
| 0432 | 2400-3000 | 朱杰 | M | Fail | Two-billion valuation answer is absent |
| 0442 | 2400-3000 | 朱杰 | M | Fail | First price question is absent at the source-time edge |
| 0444 | 2400-3000 | 朱杰 | CW | Fail | Repeated price question is fragmented across wrong known speakers |
| 0457 | 2400-3000 | 石一 | CW | No | Clarification is reduced and assigned to 徐子景 |
| 0461 | 2400-3000 | 唐云峰 | CW | Fail | Financial-approval responsibility is assigned to 石一 |
| 0478 | 3000-3600 | 朱杰 | CW | Fail | Complete tax-responsibility response is assigned to 唐云峰 and 石一 |
| 0499 | 3000-3600 | 石一 | CW | Fail | Spouse-finance conclusion is split between 唐云峰 and `unknown` |
| 0503 | 3000-3600 | 朱杰 | CW | Fail | Sustained nominee-ownership turn is assigned predominantly to 石一 |
| 0504 | 3000-3600 | 唐云峰 | CW | Fail | Ownership correction is split into unrelated known speakers |
| 0505 | 3000-3600 | 唐云峰 | CW | Fail | Hangzhou-stake statement appears under 石一 before its display edge |
| 0506 | 3000-3600 | 唐云峰 | U | No | Interjection is explicitly `unknown` |
| 0507 | 3000-3600 | 唐云峰 | CW | Fail | `5.6个亿` expectation is assigned to 朱杰 |
| 0509 | 3000-3600 | 唐云峰 | CW | Fail | Strategy confirmation is assigned to 石一 |
| 0517 | 3000-3600 | 唐云峰 | CW | Fail | First company-structure question is assigned to 朱杰 |
| 0518 | 3000-3600 | 朱杰 | CW | Fail | `老师最有发言权` is assigned to 唐云峰 |
| 0537 | 3000-3600 | 唐云峰 | CW | No | `甩手掌柜` response is assigned to 石一 |

The manual classification is 44 confident-wrong, six missing, and one
uncertain contribution. Thirty-two confident-wrong contributions and two
missing contributions carry critical business meaning. The critical-speaker
gate therefore fails. The confident-wrong result is manually derived as
`44/556`, approximately `7.91%`, so it also fails the at-most-two-percent gate.

The complete reread accepts `ref-0009`, `ref-0024`, `ref-0045`, `ref-0249`,
`ref-0253`, `ref-0296`, and `ref-0338`, which were errors in T111. It also
retains the intended FR29 repair at `ref-0073` and the following 唐云峰 handoff
at `ref-0074`. Those repairs do not compensate for the new full-session
regressions listed above.

## Manually checked breakdowns

### Fixed blocks

| Block | Run A | Run B | Gate |
|---|---:|---:|---|
| 0-600 | 86 / 93 (92.47%) | 86 / 93 (92.47%) | Pass |
| 600-1200 | 78 / 84 (92.86%) | 78 / 84 (92.86%) | Pass |
| 1200-1800 | 74 / 80 (92.50%) | 74 / 80 (92.50%) | Pass |
| 1800-2400 | 74 / 80 (92.50%) | 74 / 80 (92.50%) | Pass |
| 2400-3000 | 114 / 129 (88.37%) | 114 / 129 (88.37%) | Fail |
| 3000-3600 | 76 / 87 (87.36%) | 76 / 87 (87.36%) | Fail |
| 3600-3615.12 | 3 / 3, reported only | 3 / 3, reported only | Not a full-block gate |

The block denominators total 556. The manually reconciled accepted totals are
505 for each run. The two failed fixed blocks independently reject promotion.

### Canonical speakers

| Speaker | Run A | Run B | Gate |
|---|---:|---:|---|
| 朱杰 | 70 / 83 (84.34%) | 70 / 83 (84.34%) | Fail |
| 唐云峰 | 170 / 189 (89.95%) | 170 / 189 (89.95%) | Fail |
| 徐子景 | 67 / 73 (91.78%) | 67 / 73 (91.78%) | Pass |
| 石一 | 198 / 211 (93.84%) | 198 / 211 (93.84%) | Pass |

The speaker denominators total 556 and the accepted totals independently total
505. This signs turn recall only; per-speaker time remains unsigned.

### Full session

Each independently reviewed run has 505 accepted and 51 incorrect natural
contributions. The manually derived full-session result is `505/556`,
approximately `90.83%`. It clears the standalone 90-percent full-session turn
threshold, but it is below the 93-percent development margin and below the
corrected T111 `514/556` result. More importantly, two fixed blocks, two
canonical speakers, critical attribution, and confident-wrong attribution all
fail. No conjunctive closing or promotion claim follows from the full average.

## Decision and next diagnosis

T123 is complete and FR29 full-session promotion is rejected. T111 remains the
best frozen comparison baseline, not an accepted closing result. The checked-in
FR29 code remains a transitional experiment because it passes focused,
120-second, and 600-second
gates and repairs `ref-0073`, but it is not an accepted closing behavior.
T102, T084, canonical closure, and industrial readiness remain open.

The A/B equality rules out speaker-registry initialization as the cause of this
candidate result. Mechanical comparison with the frozen T111 evidence shows
identical Sortformer diarization and primary-speaker exports, while ASR,
alignment, voiceprint, and final business exports differ. The next work uses
the frozen full tracks to trace the first changed source interval in each failed
late block through VAD-gated ASR finalization, forced alignment, derived
voiceprint queries, and business revision order. It must identify a generic
source-free correction and pass complete changed-context review before another
audio run. No new full-length capture is justified yet.
