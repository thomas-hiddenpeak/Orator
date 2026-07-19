# FR49 Real-Path Promotion Review (2026-07-20)

## Scope and authority

This record covers the staged production-WebSocket promotion of the bounded
FR49 source-leading primary-prefix policy. It covers two independent
120-second runs, one 600-second run, full empty-registry Run A, and full
restarted Run B using only Run A's frozen registry. Every capture used the
same clean pushed revision, binary, and checked-in `orator.toml`, with 100 ms
frames, requested `1.0x` streaming, direct `end`, early and late observers,
and required runtime/device telemetry.

`test/data/reference/test.txt` is the authoritative human-listened reference.
No code, script, query, formula, notebook, metric, hash, or equality check
assigned correctness, counted or aggregated a product result, ranked a
candidate, or issued a promotion verdict. Automation only executed and
captured streams, checked mechanical contracts, and arranged unjudged evidence
for reading. Every product decision below was manually transcribed after
complete contextual reading against `test.txt` in chronological order and
again in reverse fixed windows.

The complete review promotes FR49 from frozen replay evidence to the current
repeatable real-WebSocket speaker-business candidate. It does not close Spec
013, satisfy the remaining critical-attribution gates, or establish industrial
readiness.

## Frozen revision and inputs

| Item | Frozen value |
|---|---|
| Git commit | `1f0905263cdef7c943eb938e9fba13ea4a12c91c` |
| Worktree during every run | Clean |
| `build/orator_ws` SHA-256 | `1bf840de9b0f1e7f2263e6e0d471954b61d061638f559cff90410aa643d7c4d8` |
| `orator.toml` SHA-256 | `b5ecc7d84e8b711b48cad3a7b4a90f4820cf28e1d1c9f4e2ee3b301d00ebf210` |
| Resolved-config SHA-256 | `2c7b4a71818aabe54b7a4be15b0c0bc3e22ebaa8e0d0935eae4bad7fb6590eba` |
| Behavioral environment/CLI overrides | None |
| `test.mp3` container SHA-256 | `b7c25d1c349b02d654b6a406bc29039749e4240a4109dda4fcc905285b14b18b` |
| Full decoded PCM SHA-256 | `17f0edda49989f3ceada60170885091023eeb9d67faae0d6dd67bb585b8857fe` |
| Full decoded input | 57,841,920 samples at 16,000 Hz (`3615.120 s`) |
| `test.txt` SHA-256 | `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86` |
| Diarizer | Sortformer v2.1 from checked-in TOML |
| FR49 switch | `speaker_fusion.source_leading_primary_prefix_enable = true` |

The implementation revision had already passed a warning-clean build and all
`71/71` CTest entries in `53.22 s`. Those checks establish engineering
consistency only and do not contribute to the product ledger.

## Promotion ladder

Artifacts, manifests, server logs, and unjudged review packets are retained at
`/tmp/orator-spec013/release-1f09052-fr49-real/` on the validation host.

| Stage | Artifact ID | Artifact SHA-256 | Manifest SHA-256 | Direct-end wait |
|---|---|---|---|---|
| 120 A | `orator-20260719T191115Z-1f0905263cde-120.000s` | `06632745a85280b641b25fc869fa98ead57e137f2dbd79d8015610398b6f8d33` | `7a1a16c3f93d736ed4c692bea45cbc8e9a0d1e7a6ac63b5b73c24bcf0ac5a0b7` | `1.211 s` |
| 120 B | `orator-20260719T191353Z-1f0905263cde-120.000s` | `2bdd5a41b8f2c1ae1a7cdcf041b37ed4ca66420f6f27c10f8f9e4c4388e60a77` | `75d0a7b74e948afe4640b1f7449bb51a3b3eabce9b0d1252340ccc9e17116c31` | `1.214 s` |
| 600 | `orator-20260719T192821Z-1f0905263cde-600.000s` | `af81c4793ff15ee99310f27889f0139ca0f45804232acde8e8fd7f5f229ea437` | `8d4b0300d4c93b0980e099abbf76099bb18743b44ac26b9ff3de0d1b3c92b840` | `4.956 s` |
| Full A | `orator-20260719T203203Z-1f0905263cde-3615.120s` | `64abe31baf51185b685c91a58529096b25d281540afe21c3bbc2354cffb5432e` | `12fda94f6408a081bf3ff5f5158d0e11d0d3c9c2ab70294ab8d24aeeec9bd0e4` | `29.015 s` |
| Full B | `orator-20260719T213531Z-1f0905263cde-3615.120s` | `0ac66dbfc7dd95d21fcb271ad3b3a020d79c565b4c62ccc0a97f5e9a14f63813` | `6574a7e87ff53ac055ecbe60b5cd52a3e6a5145a9f1e9e8ddd62af2cf1ceef03` | `28.820 s` |

Both 120-second runs consume 1,920,000 samples and close all seven tracks on
the common clock. Their normalized product-track bundles are mechanically
identical at SHA-256
`e8613dfbdffbbb3394d5e80955eb73d30e7f200c4ffb4b3058df3a1b805928b8`.
Separate complete forward and reverse readings of `ref-0001` through
`ref-0018` retain the candidate and find no new contextual speaker regression;
known cold-start and rapid-turn defects remain. `ref-0061` and `ref-0121` are
outside this 120-second scope and are not used to pass this stage.

The 600-second run consumes 9,600,000 samples and closes all tracks. It emits
98 diarization, 166 primary-speaker, 52 ASR, 159 VAD, 52 alignment, 2,639
speaker-voiceprint, and 197 business entries. Complete forward and reverse
reading of `ref-0001` through `ref-0093` manually records `89/93`: the known
confident-wrong rows are `ref-0049`, `ref-0058`, and `ref-0066`, and
`ref-0063` remains missing. The review confirms the intended `ref-0061`
repair: 石一's leading `我` remains separate from 唐云峰's continuation. No new
contextual regression appears, so the reviewer authorizes full A/B. The
mechanical changed-scope display and track equality do not make that decision.

## Full-run mechanical evidence

Run A starts from isolated empty storage. Run B starts after a server restart
with only Run A's frozen speaker registry. The registry is unchanged after B
at SHA-256
`66461a77755984a08231d06306da7ce9e1eeac07be1927e91f8a772fc54c7b3f`.
The isolated storage trees are archived beside the artifacts.

Both full runs consume the exact 57,841,920 samples. Run A takes `3644.447 s`
wall time with `3615.200 s` push time; Run B takes `3644.258 s` with the same
push time. Each reports `0.992x`. Their `29.015 s` and `28.820 s` direct-end
waits pass the frozen 30-second limit but leave only `0.985 s` and `1.180 s`
of margin, so terminal latency remains a release risk to recheck rather than a
speaker-accuracy conclusion.

Each terminal document is complete JSON. All seven product extents close at
57,841,920 samples with zero declared gap and true common-time-base,
reconciliation, and wall-clock flags. Each run contains 755 diarization,
1,348 primary-speaker, 308 ASR, 972 VAD, 308 alignment, 17,452
speaker-voiceprint, and 1,718 business entries. Producer, early observer, and
late observer terminal documents agree within each run; neither run reports an
unexpected observer error.

Both full runs record 3,615 runtime telemetry and 3,634 `tegrastats` samples.
Required GPU utilization, GPU memory, system power, CPU, RAM, temperature, and
cadence fields have complete mechanical coverage. Run A's runtime GPU mean is
`51.956%`, memory mean/max is `68,657.99/70,033.4 MB`, and system-power
mean/max is `39.765/84.430 W`; Run B records `43.372%`,
`68,937.06/70,315.3 MB`, and `40.113/92.600 W`. These are operational
observations only.

The normalized full product-track bundle is mechanically repeatable across A
and B at SHA-256
`da183de16e888c1f12a8c186f1dbcce63ac582b8d8e0c74e06948815821ac5cb`.
This equality is not used to infer or accept speaker correctness.

## Complete contextual review

The full review uses windows `0-600`, `600-1200`, `1200-1800`, `1800-2400`,
`2400-3000`, `3000-3600`, and `3600-3615.12`. Run A is read completely from
start to end and again from the last window to the first; Run B receives the
same two independent complete readings. The packets contain reference context
and final business output but no correctness field:

| Packet | SHA-256 |
|---|---|
| Run A chronological | `de8295bcef5e8706ab0176f516a16fbccc9591aa85b4d2f7b0dfda3b93c9826c` |
| Run A reverse windows | `2a4039c882e005597ed37a19249bfe9bc48d6e683f34d5e54bbf6d3c927fa1c9` |
| Run B chronological | `a30eb0db988c94a052dd243bbc4235e53f89108a87c900c2ed2d7455ddc53a40` |
| Run B reverse windows | `9850621f95b782ae7aecddc0b8013ee6c8f1249479f67941cbb27b9053cc3e02` |

The contextual identity mapping is `spk_0 = 朱杰`, `spk_1 = 唐云峰`,
`spk_2 = 徐子景`, and `spk_3 = 石一`. Wrong ASR wording is not counted as a
speaker error when ownership remains clear; wrong ownership, absent material
speech, and genuinely indeterminate ownership retain separate manual classes.

All four complete readings produce the same manually signed ledger:

- natural-turn ledger: `523/556`, approximately `94.1%` as a presentation of
  the manually signed judgments;
- fixed-window ledgers: `89/93`, `79/84`, `76/80`, `75/80`, `119/129`,
  `82/87`, and `3/3`;
- per-speaker ledgers: 朱杰 `77/83`, 唐云峰 `176/189`, 徐子景 `70/73`, 石一
  `200/211`;
- residual classes: 27 confident-wrong, five missing, and one uncertain;
- retained FR49 repairs: `ref-0061` and `ref-0121`;
- missing: `ref-0063`, `ref-0341`, `ref-0409`, `ref-0417`, and `ref-0442`;
- uncertain: `ref-0506`.

The 20 business-critical residuals remain `ref-0049`, `ref-0058`,
`ref-0066`, `ref-0099`, `ref-0102`, `ref-0118`, `ref-0252`, `ref-0313`,
`ref-0327`, `ref-0331`, `ref-0333`, `ref-0354`, `ref-0390`, `ref-0426`,
`ref-0442`, `ref-0444`, `ref-0461`, `ref-0499`, `ref-0503`, and
`ref-0505`. The remaining noncritical confident-wrong rows are `ref-0135`,
`ref-0171`, `ref-0221`, `ref-0239`, `ref-0241`, `ref-0298`, `ref-0457`,
and `ref-0537`.

Complete context confirms both intended source-prefix repairs. At
`467.564-469.564`, 石一's leading `我` is restored to `spk_3` and 唐云峰's
continuation remains `spk_1`. At `817.692-820.572`, 石一's leading `你不低于`
is restored to `spk_3` and 唐云峰's continuation remains `spk_1`. Accepted
neighbors and the `ref-0304` abstention are preserved. No complete reading
finds a whole-session identity permutation, accumulating late drift, or a new
tail-only regression. Residuals remain distributed across the session and are
concentrated around rapid interjections, overlap, source boundaries, and
source-absent or contradictory evidence.

## Decision and remaining gates

FR49 is retained and promoted to the current repeatable production-WebSocket
speaker-business candidate at the manually signed `523/556` ledger. This is a
real-path result, not merely a frozen replay result. It clears the staged
120/600/full A/B contextual promotion ladder without a new attribution
regression.

Speaker-business closure remains open because 27 confident-wrong rows, five
missing rows, one uncertain row, and all 20 critical residuals remain. T102,
T084, speaker-time, per-speaker time, source-time-offset, locked-holdout,
final-report, release-signing, ASR, browser/microphone, and broader industrial
readiness gates remain separate and open.

The pre-existing business storage has been restored at `/tmp/orator/storage`.
No validation server, client, or `tegrastats` process remains active.
