# Sortformer Runtime Oracle and Transitional Full Run - 2026-07-15

## Status

The exact runtime Sortformer model/profile now has a pinned NVIDIA source,
independently regenerated NeMo output, and a multi-update C++ numerical gate.
This closes the implementation-parity defect; it does not close speaker
business accuracy or Spec 013 product acceptance.

Numerical-oracle and mechanical values in this report are component evidence
only. No code, script, test, notebook, formula, query, metric, or algorithm may
use them to evaluate semantic/speaker accuracy, rank candidates, select a
configuration, or issue a product verdict. Those decisions require complete
contextual semantic review and manual result verification.

## Pinned model and profile

- NVIDIA source revision:
  `a16aa88603f758b4e4788177c6345ba3594edef6`
- Source `.nemo` SHA-256:
  `48bf1aeef978a35478181d578c50a97b87f9aebcd3d81d7263f414196c19ee5a`
- Runtime safetensors SHA-256:
  `754e4468514139e5d8d1c8e07f802338ac05ababa69a1722b35dc7a2cfbc2edb`
- The 973 tensor descriptors and tensor payload bytes are identical between
  the source archive and runtime file; only conversion metadata differs.
- Checkpoint profile: cache/chunk/update `188/188/188`, FIFO `0`, context
  `1+1`, silence placeholders `3`.
- Runtime TOML profile: cache/chunk/update `188/340/188`, FIFO `188`, context
  `1+1`, silence placeholders `3`. This is an Orator async candidate, not a
  claimed checkpoint default.

The NeMo oracle manifest is
`/tmp/orator-spec013/models/nvidia/diar_streaming_sortformer_4spk-v2-a16aa886/oracle-manifest.json`
(SHA-256 `2939d8e8abd020c6540b90bd5f40fb77138f7edb7a75e33c113a2f90eaed2dbb`).
It records source, config, runtime weights, processed input, output, tool
versions, and invocation hashes.

## Corrected implementation

The previous C++ async path did not match NeMo: it omitted the FIFO from the
next model input and could discard the oldest FIFO frames before transferring
them into the speaker cache. The corrected path uses
`[speaker cache, FIFO, current chunk]`, refreshes FIFO predictions from each
forward pass, admits only valid center frames, drains overflow into the cache,
and applies NeMo cache compression with the configured silence-placeholder
count.

`use_silence_profile` and `spkcache_refresh_rate` were local controls with no
counterpart in the audited NeMo async updater. They are removed from typed
runtime configuration. A TOML that still contains either key is rejected
instead of silently ignoring it. The complete streaming profile must be
applied before `Initialize()` allocates state; late tuning is rejected.

## Numerical evidence

The processed-signal fixture repeats the valid source three times and crosses
five 340-frame runtime chunks, including repeated FIFO/cache updates:

- processed signal SHA-256: `8d595d81d481e895eaffe897babd3d23e8b6ca0e41570336a9e154f7b6fa6637`
- metadata SHA-256: `be73e8b7e1caf10a3b84eaec344ccd974b5f1005320a3f8eff84378c5b0b9d36`
- regenerated raw oracle SHA-256:
  `0fb3b6d0451d68660b757c131b67cf83ff26d2c59e0690708183d5b90e793c48`
- C++ versus NeMo: 1502 frames, `max_abs=1.43051e-6`,
  `mean_abs=9.48068e-8`, argmax `1502/1502`, tolerance `1e-5`.
- Separate v2.1 synchronous migration guard: 502 frames,
  `max_abs=1.56462e-6`, `mean_abs=2.02086e-7`, argmax `502/502`, tolerance
  `1e-5`.

The configured suite passed 61/61 after regeneration.

## Transitional full-session diagnostic

`/tmp/orator-spec013/runtime-async-fixed/orator-async-fixed-full-v164.json`
(SHA-256 `ca393fb2bcf6708ea62c3c9d6c611e3de9acbf42fa67e83b92ffcb0253b8121b`)
streamed 3615.12 seconds at 1x in 3616.56 seconds. All seven extents ended at
57,841,920 samples with zero gaps; time-base reconciliation and wall-clock
checks passed. It produced 735 diarization, 288 ASR, 972 VAD, 288 alignment,
and 915 business-speaker entries.

This package is diagnostic only. Its server loaded an earlier TOML/runtime
schema before source edits completed, and the old client did not take pre-run
source snapshots. The resolved document still contains the removed
`use_silence_profile=false` field. The binary SHA-256 was `093161eb...`, while
the verified rebuilt binary is `85a74a19...`. It cannot be promoted to an
acceptance run.

The fixed FIFO changed assignments in 464/556 reference intervals relative to
the frozen runtime baseline, so it is a full-session sequence correction, not
a tail-only adjustment. The already completed frozen text-context review of
the corresponding clean v2 fixed candidate remained `378/556` correct,
`177/556` incorrect, and `1/556` ambiguous. That is a provisional written-
context diagnostic, not the signed audible result. FIFO parity therefore fixes
the implementation defect but does not recover business accuracy.

## Rebuilt runtime and UI evidence

The source-stable rebuilt 120-second real-WebSocket run is
`/tmp/orator-spec013/runtime-async-fixed/orator-rebuilt-120s-v165.json`
(SHA-256 `8c0b7712f9ed077a6e0103904d3b6a9f396f06b2a5d71b5f5ceb6ef9c5ca0dac`).
It completed in 120.83 seconds at 0.993x, passed every mechanical contract,
and retained zero gaps on all seven extents. Config, Git workspace-content,
and server-binary pre/post hashes were unchanged.

Telemetry contained 115 runtime and 120 `tegrastats` samples. Required field
coverage was 100 percent; cadence coverage was 95.83 and 100 percent. GPU
utilization came from `nvidia-smi` (mean 54.95 percent, P95 98 percent), CUDA
unified-memory use averaged 91,166.7 MB, and system `VIN` power averaged
41.25 W with P95 62.59 W. The old full run's 2300/3615 runtime samples fail the
new cadence gate at 63.62 percent, demonstrating why cadence is checked
separately from field presence.

The real Chromium 12-second flow passed file streaming, terminal convergence,
exact download, persisted reload, reconnect, and fake-device microphone
start/stop. Desktop and mobile screenshots are retained beside the runtime
artifacts. Physical-microphone and full 556-row audible acceptance remain open.
