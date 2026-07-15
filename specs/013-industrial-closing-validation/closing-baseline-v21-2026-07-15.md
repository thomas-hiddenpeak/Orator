# v2.1 Closing Baseline Capture - 2026-07-15

## Scope

This record closes T044's reproducible full-capture requirement. It establishes
transport, common-clock, telemetry, persistence, and Web UI evidence for the
sole v2.1 closing baseline. It does not assign speaker correctness and does not
close T031 or T045. The 556-row audible reference ledger remains unsigned, so
no constitutional natural-turn or speaker-time accuracy follows from this run.

## Frozen Source

| Item | Value |
|---|---|
| Source commit | `3b402453c7886e0e884f2eeb168f0f3405aa03fe` |
| Worktree before/after | clean / clean; workspace SHA-256 unchanged |
| Fixture content SHA-256 | `dce7cd2cafa4b76111cdf751be635af022276bbec14852370c86d8cdf32328c5` |
| Fixture file SHA-256 | `3916045c74482ddb328a57cc3c1fb51d8ddb57c6c2ea9dba6fc2e57bf97173d1` |
| `orator.toml` SHA-256 | `4b8f68263de60fff90ca6a4df508d909cb2cf016e9efe2f3e015de352fb793d6` |
| Server binary SHA-256 | `660b31e6ccc6db39ca10c7ce8d2f3927a2ae597e7fc17dcdcf18a387625a0569` |
| v2.1 model SHA-256 | `d036020b6b93977098929d417b1b106a952ec02cc38cafc9d3315ae0ec4d90b8` |
| `test.mp3` SHA-256 | `b7c25d1c349b02d654b6a406bc29039749e4240a4109dda4fcc905285b14b18b` |
| `test.txt` SHA-256 | `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86` |
| Initial registry SHA-256 | `d641642227dcad9d063f4b6d649bea0d8712ac6e81d1824420efc8f4b34a8f44` |
| Final registry SHA-256 | `f21893831a7c07ce90b26b3d94ee81c54606e0a1bab18b994024a5d2f153efe0` |

The resolved diarizer is
`models/sortformer_4spk_v2.1.safetensors` with the checked-in inherited profile:
chunk `340`, left/right context `1/1`, speaker cache `188`, FIFO `188`, and
cache-update period `188`. The client recorded no `ORATOR_*` behavior override.

## Full WebSocket Result

The producer sent `test.mp3` as mono 16 kHz PCM at 1x through the real
WebSocket path while a Chromium observer was connected.

| Measurement | Result |
|---|---:|
| Audio | 3615.12 s / 57,841,920 samples |
| Push wall time | 3615.20 s |
| Total wall time | 3616.442 s |
| Stream rate | 1.000x |
| Diarization | 755 entries; compute RTF 77.024x |
| ASR | 287 entries; compute RTF 1.899x |
| VAD | 972 entries |
| Forced alignment | 287 entries; 287/287 ASR coverage |
| Business speaker | 935 entries |
| Fusion-audit candidate unknown | 88.840 s / 2.46% |
| Mechanical contract issues | 0 |

`ws_input`, diarization, speaker identity, ASR, VAD, forced alignment, and
business speaker each finalized at exactly 57,841,920 samples with zero gap.
`timebase_ok`, `timebase_reconciled`, and `wall_clock_ok` are true. The offline
mechanical `fusion_audit.py` pass found no issue and did not score correctness.

## Telemetry

The run contains 3,441 runtime telemetry samples and 3,606 independent
`tegrastats` samples. Required GPU utilization, GPU memory, system power, CPU,
RAM, temperature, and cadence coverage all pass the 95 percent contract:

| Measurement | Result |
|---|---:|
| Runtime cadence | 95.187% |
| `tegrastats` cadence | 99.751% |
| GPU utilization mean / P95 / max | 52.07% / 98% / 98% |
| GPU memory mean / P95 / max | 97,942.9 / 98,416.9 / 98,664.9 MB |
| Runtime system power mean / P95 / max | 43.08 / 66.07 / 82.24 W |
| `tegrastats` system power mean / max | 43.83 / 82.40 W |
| Maximum temperature | 65.531 C |

This Thor host's `tegrastats` output does not expose a `GR3D_FREQ`/`GPU_FREQ`
utilization field. The runtime utilization source selected by the documented
fallback order was `nvidia-smi`; continuous `tegrastats` still independently
captured CPU, RAM, temperature, GPU rail power, and system power.

## Browser Evidence

The browser connected before the producer. At 120 seconds it showed ten live
rows, ten ASR entries, 21 diarization entries, 98 percent GPU utilization,
97,495 / 125,809 MB video memory, and 32.2 W system power. It remained connected
for the full producer run. The first automation script used an invalid terminal
completion predicate and therefore did not capture its in-memory final frame.
That harness error is retained as a limitation rather than reported as a pass.

The server-persisted terminal session was then loaded through the real
`load_session` WebSocket RPC into a fresh Chromium process. Parsed-object
comparison proved:

- producer terminal equals the persisted session;
- producer terminal equals the rendered Web UI JSON;
- producer terminal equals the browser-downloaded JSON;
- the UI reports 3615.1 s, 287 ASR entries, and 755 diarization entries;
- 1440-pixel desktop and 390-pixel mobile views have no horizontal overflow;
- console errors, page errors, and failed HTTP responses are all empty.

The registered 12-second observer gate separately proves live and terminal
equality on the same connection. Together these artifacts satisfy T044's
full-capture browser evidence. The broader physical-microphone and reconnect
acceptance in T073 remains open.

## Controlled 120-Second Repeatability

After the full capture was recorded, clean commit `028f2fe` ran the same first
120 seconds twice at 1x through one current server binary and the same checked-in
TOML. Both runs completed in 120.80 seconds with source-stable manifests, no
contract issue, and complete telemetry. The terminal entry arrays were exactly
equal in every track: 23 diarization, ten ASR, 39 VAD, ten forced-alignment, and
28 business-speaker entries. The `comprehensive` arrays were also exactly equal.

Expected runtime metadata was not equal: each session has a different wall-clock
anchor, and cold/warm execution changes `compute_sec` and reported RTF. This
check establishes deterministic terminal business content for the controlled
120-second prefix on the current binary. It is not the two-full-run acceptance
requirement in T080-T084.

## Evidence Paths and Hashes

Artifacts are retained outside Git under
`/tmp/orator-spec013/closing-v21-3b40245/full-02/`:

| Artifact | SHA-256 |
|---|---|
| Full producer package | `66e1b23d8f0f35e4edc70cf9bd4b41a256062c6cf781fd3f0a5a482b26591665` |
| Per-run manifest | `bccdeecc3ae749d341bfb775347558a85d509dc382cfe98a59acf300e1dd3c4f` |
| Fusion audit | `b2e8a02acd6db65f1c5d466359db9db6e19bfe8dbc7e9b689a6049e6738a99` |
| Browser result | `752e6744d9a8333687dee971d20e000ec639715895fe8491b7b4fe4eb3ee9f24` |
| Browser download | `a83ef6c7f20671a2f5e1237e9216dfc573130737638131cc9c175825ec5c02e4` |
| Desktop final screenshot | `2282ddaf0590e2d783c57324c4452b6449f32bfb45eba4e2ed5b62b1aa03123d` |
| Mobile final screenshot | `261ddc5656df756720050609f731a27d57294e56080b02197834369bc4077304` |

The source log has no error, fatal, failed, exception, timeout, OOM, or
out-of-memory record. All run processes were stopped after artifact capture.

## Decision

T044 is complete as reproducible system evidence. Accuracy promotion is still
blocked: all 556 reference rows require audible boundary, overlap, criticality,
ambiguity, chronological-pass, reverse-block-pass, reconciliation, and reviewer
signatures. Until that work closes T031-T035 and T045, the previously reported
74.2806 percent written-context diagnostic remains historical only, and this
run has no constitutional accuracy percentage.
