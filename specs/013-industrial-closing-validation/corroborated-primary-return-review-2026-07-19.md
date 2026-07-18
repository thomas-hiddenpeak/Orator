# Corroborated Primary-Return Review (2026-07-19)

## Scope and governance

This review evaluates the first FR31 candidate on the frozen T111 and T123
typed packages. `test/data/reference/test.txt` is the human-listened authority.
No executable mechanism assigned correctness, aggregated an accuracy result,
ranked the candidate, or issued the verdict. Shell and `jq` only displayed the
immutable before/after entries, reference packets, typed tracks, and hashes.
The reviewer read every changed source in surrounding conversation from the
reference toward the candidate and then traced every candidate-only fragment
back to the reference.

Repeated T123 candidate replays are byte-identical at SHA-256
`23f92e6bc26b925d07666e9969889251c3202e51bab744db3cf632b232e9f58f`.
Repeated T111 candidate replays are byte-identical at SHA-256
`c15ccd6988fe3c6d4bca9afa9e3d1c37c56bab6402504146c99f609801ba272b`.
These hashes establish determinism only.

## T123 complete changed-context review

| Text ID | Complete contextual judgment |
|---|---|
| 43 | Repair. The protected `44 45` fragment belongs to Shi Yi between Tang Yunfeng contributions (`ref-0071`). |
| 84 | Repair. The protected `不含` answer belongs to Tang Yunfeng (`ref-0154`); the following `哦` remains outside the repair. |
| 97 | Repair. The protected `好点` fragment returns to Shi Yi inside the Tang-Shi exchange (`ref-0171`). |
| 101 | Partial repair. The source phrase belongs to Shi Yi; preserving its final `呢` removes one Tang fragment without changing the following Shi sentence (`ref-0179`). |
| 125 | Regression. The human reference keeps the complete explanation with Xu Zijing, while FR31 expands a Shi label across `我知道，我知` (`ref-0209`). |
| 166 | Repair. The first short `啊` belongs to Zhu Jie between Xu Zijing contributions (`ref-0268`). |
| 208 | Regression. `放成都的` and its continuation belong to Tang Yunfeng; FR31 cuts `都的` and additional characters to Shi Yi (`ref-0335`). |
| 220 | Regression. `后面要后面再做架构` belongs to Tang Yunfeng; FR31 assigns its prefix to Xu Zijing (`ref-0358`). |
| 233 | Regression. `只要...不回购` belongs to Tang Yunfeng; FR31 assigns the initial `只` to Shi Yi (`ref-0392`). |
| 242 | No repair. The changed `打` remains assigned to an identity different from the Shi Yi reference (`ref-0426`). |
| 249 | Regression. Xu Zijing owns the architecture/price statement; FR31 assigns its initial `这在` to Shi Yi (`ref-0440`). |
| 251 | Regression. The repeated `五六百万` contribution belongs to Tang Yunfeng; FR31 creates two Shi fragments inside it (`ref-0452`). |
| 255 | No repair. The changed `再` remains assigned to an identity different from the Shi Yi reference at the next contribution (`ref-0471`). |
| 303 | Regression. Shi Yi owns `如果全都出这个`; FR31 extends Tang Yunfeng through the initial `如` (`ref-0542`). |

## T111 complete changed-context review

| Text ID | Complete contextual judgment |
|---|---|
| 36 | Partial repair. The changed decimal fragment is part of Tang Yunfeng's `相差 0.7` response (`ref-0058`). |
| 40 | Regression. Both repetitions of `40` remain in Shi Yi's calculation; FR31 assigns the second repetition to Tang Yunfeng (`ref-0086`). |
| 72 | Partial repair. The changed characters lie in Tang Yunfeng's current-round response (`ref-0155`). |
| 94 | Regression. `我录着音呢` belongs to Shi Yi; FR31 assigns its first `我` to Tang Yunfeng (`ref-0193`). |
| 108 | Regression. The complete pricing explanation belongs to Xu Zijing; FR31 expands a Shi fragment inside `我知道` (`ref-0209`). |
| 131 | Regression. `就是啊，不是说` belongs to Tang Yunfeng; FR31 assigns `啊` to Shi Yi (`ref-0246`). |
| 146 | Regression. Xu Zijing owns the technology-origin question; FR31 assigns its final `的` to Zhu Jie. The separately aligned Zhu `啊` was already present (`ref-0267`/`ref-0268`). |
| 192 | Regression. `后面后面再做架构` belongs to Tang Yunfeng; FR31 assigns the first `后` to Xu Zijing (`ref-0358`). |
| 203 | Regression. `只要...` belongs to Tang Yunfeng; FR31 assigns its initial `只` to Shi Yi (`ref-0392`). |
| 220 | Regression. The repeated `五六百万` contribution belongs to Tang Yunfeng; FR31 expands a Shi fragment into it (`ref-0452`). |
| 258 | Regression. `能改半年时间` belongs to Xu Zijing; FR31 assigns the final `间` to Shi Yi (`ref-0522`). |
| 270 | Regression. Shi Yi owns `如果全都出这个`; FR31 extends Tang Yunfeng through the initial `如` (`ref-0542`). |

## Decision and next evidence boundary

FR31 is rejected. A short primary A-B-A run plus complete same-identity activity
coverage can represent a real return, but it can also represent native boundary
leakage into an uninterrupted contribution. The failure appears in both frozen
baselines and across different speakers, so a global confidence or duration
threshold would only fit this recording and is not justified.

One narrower evidence case remains defensible without a new threshold: an
exact typed business interval can share the primary B run's common-clock bounds
and independently select B in both TitaNet galleries under the existing gates.
FR32 tests that evidence-precedence contract. Ordinary A-B-A returns without
that exact cross-scale corroboration continue to abstain.
