# FR30 600-Second Context Review (2026-07-19)

## Scope and authority

This report records the T132 gate for the single-variable FR30 candidate. The
real-WebSocket artifact was produced from clean commit
`30162d1c844dd53506551cfc9ca1a5fe7b3fa67a` after T131 had been retained.
`test/data/reference/test.txt` remains the human-listened authority.

No code, script, query, formula, metric, interval count, transcript similarity,
or model score assigned speaker correctness, aggregated accuracy, ranked this
candidate, or issued the decision below. Automation captured and hashed the
artifact, checked mechanical contracts, and arranged source and candidate
evidence for reading. The product judgment came from complete contextual
reading of all 93 contributions in both directions.

Passing T132 authorizes a new full empty-registry/frozen-registry A/B gate. It
does not accept FR30, replace T111, or establish a full-session result.

## Production artifact

The run used `test.mp3`, 100 ms incremental frames at 1.0x pacing, the
production `orator_ws` path, Sortformer v2.1, direct `end`, an isolated empty
speaker registry and protocol store, early/transient/late observers, runtime
GPU telemetry, and `tegrastats`. The temporary TOML differs from checked-in
`orator.toml` only in its registry and storage paths; every behavioral value,
including `vad.threshold = 0.3`, is unchanged.

| Evidence | Value |
|---|---|
| Artifact ID | `orator-20260718T190015Z-30162d1c844d-600.000s` |
| Artifact SHA-256 | `70dad94d9c27365cd767f9ccd1cbf5cd43f7411150e218c3e7d5c2ac785b7947` |
| Manifest SHA-256 | `1893357eb4a8924c64dfb2bf22635bb29c68fc80fbf8a27dfb7f66f6018ca4af` |
| Source `test.mp3` SHA-256 | `b7c25d1c349b02d654b6a406bc29039749e4240a4109dda4fcc905285b14b18b` |
| Isolated TOML SHA-256 | `85edbcb8e2e28e5f3c189187111e03d22ddc05ee93c7b5f9bc3de4d74a3159f4` |
| Resolved config SHA-256 | `0863f111d74a3ec78f394440f99322ab3a1852ad7053b2e14db75afe620aaf88` |
| Server binary SHA-256 | `4e89d98710f49dd2d09ec8cb526949271b18fd449ef727ee3da6a8520505c1ca` |
| Audio / total wall time | `600.000 s / 603.342 s` |
| Direct-end wait | `3.342 s` |

All seven common-clock extents end at exactly `9,600,000` samples with zero
gap. Time-base, reconciliation, and wall-clock flags are true, and the client
reports no mechanical contract issue. The final tracks contain 98
diarization, 166 primary-speaker, 51 ASR, 150 VAD, 51 forced-alignment, 2,458
voiceprint, 188 business-speaker, and 188 comprehensive records. These counts
describe structure only.

The producer, early observer, and late observer terminal hashes all equal
`374360c5d31f1ac3d052021cfa6bef897d0b1c01a7b1e97e6e3e5995e986f0b0`.
Runtime telemetry contains 599 samples and `tegrastats` contains 601 samples.
Required GPU utilization, GPU memory, system power, CPU, RAM, and temperature
fields each have complete sample coverage; cadence is `599/600` and `601/600`
respectively. These are operational contracts, not product judgments.

## Complete contextual review

The full candidate was read against every in-scope `test.txt` contribution,
`ref-0001` through `ref-0093`, in ten chronological 60-second windows. It was
then read again from the `540-600 s` window back through `0-60 s`. The reviewer
used the whole question, answer, interruption, continuation, and neighboring
speaker context rather than assigning correctness from interval overlap.

The complete conversation establishes the candidate identity mapping as
`spk_0 = 朱杰`, `spk_1 = 唐云峰`, `spk_2 = 徐子景`, and `spk_3 = 石一`.
The historically blocking contexts remain substantively correct:

- `ref-0037`: 唐云峰 owns `不能再等了`; an earlier short 石一 interjection
  does not take over the sentence.
- `ref-0073`: 石一 owns the substantive response `否决了，对，四十五。`;
  唐云峰 starts the following question in `ref-0074`.

Known short-interjection and cross-boundary defects from the accepted T128
600-second evidence remain visible, including the `ref-0069`/`ref-0071`
continuation around `28+15` and `44/45`. They are not relabeled as correct, but
they are not introduced by FR30.

## Complete changed-context review

An assignment-only display against the already retained T128 600-second
artifact exposes ten changed reference contexts. Every one was read in its
full surrounding exchange in both directions:

- `ref-0001` now keeps the complete opening contribution with 朱杰 instead of
  assigning its first phrase to 唐云峰.
- `ref-0008` now keeps the substantive `对，跟成都没关系` reply with 唐云峰.
- `ref-0009`, `ref-0011`, and `ref-0017` recover additional 朱杰 context at
  short handoffs without changing the following speaker's natural turn.
- `ref-0021`, `ref-0022`, and `ref-0023` change only the partition around
  duplicate whole-second source timestamps; 唐云峰 and 石一 retain their
  substantive alternating calculations.
- `ref-0036` adds a short 石一 interjection between 唐云峰's proposal and his
  later `不能再等了`; it does not repaint either 唐云峰 sentence.
- `ref-0063` restores 石一's short `对` response.

No changed context introduces a new natural-turn speaker regression. This is a
manual semantic conclusion; the comparison utility displayed speaker-label
sequences but did not evaluate them.

Review packet SHA-256 values are:

- chronological: `51d2db845f87e7677e2d6693557a834b7b0039cc033de21a3260c62f9e0867a4`;
- reverse windows: `737f8dfb75ca0587a9b1eeb41de67a36ab8846abd00a28981fe9f88fd824d44b`;
- T128 changed-context display:
  `8cd7b45573cefa4a8f3b49f61bb9633136dbada2ec183acd0a730d90b3e28b9e`.

## Decision boundary

T132 is retained. FR30 may proceed to one clean full-length empty-registry run
and one independently restarted full-length run using the first run's frozen
registry. Both artifacts must pass direct-end, provenance, observer, telemetry,
common-clock, and repeatability contracts. Each artifact then requires its own
complete 556-contribution chronological review and independent reverse
600-second-block review before FR30 can be promoted or rejected. T111 remains
the accepted full-session baseline until all of those manual gates pass.
