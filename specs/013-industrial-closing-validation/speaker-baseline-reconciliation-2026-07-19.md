# Speaker Baseline Reconciliation (2026-07-19)

## Scope and authority

This report corrects the natural-turn speaker ledgers used to compare T111,
T123, and T133. It does not rerun audio, evaluate ASR wording, sign speaker
time, or change any runtime behavior. `test/data/reference/test.txt` remains the
human-listened authority.

The frozen T111 Run A and Run B artifacts were each reread across all 556
reference contributions in chronological order and again in reverse
600-second-block order. The already completed T123 and T133 full reviews were
then reconciled under the same material-fragment rule: a natural turn is
confident-wrong when the user-facing view assigns a substantive part of its
recognizable meaning to a known wrong speaker, even if another fragment of the
same turn has the correct identity.

No compiled code, script, notebook, formula, query, metric, or algorithm
assigned a judgment, counted a result, calculated a percentage, compared a
gate, or issued this conclusion. Shell commands only displayed immutable
reference and runtime evidence. Every correction and total below was derived
and cross-checked manually from complete conversational context.

## Frozen evidence

| Evidence | Run A | Run B |
|---|---|---|
| T111 artifact | `d5c97db9ff91b41da4ccd5414d5f2bca4966592e60fb2717058fee2e600132e9` | `b5dfefc8c30ec9458bbe70a8f7e789a6997d082c9bcce7d834b1df12e6c725f4` |
| T111 forward packet | `6a70b4e979599b42b348375154e7ae225386be33b73a93a58ccc70f14a6e6da0` | `38ba63459d3aea6aed9b3ac3e5b8100488bfa3d509d3d25b52954401f0da9c4f` |
| T111 reverse packet | `80afedbb71292862d832a96ee3cf9fe9a155ec05e2a85806eb0f0293a47af9db` | `4d571aede4d5498d867fea011f29e88145a18682dbde7597599c52f00648e4d5` |
| T123 artifact | `61119ab2eb4f66ed08be85652df44619001227b4986fa6b770239a840b26a9f0` | `fa3a0564547e4961e9d1050b54c934ddd066c2536f300f339b5647e44f56f972` |
| T133 artifact | `60ee7a9a5df6bdac27c2860256c2e16d1b58d11668e9cc204dbdcdd987bfdab3` | `affed8ded3bb8a6e88897e7b615dd3b568dc502229ed6de38a1f4c597a415be` |
| Human reference | `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86` | Same |

The runtime identity map is `spk_0=朱杰`, `spk_1=唐云峰`,
`spk_2=徐子景`, and `spk_3=石一` for all three comparisons.

## Corrected contextual judgments

The previous T111 ledger omitted four errors that the later T123 and T133
reviews already classified under the same reference and business rule. A fifth
error, `ref-0099`, was omitted from all three ledgers.

| Ref | Applies to | Speaker | Class | Critical | Complete-context finding |
|---|---|---|---|---|---|
| 0099 | T111, T123, T133 | 石一 | CW | Fail | `就是我俩定了` starts under 石一, but the substantive consequence `他们就啥都别说了` is asserted under 朱杰; the final particle returning to 石一 does not make the complete governance turn usable |
| 0239 | T111 | 徐子景 | CW | No | The `嗯？啊？` response is assigned to 唐云峰 |
| 0426 | T111 | 石一 | CW | Fail | The B/C packaging proposal is assigned across 唐云峰 and 徐子景 with no usable 石一 identity |
| 0503 | T111 | 朱杰 | CW | Fail | The initial question is under 朱杰, but most of the sustained nominee-ownership proposal is asserted under 石一 |
| 0518 | T111 | 朱杰 | CW | Fail | `老师最有发言权` is asserted under 唐云峰 |

`ref-0099` is stable in both A/B paths and both reading directions for T111,
T123, and T133. The other four rows are stable in both T111 paths and both
reading directions. They are not run-to-run uncertainty.

This correction also changes the causal interpretation of `ref-0503`. T123
has less ASR evidence in that low-energy region, while T133 changes the
VAD/ASR boundaries again; both remain wrong. T111 already assigns most of the
recognizable speaker turn to 石一. The VAD gap therefore explains an
evidence-availability change; it does not explain the original
speaker-attribution failure or provide a speaker repair by itself.

## Corrected T111 result

The corrected T111 ledger contains 36 confident-wrong, five missing, and one
uncertain contribution. Twenty-six confident-wrong contributions and one
missing contribution carry critical business meaning. The manually derived
confident-wrong share is `36/556`, approximately `6.47%`.

### Fixed blocks

| Block | Run A | Run B | Gate |
|---|---:|---:|---|
| 0-600 | `87/93` (93.55%) | `87/93` (93.55%) | Pass |
| 600-1200 | `80/84` (95.24%) | `80/84` (95.24%) | Pass |
| 1200-1800 | `74/80` (92.50%) | `74/80` (92.50%) | Pass |
| 1800-2400 | `74/80` (92.50%) | `74/80` (92.50%) | Pass |
| 2400-3000 | `118/129` (91.47%) | `118/129` (91.47%) | Pass |
| 3000-3600 | `78/87` (89.66%) | `78/87` (89.66%) | Fail |
| 3600-3615.12 | `3/3`, reported only | `3/3`, reported only | Not a full-block gate |

### Canonical speakers

| Speaker | Run A | Run B | Gate |
|---|---:|---:|---|
| 朱杰 | `73/83` (87.95%) | `73/83` (87.95%) | Fail |
| 唐云峰 | `173/189` (91.53%) | `173/189` (91.53%) | Pass |
| 徐子景 | `69/73` (94.52%) | `69/73` (94.52%) | Pass |
| 石一 | `199/211` (94.31%) | `199/211` (94.31%) | Pass |

Each independently reviewed T111 run has 514 accepted and 42 incorrect
natural contributions. The manually derived full-session result is `514/556`,
approximately `92.45%`. It passes only the standalone full-session natural-turn
floor. The final fixed block, 朱杰 recall, critical attribution, and
confident-wrong attribution fail. Speaker-time, per-speaker time, and
source-time-offset results remain unsigned.

## Corrected cross-version comparison

The same `ref-0099` correction changes T123 to 505 accepted and 51 incorrect
contributions (`505/556`, approximately `90.83%`) and T133 to 497 accepted and
59 incorrect contributions (`497/556`, approximately `89.39%`). T123 now has
44 confident-wrong, six missing, and one uncertain contribution; T133 has 52
confident-wrong, six missing, and one uncertain contribution.

| Result | T111 | T123 | T133 |
|---|---:|---:|---:|
| Full natural turns | `514/556` | `505/556` | `497/556` |
| 600-1200 block | `80/84` | `78/84` | `78/84` |
| 石一 recall | `199/211` | `198/211` | `198/211` |
| Confident-wrong | `36/556` | `44/556` | `52/556` |

T111 to T123 repairs seven T111 errors (`0009`, `0024`, `0045`, `0249`,
`0253`, `0296`, `0338`) but introduces sixteen errors (`0025`, `0063`,
`0066`, `0071`, `0154`, `0171`, `0192`, `0194`, `0268`, `0350`, `0406`,
`0409`, `0420`, `0432`, `0478`, `0517`). The net result is nine fewer
accepted contributions.

T123 to T133 repairs `0063` and `0499` but introduces `0009`, `0250`, `0261`,
`0280`, `0377`, `0388`, `0459`, `0460`, `0463`, and `0491`. The net result is
eight fewer accepted contributions.

Directly comparing T111 with T133, T133 repairs seven T111 errors (`0024`,
`0045`, `0249`, `0253`, `0296`, `0338`, `0499`) and introduces twenty-four
(`0025`, `0066`, `0071`, `0154`, `0171`, `0192`, `0194`, `0250`, `0261`,
`0268`, `0280`, `0350`, `0377`, `0388`, `0406`, `0409`, `0420`, `0432`,
`0459`, `0460`, `0463`, `0478`, `0491`, `0517`). The net result is seventeen
fewer accepted contributions.

## Decision and next boundary

T111 remains the best frozen full-session comparison among these three runs,
but it is not an accepted closing result. The earlier claim that all T111
fixed-block and per-speaker natural-turn gates passed is withdrawn. Any report
that still displays `519/556`, `506/556`, or `498/556` records the superseded
pre-reconciliation ledger and must defer to this report.

FR29 and FR30 remain rejected. Restoring TOML `vad.threshold = 0.5` removes the
FR30 candidate but leaves the checked-in FR29/FR28 path closer to T123 than to
T111. The next change must start from the corrected error sets, preserve the
common time base, and distinguish upstream evidence loss from final fusion
misattribution. No new full audio capture is justified until a source-free
candidate has passed complete changed-context review on the frozen evidence.

## Engineering synchronization

The documentation-only reconciliation builds warning-clean and all `69/69`
configured CTest entries pass. These checks establish repository consistency
only; they do not assign or support any product-accuracy judgment above.
