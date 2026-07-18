# FR30 Full Promotion Review (2026-07-19)

> **Reconciled 2026-07-19.** The complete cross-version audit applies the
> material-fragment rule to `ref-0099`, which was split between 石一 and 朱杰.
> The corrected T133 result is `497/556`, not `498/556`; the tables below
> include the correction. See
> `speaker-baseline-reconciliation-2026-07-19.md`.

## Scope and authority

This report completes T133 and T134 for clean commit
`a96e278ea340b2f5af15a8f47c2523405b134cd1`. Run A streamed the complete
`test.mp3` through the production WebSocket path with an empty isolated speaker
registry. Run B restarted the server and used the registry frozen at the end of
Run A. Both runs used the checked-in streaming Sortformer v2.1
`340/1/188/188` profile, the single-variable FR30
`vad.threshold = 0.3` candidate, 100 ms frames, 1.0x pacing, and direct `end`
finalization.

`test/data/reference/test.txt` is the human-listened authority. Each run was
read independently across all 556 reference contributions in chronological
order and again in reverse 600-second-block order. ASR wording was used only to
locate a contribution. Speaker correctness, classifications, counts,
percentages, comparisons, and the rejection below were all derived manually
from complete conversational context.

No compiled code, script, notebook, query, formula, automated metric, or model
score assigned correctness, aggregated an accuracy result, ranked this
candidate, or issued the verdict. Tools only executed and captured the real
stream, verified mechanical contracts, hashed immutable artifacts, and placed
unjudged reference and runtime evidence beside each other for reading.

## Production runs

| Item | Run A | Run B |
|---|---:|---:|
| Registry | Empty | Run A registry, frozen before restart |
| Audio / duration | `test.mp3` / `3615.120 s` | `test.mp3` / `3615.120 s` |
| Wall time | `3630.825 s` | `3630.748 s` |
| Stream factor | `0.996x` | `0.996x` |
| Direct-end terminal wait | `15.624 s` | `15.476 s` |
| Runtime / tegrastats samples | `3615 / 3620` | `3615 / 3620` |
| Product-track contract issues | None | None |
| Manual natural-turn result | `497/556` | `497/556` |
| Promotion decision | Reject | Reject |

The first Run B attempt is excluded. Its copied registry retained read-only
mode and the server reported that it could not save the registry. The valid
retry started from a writable byte-identical frozen registry, emitted no
warning or error, and left the registry hash unchanged.

## Frozen evidence

| Item | Value |
|---|---|
| Source commit | `a96e278ea340b2f5af15a8f47c2523405b134cd1` |
| Server binary SHA-256 | `4e89d98710f49dd2d09ec8cb526949271b18fd449ef727ee3da6a8520505c1ca` |
| Human reference SHA-256 | `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86` |
| Run A artifact SHA-256 | `60ee7a9d878d931b48621d18dab3fa17bf7136538ac54bf009a4cebb63bfdab3` |
| Run A manifest SHA-256 | `e7bce597576582fedcbb2eaa43daf193cbf411438c8ef02061fbac29f6dde30e` |
| Run B artifact SHA-256 | `affed8dccb5d8567177705b23b7e236e0ff34ce14b05ccfe1559559afbc415be` |
| Run B manifest SHA-256 | `468aa8cdc51fcadf4746693b6bef348b89536decf4a5102931db818e75004b1e` |
| Frozen registry SHA-256 | `66461a77755984a08231d06306da7ce9e1eeac07be1927e91f8a772fc54c7b3f` |
| Run A forward / reverse packets | `787fe25b497fa31748284bb747b57ccfd7450b360f664ed4180bdddf72651804` / `28240b03573fe7140c274a88c8ba74bb9c7078e38ccfa7b09dc90d232599607b` |
| Run B forward / reverse packets | `c5b6c42cd5ac9deeff050b1571161bd2c0b7b7e8225434f565841058e7bb0fb9` / `2562f68cb0ab328bda9775aaa738f08b8ce8f55180463175f99ffd5bc83e9906` |

Both artifacts contain 755 diarization, 1,348 primary-speaker, 294 ASR, 903
VAD, 294 forced-alignment, 16,074 voiceprint, and 1,726 business/comprehensive
records. All seven product tracks end at exactly `57,841,920` samples with
zero gap. Time-base, wall-clock, reconciliation, direct-end, provenance,
observer-convergence, and required telemetry-field contracts pass.

The normalized seven-track packages are exactly equal between Run A and Run B,
with SHA-256
`235e3230f5e8362e4c2d0de627a6072dc00bcfb4d220dfbd67fd881e490c119e`.
The comprehensive views are also exactly equal, with SHA-256
`ee025f4e13591d4626a1b564538f48e8236a359bace97ee16c10e3b8b55e035b`.
These are mechanical repeatability facts only. They do not establish speaker
correctness.

## Complete contextual reconciliation

The complete conversation establishes `spk_0 = 朱杰`, `spk_1 = 唐云峰`,
`spk_2 = 徐子景`, and `spk_3 = 石一`. The existing contextual reconciliation
for `ref-0160` remains valid: although that source row is labeled 石一, the
board-attendance exchange unambiguously identifies 唐云峰, and the runtime
correctly assigns the proposition to `spk_1`. `ref-0182` also remains accepted
because the recognizable 徐子景 contribution appears under `spk_2` in the
complete surrounding valuation exchange.

Run A and Run B independently produce the same final contextual judgments.
There is no session-wide identity permutation, monotonic long-session drift,
or tail-wide collapse. The errors are distributed across rapid handoffs, short
interjections, and several substantive natural turns. The strongest residual
is `ref-0503`: most of 朱杰's sustained nominee-ownership proposal is asserted
under 石一. The final `ref-0553` through `ref-0556` identities recover, so this
is not a terminal identity-cluster swap.

`CW` means that a recognizable contribution, or a substantive part of it, is
asserted under a known wrong speaker. `M` means the contribution is missing.
`U` means the contribution is explicitly uncertain. The final incorrect
ledger for each run is:

| Ref | Block | Speaker | Class | Critical | Contextual finding |
|---|---|---|---|---|---|
| 0009 | 0-600 | 朱杰 | CW | No | Continuation prompt is assigned to 徐子景 |
| 0025 | 0-600 | 徐子景 | CW | No | Backchannel is assigned to 朱杰 |
| 0049 | 0-600 | 唐云峰 | CW | Fail | Endorsement of the 50-billion proposal is assigned to 石一 |
| 0058 | 0-600 | 唐云峰 | CW | Fail | `相差0.7` is assigned to 石一 |
| 0061 | 0-600 | 石一 | CW | No | Duplicate-time preface is merged into 唐云峰's following turn |
| 0066 | 0-600 | 唐云峰 | CW | Fail | `你们俩可以` is absorbed into 石一's calculation |
| 0071 | 0-600 | 石一 | CW | Fail | `才44、45` is assigned to 唐云峰 |
| 0099 | 600-1200 | 石一 | CW | Fail | The first clause is assigned to 石一, but the substantive governance consequence is assigned to 朱杰 |
| 0102 | 600-1200 | 唐云峰 | CW | Fail | Governance conclusion is assigned to 石一 |
| 0118 | 600-1200 | 唐云峰 | CW | Fail | Fifteen-percent confirmation is predominantly assigned to 石一 |
| 0135 | 600-1200 | 石一 | CW | No | Calculation tag question is assigned to 唐云峰 |
| 0154 | 600-1200 | 唐云峰 | CW | Fail | `不含` scope answer is assigned to 石一 |
| 0171 | 600-1200 | 石一 | CW | No | Short expectation lead-in is predominantly assigned to 唐云峰 |
| 0192 | 1200-1800 | 朱杰 | CW | Fail | `没有意见` is folded into 石一's response |
| 0194 | 1200-1800 | 徐子景 | CW | Fail | `老唐有点少` is almost entirely assigned to 唐云峰 |
| 0221 | 1200-1800 | 徐子景 | CW | No | Recognizable main fragment is assigned to 唐云峰 |
| 0239 | 1200-1800 | 徐子景 | CW | No | Backchannel is assigned to 唐云峰 |
| 0241 | 1200-1800 | 徐子景 | CW | No | Hat question is assigned to 唐云峰 |
| 0250 | 1200-1800 | 唐云峰 | CW | Fail | Direct `不一定啊` stance is assigned to 石一 |
| 0252 | 1200-1800 | 朱杰 | CW | Fail | Decision-ownership statement is assigned to 唐云峰 |
| 0261 | 1800-2400 | 徐子景 | CW | Fail | Financial-supervision proposition is assigned to 唐云峰 inside the Xu turn |
| 0268 | 1800-2400 | 朱杰 | CW | No | Backchannel is assigned to 徐子景 |
| 0280 | 1800-2400 | 唐云峰 | CW | No | Agreement is assigned to 徐子景 |
| 0298 | 1800-2400 | 唐云峰 | CW | No | Preface is assigned to 石一 |
| 0313 | 1800-2400 | 石一 | CW | Fail | Equity-transfer confirmation is not usable under 石一 |
| 0327 | 1800-2400 | 石一 | CW | Fail | Licensing confirmation is assigned to 唐云峰 |
| 0331 | 1800-2400 | 石一 | CW | Fail | Terminal confirmation is appended to 唐云峰's row |
| 0333 | 1800-2400 | 石一 | CW | Fail | Terminal confirmation is appended to 唐云峰's row |
| 0341 | 2400-3000 | 唐云峰 | M | No | Reaction is absent at the duplicate source time |
| 0350 | 2400-3000 | 朱杰 | CW | Fail | Independent-company endorsement is assigned to 唐云峰 |
| 0354 | 2400-3000 | 朱杰 | CW | Fail | Independent-company confirmation is assigned to 唐云峰 |
| 0375 | 2400-3000 | 徐子景 | CW | Fail | Approval is reduced before 石一's following turn |
| 0377 | 2400-3000 | 唐云峰 | CW | Fail | `那就这样子` is merged into 石一's surrounding proposal |
| 0388 | 2400-3000 | 朱杰 | CW | Fail | `哪怕他是一个投后` is assigned across 徐子景 and 石一 |
| 0390 | 2400-3000 | 唐云峰 | CW | Fail | Negotiation confirmation is assigned to 石一 |
| 0406 | 2400-3000 | 唐云峰 | CW | Fail | Ten-working-day confirmation is assigned to 朱杰 |
| 0409 | 2400-3000 | 朱杰 | M | No | Only one wrong-speaker syllable remains; the contribution is not recovered |
| 0417 | 2400-3000 | 唐云峰 | M | No | Clarification prompt is absent at the duplicate source time |
| 0420 | 2400-3000 | 朱杰 | CW | No | Backchannel is assigned to 唐云峰 |
| 0426 | 2400-3000 | 石一 | CW | Fail | B/C package statement is assigned across 唐云峰 and 徐子景 |
| 0432 | 2400-3000 | 朱杰 | M | Fail | Two-billion valuation answer is absent |
| 0442 | 2400-3000 | 朱杰 | M | Fail | First price question is absent |
| 0444 | 2400-3000 | 朱杰 | CW | Fail | Repeated price question is fragmented across wrong known speakers |
| 0457 | 2400-3000 | 石一 | CW | No | Clarification is reduced and assigned to 徐子景 |
| 0459 | 2400-3000 | 石一 | CW | Fail | Financial-responsibility question is assigned to 唐云峰 |
| 0460 | 2400-3000 | 徐子景 | M | No | Short response is absent between the neighboring turns |
| 0461 | 2400-3000 | 唐云峰 | CW | Fail | Financial-approval responsibility is assigned to 石一 |
| 0463 | 2400-3000 | 唐云峰 | CW | No | Reaction is assigned to 石一 |
| 0478 | 3000-3600 | 朱杰 | CW | Fail | Tax-responsibility response is assigned to 唐云峰 and 石一 |
| 0491 | 3000-3600 | 石一 | CW | Fail | Nominee-transfer statement is assigned predominantly to 唐云峰 |
| 0503 | 3000-3600 | 朱杰 | CW | Fail | Sustained nominee-ownership turn is assigned predominantly to 石一 |
| 0504 | 3000-3600 | 唐云峰 | CW | Fail | Ownership correction is split into unrelated known speakers |
| 0505 | 3000-3600 | 唐云峰 | CW | Fail | Hangzhou-stake statement is assigned to 石一 |
| 0506 | 3000-3600 | 唐云峰 | U | No | Interjection is explicitly `unknown` |
| 0507 | 3000-3600 | 唐云峰 | CW | Fail | `5.6个亿` expectation is assigned to 朱杰 |
| 0509 | 3000-3600 | 唐云峰 | CW | Fail | Strategy confirmation is assigned to 石一 |
| 0517 | 3000-3600 | 唐云峰 | CW | Fail | First company-structure question is assigned to 朱杰 |
| 0518 | 3000-3600 | 朱杰 | CW | Fail | `老师最有发言权` is assigned to 唐云峰 |
| 0537 | 3000-3600 | 唐云峰 | CW | No | `甩手掌柜` response is assigned to 石一 |

The ledger contains 52 confident-wrong, six missing, and one uncertain
contribution. Thirty-seven confident-wrong contributions and two missing
contributions carry critical business meaning. The critical-speaker gate fails.
The manually derived confident-wrong share is `52/556`, approximately `9.35%`,
so both confident-wrong requirements fail.

## Manually checked gate breakdowns

### Fixed blocks

| Block | Run A | Run B | Gate |
|---|---:|---:|---|
| 0-600 | `86/93` (92.47%) | `86/93` (92.47%) | Pass |
| 600-1200 | `78/84` (92.86%) | `78/84` (92.86%) | Pass |
| 1200-1800 | `73/80` (91.25%) | `73/80` (91.25%) | Pass |
| 1800-2400 | `72/80` (90.00%) | `72/80` (90.00%) | Pass |
| 2400-3000 | `109/129` (84.50%) | `109/129` (84.50%) | Fail |
| 3000-3600 | `76/87` (87.36%) | `76/87` (87.36%) | Fail |
| 3600-3615.12 | `3/3`, reported only | `3/3`, reported only | Not a full-block gate |

### Canonical speakers

| Speaker | Run A | Run B | Gate |
|---|---:|---:|---|
| 朱杰 | `68/83` (81.93%) | `68/83` (81.93%) | Fail |
| 唐云峰 | `166/189` (87.83%) | `166/189` (87.83%) | Fail |
| 徐子景 | `65/73` (89.04%) | `65/73` (89.04%) | Fail |
| 石一 | `198/211` (93.84%) | `198/211` (93.84%) | Pass |

### Full session and baseline comparison

Each independently reviewed run has 497 accepted and 59 incorrect natural
contributions. The manually derived full-session result is `497/556`,
approximately `89.39%`. It fails the 90-percent industrial floor.

Relative to corrected rejected T123, FR30 repairs `ref-0063` and `ref-0499`, but adds
errors at `ref-0009`, `ref-0250`, `ref-0261`, `ref-0280`, `ref-0377`,
`ref-0388`, `ref-0459`, `ref-0460`, `ref-0463`, and `ref-0491`. The resulting
ledger is eight contributions below T123's `505/556` and 17 below corrected
T111's `514/556`. Those comparisons were counted manually after every changed
context had been read in both directions; the assignment-only display did not
produce them.

## Decision and next evidence boundary

T133 and T134 are complete. FR30 full promotion is rejected. The checked-in
TOML returns `vad.threshold` to `0.5`; T111 remains the best frozen comparison
baseline, not an accepted closing result. The retained FR29 code is still a
transitional experiment, not a closed production configuration.

The result rejects a global VAD-sensitivity change as the recovery mechanism.
Threshold `0.3` exposes some low-energy speech, but it also changes or merges
many VAD/ASR boundaries across the session. The next diagnosis must preserve
the threshold-`0.5` baseline segmentation and examine a source-free,
common-time-base rescue of only low-energy gaps corroborated by independent
speaker activity. It must first be evaluated from frozen typed tracks and
complete changed-context reading. No new full audio run is authorized until a
bounded candidate passes that evidence gate.
