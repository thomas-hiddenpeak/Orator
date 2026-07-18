# Speaker Gate Breakdown Review (2026-07-18)

> **Reconciled 2026-07-19.** A new complete A/B forward-and-reverse reread
> found five omissions in this ledger: `ref-0099`, `ref-0239`, `ref-0426`,
> `ref-0503`, and `ref-0518`. The corrected T111 result is `514/556`, not
> `519/556`. The tables and decision below include the correction; see
> `speaker-baseline-reconciliation-2026-07-19.md` for the cross-version audit.

## Scope and method

This review continues T102 from the already completed T111 full-session review.
`test.txt` is the human-listened reference. Run A and accepted Run B had already
received a complete chronological read of all 556 contributions followed by a
complete reverse-600-second-block read. This review did not replay the audio or
replace those judgments. It reread every previously rejected contribution in
both forward packets, reread its surrounding reference conversation, checked the
same context in both reverse packets, and manually derived the remaining turn-
level breakdowns.

No compiled code, script, notebook, formula, query, metric, or algorithm
assigned a label, counted a result, calculated a percentage, compared a gate, or
issued this verdict. Shell tools only displayed immutable reference and runtime
evidence. Every total below was counted, calculated, and checked manually.

## Frozen evidence

| Item | Value |
|---|---|
| Source experiment | `6b1cb79fa4f5e942c310ea5a449225c33ccb7302` |
| Run A | `/tmp/orator-spec013/release-6b1cb79-t111/full-a-empty-registry-direct-end-ws.json` |
| Run A SHA-256 | `d5c97db9ff91b41da4ccd5414d5f2bca4966592e60fb2717058fee2e600132e9` |
| Run B | `/tmp/orator-spec013/release-6b1cb79-t111/full-b-retry-frozen-registry-direct-end-ws.json` |
| Run B SHA-256 | `b5dfefc8c30ec9458bbe70a8f7e789a6997d082c9bcce7d834b1df12e6c725f4` |
| Human reference SHA-256 | `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86` |
| Run A forward/reverse packets | `6a70b4e979599b42b348375154e7ae225386be33b73a93a58ccc70f14a6e6da0` / `80afedbb71292862d832a96ee3cf9fe9a155ec05e2a85806eb0f0293a47af9db` |
| Run B forward/reverse packets | `38ba63459d3aea6aed9b3ac3e5b8100488bfa3d509d3d25b52954401f0da9c4f` / `4d571aede4d5498d867fea011f29e88145a18682dbde7597599c52f00648e4d5` |
| Runtime identity map | `spk_0=朱杰`, `spk_1=唐云峰`, `spk_2=徐子景`, `spk_3=石一` |

The 42 final error contexts and their candidate evidence are identical between
the two retained runs. The judgments and gate decisions below were nevertheless
checked and signed for Run A and Run B independently.

## Reference reconciliation

`ref-0160` is a reference-label conflict, not a runtime speaker error. Its line
is labeled 石一, but the contribution says that below ten percent the speaker
can stop attending the board. The following 石一 turn asks that person whether
he wants to avoid arguing with them in the board office, and 唐云峰 then answers
that he does not want to participate in decisions. Both runtime paths assign the
complete `ref-0160` proposition to `spk_1`/唐云峰. The chronological and reverse
contexts therefore both accept the runtime attribution. This is the same
contextual reconciliation already recorded in the 2026-07-15 closing-baseline
review; later error lists mechanically reintroduced the source label and were
wrong to do so.

`ref-0182` is also accepted after complete-context review, but for a different
reason. The source-time display edge makes the contribution look absent when the
reference row is viewed in isolation. In the full conversation, the 徐子景
valuation contribution appears later in the runtime wording and is assigned to
`spk_2`/徐子景. The wording and timing are imperfect, but this review evaluates
speaker attribution only; the runtime therefore provides the correct speaker
for the recognizable contribution. The earlier missing-speaker label was an
invalid boundary-only judgment.

The manually verified source-label counts are 朱杰 83, 唐云峰 188, 徐子景 73,
and 石一 212. After the `ref-0160` contextual reconciliation, the canonical
speaker denominators used here are 83, 189, 73, and 211 respectively. The total
remains 556.

## Error classifications

`CW` means that the user-facing view materially asserts a known but wrong
speaker, including a mixed row with a substantive wrong-known fragment. `M`
means the recognizable contribution is missing rather than attributed to a
wrong known speaker. `U` means the recognizable contribution is explicitly
uncertain. A critical result states whether the speaker of the business-critical
meaning is usable. For `ref-0249`, the core `那拆不拆呢` question is fragmented
across `spk_0`, `unknown`, `spk_1`, and `spk_3`; the first fragment alone does
not make the speaker of the complete business question usable.

| Ref | Block | Canonical speaker | Class | Critical result | Context note |
|---|---|---|---|---|---|
| 0009 | 0-600 | 朱杰 | CW | Noncritical | Continuation prompt assigned to 徐子景 |
| 0024 | 0-600 | 朱杰 | CW | Noncritical | Reaction assigned to 徐子景 |
| 0045 | 0-600 | 石一 | CW | Fail | Agreement on control structure assigned to 徐子景 |
| 0049 | 0-600 | 唐云峰 | CW | Fail | Endorsement of the 50-billion proposal assigned to 石一 |
| 0058 | 0-600 | 唐云峰 | CW | Fail | `相差0.7` assigned to 石一 |
| 0061 | 0-600 | 石一 | CW | Noncritical | Duplicate source time; preface merged into 唐云峰's following turn |
| 0099 | 600-1200 | 石一 | CW | Fail | The first clause is assigned to 石一, but the substantive governance consequence is assigned to 朱杰 |
| 0102 | 600-1200 | 唐云峰 | CW | Fail | Governance conclusion assigned to 石一 |
| 0118 | 600-1200 | 唐云峰 | CW | Fail | Fifteen-percent confirmation is predominantly assigned to 石一; duplicate source time |
| 0135 | 600-1200 | 石一 | CW | Noncritical | Calculation tag question assigned to 唐云峰; duplicate source time |
| 0221 | 1200-1800 | 徐子景 | CW | Noncritical | Opening is correct, but the recognizable main fragment is assigned to 唐云峰 |
| 0239 | 1200-1800 | 徐子景 | CW | Noncritical | Backchannel is assigned to 唐云峰 |
| 0241 | 1200-1800 | 徐子景 | CW | Noncritical | Hat question is assigned to 唐云峰 |
| 0249 | 1200-1800 | 朱杰 | CW | Fail | The company-split question is fragmented across one correct and three wrong/unknown identities |
| 0252 | 1200-1800 | 朱杰 | CW | Fail | Decision-ownership statement is split across 石一 and 唐云峰 |
| 0253 | 1200-1800 | 唐云峰 | CW | Fail | Decision confirmation assigned to 石一 |
| 0296 | 1800-2400 | 朱杰 | M | Noncritical | Acknowledgement is absent |
| 0298 | 1800-2400 | 唐云峰 | CW | Noncritical | Preface assigned to 石一 |
| 0313 | 1800-2400 | 石一 | CW | Fail | Equity-transfer confirmation assigned to 唐云峰 |
| 0327 | 1800-2400 | 石一 | CW | Fail | Licensing confirmation assigned to 唐云峰 |
| 0331 | 1800-2400 | 石一 | CW | Fail | Terminal confirmation is present but appended to 唐云峰's row |
| 0333 | 1800-2400 | 石一 | CW | Fail | Terminal confirmation is present but appended to 唐云峰's row |
| 0338 | 2400-3000 | 石一 | M | Noncritical | Incomplete lead-in is absent; following Tang turn is not substituted |
| 0341 | 2400-3000 | 唐云峰 | M | Noncritical | Reaction is absent; duplicate source time |
| 0354 | 2400-3000 | 朱杰 | CW | Fail | Independent-company confirmation assigned to 徐子景 |
| 0375 | 2400-3000 | 徐子景 | CW | Fail | Approval to discuss the split is divided after one correct syllable and assigned to 石一 |
| 0390 | 2400-3000 | 唐云峰 | CW | Fail | Negotiation confirmation assigned to 石一; duplicate source time |
| 0417 | 2400-3000 | 唐云峰 | M | Noncritical | Clarification prompt is absent; duplicate source time |
| 0426 | 2400-3000 | 石一 | CW | Fail | B/C packaging proposal is assigned across 唐云峰 and 徐子景 |
| 0442 | 2400-3000 | 朱杰 | M | Fail | First price question is absent at the source-time edge |
| 0444 | 2400-3000 | 朱杰 | CW | Fail | Repeated price question is fragmented across three wrong known speakers |
| 0457 | 2400-3000 | 石一 | CW | Noncritical | Clarification is reduced to a short fragment and assigned to 徐子景 |
| 0461 | 2400-3000 | 唐云峰 | CW | Fail | Financial-approval responsibility statement assigned to 石一 |
| 0499 | 3000-3600 | 石一 | CW | Fail | Spouse-finance conclusion split between 唐云峰 and `unknown` |
| 0503 | 3000-3600 | 朱杰 | CW | Fail | Most of the sustained nominee-ownership proposal is assigned to 石一 |
| 0504 | 3000-3600 | 唐云峰 | CW | Fail | Ownership correction assigned to 朱杰/石一 |
| 0505 | 3000-3600 | 唐云峰 | CW | Fail | Hangzhou-stake statement occurs before its display edge and is assigned to 石一 |
| 0506 | 3000-3600 | 唐云峰 | U | Noncritical | Interjection is explicitly `unknown` |
| 0507 | 3000-3600 | 唐云峰 | CW | Fail | `5.6个亿` expectation assigned to 朱杰 |
| 0509 | 3000-3600 | 唐云峰 | CW | Fail | Strategy confirmation assigned to 石一 |
| 0518 | 3000-3600 | 朱杰 | CW | Fail | `老师最有发言权` is assigned to 唐云峰 |
| 0537 | 3000-3600 | 唐云峰 | CW | Noncritical | `甩手掌柜` response assigned to 石一 |

The manual classification contains 36 confident-wrong turns, five missing
turns, and one uncertain turn. Twenty-six of the confident-wrong turns carry
business-critical meaning. One more critical turn is missing. The critical
speaker gate therefore fails regardless of the still-unneeded complete critical
denominator. The confident-wrong result is manually calculated as `36 / 556`,
approximately `6.47%`; it fails the at-most-two-percent gate, and its 26
critical confident errors also fail the zero-critical requirement.

## Manually checked turn breakdowns

### Fixed blocks

| Block | Run A | Run B | Manual result |
|---|---:|---:|---|
| 0-600 | 87 / 93 (93.55%) | 87 / 93 (93.55%) | Pass |
| 600-1200 | 80 / 84 (95.24%) | 80 / 84 (95.24%) | Pass |
| 1200-1800 | 74 / 80 (92.50%) | 74 / 80 (92.50%) | Pass |
| 1800-2400 | 74 / 80 (92.50%) | 74 / 80 (92.50%) | Pass |
| 2400-3000 | 118 / 129 (91.47%) | 118 / 129 (91.47%) | Pass |
| 3000-3600 | 78 / 87 (89.66%) | 78 / 87 (89.66%) | Fail |
| 3600-3615.12 | 3 / 3, reported only | 3 / 3, reported only | Not a full-block gate |

The block denominators sum to 556. The accepted block totals sum independently
to 514 for each run.

### Canonical speakers

| Speaker | Run A | Run B | Manual result |
|---|---:|---:|---|
| 朱杰 | 73 / 83 (87.95%) | 73 / 83 (87.95%) | Fail |
| 唐云峰 | 173 / 189 (91.53%) | 173 / 189 (91.53%) | Pass |
| 徐子景 | 69 / 73 (94.52%) | 69 / 73 (94.52%) | Pass |
| 石一 | 199 / 211 (94.31%) | 199 / 211 (94.31%) | Pass |

The four speaker denominators sum to 556 and the four accepted totals sum to
514 for each run. This signs per-speaker natural-turn recall, not per-speaker
speaker time.

### Full session

After reconciling `ref-0160`, `ref-0182`, and the five omitted errors, each run
has 514 accepted and 42 incorrect natural contributions. The manually
calculated full-session result is `514 / 556`, approximately `92.45%`. Run A
and Run B independently pass the standalone full natural-turn gate, but the
3000-3600 fixed block and 朱杰 per-speaker natural-turn gate fail.

## Gate decision and next diagnosis

The frozen runs pass the standalone full natural-turn gate. They fail one
fixed-block gate, the 朱杰 per-speaker gate, the critical-speaker gate, and the
confident-wrong gate.
Speaker-time, per-speaker time, and source-time-offset totals remain unsigned;
the whole-second source display indicates that the residuals are short, but that
observation is not substituted for the required manual time result.

T102 and T084 remain open. The next implementation work must reduce the 22
critical confident-wrong attributions and one critical missing contribution
without regressing the already passed turn gates. Diagnosis starts from the
final business view and traces each failed critical context through forced
alignment, VAD, primary Sortformer, full diarization, and both TitaNet galleries
on the common time base. No new full audio run is justified until one source-free
rule passes complete changed-context review on both frozen A/B tracks.
