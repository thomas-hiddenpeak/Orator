# Sortformer v2.1 Posterior Future-Epoch Candidate Review

**Date:** 2026-07-19
**Scope:** FR47 frozen candidates, T195-T200
**Authority:** `test/data/reference/test.txt` is the human-listened reference

## Governance

No executable mechanism assigned correctness, aggregated accuracy, selected a
candidate, or issued the verdict below. The replay executable only parsed and
deposited frozen typed evidence. `rg`, `diff`, and hashes only displayed raw
changed scope and mechanical repeatability. Every changed finalized source was
then read in complete chronological context and again from the end of the
affected windows toward the beginning against `test.txt`.

The frozen producer inputs, source text, alignment, identity tracks, and frame
posterior were not changed. The identity map is `spk_0=朱杰`,
`spk_1=唐云峰`, `spk_2=徐子景`, and `spk_3=石一`.

## Mechanical Evidence

Both replays consumed 755 activity segments, 1,348 primary segments, 308 ASR
finals, 308 alignment groups, 16,103 voiceprint evidence records, and one
45,189-frame posterior block. Their 1,753-entry outputs are byte-identical at
SHA-256 `d5396c07a2f7dd0c485967a96fb65deab79441b93bf38af19c152af2de2d5be0`.
This establishes replay determinism only.

The raw override scope contains sixteen finalized source records:
`175`, `176`, `177`, `179`, `181`, `253`, `254`, `255`, `256`, `257`, `264`,
`266`, `267`, `268`, `271`, and `283`. These identifiers and the 43 displayed
override pieces are scope facts, not an accuracy score.

## Forward Review

| Source | Complete contextual finding |
|---|---|
| `175` | The changed pieces `对贵`, `就价格下来了`, `那肯定`, `对`, `就没`, and `的` cross the 33:54 唐云峰 handoff into 33:59 朱杰. Relabeling the Zhu continuation as Tang is wrong. |
| `176` | `就是个` continues 朱杰, while the changed `对` and `所以甚至` belong to 石一 at 34:08 and 34:23. All are rewritten as 唐云峰 and are wrong. |
| `177` | The changed `对` is 石一's 34:45 response, not 唐云峰. |
| `179` | `主要说` belongs to 石一's explanation; `对`, `就是`, and `对并表` are 朱杰's 35:03-35:06 responses. Rewriting them as 唐云峰 is wrong. |
| `181` | `是哈` is 朱杰's 35:14 response. The Tang epoch begins only after this finalized source. |
| `253` | The changed `哎` begins 唐云峰's 49:53 remark. Rewriting it as 朱杰 is wrong. |
| `254` | The changed price/accounting question and `没问题` fragments belong to 石一, followed by 唐云峰 at 50:24. Rewriting all of them as 朱杰 is wrong. |
| `255` | The changed 50:32-50:40 confirmations are 唐云峰's capital-injection explanation, not 朱杰. |
| `256` | The changed range crosses 朱杰, 石一, then 朱杰 again around the tax exchange. Some isolated pieces happen to match Zhu, but the rule rewrites the intervening Shi question and is not valid for the complete source. |
| `257` | The short continuation is attached to the preceding mixed exchange. Its later Zhu epoch is outside this finalized source and cannot resolve the source safely. |
| `264` | `万一成功，是要告诉你们` is 唐云峰 at 52:42, not 石一. |
| `266` | The changed hesitation precedes 朱杰's 53:02 legal-representative statement, not a Shi turn. |
| `267` | `呃` opens 徐子景's 53:06 answer. Rewriting it as 石一 is wrong. |
| `268` | The changed range joins 唐云峰's 53:21 introduction with 石一's following answer. One future Shi label cannot replace the complete mixed source range. |
| `271` | The changed sequence contains Shi's repeated `不行`, Tang's intervening explanation, then Shi's `哦，没区别`. The rule is correct on some pieces and wrong on others, so the source is not safely revisable. |
| `283` | `我在等着我的五点六个亿呀` and the first aligned `对` are 唐云峰; the following second `对` remains 石一. Both changed pieces are contextually correct and preserve the handoff. |

## Reverse Review

Reading backward from source `283` preserves Tang's first `对`, Shi's second
`对`, and Tang's later strong epoch within the same finalized source. Moving
backward across source `271`, then `268-264`, `257-253`, and `181-175`, the
future identity epochs no longer belong to the source being rewritten. The
reverse speaker order exposes the same Zhu/Tang/Xu/Shi interruptions described
above and confirms that the broad horizon is not a valid source bridge.

The structural boundary is explicit in the frozen source clock. The false
windows all end before their selected future epoch: source `181` ends at
`2132.356` before `2136.640`, source `257` ends at `3078.980` before
`3108.480`, and source `271` ends at `3242.276` before `3243.280`. Source
`283` instead spans `3315.388-3338.244`, containing the `3330.640` epoch and
its matching primary support.

## Decision

The first candidate is manually rejected and may not run on audio. Its missing
contract is source ownership, not another score or duration. A revised frozen
candidate may proceed only if the future epoch begins before the current ASR
final ends and the existing minimum epoch duration plus matching primary
support are both satisfied inside that same source. All prior posterior,
identity, alignment, uniform-incumbent, uniqueness, and abstention contracts
remain mandatory. This retry adds no numerical parameter and carries no
ledger or acceptance change.

## Source-Bounded Retry

The retry requires the selected future identity epoch to begin before the same
immutable ASR final ends. Its existing minimum duration and matching primary
coverage must also be available before that source end. A focused cross-text
test confirms that a later epoch belonging only to the next finalized source
cannot rewrite the earlier source. All earlier missing-frame, rank-tie,
threshold, local-slot, incumbent-label, identity-ambiguity, primary-support,
phrase, and aligned-unit abstentions remain active.

The complete build is warning-clean and all `70/70` registered CTest entries
pass. Two revised frozen T123 replays each consume the same 45,189-frame input
at SHA-256
`79fd2c416ac76a0af477f98bf8d848f6e604b2d94c5c4445e653978afd6c7e41`.
Their 1,715-entry outputs are byte-identical at SHA-256
`27f90ce43f4b226750cadaf5b11b949986536478e84524c0715c2c477b0c85e6`.

A contemporaneous control changes only
`speaker_fusion.posterior_future_epoch_enable` from `true` to `false`; its TOML
has SHA-256
`016d5f2a0884e7f5abdfde2aab97fe7e4ac67876453955c2a1fd2b8dd29be46c`.
Its two 1,714-entry outputs are byte-identical at SHA-256
`174319361040f648b4f930e312986e626f6b5cba9e3d8eaad9aeaa4a0bc7e7f1`.
These hashes establish deterministic replay and the one-boolean control only;
they do not evaluate a product result.

The revised override reason appears only twice, both inside source `283`:

| Range | Revised identity | Complete contextual finding |
|---|---|---|
| `3315.548-3317.788` | 唐云峰 (`spk_1`) | `我在等着我的五点六个亿呀，` is Tang's expectation before Shi resumes the strategy explanation. |
| `3328.508-3328.828` | 唐云峰 (`spk_1`) | The first `对` is Tang's confirmation; the second `对，一点影响都没有` remains Shi's continuation. |

The later Tang epoch, Shi continuation, and Tang `而且，纯贸易...` handoff all
remain inside their original source and common-clock bounds. Complete forward
and reverse reading of this conversation against `test.txt` retains both
changes and finds no neighboring regression.

## Complete 556-Contribution Review

The revised 3,615.12-second business view was then read against all 556 human-
listened reference contributions in chronological order and again in reverse
fixed windows. The display-only chronological packet has SHA-256
`da0e5a732cfd8c3233e99f3a35abdba533cb3af8115a9033d8997fd311322099`;
the reverse packet has SHA-256
`1118ab2b882436d1305bd6ae7d218fa1dcabe386b27de8198d1d24ff5762fed4`.
The packet generator only arranged the complete reference and candidate text;
it emitted no correctness field, count, score, ranking, or verdict.

The manual two-direction review confirms that only `ref-0507` and `ref-0509`
change product judgment from the FR45 ledger. All other previously signed
residuals remain visible, and no new contextual speaker failure is introduced.
The manually transcribed and cross-checked ledger is `521/556`: 29 confident-
wrong, five missing, and one uncertain contribution remain. Twenty confident-
wrong and one missing contribution remain business-critical. The seven manual
block ledgers are `88/93`, `79/84`, `76/80`, `75/80`, `118/129`, `82/87`, and
`3/3`; the per-speaker ledgers are 朱杰 `77/83`, 唐云峰 `176/189`, 徐子景
`69/73`, and 石一 `199/211`.

## Final FR47 Decision

The source-bounded FR47 frozen candidate is manually retained. Its full-session
natural-turn ledger is above the product floor and all complete 600-second and
per-speaker natural-turn floors remain passed. It is not a closing result:
21 business-critical failures, 29 confident-wrong contributions, all unsigned
time-based gates, independent real-path repeatability, locked holdout, and
release review remain open.

This review authorizes a clean real-WebSocket promotion ladder on one committed
revision. Two independent 120-second runs must pass mechanical contracts and
complete in-scope forward/reverse semantic review before a 600-second run; the
600-second result must pass the same gates before full empty-registry and
restarted frozen-registry runs are permitted. Each full run requires its own
complete 556-contribution contextual review. No frozen-replay result is
relabelled as real-path evidence.
