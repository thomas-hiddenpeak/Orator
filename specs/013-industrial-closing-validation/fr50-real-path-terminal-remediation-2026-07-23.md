# FR50 real-path gate and terminal remediation

This record covers the first T232 production-WebSocket ladder on clean pushed
commit `b449dfaa644c400fe1cc37a760e57a6c945c8c2c` and the subsequent T232A
terminal-path diagnosis. It does not promote FR50 or close the speaker
business.

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

## Next gate

T232A and T232B are complete. After commit/push of the exact remediation
revision, repeat the complete
120 A/120 B/600/full empty-registry A/restarted frozen-registry B ladder. Every
output-affecting gate requires the same complete forward and reverse
contextual-semantic review against `test.txt`. T232 may close only if both full
runs independently satisfy the unchanged 30-second direct-end limit and the
complete readings retain the speaker interpretation.
