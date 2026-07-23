# FR50 real-path gate and terminal remediation

This record covers the first T232 production-WebSocket ladder on clean pushed
commit `b449dfaa644c400fe1cc37a760e57a6c945c8c2c`, the subsequent T232A/T232B
terminal-path remediation, and the exact-clean T232C acceptance ladder on
pushed commit `a6f0d33730326b19a3831019b1aba21fd900f126`. T232C promotes FR50 as
the current real-path speaker baseline. It does not close the speaker business.

`test/data/reference/test.txt` is the authoritative human-listened reference.
No compiled code, script, query, formula, notebook, metric, hash, equality
check, or algorithm assigned correctness, counted or aggregated a product
result, ranked or selected a candidate, or issued the semantic decision below.
Automation only captured the real path, checked mechanical contracts, hashed
artifacts, and arranged raw evidence for complete contextual reading.

## Frozen real-path source

| Item | Frozen value |
|---|---|
| Commit | `b449dfaa644c400fe1cc37a760e57a6c945c8c2c` |
| Branch | `master`, clean before and after every accepted capture |
| `orator.toml` SHA-256 | `d00150ae376d802af0fcf8c0a89aa3fae1e0abb2bf5d10601c55cefd570a40db` |
| Server binary SHA-256 | `408eca839bd8e9281b4d780c318ec517c330a2535d97fda02a1ef7d825b3d254` |
| Full input | 57,841,920 samples at 16,000 Hz (`3615.120 s`) |
| Transport | production WebSocket, 100 ms PCM frames, `1.0x`, direct `end` |
| Run A registry | isolated and empty |
| Run B registry | server restart with only Run A's frozen registry |
| Artifacts | `/tmp/orator-spec013/release-b449dfa-fr50-real/` |

## Mechanical ladder result

| Run | Stream factor | Direct-end wait | Mechanical result |
|---|---:|---:|---|
| 120 A | `0.990x` | `1.211 s` | pass |
| 120 B | `0.990x` | `1.212 s` | pass |
| 600 | `0.992x` | `4.997 s` | pass |
| Full A | `0.992x` | `30.144 s` | fail: exceeds fixed `30.0 s` limit |
| Full B | `0.992x` | `29.258 s` | pass, but without adequate margin |

Every run received a complete terminal document and required runtime/device
telemetry. The full artifacts each report 3,615 runtime telemetry samples and
at least 3,634 device samples. The source, config, Git worktree, and server
binary remained unchanged during each run. These are mechanical observations
only.

Full A's failed terminal bound stops promotion. Full B cannot waive or average
that failure. The first ladder therefore remains non-promotable even though its
product output was fully reviewed.

## Contextual-semantic review

The speaker mapping is `spk_0 = Zhu Jie`, `spk_1 = Tang Yunfeng`,
`spk_2 = Xu Zijing`, and `spk_3 = Shi Yi`. The 120-second A and B artifacts
were each read against all 18 in-scope `test.txt` contributions chronologically
and again in reverse fixed windows. The 600-second artifact was read against
all 93 in-scope contributions in both directions before the full gate ran.

Each full artifact was then read against all 556 reference contributions in
windows `0-600`, `600-1200`, `1200-1800`, `1800-2400`, `2400-3000`,
`3000-3600`, and `3600-3615.12`, first chronologically and then independently
from the final window back to the first.

| Complete full reading | Display packet SHA-256 | Manual result |
|---|---|---|
| Run A chronological | `04a034fcf8208f5c9eddbb349ceb4c3f180837912bd16946f867734d12fec9ba` | retain FR50 interpretation |
| Run A reverse windows | `5b92ef64ed2a2c386defe95f515633d2b869e6e9602dc871ac739b48a3a551d7` | retain FR50 interpretation |
| Run B chronological | `5454945b271f2427872dde7dad2ffcbd287d3b8ad054dbed02a3874a89848487` | retain FR50 interpretation |
| Run B reverse windows | `f76107e90f42d67c6496c89fc5a80fd51508f293df78b0fbb6bd0ac13f86d869` | retain FR50 interpretation |

All four complete readings retain the two already authorized FR50 repairs,
find no new attribution regression, and manually reconcile the same
`525/556` speaker ledger: 26 confident-wrong, four missing, one uncertain, and
19 business-critical residuals. This transcription comes only from complete
conversational reading. It is not a script result and does not override the
failed terminal gate.

## Root cause

FR49 added source-independent `primary_run` acoustic evidence. The final
primary top-1 track is intentionally built only after producers and forced
alignment drain, so all 1,348 full-session primary runs first became visible to
the unlimited final acoustic-precompute pass. This moved work that is
independent of final global identity onto the direct-end critical path.

The remediation observes each already deposited immutable
`DiarFrameBlock` and coalesces active top-1 frames into common-clock,
reset-aware local-slot runs. Completed spans enter the existing low-priority
precompute worker. This fills only the acoustic embedding cache; it neither
assigns an identity nor emits a speaker vote. Final primary construction,
global-gallery maturity, evidence scoring, and business projection remain in
their existing order.

The first dirty-tree 600-second diagnostic queued primary spans but retained
the old global-ID population gate for extraction. It completed at `0.992x`
with `4.749 s` direct-end wait. Final diagnostics recorded 760 successful
preparations, 338 live and 422 during a `4412.655 ms` final drain. This showed
that queue visibility alone was insufficient.

`SpeakerIdentityStage::PrecomputeSpan` is acoustic-only and does not read a
gallery. The extraction readiness gate was therefore corrected to the existing
minimum population of distinct typed diarization-local tracks. The unchanged
final `BuildVoiceprint` path still requires mature global IDs before emitting
evidence.

The second dirty-tree 600-second diagnostic retained the checked-in behavioral
TOML values (`0.5 s` cadence and one successful span per live cycle). It
completed at `0.997x` with `1.769 s` direct-end wait. Final diagnostics record
783 successful preparations, 651 live and 132 during a `1429.699 ms` final
drain. The canonical product-track entries and comprehensive entries of the
two diagnostics are byte-identical at SHA-256
`f25c9e1efdbdf4ab3e5c510736fd3e2ab5bc5d0740d3d4c784586fb9dfb7210a`.
That equality establishes only that cache scheduling did not alter the
captured product records; it does not evaluate their correctness.

Both diagnostics used isolated storage and a deliberately dirty worktree, so
neither is acceptance evidence. No numeric TOML value, client timeout, model,
producer value, time coordinate, or final speaker rule changed.

The final clean build emits no `warning:` or `error:` diagnostic. All `72/72`
CTest entries pass in `53.23 s`, including the focused frame-block run
formation, disabled-path, per-cycle bound, final drain, and cache-reuse
contracts. These checks establish implementation consistency only.

## Exact-clean remediation acceptance

The remediation is committed and pushed directly on `master` at
`a6f0d33730326b19a3831019b1aba21fd900f126`. The worktree, checked-in TOML,
server binary, input audio, frame pacing, and behavioral environment remain
unchanged within every accepted run. The server binary SHA-256 is
`0c3f71d6077edc6805c394f82908c3c91a8d746aa853bd004582a9ff66822692`;
the TOML SHA-256 remains
`d00150ae376d802af0fcf8c0a89aa3fae1e0abb2bf5d10601c55cefd570a40db`.
Artifacts are retained under
`/tmp/orator-spec013/release-a6f0d33-fr50-precompute/`.

| Run | Stream factor | Direct-end wait | Mechanical result |
|---|---:|---:|---|
| 120 A, independent empty registry | `0.980x` | `2.422 s` | pass |
| 120 B, independent empty registry | `0.980x` | `2.426 s` | pass |
| 600, independent empty registry | `0.997x` | `1.752 s` | pass |
| Full A, empty registry | `0.993x` | `26.013 s` | pass |
| Full B, restarted with Run A registry | `0.993x` | `26.789 s` | pass |

All runs use production WebSocket transport, 100 ms PCM frames, `1.0x`
pacing, direct `end`, required observers, and runtime/device telemetry. Both
full runs independently satisfy the unchanged `30.0 s` terminal limit. Run A
and post-Run-B registry SHA-256 values are both
`66461a77755984a08231d06306da7ce9e1eeac07be1927e91f8a772fc54c7b3f`;
this establishes storage integrity only.

The two 120-second artifacts each received complete forward and reverse
contextual reading of 18 contributions. The 600-second artifact received both
readings for all 93 in-scope contributions. Full A and full B were each read
against all 556 `test.txt` contributions chronologically and independently in
reverse fixed-window order.

| Complete full reading | Display packet SHA-256 | Manual result |
|---|---|---|
| Remediation Run A chronological | `730506a70dffeeef673051fdc9455fedb7e1e20bddd1f47e23a4beed63e13d69` | retain FR50 interpretation |
| Remediation Run A reverse windows | `2e716af703b0045fadb43cd3804389be65f117add6fc961d858f5b74fc95adf6` | retain FR50 interpretation |
| Remediation Run B chronological | `d9a580288aa745aa894e997d1e08911a29ab23b5cb893841b147c725044e17d7` | retain FR50 interpretation |
| Remediation Run B reverse windows | `bb6fb4fff19e64d630a3bee5245d634bd9bb9c0e999072c0467b7b68a3032a4a` | retain FR50 interpretation |

The four complete full readings independently preserve
`spk_0 = Zhu Jie`, `spk_1 = Tang Yunfeng`, `spk_2 = Xu Zijing`, and
`spk_3 = Shi Yi`. Each full run manually retains 525 of 556 reference
contributions, with 26 confidently wrong, four missing, one uncertain, and 19
business-critical residuals. The retained share is approximately 94.4% and
remains above the 90% bottom line. `ref-0327` and `ref-0417` stay repaired.
There is no whole-session identity permutation, accumulating drift, or
tail-only collapse; the remaining errors are local handoff, overlap, and
evidence-boundary failures, including the longer local error at `ref-0503`.

The missing contributions are `ref-0063`, `ref-0341`, `ref-0409`, and
`ref-0442`; `ref-0506` remains uncertain. The 19 critical residuals are
`ref-0049`, `ref-0058`, `ref-0066`, `ref-0099`, `ref-0102`, `ref-0118`,
`ref-0252`, `ref-0313`, `ref-0331`, `ref-0333`, `ref-0354`, `ref-0390`,
`ref-0426`, `ref-0442`, `ref-0444`, `ref-0461`, `ref-0499`, `ref-0503`, and
`ref-0505`.

T232A, T232B, T232C, and T232 are complete. FR50 is the current repeatable
real-path speaker baseline. Canonical speaker-business closure remains open
because the 19 critical residuals, speaker-time evidence, locked holdout,
browser/microphone, report review, and release gates are not closed. The
pre-test business storage is restored and no validation process remains.
