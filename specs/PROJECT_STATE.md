# Project State — Orator

A point-in-time record of where the project stands. Updated at meaningful
checkpoints. Authoritative engineering rules live in
[.specify/memory/constitution.md](../.specify/memory/constitution.md); active
work is specified under [specs/](.).

> **How this document stays truthful (Constitution Article VIII).** The code is
> authoritative; this file is subordinate to it. Every claim below names how to
> confirm it against the code (a symbol/file, a test, or a commit reference). If
> a claim and the code disagree, the code is correct and this file is the defect
> — fix it. Before acting on any claim here, verify it: a clean
> `cmake --build build -j` plus a full `cd build && ctest --output-on-failure`
> pass is the consistency proof. Status lines advance to `Implemented` in the
> same change that lands the code, with the commit reference.

- **Last updated**: 2026-07-23 (FR51's exact-baseline critical-residual audit
  stops the unsupported final-fusion branch; FR50 remains the current real-
  path speaker baseline and FR52's source-edge provenance gate is open)
- **Branch**: `master`
- **Constitution**: v1.7.0
- **Speaker-business closure**: **CURRENT REAL-PATH BASELINE `525/556`;
  19 CRITICAL RESIDUALS REMAIN; FULL CANONICAL CLOSURE OPEN**. FR49's
  bounded policy repairs only `ref-0061` and `ref-0121`. Clean commit
  `1f09052` completes two 120-second runs, one 600-second run, and independent
  full empty/frozen-registry A/B runs. Full A and B are each read completely
  in chronological and reverse fixed-window context. The four readings retain
  `523/556`, with 27 confident-wrong, five missing, one uncertain, and the
  same 20 critical residuals; no executable result produces this judgment.
  FR50 adds one false-default, TOML-enabled final-fusion policy for the exact
  right-bounded single-codepoint topology shared by `ref-0327` and
  `ref-0417`. Frozen FR49 A/B replay is deterministic, and four complete A/B
  chronological/reverse readings manually retain both repairs with no new
  contextual regression. Clean pushed commit `b449dfa` repeats that
  interpretation but is not promoted because full A takes `30.144 s` after
  direct `end`, above the unchanged `30.0 s` limit. Transitional remediation
  commit `a6f0d33730326b19a3831019b1aba21fd900f126` moves acoustic-only
  primary-run preparation into the existing low-priority live worker without
  changing a TOML value or final speaker decision. Its exact-clean 120 A/B,
  600-second, and full empty/frozen-registry A/B ladder completes all
  mechanical contracts. Full A/B both run at `0.993x` and independently return
  terminal timelines in `26.013/26.789 s`. Every full artifact is read against
  all 556 `test.txt` contributions chronologically and independently in
  reverse fixed windows. Both runs manually retain `525/556`, with 26
  confident-wrong, four missing, one uncertain, and 19 critical residuals; no
  executable result produces that ledger. There is no whole-session identity
  permutation, accumulating drift, or tail-only collapse. T232 is complete and
  FR50 is promoted as the current real-path baseline, but the critical
  residuals and remaining speaker-time, holdout, browser/microphone, report,
  and release gates keep canonical closure open.
  FR51 then freezes those 19 critical residuals and their accepted controls
  from exact full Run A and Run B. Independent chronological and reverse
  complete-context readings of both runs identify `ref-0099` as the only
  final-only overwrite; every other focus loses, displaces, or contradicts
  usable evidence before final fusion. Because the accepted short-direct
  controls provide no second material `ref-0099` topology, FR51 stops the
  final-policy branch without changing code, TOML, model, ledger, or the FR50
  baseline. FR52 is open only to investigate the upstream short-response
  relationship among ASR source boundaries, forced-alignment units, and
  surviving Sortformer speaker islands. See
  [fr51-exact-baseline-critical-residual-review-2026-07-23.md](013-industrial-closing-validation/fr51-exact-baseline-critical-residual-review-2026-07-23.md).
  Transitional experimental commit `6b1cb79fa4f5` completed
  warning-clean build, `68/68` CTest, 120-second, 600-second, and full-length
  FR16ABN real-WebSocket promotion. Full Run A used an empty registry and the
  retained Run B restarted with Run A's registry frozen. All 556 contributions
  in each run were reconciled under complete forward and reverse conversational
  context. The T102 breakdown reread corrects the `ref-0160` source-label
  conflict and the `ref-0182` boundary-only missing judgment. T135's complete
  A/B reread corrects five omitted errors; both runs are `514/556` (about 92.45
  percent). Direct terminal waits
  of `25.849 s` and `25.585 s` sign the latency gate mechanically. The first
  Run B attempt is retained but excluded because runtime telemetry cadence was
  `94.965%`; one controlled retry passed at `95.214%` without a behavioral
  parameter change. `test.txt` is the human-listened reference; the 3000-3600
  fixed block, 朱杰 natural-turn recall, critical-turn, and confident-wrong
  gates fail. Speaker-time, per-speaker time, and source-time-
  offset breakdowns remain unsigned, so T102, T084, and
  Spec 013 remain open. ASR and independent holdout are not implied.
  Transitional experimental commit `d610de36ed13` separately closes T112 with
  a warning-clean build, `69/69` CTest, and a clean 120-second real-WebSocket
  cadence check; it changes no speaker behavior or product conclusion.
  T117-T121 then identify scheduling-sensitive VAD-gated ASR as the source of
  the rejected T116 A/B producer variance and implement FR28 typed VAD
  frontiers plus an ASR-owned pending buffer. The warning-clean build, VAD
  oracle, and `69/69` CTest pass. Two independent 120-second production
  WebSocket runs have identical canonical entries in all seven product tracks;
  complete forward/reverse review of `ref-0001`-`ref-0018` finds no new speaker
  regression. Clean commit `1d511a946b29` then passes the 600-second mechanical
  gate, but complete forward/reverse review of `ref-0001`-`ref-0093` finds new
  contextual speaker regressions at `ref-0037` and `ref-0073`; full promotion
  is blocked. Clean successor commit `7579bc25411c` keeps all short within-trail
  source samples in one decoder session and retains a trailing source-clock
  bound without decoding terminal silence. Its three blank runs emit no product
  records, its two 120-second runs are byte-identical across all seven tracks,
  and complete forward/reverse review finds no new speaker regression. Its new
  600-second run restores `ref-0037` but still fails `ref-0073`. FR29 now makes
  activity and primary handoff evidence robust to a straddling abnormal
  alignment unit and prevents a containing voiceprint interval from erasing
  both base identities. Three frozen production-projector replays are
  byte-identical; complete changed-context review forward and reverse retains
  the restored Shi Yi response and the following Tang Yunfeng handoff. Clean
  commit `2ce4a12b7973` then passes two byte-identical 120-second real-WebSocket
  captures and one 600-second mechanical gate. Complete `test.txt` review in
  both directions finds no new 120-second regression and restores `ref-0073`
  while keeping `ref-0074` with Tang Yunfeng in the 600-second context. The
  real run confirms that final business-interval voiceprint queries
  intentionally derive from the revised base business view; the retained
  handoff partitions one containing query into two without changing
  diarization, primary speaker, ASR, VAD, or alignment. Clean commit
  `2ff9ce3655b2a12e90a5d0def25c0a30f171f2d9` then completes full T123 Run A
  with an empty registry and restarted Run B with Run A's frozen registry. Both
  paths run all `3615.120` seconds at `0.995x`, satisfy direct-end, common-clock,
  observer, provenance, and telemetry contracts, and produce identical
  normalized entries in all seven product tracks. Complete independent
  chronological and reverse-block reading of all 556 `test.txt` contributions,
  corrected by T135 for `ref-0099`, manually records `505/556` for each run.
  The full average remains above 90
  percent, but two fixed blocks, 朱杰 and 唐云峰 turn recall, critical
  attribution, confident-wrong attribution, and the 93-percent development
  margin fail. FR29 full promotion is rejected and T111 remains the best frozen
  comparison baseline, not an accepted closing result. See
  `013-industrial-closing-validation/cross-view-handoff-full-promotion-review-2026-07-18.md`.
  Frozen T111/T123 diagnosis then finds identical Sortformer diarization and
  primary-speaker tracks and zero current-projector speaker-sequence changes
  when the T111 typed inputs are replayed. The first causal loss is upstream:
  FR28 now correctly skips audio outside stable VAD evidence, exposing
  low-energy speech gaps that T111 consumed only through scheduling. T135
  confirms that T111 already misattributes the sustained `ref-0503` turn, so
  the VAD gap explains changed evidence availability rather than that speaker
  failure. A one-
  variable FR30 candidate lowers TOML `vad.threshold` from `0.5` to `0.3`;
  temporary production probes expose the reviewed missing intervals and retain
  zero VAD segments on the frozen silence fixture. The checked-in candidate
  passes `test_vad`, a warning-clean build, all `69/69` CTest entries, three
  independent real-WebSocket silence sessions, and two repeatable 120-second
  sessions. Complete independent forward and reverse reading of all 18
  in-scope contributions finds no new natural-turn regression. The following
  clean 600-second real-WebSocket run closes all seven pipelines at
  `9,600,000` samples, converges all observer terminal hashes, and has complete
  required telemetry. Complete forward and reverse reading of all 93
  contributions and all ten T128 sequence changes finds no new natural-turn
  regression and authorizes full A/B. Clean commit `a96e278ea340` then
  completes valid empty-registry Run A and restarted frozen-registry Run B at
  `0.996x`. Both pass direct-end, common-clock, provenance, observer,
  telemetry, and exact normalized seven-track repeatability contracts.
  Complete independent chronological and reverse-block reading of all 556
  contributions, corrected by T135 for `ref-0099`, manually records `497/556`
  for each run. The full 90-percent
  floor, two fixed blocks, 朱杰/唐云峰/徐子景 turn recall, critical
  attribution, and confident-wrong attribution fail. FR30 is rejected,
  checked-in `vad.threshold` returns to `0.5`, and T111 remains the best frozen
  comparison baseline without satisfying closing; see
  `013-industrial-closing-validation/vad-sensitivity-diagnosis-2026-07-19.md`
  and
  `013-industrial-closing-validation/vad-sensitivity-120-context-review-2026-07-19.md`
  and
  `013-industrial-closing-validation/vad-sensitivity-600-context-review-2026-07-19.md`
  and
  `013-industrial-closing-validation/vad-sensitivity-full-promotion-review-2026-07-19.md`
  and
  `013-industrial-closing-validation/speaker-baseline-reconciliation-2026-07-19.md`.
  T136 then separates missing upstream evidence from present-text fusion
  regressions on the frozen T111/T123 packages. The first FR31 A-B-A return
  guard replays deterministically but complete forward/reverse review of every
  changed context rejects it because it also preserves primary boundary
  leakage inside uninterrupted contributions. FR32 replaces that broad guard
  with exact cross-scale evidence precedence: one primary-run
  `business_interval` must select the native return identity in both TitaNet
  galleries under existing TOML gates, activity must cover the run, and a
  third activity identity vetoes. Repeated T123 frozen replays are byte-stable
  and change only `text_id=84`; repeated T111 replays are byte-stable and
  unchanged. Complete contextual reading in both directions retains the exact
  Tang Yunfeng `不含` repair at `ref-0154` without changing adjacent
  contributions. The full build is warning-clean and 69/69 CTest entries pass.
  T142 then completes one silence run, independent 120-second runs, one
  complete 600-second run, and independent full empty/frozen-registry runs
  from clean commit `72d81c8084757b4c4210ba90ac14b5d1c1155e89`.
  The full paths both run at `0.995x`, close all tracks at `57,841,920`
  samples, satisfy direct-end, provenance, observer, telemetry, and
  repeatability contracts, and produce byte-identical normalized seven-track
  entries. Independent complete chronological and reverse-block reading of all
  556 contributions manually records `506/556` for each path. FR32 repairs
  only `ref-0154` and introduces no new contextual regression, but the
  2400-3000 and 3000-3600 blocks, 朱杰 recall, critical attribution, and
  confident-wrong attribution fail. FR32 remains a bounded transitional repair
  and every conjunctive closing claim remains open; see
  `013-industrial-closing-validation/corroborated-primary-return-review-2026-07-19.md`
  and
  `013-industrial-closing-validation/exact-cross-scale-primary-return-review-2026-07-19.md`
  and
  `013-industrial-closing-validation/exact-cross-scale-primary-return-full-promotion-review-2026-07-19.md`.
  T143/T144 then isolate a partition-sensitive `ref-0517` write on the same
  frozen typed tracks. FR33 preserves the uniform native Tang identity only
  when the short phrase, unique containing VAD, containing business interval,
  and complete source exhibit the specified opposite-rank abstention topology
  under existing TOML gates. Repeated T123 replays are byte-identical and
  change only `text_id=289`; T111 remains byte-identical to FR32. Complete
  forward and reverse reading of `ref-0508` through `ref-0525` retains the
  Tang question and finds no neighboring regression. The manually reconciled
  frozen candidate is `507/556`; no real-WebSocket result is attributed to
  FR33. Two fixed blocks, 朱杰 recall, critical attribution, and confident-
  wrong attribution still fail; see
  `013-industrial-closing-validation/partition-invariant-cross-scale-abstention-diagnosis-2026-07-19.md`
  and
  `013-industrial-closing-validation/partition-invariant-cross-scale-abstention-review-2026-07-19.md`.
  T146-T148 then isolate a separate exact phrase/unique-VAD conflict at
  `ref-0406`. FR34 requires one regular coarse interval to select current
  identity A in both galleries, the exact phrase and its unique containing VAD
  to select B in all four views, activity B to cover the phrase, and one
  covering activity/primary identity C distinct from A and B. The rule writes
  only the exact phrase and uses no new threshold or TOML value. Repeated T123
  replays are byte-identical and split only `text_id=236`; repeated T111
  replays remain byte-identical to FR33. Complete chronological and reverse
  reading of `44:28-46:57` retains Tang Yunfeng's substantive ten-working-day
  answer while recording the preceding `对，` as a `0.160 s` boundary
  residual. The warning-clean build and all 69 CTest entries pass. The manually
  reconciled frozen candidate is `508/556`; no real-WebSocket result is
  attributed to FR34. The 2400-3000 and 3000-3600 fixed blocks, Zhu Jie recall,
  speaker-time sign-off, critical attribution, and confident-wrong attribution
  still fail; see
  `013-industrial-closing-validation/exact-phrase-vad-direct-conflict-diagnosis-2026-07-19.md`
  and
  `013-industrial-closing-validation/exact-phrase-vad-direct-conflict-review-2026-07-19.md`.
  T149-T151 then isolate the T111/T123 partition-sensitive `ref-0420`
  acknowledgement. FR35 reuses the existing alignment boundary tolerance only
  in the two neighbouring-gap checks of the already-conjunctive isolated
  no-embedding aligned-unit rule. It changes no TOML value or producer track;
  every identity, activity, primary, VAD uniqueness, gallery, duration, and
  provenance condition remains required. Repeated T123 replays are
  byte-identical and change only `text_id=242`; repeated T111 replays remain
  byte-identical to FR34. Complete chronological and reverse reading of
  `46:19-47:53` retains Zhu Jie's isolated acknowledgement between Tang
  Yunfeng's instruction and Shi Yi's confirmation question and finds no
  neighbouring substantive change. The warning-clean build and all 69 CTest
  entries pass. The manually reconciled frozen candidate is `509/556`; no
  real-WebSocket result is attributed to FR35. The 2400-3000 and 3000-3600
  fixed blocks, Zhu Jie recall, speaker-time sign-off, critical attribution,
  and confident-wrong attribution still fail; see
  `013-industrial-closing-validation/aligned-unit-isolation-tolerance-diagnosis-2026-07-19.md`
  and
  `013-industrial-closing-validation/aligned-unit-isolation-tolerance-review-2026-07-19.md`.
  T152-T154 then isolate the separate T111/T123 `ref-0350` regular-phrase
  partition regression. FR36 requires one native current identity A in the
  same uncontested activity/primary local slot, that slot's different initial
  identity B, exact phrase views that rank A first in the specified existing-
  gate abstention pattern, and unique containing VAD plus complete-source views
  that all reverse to B under their unchanged regular gates. It adds no future
  epoch, TOML value, or threshold. Repeated T123 replays are byte-identical and
  change only the complete `text_id=217` phrase; repeated T111 replays remain
  byte-identical to FR35. Complete chronological and reverse reading of
  `40:45-42:32` retains Zhu Jie's response between Tang Yunfeng's profitability
  question and follow-up question and finds no neighbouring change. The
  warning-clean build and all 69 CTest entries pass. The manually reconciled
  frozen candidate is `510/556`; the 2400-3000 fixed block now passes at
  `117/129`. No real-WebSocket result is attributed to FR36. The 3000-3600
  block, Zhu Jie recall, speaker-time sign-off, critical attribution, and
  confident-wrong attribution still fail; see
  `013-industrial-closing-validation/partition-invariant-regular-initial-slot-diagnosis-2026-07-19.md`
  and
  `013-industrial-closing-validation/partition-invariant-regular-initial-slot-review-2026-07-19.md`.
  T155-T157 then isolate the T111/T123 `ref-0478` punctuation-partition
  regression. FR37 requires a short primary A island gaplessly bracketed by C,
  completely covering A/C activity, A-slot initial identity B, exact A/C and
  A/B interval top-two order, a source-adjacent phrase selecting B, and a
  unique containing VAD independently selecting B under unchanged gates. It
  adds no TOML value or threshold. Repeated T123 replays are byte-identical and
  change only `3075.096-3075.496` `我向国家交。`; repeated T111 replays remain
  byte-identical to FR36. Complete chronological and reverse reading of
  `50:35-52:28` retains Zhu Jie's answer between Shi Yi's question and
  clarification and finds no neighbouring change. The warning-clean build and
  all 69 CTest entries pass. The manually reconciled frozen candidate is
  `511/556`; the 3000-3600 block advances to `78/87` but still fails. No
  real-WebSocket result is attributed to FR37. Zhu Jie recall, speaker-time
  sign-off, critical attribution, and confident-wrong attribution also fail;
  see
  `013-industrial-closing-validation/bracketed-primary-adjacent-vad-reconstruction-diagnosis-2026-07-19.md`
  and
  `013-industrial-closing-validation/bracketed-primary-adjacent-vad-reconstruction-review-2026-07-19.md`.
  T158-T160 then correct the separate T123 `ref-0504` diagnosis boundary.
  FR38 requires one source-leading punctuation phrase split across two VADs:
  eligible leading B-over-A direct evidence, a one-visible-character
  no-embedding tail inherited by following native C, unique A/C
  activity/primary support, exact phrase top-two reversal, and distinct
  leading/tail VAD rank patterns under unchanged gates. It adds no TOML value
  or threshold and writes only the phrase tail. Repeated T123 replays are
  byte-identical and split only `3301.164-3301.244` `着，`; repeated T111
  replays remain byte-identical to FR37. Complete chronological and reverse
  reading of `53:49-55:45` retains Tang Yunfeng's complete ownership correction
  while preserving the adjacent known failures. The warning-clean build and
  all 69 CTest entries pass. The manually reconciled frozen candidate is
  `512/556`; the 3000-3600 natural-turn block reaches `79/87` and now passes.
  No real-WebSocket result is attributed to FR38. Zhu Jie recall, speaker-time
  sign-off, critical attribution, confident-wrong attribution, real-path
  repeatability, and holdout evidence remain open; see
  `013-industrial-closing-validation/cross-vad-phrase-tail-reconstruction-diagnosis-2026-07-19.md`
  and
  `013-industrial-closing-validation/cross-vad-phrase-tail-reconstruction-review-2026-07-19.md`.
  T161-T163 then reconcile and repair the partition-stable `ref-0518` defect.
  FR39 requires the source-leading exact phrase to expose slot-initial A over
  native B, dual A/B phrase support, dual B/A containing-interval support,
  cross-ordered abstaining VAD evidence, abstaining complete-source A/B
  evidence, and one independently aligned third-primary tail. It adds no TOML
  value or threshold and writes only the exact phrase. Repeated T123 and T111
  replays are separately byte-identical and split only their corresponding
  `3384 s` phrase from the trailing `对。`. Complete chronological and reverse
  reading of `54:05-57:54` retains Zhu Jie's substantive answer between Tang
  Yunfeng's question and later response. The trailing `0.080 s` `对。` remains
  a visible boundary residual and no neighbouring contribution changes. The
  warning-clean build and all 69 CTest entries pass. The manually reconciled
  frozen candidate is `513/556`; the 3000-3600 block reaches `80/87`. No
  real-WebSocket result is attributed to FR39. Zhu Jie recall, speaker-time
  sign-off, critical attribution, confident-wrong attribution, real-path
  repeatability, and holdout evidence remain open; see
  `013-industrial-closing-validation/source-leading-phrase-tail-isolation-diagnosis-2026-07-19.md`
  and
  `013-industrial-closing-validation/source-leading-phrase-tail-isolation-review-2026-07-19.md`.
  T164-T166 then retain FR40's partition-invariant two-unit handoff. One short
  VAD contains exactly Zhu's `啊？` and Xu's `嗯？`; activity coarsely retains
  Zhu, primary changes exactly Zhu-to-Xu, and both VAD galleries rank Xu then
  Zhu under the unchanged short gates. T123 crosses an ASR source boundary and
  uses only the checked-in alignment tolerance; T111 keeps both units in one
  source. Repeated outputs are separately byte-identical. Complete
  chronological and reverse reading of `02:07-03:45` retains the same
  Zhu-to-Xu-to-Zhu sequence and finds no neighboring change. Only current T123
  `ref-0025` advances the manual ledger, to `514/556`; T111's complementary
  `ref-0024` repair is not double-counted. The 0-600 block reaches `87/93` and
  Xu Zijing reaches `68/73`, while Zhu Jie remains `74/83`. No real-WebSocket
  result is attributed to FR40. All other closing failures remain open; see
  `013-industrial-closing-validation/two-unit-primary-handoff-diagnosis-2026-07-19.md`
  and
  `013-industrial-closing-validation/two-unit-primary-handoff-review-2026-07-19.md`.
  T167-T169 then retain FR41's single-unit source partition of the existing
  primary-onset aligned-island rule. T123 places the punctuation-separated
  previous aligned unit before the Zhu primary island, while T111 places it
  inside the island; every activity, primary, VAD, gallery, and common-clock
  boundary remains the same. Repeated T123 and T111 outputs are separately
  byte-identical, and T111 is unchanged from FR40. Complete chronological and
  reverse reading of `31:17-33:23` retains the Xu-to-Zhu-to-Xu sequence and
  finds no neighboring change. Only current T123 `ref-0268` advances the
  manual ledger, to `515/556`; T111 is not double-counted. Zhu Jie reaches
  `75/83`, so all four per-speaker natural-turn floors pass. No real-WebSocket
  result is attributed to FR41. Critical attribution, confident-wrong
  attribution, speaker-time sign-off, real-path repeatability, and holdout
  evidence remain open; see
  `013-industrial-closing-validation/primary-onset-single-unit-partition-diagnosis-2026-07-19.md`
  and
  `013-industrial-closing-validation/primary-onset-single-unit-partition-review-2026-07-19.md`.
  T170-T172 then retain FR42's single-character alignment-dropout
  representation of the existing isolated-VAD aligned-island rule. T123 has
  two positive one-character aligned units around one visible zero-duration
  character inside the isolated Zhu VAD; T111 exposes the same response as a
  contiguous positive island. The rule preserves every existing gallery,
  isolation, direct-label, activity, primary, duration, and common-clock guard
  and adds no TOML value. Repeated T123 and T111 outputs are separately byte-
  identical, and T111 is unchanged from FR41. Complete chronological and
  reverse reading of `45:47-49:42` retains the Tang-to-Zhu-to-Tang valuation
  handoff. T123 keeps the unsupported `思？` suffix visible under Tang as a
  source-time residual; it does not form a separate substantive contribution.
  Only current T123 `ref-0432` advances the manual ledger, to `516/556`; T111
  is not double-counted. Zhu Jie reaches `76/83`, and the 2400-3000 block
  reaches `118/129`. No real-WebSocket result is attributed to FR42. Critical
  attribution, confident-wrong attribution, speaker-time sign-off, real-path
  repeatability, and holdout evidence remain open; see
  `013-industrial-closing-validation/isolated-vad-single-character-alignment-gap-diagnosis-2026-07-19.md`
  and
  `013-industrial-closing-validation/isolated-vad-single-character-alignment-gap-review-2026-07-19.md`.
  T173-T175 then retain FR43's zero-duration local-pair-tie representation of
  the existing complete-source aligned-VAD closure. T123 has exactly one raw
  zero-duration character whose business interval lacks a positive alignment
  anchor. The phrase galleries agree on a third identity with no local
  activity or primary coverage, while Tang and Xu remain indistinguishable
  under the existing configured short margin. Complete-source and containing-
  VAD session/robust views independently select Xu. The rule preserves every
  existing source-partition, duration, label, activity, primary, gallery, and
  common-clock guard and adds no TOML value. Repeated T123 and T111 outputs are
  separately byte-identical, and T111 is unchanged from FR42. Complete
  chronological and reverse reading of `20:07-22:56` retains Xu's response
  between Shi's prompt and Tang's explanation and finds no neighboring change.
  Only current T123 `ref-0194` advances the manual ledger, to `517/556`; T111
  is not double-counted. Xu Zijing reaches `69/73`, and the 1200-1800 block
  reaches `75/80`. No real-WebSocket result is attributed to FR43. Critical
  attribution, confident-wrong attribution, speaker-time sign-off, real-path
  repeatability, and holdout evidence remain open; see
  `013-industrial-closing-validation/complete-source-local-pair-tie-diagnosis-2026-07-19.md`
  and
  `013-industrial-closing-validation/complete-source-local-pair-tie-review-2026-07-19.md`.
  T176-T179 then retain FR44's three-run middle-slot phrase abstention. T123
  represents one regular punctuation phrase as Shi-Tang-Shi base runs across
  two ordered non-containing VAD records. The Tang middle run has one aligned
  source character. The session phrase view is eligible, while the robust view
  has the same raw top identity and eligible margin but abstains on the existing
  regular score gate. The rule prevents only that generic session-only phrase
  write after independently validating source runs, adjacent alignment,
  primary topology, outer aligned/activity/primary coverage, and VAD rankings;
  it adds no TOML value. Repeated T123 and T111 outputs are separately byte-
  identical, and T111 is unchanged from FR43. Complete chronological and
  reverse reading of `07:42-08:33` retains Shi's calculation, Tang's short
  interjection, Shi's `44/45` answer, and Tang's veto statement. Only current
  T123 `ref-0071` advances the manual ledger, to `518/556`; neighboring source
  partitions are not additional natural-turn repairs. Shi Yi reaches `199/211`,
  and the 0-600 block reaches `88/93`. No real-WebSocket result is attributed
  to FR44. Critical attribution, confident-wrong attribution, speaker-time
  sign-off, real-path repeatability, and holdout evidence remain open; see
  `013-industrial-closing-validation/three-run-middle-slot-phrase-abstention-diagnosis-2026-07-19.md`
  and
  `013-industrial-closing-validation/three-run-middle-slot-phrase-abstention-review-2026-07-19.md`.
  T180-T186 then separate the source-absent `ref-0066` from the source-present
  `ref-0192`, reproduce every captured T123 identity assignment from exact
  streamed PCM, and retain FR45's typed primary-island/alignment-gap echo
  repair. FR45 requires one unique strict-suffix phrase mapping, one exact
  middle primary island inside one positive-character alignment gap, complete
  agreeing session/robust galleries under existing short gates, matching
  activity, and independently reconstructed topology. It adds no TOML value,
  threshold, fitted constant, model, source text, or time rewrite. The final
  warning-clean build passes all 70 CTest entries. Repeated T123 output is
  byte-identical and splits only `text_id=111`; repeated T111 output is byte-
  identical and unchanged from FR44. Complete chronological and reverse
  reading of `20:33-22:42` retains Zhu Jie's independent `没有意见` between
  Shi Yi's answer and request. Only current T123 `ref-0192` advances the manual
  ledger to `519/556`, the 1200-1800 block to `76/80`, and Zhu Jie to `77/83`.
  No real-WebSocket result is attributed to FR45. Critical attribution,
  confident-wrong attribution, speaker-time sign-off, real-path repeatability,
  and holdout evidence remain open; see
  `013-industrial-closing-validation/primary-island-alignment-gap-evidence-diagnosis-2026-07-19.md`
  and
  `013-industrial-closing-validation/primary-island-alignment-gap-echo-review-2026-07-19.md`.
  T187-T200 then complete FR46 and FR47. FR46 stops without a policy after
  complete review finds no shared topology across its 23 critical residuals.
  FR47 captures the exact 45,189-frame four-channel Sortformer v2.1 posterior,
  identifies a bounded same-source future-identity topology, rejects the first
  cross-source candidate, and retains the source-bounded retry. The final build
  is warning-clean, all `70/70` CTest entries pass, and repeated frozen output
  is byte-identical. Complete chronological and reverse review of all 556
  `test.txt` contributions retains only `ref-0507` and `ref-0509`. Its ledger
  was initially transcribed as `521/556`. Clean commit `70f1186` subsequently completes
  the restarted 120/600/full A/B real-WebSocket ladder. Both full artifacts
  receive separate complete chronological and reverse contextual review.
  FR48 repeats all four full readings under the speaker-only boundary and
  corrects only `ref-0375`: its ASR wording is wrong, while `spk_2` already
  identifies 徐子景. FR49 then corrects the omitted `ref-0121` ledger error and
  implements the bounded source-leading topology shared with `ref-0061`.
  Clean commit `1f09052` completes a restarted 120/600/full A/B real-WebSocket
  ladder. Four complete full-artifact readings retain `523/556`, with 27
  confident-wrong, five missing, one uncertain, and 20 critical residuals.
  No whole-session permutation or accumulating tail drift is found. Time-based
  evidence, holdout, report, and release gates remain open.
  See
  `013-industrial-closing-validation/sortformer-v21-posterior-future-epoch-candidate-review-2026-07-19.md`
  and
  `013-industrial-closing-validation/post-fr47-residual-reconciliation-2026-07-19.md`
  and
  `013-industrial-closing-validation/fr47-real-path-promotion-review-2026-07-19.md`.
  See also
  `013-industrial-closing-validation/fr48-speaker-only-reconciliation-and-consensus-diagnosis-2026-07-19.md`.
  See also
  `013-industrial-closing-validation/fr49-real-path-promotion-review-2026-07-20.md`.
- **Result-evaluation rule**: product accuracy and candidate decisions may be
  produced only by complete item-by-item contextual semantic review. No code,
  test, script, notebook, formula, query, automated metric, or algorithm may
  assign correctness, aggregate an accuracy result, rank/select a candidate, or
  issue a product verdict. Automation is limited to execution, capture,
  mechanical/numerical validation, and display of unjudged evidence.

---

## 1. What Orator is

A real-time, edge-deployed (Jetson Orin / Thor) auditory pipeline, **pure C++/CUDA with
zero runtime third-party dependencies**. It ingests a live mono-audio stream over
WebSocket and produces a comprehensive timeline that carries both **speaker
separation** and **ASR transcript** content, one track per pipeline, on one
absolute time base.

## 2. Current phase

**Spec 013 is the active approved closing-validation work.** It does not replace the
implemented feature contracts in Specs 001-012; it defines the architecture
corrections, complete reference review, and conjunctive evidence required before
the combined product can be accepted. Spec 004 remains the feature specification
for time base, comprehensive timeline, and protocol behavior, but its claimed
completion is under a code-compliance review because the current production path
does not satisfy all Article III details.

Spec 004 covers:
- Time base system (`core::TimeBase`, three consistency principles, wall clock anchor)
- Comprehensive timeline (native stateful, revisable, diarization-driven view split)
- Protocol layer (topic-based registration, schema registry, QoS, storage backends)

Spec 005 (time base) and Spec 007 (protocol layer) were merged into Spec 004 and
deleted (2026-06-18). Their historical task status does not override the current
code findings recorded below.

The system runs three active producer pipelines —
diarization (who/when), ASR (what/when), and VAD speech-activity detection — each
feeding one **native, revisable comprehensive timeline** on a single absolute
seconds scale. `AuditoryStream` now owns one immutable session `TimeBase` and
injects it into each private cache, worker, and retained audio store. Pipeline
records now commit as typed `ComprehensiveTimeline` evidence before protocol
serialization: ASR reads VAD snapshots there, and forced alignment subscribes
to finalized ASR records there. A registered `BusinessSpeakerPipeline` consumes
raw typed evidence, owns speaker choice/text projection/gap policy, and writes a
separate revisable `business_speaker` track. `ComprehensiveTimeline` is now a
pure thread-safe typed store; raw ASR and alignment records are append-once and
reject conflicting same-ID deposits.

### Target pipeline responsibility boundaries

- **ASR** outputs ONLY plain transcript text + its own time codes. It has **no**
  speaker awareness and never attributes speakers. ASR now includes `text_id` in
  incremental messages for stable in-place segment tracking.
- **Diarization** outputs ONLY its own speaker identities + time codes. It never
  attributes text.
- **Comprehensive timeline** stores and aligns typed tracks on the common base.
  The registered business-speaker fusion pipeline derives the user-facing
  attribution track; the container itself does not choose a speaker or fill
  missing evidence.

**2026-07-13 Phase 1 verification**: the configured CTest suite passed 51/51.
A 120 s, rate=1 real-WebSocket run with committed `orator.toml`
(`/tmp/orator_spec013_t011_120s.json`) produced 25 diarization, 10 ASR, 39 VAD,
10 alignment, and 25 business-speaker entries. Every alignment/business
`text_id` resolved to one raw ASR record, business spans stayed inside their
source span, the business track exactly matched the compatibility
`comprehensive` view, and each source text was reconstructed byte-for-byte from
its business slices. All seven active extents ended at 1,920,000 samples with
zero gap; `timebase_reconciled`, `timebase_ok`, and `wall_clock_ok` were true.
The single-reader client captured 120 continuous `tegrastats` samples and
finished in 121.214 s (0.990x). This is architecture/transport evidence, not
contextual accuracy evidence.

**2026-07-13 contract/UI verification**: CMake now registers 55 tests (51 C++,
two Python tooling tests, one real-WebSocket Python integration test, and one
Node browser-model test). The
registered WS gate uses an isolated generated TOML and the sole unified socket
client to run 12 s of canonical speech plus 30 s of generated zero PCM; both
had no mechanical contract errors, and silence produced no live or terminal
ASR/business text. A real Chromium 12 s file run passed live rows, flush/end,
terminal ID/extent checks, exact JSON download, persisted-session lookup and
exact reload, deliberate server restart with clean reconnect, fake-device
microphone capture, and desktop/mobile screenshots with no unexpected browser
errors. This is short-path product-contract evidence, not the required physical
microphone, full-session context review, or accuracy result.

**2026-07-15 concurrent UI observation correction**: `SessionEmit` now
broadcasts each stream event to one audio producer and all registered observer
connections. Opening or closing an observer no longer resets the shared
`AuditoryStream`; a second audio producer receives an explicit error and its
bytes are not ingested. The registered 12-second real-WebSocket gate connects
an early observer, connects and disconnects a rejected producer during the
stream, then connects a late observer. The early observer's 37 business events
and nine telemetry events exactly matched the producer, and all retained
connections received terminal SHA-256 `9b1f2b3c...` over the full 12.0-second
extent. A separate real Chromium observer showed live text plus GPU, video
memory, and power, then converged to 2 ASR / 5 diar entries and exported the
same parsed terminal document as the unified client. Desktop and 390-pixel
screenshots had no horizontal overflow or browser errors. The configured suite
passes 64/64. This is transport and UI convergence evidence, not a contextual
accuracy result.

**2026-07-14 frozen baseline**: clean commit `ee0dd82` completed committed-TOML
120/360/600 s real-WebSocket runs at 0.990x/0.998x/0.999x with no mechanical
contract errors. Its 3615.12 s diagnostic run completed in 3616.496 s (1.000x),
and all seven tracks reached 57,841,920 samples with zero gaps, but the package
was correctly rejected because three equal-start overlapping diar pairs had a
different live and terminal order. The 773 normalized records were otherwise
identical. `HandleSpeakerSink` now canonicalizes producer records before both
typed deposit and live serialization; the strict validator remains unchanged,
and `test_typed_evidence_flow` covers the equal-start case. The configured
55-test suite passes. See
[baseline-2026-07-14.md](013-industrial-closing-validation/baseline-2026-07-14.md).
No full-session accuracy result or closing claim follows from this evidence.

**2026-07-15 Sortformer/oracle correction**: the runtime NVIDIA v2 weights and
source checkpoint are pinned by full hashes. The corrected async path now feeds
`[speaker cache, FIFO, current chunk]`, transfers overflow before discard, and
uses the configured silence-placeholder count. The exact five-chunk NeMo
fixture passes at `max_abs=1.43051e-6`, `mean_abs=9.48068e-8`, and 1502/1502
argmax. Non-NeMo `use_silence_profile` and `spkcache_refresh_rate` controls are
removed and rejected when present in TOML. A transitional 3615.12 s run
completed at 1.000x with all seven extents exact, but is diagnostic because its
source/config changed while the old client was running. Its 735 diar records
changed 464/556 reference intervals relative to the baseline, while the
corresponding frozen written-context candidate remained 378 correct, 177
incorrect, and one ambiguous. Parity is fixed; business accuracy is not.

The rebuilt source-stable 120 s run passed all contracts at 0.993x and verified
GPU utilization, CUDA unified-memory use, `VIN` system power, CPU, RAM, and
temperature with 95.83 percent runtime-sample cadence. The old full run's
2300/3615 runtime telemetry samples now correctly fail cadence completeness.
The configured suite passes 64/64, and a real Chromium desktop/mobile run
passed file, final/export, persistence, reconnect, and fake-microphone flows.
See [sortformer-oracle-2026-07-15.md](013-industrial-closing-validation/sortformer-oracle-2026-07-15.md).

**Closing baseline decision (2026-07-15)**: all remaining Spec 013 work uses
streaming Sortformer v2.1 under the checked-in `340/1/188/188` profile. The
compile-time default and `orator.toml` now select the same v2.1 weight file, and
`test_config` prevents either from silently reverting to v2 and verifies that
the deprecated v2 weight is absent. The v2 checkpoint and its obsolete CTest
were deleted; only historical reports and hashes remain. This selects the model
line for closing work; it does not accept any historical contextual diagnostic
as the clean closing-baseline result.

**Reference-ledger start (2026-07-15)**: commit `43523ba` now has a fresh,
hash-validated 556-row ledger for the canonical 3615.12-second input. All rows
remain unsigned. A mechanical source audit found 22 duplicate-timestamp groups,
25 non-positive provisional intervals, and one backward timestamp pair. The
seven continuous work batches cover all 556 rows; no code-based judgment or
provisional boundary is accepted as manual adjudication. No code may assign or
aggregate a result. See
[reference-ledger-v21-2026-07-15.md](013-industrial-closing-validation/reference-ledger-v21-2026-07-15.md).

**Full closing-baseline capture (2026-07-15)**: clean commit `3b40245` streamed
the complete 3615.12-second canonical audio through the real WebSocket path in
3616.442 seconds at 1x. All seven pipeline extents reached 57,841,920 samples
with zero gap; `timebase_ok`, `timebase_reconciled`, and `wall_clock_ok` are
true. The package contains 755 diarization, 287 ASR, 972 VAD, 287 forced-align,
and 935 business-speaker entries, with 287/287 align coverage and no mechanical
fusion issue. Continuous evidence contains 3,441 runtime and 3,606 `tegrastats`
samples; every required field and cadence exceeds 95 percent coverage. A browser
connected during the producer run showed live text, GPU, video-memory, and power
updates. Persisted-session replay then proved exact producer, rendered Web UI,
and downloaded JSON equality at desktop and 390-pixel mobile sizes with no
browser errors or horizontal overflow. See
[closing-baseline-v21-2026-07-15.md](013-industrial-closing-validation/closing-baseline-v21-2026-07-15.md).
Two subsequent source-stable 120-second runs on the current binary produced
exactly equal entry arrays in all five terminal tracks and in the comprehensive
view; only the wall-clock anchor and cold/warm compute metadata differed. This
is a prefix repeatability check, not the two-full-run acceptance requirement.
This completes T044 system evidence. A subsequent complete chronological and
reverse-block manual review of all 556 written-context rows records 443 correct,
112 incorrect, and one ambiguous contribution (`79.6763%`). Tools only arranged
the evidence; no code assigned judgments, calculated the result, ranked a
candidate, or issued the verdict. The reviewer manually derived and checked the
  totals. The result fails the full-session gate and five fixed 600-second block
  gates. It was therefore rejected without spending more review effort on
  speaker-time, source-time-offset, criticality, and independent breakdowns. See
[closing-baseline-v21-context-review-2026-07-15.md](013-industrial-closing-validation/closing-baseline-v21-context-review-2026-07-15.md).

**Engineering closing gates (2026-07-15)**: clean `ce388a7` passed a warning-free
Release build and the complete 64/64 CTest suite, including JavaScript and the
real-WebSocket observer gate. A separate ASan/UBSan Debug build passed 25/25
selected host, threading, transport, timeline, identity, and fusion tests.
Compute Sanitizer reported zero errors for full inherited-v2.1 memcheck and
initcheck, public-kernel racecheck/memcheck/synccheck, batched SGEMM memcheck,
and ASR GEMM memcheck. The attempted 1502-frame full-model racecheck was stopped
when sanitizer instrumentation exceeded approximately 79 GiB host memory and is
not claimed as a pass. T042 is complete; no accuracy claim follows. See
[engineering-gates-2026-07-15.md](013-industrial-closing-validation/engineering-gates-2026-07-15.md).

**Business-speaker audit contract (2026-07-15)**: every live and terminal
`business_speaker` entry now carries a structured, reference-free
`speaker_decision` containing evidence/projection sources, reason, selected and
rejected diar candidates, overlap/coverage/confidence/islands, and decision
margins. The selection policy and raw tracks are unchanged. The complete 64/64
suite passed; a 120-second 1x real-WebSocket run preserved the prior frozen
terminal result exactly after removing only the new field, and a real Chromium
run proved rendered/downloaded/persisted equality, reconnect, fake-microphone,
telemetry, and desktop/mobile rendering. This closes T071 only; physical
microphone, attribution-changing T072 validation, promotion durations, and the
signed 556-row context review remain open. See
[speaker-decision-audit-2026-07-15.md](013-industrial-closing-validation/speaker-decision-audit-2026-07-15.md).
The follow-up T071A utility replays the same decision structure for legacy
terminal packages without a model run or reference labels. It preserves exact
discrete evidence and declares bounded uncertainty for historical three-decimal
confidence and millisecond boundaries; new diar output retains round-trip
confidence. The clean-`3b40245` full package replayed all 935 business entries
and isolated 347 competing-support decisions for later contextual candidate
review without changing a frozen track.

**Accepted-policy maintainability checkpoint (2026-07-17)**:
`BusinessSpeakerPipeline` now owns orchestration and typed-track publication,
while the internal `SpeakerFusionPolicy` owns the accepted fusion-rule
execution. The public API, timeline ownership, TOML surface, rule order,
reasons/sources, and serialized result are unchanged. A retained 3615.120-second
typed-track replay emitted 1,775 business entries before and after extraction;
both files are byte-identical with SHA-256 `04ba82a8...51db9`. The focused tests
passed 3/3, the complete suite passed 101/101, and a new-binary 120-second 1x
real-WebSocket run passed observer, provenance, time, and telemetry contracts.
This is mechanical equivalence and engineering evidence, not a new accuracy
evaluation. See
[speaker-policy-maintainability-2026-07-17.md](013-industrial-closing-validation/speaker-policy-maintainability-2026-07-17.md).

**Speaker-policy active-surface checkpoint (2026-07-17)**: four duplicate
top-two voiceprint rankers and seven duplicate aligned-unit guards now have one
owner each; all challenge-specific conditions and invocation order remain
unchanged. Forty-three one-off candidate generators, their 43 tests, and 51
non-production TOMLs moved to explicit inactive archives. Thirty-three historical
experiment tests left CTest, while the exhaustive C++ production fusion test and
seven evidence/replay/review Python tests remain active. Two product-result
scripts and the compiled reference-projection harness were removed. The retained
full replay remains byte-identical at SHA-256 `04ba82a8...51db9`; the clean suite
passes 68/68, and a new-binary 120-second 1x real-WebSocket run passed observer,
provenance, time, and telemetry contracts. No product-result evaluation follows.
See
[speaker-policy-active-surface-2026-07-17.md](013-industrial-closing-validation/speaker-policy-active-surface-2026-07-17.md).

**Direct-end terminal-path engineering checkpoint (2026-07-18)**: the first
full direct-end recapture at `7721024ceb60` exposed `66.915 s` of terminal work,
including final TitaNet extraction and repeated full evidence scans. FR26 now
adds a reduced typed evidence snapshot, common-sample-span embedding cache,
bounded mature-gallery precompute worker, lowest-priority CUDA stream, maximum-
window warmup, stable evidence-kind indexes, and explicit finalization phase
timings. Final mature-gallery scoring and accepted fusion-policy order remain
authoritative. A warning-clean build and all 68 CTest entries pass. Same-binary
cached/uncached real-WebSocket runs produced exactly equal seven-track payloads
at 120 seconds (`7b291c32...a97634f`) and 600 seconds
(`a6a7ea95...24aa11b`), with direct terminal waits of `0.805/0.844 s` and
`4.514/6.568 s`, respectively. The 600-second cached path moved final
voiceprint construction from `5934.740 ms` to `10.886 ms`, with
`3895.547 ms` spent draining remaining acoustic work first. These are
mechanical engineering observations, not correctness or acceptance judgments.
The subsequent full clean-commit A/B recapture completed with direct terminal
waits of `25.597 s` and `26.305 s`. Complete contextual semantic review retains
the natural-turn gate at 512/556 and 509/556. T101 is complete; T102 and T084
remain open.

**Native-handoff full promotion checkpoint (2026-07-18)**: FR16ABM prevents a
generic phrase-level voiceprint decision from erasing exactly one sustained,
forced-aligned native two-speaker handoff. No TOML parameter changed. Clean
commit `1a475e6b7473` passed the warning-clean build and `68/68` CTest, then
passed 120-second and 600-second production WebSocket promotion with direct
terminal waits of `0.808 s` and `4.590 s`. Empty-registry Run A and restarted
frozen-registry Run B then completed the full `3615.120` seconds at `0.993x`,
with direct waits of `26.503 s` and `26.185 s`, exact seven-track extents,
observer convergence, and complete required telemetry. Complete manual forward
and reverse contextual review records 514/556 and 515/556. The rule repairs
`ref-0071` on both paths without a contextual regression. The review also
  accepts `ref-0250`, whose correct Tang Yunfeng evidence crosses a mechanically
  displayed next-timestamp edge; the whole-second source mark cannot provide a
  contradictory sub-second boundary. T106 is complete. T107-T109 subsequently
  traced and retained the separate delayed-alignment candidate described below.
  T102's manual result breakdowns remain open; full speaker closure is not
  claimed. See
[native-handoff-full-promotion-review-2026-07-18.md](013-industrial-closing-validation/native-handoff-full-promotion-review-2026-07-18.md).

**Delayed-alignment frozen replay checkpoint (2026-07-18)**: FR16ABN reuses
the checked-in TOML punctuation, alignment pause/tolerance, minimum embedding
duration, and aligned-unit count to recover one delayed short clause group only
under a corroborated activity/primary/VAD/forced-alignment A-B-A topology. No
TOML value or producer track changed. The warning-clean build and all 68 CTest
entries pass. Three C++ replays per promoted A/B typed track are byte-stable
within each path and change only the same `569.26-569.42` speaker sequence.
Complete chronological and reverse conversational review retains Xu Zijing's
short confirmations while preserving Shi Yi's following substantive turn. The
candidate advances to the 120/600-second real-WebSocket ladder but is not yet a
new production baseline. T102, T084, and full closure remain open. See
[delayed-alignment-clause-review-2026-07-18.md](013-industrial-closing-validation/delayed-alignment-clause-review-2026-07-18.md).

**Delayed-alignment full promotion checkpoint (2026-07-18)**: clean
transitional experimental commit `6b1cb79fa4f5` passed the warning-clean build,
`68/68` CTest, and 120/600-second real-WebSocket promotion with direct terminal
waits of `0.803/4.607 s`. Empty-registry Run A and restarted frozen-registry
Run B then completed all `3615.120` seconds at `0.993x`. The retained artifacts
have direct waits of `25.849/25.585 s`, exact seven-track common-clock extents,
observer convergence, and required telemetry coverage. The first Run B attempt
is preserved as an excluded mechanical failure because its runtime telemetry
cadence was `94.965%`; a single controlled retry with unchanged behavioral
TOML values passed at `95.214%`. Complete forward and reverse contextual review
of all 556 contributions, plus the T102 `ref-0160` and `ref-0182` context
reconciliations, originally recorded `519/556`. T135's complete A/B reread
supersedes that result with `514/556` for each frozen run and records failures
in the 3000-3600 block and 朱杰 recall.
FR16ABN repairs `ref-0090` on both paths without a contextual regression or
long-range identity drift. T110 and T111 are complete. T102, T084, and full
closure remain open. The separate T112 telemetry cadence fix is now complete.
See
[delayed-alignment-full-promotion-review-2026-07-18.md](013-industrial-closing-validation/delayed-alignment-full-promotion-review-2026-07-18.md).

**GPU telemetry absolute cadence (2026-07-18)**: transitional experimental
commit `d610de36ed13` schedules GPU samples from monotonic absolute deadlines,
skips expired slots, and preserves the checked-in one-second interval and JSON
payload. The clean build and all `69/69` CTest entries pass. A clean 120-second
1.0x real-WebSocket run records 119 runtime samples (`99.167%` cadence), 120
tegrastats samples, exact one-second runtime steps, and 100 percent coverage of
GPU utilization, GPU memory, system power, CPU, RAM, and temperature fields.
This is mechanical evidence only. See
[gpu-telemetry-deadline-review-2026-07-18.md](013-industrial-closing-validation/gpu-telemetry-deadline-review-2026-07-18.md).

**FR16ABO full promotion rejected (2026-07-18)**: clean transitional commit
`f49a8278e0d8` passed the warning-clean build, `69/69` CTest, 120/600-second
real-WebSocket promotion, and full empty/frozen-registry A/B capture. Both full
runs have exact seven-track common-clock extents, direct-end waits below 30
seconds, observer convergence, stable provenance, and complete required
telemetry coverage. Each run then received complete chronological and tail-to-
start contextual semantic review of all 556 contributions against the human-
listened `test.txt`. The historical manually checked result is `518/556` for
each run, and the A/B error sets differ. It predates T135's uniform
material-fragment reconciliation and is no longer ranked against T111. No code
assigned, aggregated, or selected that result. FR16ABO remains dormant in code
for evidence tracing, while checked-in TOML sets its lookahead to zero. See
[future-epoch-full-promotion-review-2026-07-18.md](013-industrial-closing-validation/future-epoch-full-promotion-review-2026-07-18.md).

**FR28 VAD-gated ASR stability checkpoint (2026-07-18)**: frozen replay of each
T116 full typed package is byte-stable. Diarization and primary-speaker tracks
are identical across T116 A/B, while ASR first differs at `text_id=49` around
`658.7/658.8 s`; alignment, ASR-derived voiceprint evidence, and business
projection then diverge. Forward/reverse semantic review of the implicated
`ref-0098`-`ref-0103` context confirms a real `ref-0102` speaker consequence.
FR28 adds typed VAD active/silence frontiers, buffers undecided ASR audio, and
freezes final VAD before ASR drain. Focused tests, the VAD oracle, a warning-
clean build, and all `69/69` CTest entries pass. Two isolated 120-second real-
WebSocket runs at `0.991x` have direct waits of `1.115/1.113 s`, exact common-
clock extents, and identical entries in every product track. Complete
chronological and reverse review of all 18 in-scope `test.txt` contributions
finds no FR28 natural-turn speaker regression. FR28 is retained for the
600-second gate; no full accuracy result or closure claim follows. See
[vad-gated-asr-stability-review-2026-07-18.md](013-industrial-closing-validation/vad-gated-asr-stability-review-2026-07-18.md).

## 3. Component status

| Component | Status | Notes |
|---|---|---|
| Streaming diarization (Sortformer) | v2.1 is the sole closing baseline; acceptance open | Compile-time defaults and the checked-in TOML select streaming v2.1 under the inherited async `340/1/188/188` profile (chunk/right/FIFO/update). Its exact clean 935-entry, 3615.12 s 1x real-WebSocket artifact passed mechanical, common-time-base, and telemetry contracts. Complete chronological and reverse-block manual written-context review records 443 correct / 112 incorrect / 1 ambiguous natural contributions (`79.6763%`); the historical 413 / 142 / 1 result belongs to a different 936-entry artifact and used a cut-oriented diagnostic rubric. NVIDIA's official high `340/40/40/300` and low `6/7/188/144` profiles pass separate 1502-frame NeMo/C++ numerical gates; their historical full native-diar contextual diagnostics record 385 / 170 / 1 (`69.2446%`) and 377 / 178 / 1 (`67.8058%`). Neither official profile advances to a real-WebSocket acceptance run. The v2 checkpoint and obsolete gate are removed; no result is an exact speaker-time acceptance score. |
| Multi-scale TitaNet fusion screening | Frozen experiment complete; runtime integration rejected | A reference-free TOML policy pairs native 3 s and 5 s TitaNet windows by absolute centre time, restricts ranking to active session identities, requires independent score/margin agreement, and permits candidate-strength rewrites only at forced-alignment pauses. From 7,224 spans it retained 1,166 points and 239 runs, changing nine of 936 business entries. Manual contextual review of all 11 affected reference rows found five repairs and no regression, raising the frozen result to 418 / 137 / 1 (`75.1799%`). This fails the 93 percent implementation gate, so policy tuning stops and no runtime/real-WebSocket claim follows. See [speaker-sliding-v21-2026-07-15.md](013-industrial-closing-validation/speaker-sliding-v21-2026-07-15.md). |
| Native Qwen3-ASR engine | Numerical oracle verified; semantic closure open | Stored stage fixtures report mel 3.9e-3, encoder 1.3e-3, and decoder argmax parity. These numerical gates do not establish the Spec 013 full contextual semantic or silence-hallucination gates. Pure bf16 compute. |
| Forced alignment (Qwen3-ForcedAligner-0.6B, Spec 009) | Implemented; numerical and prior WS evidence recorded | The registered `AlignWorker` consumes typed finalized ASR records from `ComprehensiveTimeline`, reads the matching retained audio span, deposits a typed alignment group, then mirrors `align/units` to protocol and WebSocket. Partials are never aligned. Stage-level torch-oracle checks and the 2026-06-30 60-minute real-WebSocket run reported 119/119 segment coverage, 13,594 units, no bounds/monotonicity failures, and no crash after the CUDA grid-stride fix. These historical results establish the aligner implementation, not current Spec 013 product closure; repeatable full-session acceptance remains open. |
| WhisperMel / BPE tokenizer / sharded safetensors loader | ✅ Verified | Unit-tested. |
| Decoupling (interfaces + registry) | Contract corrected; product acceptance open | Model interfaces and registry construction are in place. VAD→ASR, ASR→forced-align, and raw evidence→business speaker now flow only through typed `ComprehensiveTimeline` reads/subscriptions. The registered `business_speaker` pipeline owns fusion policy and writes its own track; protocol topics mirror committed records for persistence and transport. Full product validation remains open under Spec 013. |
| `OverlapTimelineMerger` / `ITimelineMerger` | 🗑️ Removed | The old one-shot max-overlap merger and its orphaned interface were deleted — superseded by `ComprehensiveTimeline` (Spec 004). |
| WebSocket server (libwebsockets v4.3.3) | ✅ Refactored | Replaced hand-rolled POSIX WS with libwebsockets (multi-client, RFC 6455/7692). One connection owns audio production while browser and diagnostic observers receive the same broadcast stream without resetting it; concurrent producer bytes are rejected. Eliminated file-scope static variables (`serve_server`, `serve_factory`, `pss_list_head`) → instance members via `lws_context_user`. Thread-safe `SendText` with wakeup/cancel-service. ServeOnce mode for unit tests. |
| ASR + WS integration | FR28 120-second stability gate passed; full acceptance open | `AuditoryStream` owns one private `PipelineAudioCache` per active producer and uses separate worker threads. One session-owned `TimeBase` is injected into all active stores and workers. Final ASR live emission and its typed sink reuse one `text_id`; partial rejection emits a matching retract, and the terminal ASR track serializes the ID. ASR now buffers undecided audio, reads typed stable VAD frontiers from `ComprehensiveTimeline`, applies TOML lead/chunk settings, and drains only after final VAD is frozen; forced alignment consumes the resulting finalized ASR records. Focused publication-order tests and two independent 120-second real-WebSocket runs produce identical canonical product tracks. Full repeatability and contextual accuracy remain open. |
| Incremental KV-cache ASR streaming (Spec 003) | ✅ Implemented, verified, committed (8cc31ab); params refined 2026-07-03 | Persistent KV cache + prefix caching + chunk-local windowed encoder; partial-emission every 1 s via WebSocket. Full 1hr CER 16.1% / 6.22x; beats production Silero-VAD at every scale. **Current params**: `kStreamWindowMel=100` (1 s), `max_new_tokens=32`, `unfixed_chunks=2`, `unfixed_tokens=15`, `segment_sec=24.0`, `vad_min_overlap_sec=0.12`. 2026-07-03 real WS `test.mp3` 600 s A/B after the VAD-overlap filter: `segment_sec=24` produced 49 ASR finals vs 67 at 12 s, with the same final comprehensive count (115) and better `To C` wording; default restored to 24 s for ASR semantic stability. |
| Revisable comprehensive timeline (Spec 004) | FR50 is the current real-path speaker baseline; canonical closure open | `ComprehensiveTimeline` stores typed diarization, raw diar frame posterior, primary speaker, ASR, VAD, alignment, voiceprint, and business tracks and publishes immutable snapshots/typed updates. Its VAD snapshot includes observed, active, and confirmed-silence frontiers used by scheduling-invariant ASR gating. `BusinessSpeakerPipeline` consumes typed `SpeakerEvidenceStage` output and owns orchestration/publication; the internal `SpeakerFusionPolicy` owns rule execution. FR49's clean `1f09052` ladder manually retains `523/556`. FR50's false-default TOML policy repairs `ref-0327` and `ref-0417`; exact-clean remediation commit `a6f0d33` moves acoustic-only primary-run work into live precompute without changing final scoring. Its full empty/frozen-registry A/B artifacts both receive complete chronological and reverse contextual review and manually retain `525/556`, with 26 confident-wrong, four missing, one uncertain, and 19 critical failures. No executable result produces the ledger. Full direct-end waits pass independently at `26.013/26.789 s`, so T232 promotes FR50 as the real-path baseline. FR51's four exact-baseline contextual evidence readings stop the unsupported single-context final-fusion branch and leave runtime behavior unchanged; FR52's upstream source-edge gate is open. Checked-in VAD threshold is `0.5`. Critical residual, speaker-time, ASR, locked holdout, browser/microphone, report, and release gates remain open. |
| Reusable common time base (Spec 004) | FR50 full-session A/B mechanical gate passed; product closure open | `AuditoryStream` owns one immutable `TimeBase` and injects it into every active private cache, worker, and retained audio store. Finalization reconciles exact sample extents for input, diarization, speaker identity, ASR, VAD, alignment, and business speaker. FR50 exact-clean full Run A and restarted Run B both close all seven tracks at exactly 57,841,920 samples with zero declared extent gap and true reconciliation flags. This is a mechanical contract result; product closure remains open under Spec 013. |
| Pipeline protocol layer (Spec 004) | ✅ Implemented | Phases 7–12 complete: data types (topic.h, schema.h), pipeline registry, topic router, storage layer (MEMORY + DISK), ProtocolTimeline integration, WS v2 envelope with describe command, --storage-disk-path flag. 25/25 tests pass. |
| Streaming validation | FR50 exact-clean ladder passed; later closure gates remain open | `ws_unified_test.py` has one socket reader, captures source/config/binary pre/post hashes, continuous `tegrastats`, runtime telemetry, and independent terminal-command timing. Acceptance mode sends `end` directly after the final audio frame. Exact clean pushed commit `a6f0d33` passes independent empty-registry 120 A/B, 600, full empty-registry A, and restarted frozen-registry B production-WebSocket runs. Full A/B close all seven tracks at 3615.120 seconds, run at `0.993x`, and return terminal timelines in `26.013/26.789 s`, each independently below `30.0 s`. Every output-affecting artifact receives complete forward/reverse contextual review; structural checks never evaluate correctness or issue a product verdict. Browser/microphone and later Spec 013 release gates remain open. |
| Logging system | ✅ Include-level `core/log.h` | Level-based macros (`LOG_DEBUG`/`INFO`/`WARN`/`ERROR`) with compile-time floor (`ORATOR_LOG_LEVEL`) and runtime env-var gate. All 14 `fprintf(stderr)` calls in src/ replaced. |
| CUDA kernel unit tests | ✅ `test_kernels`: 13/13 passed | GPU kernel operations (Add, Multiply, NormalizeVector, CosineSimilarity, BatchCosineSimilarity) validated against CPU reference; includes edge cases (zero, single-element, large 1M vectors). |
| CI pipeline | ✅ GitHub Actions | `.github/workflows/ci.yml`: CUDA 12.5, CMake build + ctest + warning check + Python syntax verification. Triggered on push/PR to master. |
| Test suite | 72 configured CTest entries | The active Sortformer gates are bound to v2.1. Focused C++ tests cover typed speaker evidence, capture-ordered speaker-identity replay input, embedding-cache lifecycle, bounded precompute, final draining, accepted production fusion/abstention policy, and monotonic telemetry deadlines; active Python tests cover source/time/config invariants, immutable raw tracks, WebSocket manifests, telemetry, evidence integrity, replay inputs, and unjudged review packets. Thirty-three historical candidate tests are archived outside CTest. FR50 covers the right-bounded aligned-unit policy plus raw-frame primary-run coalescing across blocks, inactive and slot boundaries, terminal closure, disabled behavior, live-cycle limits, final drain, and exact cache reuse. The final clean build emits no warning or error diagnostic, and all `72/72` CTest entries pass in `53.23 s`. Browser and physical-microphone acceptance remain outside CTest. No automated result may assign correctness, aggregate accuracy, rank/select a candidate, or produce product acceptance. |
| Diar tail parameter experiments | ❌ No accepted fix | 2026-07-10 TOML experiments used `diar_evidence_probe` on full `test.mp3` for strict onset/offset, `min_dur_on=1.2`, `min_dur_on=2.0`, `chunk_left_context=2`, `chunk_right_context=0`, and `left2_right0`. Threshold/min-duration changes deleted evidence without recovering the correct speaker; context variants did not solve 3270-3304 s and some removed the small local-2 hint at 3299.76 s. NeMo full-length reference on the same audio produced the same hard-window spk3 bias (`3270-3304.5`: spk3 313/431 frames; `3240-3360`: spk3 1013/1500 frames). The historical v2 numerical gate passed at that time; its checkpoint and CTest have since been removed. See Spec 012 `diar-tail-toml-experiments-2026-07-10.md`. |
| TitaNet tail voiceprint review | ❌ No accepted override | 2026-07-10 orthogonal speaker-embedding review used `speaker_embedding_probe` on full `test.mp3` with 600 s, 60 s, and 30 s buckets. The hard-window `L3@3270-3300` bucket remains closest to historical L3 (`L3@3300-3330=0.762`, historical L3 up to 0.724) while best non-L3 alternatives are lower (`L0=0.440`, `L1=0.424`, `L2=0.321`). This rejects direct TitaNet override for 3270-3304 s. See Spec 012 `titanet-tail-evidence-2026-07-10.md`. |
| OnText protocol matching | ✅ Fixed | Substring `text.find("end")` → JSON key `text.find("\"end\"")` to prevent false positives on partial matches. Same for reset/flush. |
| GPU telemetry | Runtime/UI verified; absolute cadence follow-up complete | Compile-time default remains disabled; committed `orator.toml` enables 1 s samples. Runtime emits GPU utilization with source, CUDA unified-memory use, frequency, `VIN` system power, and rails; the client combines these with `tegrastats` CPU/RAM/temperature. Accepted FR16ABN Run A and Run B had 100 percent required-field coverage and `95.104%/95.214%` runtime cadence; the Web UI displayed GPU, VRAM, and power on desktop and 390-pixel mobile Chromium. A first Run B attempt failed at `94.965%` because relative waiting plus probe latency accumulated. T112 at `d610de36ed13` now uses monotonic absolute deadlines without changing the configured interval or payload; its clean 120-second real-WebSocket run records `99.167%` runtime cadence, exact one-second steps, and 100 percent required-field coverage. |
| VAD model path | ✅ Migrated | `models/asr/silero_vad.safetensors` → `models/vad/`. Updated 6 file references across test, include, and tools. |
| Web UI (Spec 006) | Contract-hardened; graphical timeline and final acceptance open | Static serving, modular state/router, exact PCM file framing, microphone capture, live transcript/evidence, telemetry, developer status, speaker naming, saved sessions, reconnect, authoritative terminal/load rebuild, and exact JSON export are implemented. Node tests and a real Chromium run verify the short path. The main timeline is currently formatted JSON, not the previously documented four-lane Canvas; graphical time-axis controls, physical microphone evidence, and Firefox/Safari evidence remain open. |
| Configuration consistency | ✅ Typed runtime boundary and resolved capture | Startup applies defaults, TOML, environment, then CLI. Only `ws_main.cc` reads `ORATOR_*` and resolves them into `AuditoryStream::Config`; model, GPU, and transport layers receive typed values. Legacy GEMM A/B environment switches were removed. Every terminal timeline includes the complete canonical `resolved_config`, and `ws_unified_test.py` writes its SHA-256 into a sibling run manifest. |
| Session persistence UI (Spec 004 T135) | Implemented and browser-verified | `SessionStore`, sessions/load RPCs, and the Web UI history panel are active. Metadata parsing now handles the current `audio_sec` field plus legacy `audio_duration`; a real Chromium run finalized, listed, and reloaded an exact 12 s terminal document. |
| ISpeakerEmbedder (core/stages.h) | ✅ Active in Spec 010 | Interface declares a fixed-dimension speaker embedding extractor. Runtime implementation: `model::TitaNetEmbedder`, wired into the diarization pipeline by `SpeakerIdentityStage` when `[speaker].enable=true` and `model_dir` is set. |
| ISpeakerRegistry (core/stages.h) | ✅ Active in Spec 010 | Interface declares a persistent enrolled-speaker registry with 1:N matching. Runtime implementation: `model::SpeakerDatabase`, loaded/saved through `[speaker].registry_path` and used by `SpeakerIdentityStage` for global speaker ids. |
| ISink (core/stages.h) | 🔒 Retained, partially active | Interface for terminal timeline consumers. The runtime uses `Emit` callbacks (std::function) instead for primary flow, but a concrete implementation `JsonSink` exists in `include/io/json_sink.h` and `src/io/json_sink.cc` for JSON serialization to streams. Retained as a contract option for non-callback consumers. |
| ComprehensiveTimeline typed subscriptions | Implemented; acceptance open | The container commits typed records under lock, then emits typed evidence updates after commit. Readers obtain immutable snapshots or ID-keyed records. Focused tests cover callback ordering, unsubscribe behavior, VAD snapshot immutability, append-once conflicts, reset, and raw/business isolation. Runtime transport and full-session convergence remain Spec 013 gates. |

## 4. Measured performance (GPU fixed at 1.3 GHz, power mode MaxN)

Measured through the **real WebSocket** at max push rate, 120 s of `test.mp3`
(`/tmp/orator_stream_120.json`):
- **Diarization**: ~9.6× real-time (compute 12.5 s).
- **ASR**: ~2.6× real-time (compute 46.4 s) — many small endpointed utterances,
  each paying fixed per-call cost. Throughput tuning is deferred by owner
  (Spec 001 NG1).
- **End-to-end stream**: ~2.26× real-time (wall 53 s). Because the two pipelines
  share ONE GPU, the GPU lock serializes device work, so wall ≈
  diar_compute + asr_compute. The threads still overlap their CPU-side work
  (buffering, endpointing, serialization); the wall is GPU-bound.
- Historical run: 25 diarization segments + 27 transcript utterances on one
  time base; transcript matches the verified engine's output. Current
  comprehensive snapshots preserve ASR `text_id` boundaries and split them
  through diarization ownership rather than grouping them into speaker turns.

Clip-based ("whole buffer") numbers are **not** treated as streaming results,
per Constitution Art. IV.

### Full-length (1 hr) verification, 2026-06-25

Full 3615 s of `test.mp3` pushed through the real WebSocket at max push rate
(380× wire speed), GPU warm, same hardware config:

| Metric | Value |
|---|---|
| Audio duration | 3615 s (1.00 hr) |
| Wall time | **3616 s** (60.3 min) |
| End-to-end speed | **1.0× real-time** (1× push rate) |
| ASR compute | ~3.65× real-time (compute RTF) |
| Diarization compute | ~89× real-time |
| VAD compute | ~300× real-time |
| `wall_clock_ok` | True (no clock drift) |
| ASR entries | 476, last at 3615.0 s (100 % coverage) |
| Diarization segments | 724, last at 3615.0 s (100 % coverage) |
| VAD segments | 972 |
| Total messages | ~1253 (comprehensive entries) |

**Historical finding (2026-06-25)**: the then-current protocol subscription plus local VAD cache eliminated the O(N²) `Replay()` calls on the ASR hot path. **Wall time ≈ audio duration (3616s vs 3615s)**. Code-derived duration mappings reported 77.3% (diar track), 67.0% (comprehensive view), and 92.8% for 600 s. These are historical mechanical records, not accuracy evaluations, candidate evidence, or the current evidence-flow architecture.

**Current implementation**: ASR reads an immutable typed VAD snapshot and monotonic processed horizon from `ComprehensiveTimeline`. Protocol messages mirror the committed VAD evidence and are not a private runtime data bus.

### 600 s verification (baseline params, VAD cache fix)

| Metric | Value |
|---|---|
| Audio duration | 600 s |
| Wall time | ~600 s (1× real-time) |
| Diar track accuracy | **92.8%** (duration-weighted vs test.txt) |
| ASR RTF | ~3.65× |
| Diar RTF | ~89× |
| `wall_clock_ok` | True |

Speaker mapping correct: [0]→朱杰, [1]→徐子景, [2]→唐云峰, [3]→石一. 600s diar track accuracy **92.8%** exceeds baseline 89.4%.

**Full-length (3615s) with baseline params**:

| Metric | Value |
|---|---|
| Audio duration | 3615 s (1.00 hr) |
| Wall time | **3616 s** (60.3 min) |
| End-to-end speed | **1.0× real-time** (1× push rate) |
| Diar track accuracy | **77.3%** (duration-weighted) |
| Comprehensive view accuracy | 67.0% (unknown gaps 14.3%) |
| Speaker mapping | 4/4 correct (朱杰/徐子景/唐云峰/石一) |

**Historical interpretation, now rejected**: the former claim that 77.3 percent
proved one-hour speaker-cache degradation was not supported by the required
full contextual review. The 2026-07-15 exact FIFO correction changes
assignments across 464/556 reference intervals, and the remaining written-
context failures are distributed across the session rather than confined to
  the tail. The figures in this subsection are retained only as old code
diagnostics and must not be used as a causal or acceptance result.

> **Note (superseded methodology)**: the 92.8% / 77.3% figures above are
> duration-weighted code metrics over the diarizer's per-window LOCAL slots
> (an optimal-mapping upper bound, not a deployable identity). Speaker accuracy is
> now judged only by **complete contextual semantic comparison** (Test Review
> Protocol), never by code. The long-session diar degradation is mitigated by the
> periodic diarizer reset (commit 7507748) and the cross-session GLOBAL identity
> layer finalized in Spec 010 (see "cross-session identity finalized" below): the
> full 60-min stream now yields exactly 4 stable global speakers.

**Historical performance note**: the 2026-06-25 VAD cache change replaced O(N²) `Replay(0.0)` calls and produced wall time near audio duration (3616 s vs 3615 s). The current implementation preserves the O(1) immutable-snapshot read through `ComprehensiveTimeline` without a protocol subscription.

### Spec 002 baseline (Phase 1, measured before any engine change)

Three configurations, 120 s of `test.mp3`, through the real WebSocket at max
push rate, GPU fixed at 1.3 GHz, power mode MaxN:

| Configuration | Wall time | GPU compute | GPU-busy fraction |
|---|---|---|---|
| Diarization only | 3.2 s (37.2×) | 3.0 s (39.9×) | 78.8% |
| ASR only | 38.4 s (3.13×) | 33.9 s (3.54×) | 72.8% |
| Both (current, global lock) | 53.3 s (2.26×) | — | ~63% |

Findings:
- The lower bound on total wall time is the larger single-pipeline compute time,
  which is ASR (~38 s). The current both-pipelines wall time is 53 s, so the
  global lock adds about 15 s of serialization.
- Diarization alone is about 3 s of GPU work, but under the global lock its
  measured time rises to 12.5 s because it waits behind ASR. The lock delays the
  latency-critical pipeline.
- ASR alone leaves the GPU idle about 27% of the time, so diarization's small
  GPU work can run during ASR's idle intervals.
- Realistic target (M3): reduce total wall time from 53 s toward the ASR-only
  floor (~38–40 s, about 3.0× real-time), a 25–28% reduction. The total cannot
  go below ASR-only without an ASR speedup (Spec 001 NG1, deferred).

## 5. Decisions on record

- **No quantization at this stage.** int8 was prototyped and **fully reverted**;
  decode is pure bf16. Any quantization is deferred to a separate, scheduled
  effort (Constitution II.3).
- **Two independent pipelines + threaded controller** is the agreed architecture
  (Spec 001). The main process owns and controls the worker threads.
- **Engineering quality is a ratified requirement** (Constitution Art. V):
  readability, organization, maintainability, extensibility, concurrency safety.
- **Spec consolidation**: Spec 004 is the unified spec for time base + comprehensive
  timeline + protocol layer. Spec 005 and Spec 007 are superseded. No new spec
  numbers will be created for overlapping scope.

## 6. SDD artifacts

- [.specify/memory/constitution.md](../.specify/memory/constitution.md) — v1.7.0 (one canonical session clock; supplemental test provenance; context-only product-result evaluation)
- [specs/001-streaming-pipeline/spec.md](001-streaming-pipeline/spec.md) — implemented
- [specs/001-streaming-pipeline/plan.md](001-streaming-pipeline/plan.md) — implemented
- [specs/001-streaming-pipeline/tasks.md](001-streaming-pipeline/tasks.md) — implemented
- [specs/002-gpu-scheduling/spec.md](002-gpu-scheduling/spec.md) — **COMPLETED** (2026-06-17): all 17 tasks done
- [specs/002-gpu-scheduling/plan.md](002-gpu-scheduling/plan.md) — **COMPLETED**
- [specs/002-gpu-scheduling/tasks.md](002-gpu-scheduling/tasks.md) — **COMPLETED**
- [specs/003-sliding-window-asr/spec.md](003-sliding-window-asr/spec.md) — implemented (8cc31ab)
- [specs/004-comprehensive-timeline/spec.md](004-comprehensive-timeline/spec.md) — **UNIFIED SPEC** (time base + comprehensive timeline + protocol layer). Implemented (all phases 1–12). Supersedes 005 and 007.
- [specs/006-web-ui/spec.md](006-web-ui/spec.md) — **In progress; contract hardening browser-verified 2026-07-13**. The modular ES client routes all known protocol/RPC types, maintains stable-ID live state, treats terminal/loaded documents as authoritative, shows device/pipeline telemetry and developer status, supports file/microphone input, speaker naming, saved sessions, reconnect, and exact export. Dependency-free Node tests and a real Chromium 12 s flow passed. The main timeline currently displays formatted authoritative JSON; the graphical multi-track time axis, physical microphone evidence, and Firefox/Safari evidence remain open, so the historical 16/16 and four-lane Canvas claims are withdrawn.
- [specs/006-web-ui/plan.md](006-web-ui/plan.md) — implemented
- [specs/006-web-ui/tasks.md](006-web-ui/tasks.md) — implemented
- [specs/011-observability/spec.md](011-observability/spec.md) — **Implemented** (2026-06-30): offline [rerun](https://rerun.io) visualization, kept entirely in `tools/` (no runtime third-party dep, Art. I). **Phase 1**: `tools/verify/py/ws_unified_test.py` captures the runtime's periodic `gpu_telemetry`/cursor WS samples into a `telemetry` array; `tools/observability/timeline_to_rerun.py` keys diarization/comprehensive lanes by the global `speaker_id` (`spk_N`) + per-pipeline RTF lanes. **Phase 2 (comprehensive dashboard)**: `TegraSampler` records a continuous `device_series`; the exporter renders six namespaced dimensions on one `audio_time` axis — `pipelines/*`, `comprehensive/<id>` swimlanes, `scheduler/<pipe>/{rtf,compute_sec,active,cuda_priority}`, `cursors/<pipe>/{position_sec,pending_sec}`, `device/{mem,cpu,gpu,temp,power}/*` (extended tegrastats parse; Orin `GR3D_FREQ` optional, omitted on Thor), and `session/summary` — laid out by a `rerun.blueprint` persisted in the `.rrd`. Methodology + best practices in `tools/observability/README.md`. **Config fix**: nested `[telemetry.cursor]` was never read (`config["telemetry.cursor"]` literal-key lookup) → now `config["telemetry"]["cursor"]`, with a `test_config` regression. Validated on a `rate=1` 120 s run: 125 gpu + 125 cursor + 126 device samples, six dimensions populated, stream_rt 0.964×, ctest 47/47, zero warnings. Follow-ups: live WS→rerun consumer, full-hour acceptance recording.
- [specs/010-speaker-id/spec.md](010-speaker-id/spec.md) — **Implemented, with Phase H experiment not accepted as accuracy fix; local-diar operating profile restored**: speaker identity (TitaNet-Large voiceprint enrollment / re-identification as a post-diarization stage inside the diar pipeline, Art. III). **Phase A complete & committed**: A1 acquire+convert weights → `models/speaker/titanet_large.safetensors` (108 tensors); A2 NeMo oracle (`tools/reference/titanet_oracle.py`, isolated `tools/.venv-nemo`); A3 pure C++/CUDA `model::TitaNetEmbedder` (`include/model/titanet_embedder.h` + `src/model/titanet_embedder.cu`, time-major [T,C]: mel+per_feature → 5-block ContextNet encoder → attentive statistics pooling → 192-d, F32 weights); A4 `test_titanet` validated vs NeMo oracle (**span cosine 1.000000/0.999999/1.000000, cross-span matrix to 4 decimals; ctest 46/46, no warnings**). **Phase B complete & committed**: `pipeline::SpeakerIdentityStage` (clean-segment gate + per-local embed/match/enroll via `SpeakerDatabase` + revisable local→global map), wired into the diar pipeline behind a `DiarizationWorker` segment-processor hook + `[speaker]` config; diar message/track expose a backward-compatible `speaker_id` field. **2026-07-06 validation**: Phase H conservative cross-session candidate (`/tmp/orator_phaseh_full.json`) was rejected by context review [local-diar-review-2026-07-06.md](010-speaker-id/local-diar-review-2026-07-06.md): it reduced wrong late globals into local-only gaps but did not restore attribution. Follow-up restored Sortformer local-diar runtime tuning to the async/no-reset profile (`spkcache_update_period=188`, `chunk_right_context=1`, `spkcache_sil_frames=3`) in `orator.toml`; lower-level `SortformerConfig` defaults remain tied to the existing NeMo oracle fixture. Full-length real WS `/tmp/orator_full_async_default_20260706.json`: 3615 s audio, 3618.487 s wall, stream RT 0.999x, diar 773, ASR 288, VAD 972, 3611 tegrastats samples, stable 4 global ids and no local-only gaps; context review [local-diar-default-188-review-2026-07-06.md](010-speaker-id/local-diar-default-188-review-2026-07-06.md) accepts the stable operating profile but records residual rapid-turn fragmentation in 3000-3615 s and an ASR repeat burst at 1927-1944 s.
- [specs/012-evidence-fusion-timeline/spec.md](012-evidence-fusion-timeline/spec.md) — **Runtime candidate validated (2026-07-08); tail evidence reviewed and support diagnostics added (2026-07-09)**: evidence-first comprehensive timeline fusion plus TOML-gated runtime adoption. `tools/verify/py/fusion_audit.py` and `speaker_business_review_packet.py` read frozen `ws_unified_test.py` JSON packages, audit ASR/diar/VAD/align consistency, and emit candidate/business-turn views without mutating captured tracks. After the 2026-07-07 context review showed forced alignment alone did not recover speaker-business accuracy, 2026-07-08 fixes added local-speaker drift/competing-identity split and backfill, per-entry comprehensive `speaker_id`, and `[timeline]` align-run split parameters. Full-length real WS run `/tmp/orator_timelinefusion_full_20260708.json`: 3615.0 s audio, 3618.74 s wall, stream RT 0.999x, diar 773, ASR 288, align 288/288. Fusion audit `/tmp/orator_timelinefusion_full_20260708_fusion_bt_timeline.json`: business_turns=728, unknown 171.860 s (4.75%), no mechanical audit issues. Complete contextual review [drift-epoch-review-2026-07-08.md](012-evidence-fusion-timeline/drift-epoch-review-2026-07-08.md) follows [speaker-business-method.md](012-evidence-fusion-timeline/speaker-business-method.md). Follow-up candidate decisions are historical context-review records. All code-derived percentages and evidence scores in Spec 012 are mechanical records only; they may not evaluate accuracy, rank/select a candidate, or issue a product verdict under Constitution 1.7.0.
- [specs/013-industrial-closing-validation/spec.md](013-industrial-closing-validation/spec.md) - **FR51 exact-baseline evidence audit is complete; FR50 remains the current real-path speaker baseline; FR52 source-edge provenance and canonical closure remain open**. Earlier FR16ABO, FR28, FR29, FR30, and FR31 promotions remain rejected; FR32-FR47 are retained bounded historical repairs and FR46 is evidence-only. FR49's clean ladder manually retains `523/556`. FR50's current-config v2.1 evidence audit authorizes a false-default TOML topology for `ref-0327` and `ref-0417`; frozen replay manually retains `525/556`. After the first clean `b449dfa` ladder fails full A's terminal limit, pushed transitional remediation commit `a6f0d33` moves acoustic-only primary-run preparation into the low-priority live worker without changing a numeric TOML value or final scoring. Its exact-clean 120 A/B, 600, and full empty/frozen-registry A/B ladder passes. Full A/B run at `0.993x` and independently return terminal timelines in `26.013/26.789 s`. Both full artifacts are read completely in chronological and reverse fixed-window context, and all four readings manually retain `525/556`, with 26 confidently wrong, four missing, one uncertain, and 19 critical residuals. FR51's four independent contextual evidence readings identify `ref-0099` as the only final-only overwrite, but the accepted controls provide no second material topology, so that final-fusion branch stops without a runtime change. See [fr50-real-path-terminal-remediation-2026-07-23.md](013-industrial-closing-validation/fr50-real-path-terminal-remediation-2026-07-23.md) and [fr51-exact-baseline-critical-residual-review-2026-07-23.md](013-industrial-closing-validation/fr51-exact-baseline-critical-residual-review-2026-07-23.md). No code evaluates correctness, calculates the ledger, or produces the product verdict. Checked-in `vad.threshold` is `0.5`. Critical residual, FR52 source-edge, speaker-time, per-speaker time, source-time-offset, ASR, browser/microphone, locked holdout, report-review, and release-tag gates remain open.

## 7. Immediate next step

Keep exact clean commit `a6f0d33`, streaming v2.1 `340/1/188/188`, the FR50
policy, checked-in TOML, audio, reference, and registry sequence fixed as the
current real-path speaker baseline. T232 is complete: full A/B independently
pass direct-end at `26.013/26.789 s`, and complete forward/reverse contextual
reading manually retains `525/556`. This exceeds the 90% bottom line but does
not close the business because 19 critical attribution residuals remain.

FR51 has completed that 19-residual audit through independent chronological
and reverse complete-context readings of exact Run A and Run B. Only
`ref-0099` is a final-only overwrite, and its single material topology does not
authorize a production guard. The final-fusion branch is therefore stopped;
the FR50 result and runtime remain unchanged.

T236 is next. Reuse the exact FR50 evidence without another audio run and
arrange complete A/B source-edge worksheets for `ref-0049`, `ref-0066`,
`ref-0118`, `ref-0313`, `ref-0331`, and `ref-0390` plus their named controls.
T237 must read each complete context chronologically and in reverse for A and
independently for B. T238 may authorize a producer experiment only if at least
two material contexts establish one reference-free source-edge contract with
explicit abstentions; otherwise it must stop and name the upstream producer.

Do not start an ASR-accuracy closing phase before this speaker residual audit.
ASR, VAD, or alignment changes can alter the source partitions used by the
current speaker judgment and would invalidate affected FR50 evidence. Capture
and structural tools may continue to arrange raw evidence and verify
mechanical contracts, but no executable result may evaluate correctness,
aggregate accuracy, rank a repair, or issue a product verdict.
T110-T116 and T135 are complete. `test.txt` is the human-
listened reference; T116 used it for complete 556-row chronological and tail-
to-start review of both new full runs, not a script score or a request for new
listening. Both T116 paths historically record `518/556`; that pre-T135 ledger
is not used for ranking, and FR16ABO's checked-in TOML lookahead is zero. T135
records the T111 3000-3600 block, 朱杰 recall, critical-speaker, and
confident-wrong attribution as failed.

T125 through T134 and T123 are complete. T127 retains FR29 after three
byte-identical frozen production-projector replays and complete changed-context
review. T128 passes two independent 120-second real-WebSocket captures and one
clean 600-second gate. T123 then completes independent full empty/frozen-
registry A/B paths with identical seven-track entries, but the corrected
complete forward/reverse review records `505/556` for each and rejects
promotion.
T130 changes only checked-in TOML
`vad.threshold` from `0.5` to `0.3` and passes the VAD oracle, clean build, full
CTest, and frozen-silence gates. T131 then passes three real-WebSocket silence
gates, two repeated 120-second captures with exact normalized seven-track
equality, and complete independent forward/reverse contextual reading of all 18
in-scope contributions. T132 passes one clean 600-second run and complete
forward/reverse contextual reading of all 93 contributions and all ten changed
T128 contexts. T133/T134 then complete one full empty-registry run and one
independently restarted frozen-registry run from clean commit `a96e278ea340`.
Both are mechanically repeatable, but corrected complete contextual semantic
review against `test.txt` records `497/556` and rejects FR30. The checked-in
threshold is restored to `0.5`. The next diagnosis must preserve that
segmentation and separate T123-only upstream evidence losses from speaker
fusion errors already present in T111. A bounded low-energy rescue cannot be
treated as a repair for `ref-0503`, which is already wrong in T111.
Speaker-time, per-speaker time, and source-time offsets remain to be manually
signed at `test.txt`'s recorded precision; no duplicate listening or invented
sub-second boundary is required.
T136-T157 are complete. FR31 is rejected after deterministic frozen replay and
complete review of every changed T111/T123 context. FR32 is retained after
changing only T123 `text_id=84`, repairing `ref-0154`, leaving T111 unchanged,
and completing the full real-WebSocket ladder. Independent full empty/frozen-
registry runs are mechanically repeatable and each complete forward/reverse
contextual review records `506/556`. Two fixed blocks, 朱杰 recall, critical
attribution, and confident-wrong attribution fail, so no closing claim follows.
T143 specifies a source-free partition-invariant abstention candidate on
frozen evidence; T144 retains FR33 after deterministic T111/T123 replay and
complete forward/reverse review of every changed context. The frozen candidate
is `507/556`, but no new full real-path result follows. T145 records FR33 as a
warning-clean, `69/69` transitional checkpoint. T146-T148 retain FR34 after a
separate deterministic T111/T123 replay and complete forward/reverse review,
advancing the frozen candidate to `508/556`. T149-T151 retain FR35 after the
same bounded frozen gate, advancing it to `509/556`. T152-T154 retain FR36
after a separate same-slot six-view diagnosis and complete forward/reverse
review, advancing the frozen candidate to `510/556` and passing the 2400-3000
fixed block at `117/129`. T155-T157 retain FR37 after a separate bracketed-
primary/adjacent-VAD diagnosis and complete forward/reverse review, advancing
the frozen candidate to `511/556` and the 3000-3600 block to `78/87`.
T158-T160 retain FR38 after a separate cross-VAD phrase-tail diagnosis and
complete forward/reverse review, advancing the frozen candidate to `512/556`
and passing the 3000-3600 natural-turn block at `79/87`. T161-T163 retain FR39
after a partition-stable source-leading phrase/tail diagnosis and complete
forward/reverse review, advancing the frozen candidate to `513/556` and the
3000-3600 block to `80/87`. None of these results is a new real-WebSocket run;
T164-T166 retain FR40 after a partition-invariant two-unit primary-handoff
diagnosis and complete forward/reverse review, advancing the frozen candidate
to `514/556` and the 0-600 block to `87/93`. T167-T169 retain FR41 after a
single-unit partition diagnosis and complete forward/reverse review, advancing
the frozen candidate to `515/556` and Zhu Jie to `75/83`. All four
per-speaker natural-turn floors now pass. T170-T172 retain FR42 after an
isolated-VAD single-character alignment-dropout diagnosis and complete
forward/reverse review, advancing the frozen candidate to `516/556`, Zhu Jie
to `76/83`, and the 2400-3000 block to `118/129`. None of these results is a
new real-WebSocket run. T173-T175 retain FR43 after a complete-source local-
pair-tie diagnosis and complete forward/reverse review, advancing the frozen
candidate to `517/556`, Xu Zijing to `69/73`, and the 1200-1800 block to
`75/80`. This is also not a new real-WebSocket run. T176-T179 retain FR44
after a three-run middle-slot phrase-abstention diagnosis and complete forward/
reverse review, advancing the frozen candidate to `518/556`, Shi Yi to
`199/211`, and the 0-600 block to `88/93`. This is not a new real-WebSocket
run; critical and confident-wrong attribution, speaker-time sign-off,
repeatability, and holdout gates remain open.
T180-T186 retain FR45 after capture-faithful exact-PCM identity replay, a
strict primary-island/alignment-gap evidence gate, deterministic T111/T123
projection, and complete forward/reverse review. Only current T123 `ref-0192`
advances the frozen candidate to `519/556`, Zhu Jie to `77/83`, and the
1200-1800 block to `76/80`. T111 remains unchanged. This is not a new real-
WebSocket run; the business coordinate remains the existing forced-alignment
time rather than the acoustic query island. Critical and confident-wrong
attribution, time-based sign-off, real-path repeatability, and holdout gates
remain open.
T187-T190 complete FR46 as an evidence-only stop. Two exact-PCM empty-registry
replays reproduce all 1,254,049 captured identity strings with zero differences
and display byte-identical dual-gallery evidence for all 1,348 frozen primary
runs, including unavailable spans. Manual reconciliation leaves 37 residuals,
23 of them critical, across every complete 600-second block. Complete forward
and reverse contextual review of all 23 finds only `ref-0099` with mutually
corroborating correct source, alignment, activity, primary, VAD, and both
identity galleries that final fusion overwrites. Every other critical context
has a distinct missing/displaced-source, subminimum-span, evidence-disagreement,
partial-support, or producer-wrong boundary. No shared multi-residual topology
is established, so FR46 changes no production code, TOML, model, audio result,
ledger, or acceptance status. The next speaker-closing phase must investigate
whether the two orthogonal speaker models expose complementary raw evidence
before another fusion policy is specified.
T191-T194 define the FR47 evidence phase. It reproduces the complete
four-channel Sortformer v2.1 posterior from T123's exact streamed PCM and frozen
TOML, mechanically proves repeatability and equality with the frozen primary
producer, and displays every raw channel beside the TitaNet and typed timeline
evidence for all 23 critical contexts plus accepted controls. Only complete
forward and reverse contextual semantic review against `test.txt` may decide
whether complementary evidence exists. At this evidence checkpoint, no policy
or audio run was authorized.
T191 is complete. Two frozen-TOML runs over T123's exact streamed PCM emit the
same 45,189-row four-channel Sortformer v2.1 posterior at SHA-256
`79fd2c416ac76a0af477f98bf8d848f6e604b2d94c5c4445e653978afd6c7e41`.
Its mechanical top-1 compression reproduces all 1,348 frozen T123 primary runs
with unchanged local slots and only six-decimal serialization differences.
This validates evidence provenance only. T192 is also complete: 23 context
directories contain the raw posterior, all six frozen upstream track kinds,
the FR45 business view, identity epochs, and every named accepted neighboring
control. The 141-file manifest has SHA-256
`f839d1efd360b812ef771585e62081ec9a4a1b7efe70e66f79e31c1e02810359`
and no captured file is empty. No result label was automated.
T193 and T194 are complete. Complete forward and reverse contextual review
rejects broad secondary-channel, primary-preference, source-continuity, and
identity-backfill rules, but identifies one bounded frozen-candidate topology
shared by `ref-0507` and `ref-0509`. In both contexts one local slot has exact
raw top-1/top-2 support before receiving a different strong global identity at
`3330.640`; the accepted `ref-0508` and `ref-0510` controls lack the existing
frame-gate crossing, and `ref-0511` is already inside the new epoch. T195-T197
test this topology with the already deposited raw frame track, existing TOML
numerical gates, deterministic frozen replay, and complete changed-context
semantic review. This authorization was limited to frozen evidence.

T195 and the first T197 review reject the initial candidate. Its two frozen
replays are byte-identical, yet complete forward and reverse review finds
cross-source wrong rewrites in ASR finals `175-181`, `253-257`, and `264-271`;
only the two changes in source `283` agree with `test.txt`. T198 therefore
requires the future epoch, minimum epoch duration, and matching primary support
to close inside the same immutable ASR final, with no new numeric parameter.

T198-T200 are complete. The source-bounded implementation passes a warning-
clean build and all `70/70` CTest entries, including the new cross-text
abstention. Its two 1,715-entry frozen outputs are byte-identical at SHA-256
`27f90ce43f4b226750cadaf5b11b949986536478e84524c0715c2c477b0c85e6`;
the one-boolean-disabled control is independently byte-identical at SHA-256
`174319361040f648b4f930e312986e626f6b5cba9e3d8eaad9aeaa4a0bc7e7f1`.
Only two exact ranges in source `283` receive the new reason. Their complete
forward/reverse context retains `ref-0507` and `ref-0509` while preserving the
Shi-Tang handoff and accepted controls.

A fresh complete read covers all 556 contributions chronologically and again
in reverse fixed windows. This review initially transcribes `521/556`. FR48's
later speaker-only reread corrects only `ref-0375`, whose ASR wording is wrong
but whose final canonical speaker is correct. The current manual ledger is
`522/556`; 28 confident-wrong, five missing, and one uncertain contribution
remain, including 20 critical failures. The seven block ledgers are `88/93`,
`79/84`, `76/80`, `75/80`, `119/129`, `82/87`, and `3/3`; all four per-speaker
natural-turn floors remain passed. FR47 is retained as a frozen transitional candidate and may
enter the clean 120/600/full A/B real-WebSocket ladder after commit. It is not
a closing or real-path result. See
`013-industrial-closing-validation/post-fr47-residual-reconciliation-2026-07-19.md`.

T201 is complete on clean pushed commit `68016fd`. Two independent
empty-registry 120-second WebSocket runs close every product track at
1,920,000 common-clock samples, pass producer/observer terminal equality and
required telemetry contracts, and have the same normalized seven-track entry
bundle SHA-256
`e8613dfbdffbbb3394d5e80955eb73d30e7f200c4ffb4b3058df3a1b805928b8`.
Complete chronological and reverse contextual reading of `ref-0001` through
`ref-0018` finds no new speaker regression; known cold-start and rapid-short-
turn defects remain visible.

The first T202 attempt stops at a mechanical terminal defect and contributes
no speaker-product result. It consumes all 9,600,000 input samples, and the
server persists a 1,385,739-byte session document at SHA-256
`6b4b48b771809a29bf2276a2659fc39900677e7a058f5b41c301180f3c4f8258`,
but fixed 256-byte `speaker_voiceprint` formatting truncates long records and
produces incomplete JSON boolean tokens. The producer receives no parseable
terminal document during its 600-second wait, so no review packet or contextual
judgment is derived from this attempt.

T204 is complete. Speaker-voiceprint records now use a
shared variable-length serializer: escaped identifiers append directly and
numeric/boolean fragments use dynamically sized formatting. A focused exact-
record regression covers escaped evidence and speaker identifiers longer than
256 bytes. The full build has no warning diagnostics and all `70/70` CTest
entries pass in 52.99 seconds. No model, fusion policy, time code, speaker
assignment, or TOML parameter changes.

T205, T202, and T203 are complete on clean pushed commit `70f1186`. The
restarted independent 120-second A/B paths and clean 600-second path pass their
mechanical contracts and complete in-scope forward/reverse contextual reviews
before promotion advances. Full Run A starts from an isolated empty registry;
Run B restarts the same binary with Run A's frozen registry. Both consume the
exact 57,841,920 PCM samples over 3615.120 seconds at `0.995x`, close all seven
tracks on the common time base, converge producer and observer terminals,
provide complete required telemetry, and finish direct-end in `17.559 s` and
`17.392 s`. The registry remains byte-identical after B.

Every one of the 556 `test.txt` contributions in each full artifact is read
chronologically and again in reverse fixed windows. No executable mechanism
assigns correctness or aggregates the result. FR48 independently repeats all
four readings under the speaker-only boundary and corrects only `ref-0375`.
Both manually signed reviews retain `522/556`, with 28 confident-wrong, five
missing, one uncertain, and 20 business-critical residuals. All seven fixed-window and four per-speaker
natural-turn floors pass, and no whole-session permutation or accumulating
late identity drift appears. FR47 is therefore the current repeatable real-
WebSocket speaker-business candidate rather than frozen-only evidence. Critical
attribution, confident-wrong attribution, speaker-time, per-speaker time,
source-time offset, locked holdout, final report, and release gates remain
open. See
`013-industrial-closing-validation/fr47-real-path-promotion-review-2026-07-19.md`.

T206-T210 complete FR48 on its stop branch. Reference-free direct-track export
from both full timelines reveals that the replay TSV omitted the already typed
`session_gallery_complete` field; the exporter and exact regression now
preserve it. Both baseline replays restore all 1,716 business records and are
byte-identical. Complete forward/reverse contextual review of all 24 ordinary
direct-short/native conflicts finds many accepted direct repairs and only one
material context, `ref-0099`, with the full proposed hierarchy. A single-
context fit cannot authorize a product policy, so no guard, TOML field,
candidate replay, or audio run is added. The focused exporter suite passes
`3/3`, the full build has no warning or error diagnostic, and all `70/70`
CTest entries pass in `53.12` seconds. These checks are mechanical engineering
evidence only. See
`013-industrial-closing-validation/fr48-speaker-only-reconciliation-and-consensus-diagnosis-2026-07-19.md`.

T211-T214 complete FR49's evidence gate and bounded implementation. A
reference-free inventory exposes 271 short-primary contexts in each frozen A/B
path. Complete review of every context and accepted control corrects the prior
manual omission of `ref-0121`, making the pre-candidate real-path ledger
`521/556`. Two independent material contexts, `ref-0061` and `ref-0121`,
authorize one source-leading right-bounded primary-prefix restore using only
existing TOML gates and typed primary/activity/alignment/TitaNet evidence. The
first replay's `ref-0304` tail leak is rejected and covered by a same-primary
candidate-source abstention.

The corrected two A and two B frozen candidate outputs are byte-identical at
SHA-256
`91e1e7ab08f6c593b73762b158b5c4ee9c58eaf68ea59eb8f9ee34c21f747c30`;
disabled A/B controls exactly reproduce FR48 at SHA-256
`75fc0b39fdf4530ec98a54f8e6ac113e8eef1aee00839c3d9c6577adafb8302e`.
Raw scope changes only `467.564-467.644` and `817.692-818.412`. Every one of
the 556 contributions is then read chronologically and in reverse fixed windows
for A and independently for B. All four complete readings manually retain only
the two authorized corrections. The frozen ledger is `523/556`, with 27
confident-wrong, five missing, one uncertain, and the same 20 critical
residuals. See
`013-industrial-closing-validation/fr49-source-leading-primary-prefix-diagnosis-2026-07-19.md`.
The final clean build has no warning or error diagnostic, both new Python files
pass syntax compilation, and all `71/71` CTest entries pass in `53.22 s`.
Post-build A1/A2/B1/B2 and disabled A/B replays reproduce the reviewed hashes
byte for byte. These checks establish engineering consistency only.

T216-T221 then complete FR49's clean real-path promotion. Commit `1f09052`
passes two independent 120-second runs and one 600-second run, with complete
in-scope forward/reverse contextual reading before each promotion. Full Run A
starts empty; Run B restarts with only A's frozen registry. Both consume
57,841,920 samples, close all seven tracks, converge observers, provide
complete telemetry, and finish direct-end in `29.015 s` and `28.820 s`.
Every full artifact is independently read across all 556 contributions in both
directions. The manually signed real-path ledger remains `523/556`; no new
regression, whole-session permutation, accumulating late drift, or tail-only
collapse is found. FR49 is therefore the current repeatable real-WebSocket
candidate, not a closing result. The business storage is restored and no
validation process remains. See
`013-industrial-closing-validation/fr49-real-path-promotion-review-2026-07-20.md`.

T112 is complete and does not alter the speaker baseline. T084 closes only
after both A and B independently pass every applicable gate.
ASR, browser/microphone, locked holdout, final-report review, and release signing
remain later workstreams. The bullets below are historical
implementation and measurement records unless explicitly marked as current
acceptance evidence.

- **Spec 012 speaker-business recovery — historical evidence line** (2026-07-09).
  The latest runtime candidate fixes the known full-length regression windows by
  combining local drift epoch handling, per-entry comprehensive `speaker_id`, and
  align-run splitting near diarization boundaries. Continue from
  [drift-epoch-review-2026-07-08.md](012-evidence-fusion-timeline/drift-epoch-review-2026-07-08.md):
  residual work is limited to short-boundary artifacts, conservative `unknown`
  spans, and broader context-aware review, not a return to diar-only script
  percentages. The follow-up
  [refresh0-context-review-2026-07-08.md](012-evidence-fusion-timeline/refresh0-context-review-2026-07-08.md)
  rejects further cache-refresh tuning and naive context inheritance for this
  round; the remaining 3270-3304 s failure originates in sparse bottom diar
  evidence, not Web UI rendering or business-turn serialization. The
  follow-up support-diagnostics change exposes this weakness in live/final
  comprehensive entries but is not yet an accepted accuracy fix; acceptance
  still requires a full-length real WebSocket run and context-aware review under
  `speaker-business-method.md`.
- **Spec 010 speaker identity — cross-session identity finalized** (commits 38cdf51, 9c02862, 17f8d92, 06875c3, 5f301ba). The voiceprint stage now assigns a persistent GLOBAL id to every diar segment. Design corrections were validated through the REAL streaming path (rate=1) and judged by complete contextual semantic comparison vs `test.txt`. Under Constitution 1.7.0, no code or metric may assign accuracy, rank/select a candidate, or issue the verdict:
  - **Trust the diarizer's within-session separation**: each local slot resolves to its own global id; same-session slots can never collapse to one id (`SpeakerDatabase::MatchExcluding`). Per-segment re-matching was removed (it collapses similar voices to the dominant centroid).
  - **Cross-session strengthening**: each global's centroid is the mean of the best references of all slots mapped to it across sessions, so a returning speaker re-matches reliably (match cosine ~0.55 → 0.7–0.87).
  - **Registry-level de-duplication, uncapped**: `MergeReconcile` merges two globals only when their centroids are confidently the same person (cosine > 0.70; a stricter 0.85 for two globals that ever co-occurred in one session, since the diarizer judged them distinct), and `SpeakerDatabase::Remove` deletes the duplicate so the registry holds exactly one entry per real speaker. The registry is never capped — it is designed to recognise many speakers (≥200) across sessions.
  - **Test-method correction**: validate speaker accuracy through the real `rate=1` stream (a `rate=0` shortcut ages clean spans out of the embed-retain window before they are delivered, starving enrollment). Full 60-min run: 4 real speakers → exactly 4 stable global ids (spk_0=朱杰, spk_1=唐云峰, spk_2=徐子景, spk_3=石一) across all 6 reset sessions; clear/substantive turns attributed correctly (~90% on 0–600 s and 1800–2400 s), the 2400–3600 s region remains the hard part — confirmed by an independent fresh run of that segment to be the **audio's inherent rapid-speaker-exchange difficulty**, not continuous-run degradation. ctest 47/47, no warnings.
- **Spec 010 Phase H — conservative cross-session identity experiment** (2026-07-06). Implemented but **not accepted** as an accuracy improvement. All new thresholds are in `[speaker]` TOML; defaults preserve current behaviour. The opt-in conservative profile requires multiple clean references before reset-session re-identification and can keep unmatched later-session slots local-only. Full-length real WebSocket candidate completed with tegrastats (`/tmp/orator_phaseh_full.json`, 0.999x real time), but context-aware review [local-diar-review-2026-07-06.md](010-speaker-id/local-diar-review-2026-07-06.md) found that it only turned some wrong late global ids into local-only labels; it did not fix diarizer local-slot fragmentation/attribution in 600-1800 or 3000-3615. Next work must isolate local diar segmentation quality before global identity stitching.
- **Spec 010 local-diar operating profile restore** (2026-07-06, historical). The then-current runtime profile used async/no-reset with Sortformer tuning (`188/1/3`) in TOML. Full-length real WS validation (`/tmp/orator_full_async_default_20260706.json`) succeeded at 0.999x real time with 3611 tegrastats samples and stable 4 global ids. Context-aware review accepted that operating profile but not a complete diar quality fix: short-turn/tail fragmentation remained in 3000-3615 s, and ASR had a repeat burst around 1927-1944 s. Its v2 numerical gate and weight were removed when v2.1 became the sole closing baseline.

- **Historical full-pipeline stability validation — all features on** (2026-06-30). A single real `rate=1` 60-min WebSocket stream with **diarization + ASR + VAD + speaker identity + forced alignment all enabled**: 0.999× real-time, no crash, no OOM. Tracks: diar 729 (RTF ~100×), asr 119 (RTF ~1.25×), vad 972, **align 119 = 100% of ASR segments** (13594 char-level units, 0 out-of-bounds / 0 non-monotonic, RTF ~35×). Speaker identity converged to 4 global IDs. This proves stability and alignment coverage for that commit; it did not perform the Spec 013 556-turn accuracy review and is not a product closing result.

- **Codebase hardening** — complete. All P0/P1 items from 2026-06-21 evaluation executed:
  - GitHub Actions CI (CUDA 12.5, cmake + ctest + Python lint)
  - CUDA kernel unit tests (`test_kernels`: 13/13 passed, GPU vs CPU reference)
  - Level-based logging (`core/log.h`) replacing raw `fprintf(stderr)`
  - WebSocket server file-static elimination (`serve_server`/`serve_factory`/`pss_list_head` → instance members)
  - OnText JSON key exact matching (fixes `end`/`flush`/`reset` false positives)
  - GPU telemetry default disabled (1.0 → 0.0)
  - VAD model path migration (`models/asr/` → `models/vad/`)
  - README env var table + Python test CTest registration + protocol envelope unwrapping in web UI
- **Spec 004 — Protocol Layer**: Implemented. Phases 7–12 and Phase 13 session persistence are complete. `SessionStore` metadata, sessions/load RPCs, and the Web UI history/reload path are unit- and browser-verified; current `audio_sec` plus legacy `audio_duration` metadata are supported.
- **Full-length streaming verification**: 2026-06-21. 3615 s (1 hr) audio pushed through real WebSocket → 382.0 s wall = **9.46× real-time**. All three tracks (ASR/diarization/VAD) cover 100 % of the audio, no crash, no clock drift, no data loss. Achieved 9.25× on a consecutive warm-GPU re-run and 5.82× on a cold-start run, confirming model-load overhead is one-time.
- **Python integration-test correction** (2026-07-13). The obsolete `test/run_py_test.py` claim is superseded by registered CTest `test_ws_contract`. Its process-only runner starts `orator_ws` with isolated TOML storage and invokes the sole `tools/verify/py/ws_unified_test.py` client for canonical speech and generated silence.
- **TOML config system** (2026-06-22; typed boundary completed 2026-07-14).
  `ws_main.cc` applies defaults → TOML → environment → CLI and is the only
  runtime environment reader. ASR, align, Sortformer, GPU scheduling, logging,
  and optional WS frame logging receive typed values. Terminal captures include
  canonical `resolved_config`; the unified client emits a SHA-256 sidecar, and
  `repro_manifest.py` freezes source/data/model/device fixtures. See Spec 013
  `reproducibility.md`.
- **VAD-gated ASR fix** (2026-06-22). VAD async-lag protection via segment-start confirmation check. ASR segments reduced from 43→18 (120s test). RTF improved 4.7→3.7. Parameters tuned: `asr_vad_trail_sec=1.0`, `vad_min_silence_ms=300`. See `src/pipeline/asr_worker.cc:61-141`.
- **Full-length verification (v7)** (2026-06-23). 3615s (1 hr) audio at 420× injection: **964s wall (3.75×)**, no crash, no data loss. 300s verification confirms 3 ASR segments cover 300s of audio (merging 90 VAD segments). Speed regression from 9.46× (pre-v7) due to VAD segment-start check keeping ASR segments open longer, causing more audio to pass through GPU processing. 120s test at 1× real-time still at RTF 3.7.
- **Historical NeMo parity claim corrected** (originally 2026-06-24;
  audited 2026-07-15). The earlier `use_silence_profile` cosine penalty and
  `spkcache_refresh_rate` were local inventions, not behavior in the audited
  NeMo v2/v2.1 async updater, and are removed. The actual async defects were
  omission of FIFO embeddings from subsequent model inputs and loss of old
  FIFO frames before cache transfer. The corrected dual sync/async path now
  passes independently regenerated exact-profile fixtures at `1e-5` tolerance.
  Historical 600 s code metrics and 39-test evidence remain mechanical records
  of that build only; they do not evaluate current business accuracy, compare
  candidates, or support a verdict. See Spec 013
  [sortformer-oracle-2026-07-15.md](013-industrial-closing-validation/sortformer-oracle-2026-07-15.md).
