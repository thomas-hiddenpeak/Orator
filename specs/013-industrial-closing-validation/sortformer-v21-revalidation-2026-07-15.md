# Streaming Sortformer v2.1 Revalidation - 2026-07-15

## Status

Official-profile numerical gates and full-session natural-turn reviews are
complete. The v2.1 checkpoint under Orator's inherited `340/1/188/188` profile
passed numerical, transport, time-base, and telemetry gates, but failed the
contextual speaker gate at 413/556 correct turns. NVIDIA's published high- and
low-latency v2.1 profiles passed exact numerical gates but scored only 385/556
and 377/556 in full contextual screening. Neither official profile qualifies
for a real-WebSocket acceptance run. The inherited profile is now the sole
v2.1 closing baseline, not an accepted product configuration.

## Recovered Context

The repository contains two converted streaming checkpoints:

- removed historical checkpoint: `sortformer_4spk_v2.safetensors` (reports and
  hashes retained; local weight deleted);
- sole closing baseline: `sortformer_4spk_v2.1.safetensors`.

Before this revalidation, the checked-in runtime had continued to select v2.
The v2.1 checkpoint was
covered only by a synchronous numerical guard and frozen-evidence diagnostics,
so it never received the same real-WebSocket full-session acceptance treatment.
Offline-only diarization models are excluded from this selection path. The
manually adjudicated `test.txt` is the business reference; NeMo is used only to
verify numerical parity of the same v2.1 streaming checkpoint.

The initial revalidation intentionally retained the v2 runtime profile to
isolate checkpoint behavior. The current NVIDIA model card instead recommends
`340/40/40/300` for 30.4-second high latency and `6/7/188/144` for 1.04-second
low latency (chunk/right-context/FIFO/update-period). Those profiles are now the
required next gates; the results below describe only the inherited Orator
profile.

## Frozen Historical Evidence

The retained first-pass written-context ledgers cover all 556 reference rows:

| Profile | Correct | Incorrect | Ambiguous |
|---|---:|---:|---:|
| v2.1 async `188/340/188`, FIFO `188` | 377 | 178 | 1 |
| v2.1 checkpoint-native sync `188/188/188`, FIFO `0` | 357 | 198 | 1 |

These judgments establish that async is the stronger v2.1 profile. They were
produced from frozen evidence rather than a source-stable real-WebSocket closing
run, so they are not constitutional accuracy claims.

## Exact Async Oracle

- Source `.nemo` SHA-256:
  `8abd32832159c6ac1148c926b7276f35ba34582c444e559dce1f1253fea42ef8`
- Runtime safetensors SHA-256:
  `d036020b6b93977098929d417b1b106a952ec02cc38cafc9d3315ae0ec4d90b8`
- Checkpoint profile: cache/chunk/update `188/188/188`, FIFO `0`, context
  `1+1`, silence placeholders `3`.
- Candidate profile: cache/chunk/update `188/340/188`, FIFO `188`, context
  `1+1`, silence placeholders `3`.
- Regenerated output: 1502 frames across five chunks.
- Raw fixture SHA-256:
  `2635b09033153aba85a413e514899238f629665bc5d08e3ec6b5b72ce9a4699e`.
- The regenerated raw fixture is byte-identical to
  `models/reference/ref_stream_async_v21_long.f32`.
- C++ versus NeMo async: 1502 frames, `max_abs=1.60933e-6`,
  `mean_abs=1.15723e-7`, raw argmax agreement 1501/1502. The numerical test
  separately rejects any argmax disagreement whose NeMo margin exceeds twice
  the `1e-5` probability tolerance.
- C++ versus NeMo checkpoint-native sync: 502 frames,
  `max_abs=1.56462e-6`, `mean_abs=2.02086e-7`, argmax 502/502.

The one async argmax disagreement is frame 258. NeMo's winning margin is only
`1.13249e-6`; the C++ top pair is `0.349344015/0.349343836` and the NeMo pair is
`0.349343628/0.349344760`. It is a tolerance-level tie, not a confident model
decision mismatch.

## Official v2.1 Profile Numerical Gates

NeMo is used here only as the Article II numerical reference for the C++ model
port. It does not supply deployment evidence and does not judge speaker
accuracy; `test.txt` and the full contextual business-view review remain the
accuracy authority.

| Profile | Frames | Max abs | Mean abs | Argmax |
|---|---:|---:|---:|---:|
| High latency `340/40/40/300` | 1502 | `1.07288e-6` | `5.7996e-8` | 1502/1502 |
| Low latency `6/7/188/144` | 1502 | `1.54972e-6` | `6.62205e-8` | 1502/1502 |

The retained raw fixture hashes are
`5ab2e78e668e749a6d518fdcb87a9719e88e3691bde73e8be098d656f778ea10`
for high latency and
`2a67312a782ac6400c058244a88f75861fea00f8716fdc1f5887f0e7052699ab`
for low latency. Both are registered as distinct CTest cases using their TOML
profiles.

The first low-latency run failed under `compute-sanitizer`: Conformer attention
scratch `ac` was allocated for `[H,T,T]` scores and then reused for
`[H,T,D/H]` context. The inherited and high-latency profiles kept `T>=64`,
masking the defect; the low-latency first block used approximately 13 frames.
The allocation now takes the larger shape. The corrected 502-frame first-block
gate passes at `max_abs=1.54972e-6`, 502/502 argmax agreement, and zero sanitizer
errors.

## Official Profile Full-Session Screening

This stage screens two streaming-capable v2.1 configurations; it does not use an
offline model as the business reference. Native C++ Sortformer processed the
complete `test.mp3`, and `test.txt` remained the sole speaker-business truth.
The whole-recording probe bypasses WebSocket transport, so its compute time is
component evidence only and is not reported as streaming performance or product
acceptance.

Both profiles produced 45,189 consecutive 80 ms diar frames, covering exactly
3615.12 seconds on the common absolute clock. Their diar evidence replaced only
the diar track in the frozen source-stable v2.1 real-WebSocket package. Frozen
ASR, VAD, forced-alignment, business-turn, and enrolled-speaker evidence stayed
fixed. Native TitaNet then evaluated the new diar spans against the same frozen
four-speaker registry before the comprehensive candidate was reconstructed.

| Profile | Diar frames | Segments | Native whole-file compute | Embedded spans | Insufficient spans |
|---|---:|---:|---:|---:|---:|
| High latency `340/40/40/300` | 45,189 | 803 | 30.8228 s | 358 | 445 |
| Low latency `6/7/188/144` | 45,189 | 819 | 805.585 s | 346 | 473 |

No tool assigned correctness. The 556-row packets were read in chronological
conversational context. For the high-latency profile, all 342 rows whose exact
speaker-evidence assignment changed from the completed inherited-profile review
were manually re-adjudicated; the other 214 rows retained their existing manual
judgment. For low latency, all 188 rows changed from high latency were manually
re-adjudicated and the other 368 retained the high-latency judgment. Thus every
reference row is covered while unchanged evidence is not judged a second time.
`unknown` is a failure, not a safe success. `ref-0022` remains the sole
reference-boundary ambiguity.

| Reference interval | Inherited v2.1 | Official high | Official low |
|---|---:|---:|---:|
| 0-600 s | 72 / 20 / 1 | 62 / 30 / 1 | 63 / 29 / 1 |
| 600-1200 s | 73 / 11 / 0 | 63 / 21 / 0 | 64 / 20 / 0 |
| 1200-1800 s | 62 / 18 / 0 | 57 / 23 / 0 | 56 / 24 / 0 |
| 1800-2400 s | 60 / 20 / 0 | 59 / 21 / 0 | 60 / 20 / 0 |
| 2400-3000 s | 80 / 49 / 0 | 81 / 48 / 0 | 73 / 56 / 0 |
| 3000-3600 s | 63 / 24 / 0 | 60 / 27 / 0 | 58 / 29 / 0 |
| 3600-3615.12 s | 3 / 0 / 0 | 3 / 0 / 0 | 3 / 0 / 0 |
| **Total** | **413 / 142 / 1** | **385 / 170 / 1** | **377 / 178 / 1** |

Each cell is `correct / incorrect / ambiguous`. The natural-turn diagnostics
are 74.2806 percent for the inherited real-WebSocket profile, 69.2446 percent
for official high latency, and 67.8058 percent for official low latency.

Relative to the inherited profile, high latency repaired 37 rows and regressed
65. The complete transition ledger is:

```text
fix 0-600:       0008 0018 0048 0089
fix 600-1200:    0173 0175
fix 1200-1800:   0185 0216 0236
fix 1800-2400:   0262 0293 0306 0312 0334
fix 2400-3000:   0343 0350 0356 0365 0379 0389 0399 0403 0405 0407 0422 0436 0447 0450 0454
fix 3000-3600:   0474 0476 0478 0499 0513 0515 0518 0532

regress 0-600:     0020 0042 0050 0051 0053 0058 0066 0070 0072 0078 0080 0088 0090 0092
regress 600-1200:  0098 0102 0111 0114 0118 0139 0146 0148 0155 0156 0158 0176
regress 1200-1800: 0180 0184 0192 0215 0217 0227 0234 0243
regress 1800-2400: 0268 0290 0294 0302 0322 0324
regress 2400-3000: 0351 0357 0366 0371 0383 0406 0414 0424 0427 0435 0440 0446 0448 0460
regress 3000-3600: 0475 0477 0485 0490 0501 0517 0519 0529 0541 0543 0550
```

Relative to high latency, low latency repaired 15 rows and regressed 23:

```text
fix:     0037 0051 0138 0158 0227 0268 0302 0322 0351 0366 0429 0501 0519 0529 0551
regress: 0033 0100 0238 0249 0264 0282 0342 0350 0365 0373 0377 0401 0405 0407 0449 0451 0453 0468 0474 0476 0487 0532 0539
```

The retained evidence hashes are:

| Artifact | SHA-256 |
|---|---|
| High TOML | `1b7e26776ef3bb2dd8f33012d013d7f4516803fb1e11a72a2fd8d8b63091b897` |
| Low TOML | `50a5c32dc90892da1e31127abd08bd1a3733b854034b215918a00e529fde92fc` |
| High frame CSV | `7012cc290b74e4153f4412447751dee654c1c42badba6d820911362cb24dcf8d` |
| Low frame CSV | `03e3fc1d4763b6e605a0c5b9e57efbb97b67aacd47264fb227fc0211620d` |
| High segment CSV | `068ee894614835280b3745ccf6475f37659b188d0344501dcb4714f7d1326b27` |
| Low segment CSV | `8e0d1d9ea0f53885f00fda9285574a54e29326d2bb8945f9f1bcf032dec17872` |
| High candidate JSON | `66f7d75cc0fdf29fb91089b34495f8d1d8e3f8751cd705af4be8becc30f9604a` |
| Low candidate JSON | `1e6e00fbf66b0afb61fb9f52ac783aef9fe84768d4ee633c711ebc629ec75433` |
| High 556-row packet | `9b6cfbd3a22c26b2c56e8c7d725558589ce25e3c4044caf08beeda4f5fa9eea7` |
| Low 556-row packet | `eb309abeb9a8bb5c8d8ddce6906868cf12d3dbc6bf151e7b94221cf84bda918c` |
| Low-versus-high 188-row packet | `a63a5bf7b4cfec85756f79c5bd1532dc85a29ed419983d7ce7f5ab607f453759` |

The official high profile is 5.04 percentage points below the inherited v2.1
real-WebSocket result; official low is 6.47 points below it. Because both fail
the 90 percent gate before transport acceptance, neither is selected for
another full real-WebSocket run. The checked-in `orator.toml` therefore stays on
v2.1 with the inherited `340/1/188/188` profile while reference-free fusion
evidence is improved.

## 120-Second Real-WebSocket Gate

The isolated run used the frozen speaker registry SHA-256 `1d1ee24b...`, the
v2.1 model hash above, a source-stable dirty-worktree snapshot, `test.mp3`, 1x
pacing, and continuous runtime plus `tegrastats` telemetry.

| Item | Result |
|---|---:|
| Audio / wall time | 120.00 s / 120.83 s |
| Stream rate | 0.993x |
| Diarization | 23 segments, 3.141 s compute, 38.201x RTF |
| ASR | 11 finals, 65.727 s compute, 1.826x RTF |
| Runtime / tegrastats samples | 115 / 120 |
| Runtime sample cadence | 95.83% |
| GPU utilization mean / p95 / max | 56.50% / 98% / 98% |
| GPU memory mean / max | 96,115.55 MB / 97,142.8 MB |
| System power mean / max | 38.56 W / 69.86 W (`tegrastats`) |
| Maximum temperature | 58.593 C |
| Mechanical contract issues | 0 |

The mechanical gate passed, but the contextual speaker promotion gate failed:

- the same-profile v2 baseline assigns the opening Zhu Jie turn to `spk_4` from
  3.12 s onward; v2.1 has no diar evidence from 4.48 to 27.20 s;
- v2.1 reuses local slots 2 and 3 across the Xu Zijing, Shi Yi, Tang Yunfeng,
  and Zhu Jie short-turn exchange from 65 to 84 s;
- from approximately 84 to 120 s, the v2.1 assignments mostly converge to the
  same `spk_1`/`spk_4` pattern as v2, with similar boundary errors.

Artifact:
`/tmp/orator-spec013/runtime-v21/120s-01/orator-candidate-v21-120s-01.json`
(SHA-256 `39d0b481d16c61c164ec47795a79d815fdccffda1c2b9db4aeaeb340c8f8d317`).

The full-length run therefore proceeded only as an explicitly requested
diagnostic to determine whether the regression was startup-localized or
session-wide.

## Full-Length Real-WebSocket Diagnostic

The full `test.mp3` session used the committed v2.1 async profile and the same
real incremental WebSocket path required for deployment. Audio was paced at 1x
while runtime telemetry and `tegrastats` were sampled continuously. Source,
configuration, binary, and dirty-worktree hashes were unchanged from start to
finish.

| Item | Result |
|---|---:|
| Audio / wall time | 3615.12 s / 3616.559 s |
| Injection factor | 1.0 (approximately 1x real time) |
| Diarization | 755 segments, 50.941 s compute, 70.967x RTF |
| ASR | 288 finals, 2003.423 s compute, 1.804x RTF |
| VAD / forced-alignment groups | 972 / 288 |
| Comprehensive speaker entries | 936 |
| Runtime / tegrastats samples | 3456 / 3606 |
| GPU utilization mean / p95 / max | 51.59% / 98% / 98% |
| GPU memory mean / p95 / max | 97,521 / 98,146.9 / 98,278.8 MB |
| Runtime system power mean / max | 40.51 / 84.69 W |
| Tegrastats system power mean / max | 40.791 / 79.942 W |
| Maximum temperature | 63.75 C |
| Mechanical contract issues | 0 |

All seven recorded extents (`ws_input`, `diarization`, `speaker_identity`,
`asr`, `vad`, `align`, and `business_speaker`) ended at exactly 57,841,920
samples with zero gaps. `timebase_ok`, `timebase_reconciled`, and
`wall_clock_ok` are all true. The server log contains no error, fatal, failed,
exception, or timeout record.

Evidence:

- terminal artifact:
  `/tmp/orator-spec013/runtime-v21/full-01/orator-candidate-v21-full-01.json`;
- artifact SHA-256:
  `653e366ba18ff61011b43d08ebd1da7f8f10c247a6b7482800a60eb4bcc4ea65`;
- manifest SHA-256:
  `b277d7f5dd791a40471a4db738d5e6e287e2a0d2d916591aa4bd30753c07e7ef`;
- isolated TOML:
  `/tmp/orator-spec013/runtime-v21/full-01/orator-v21-full.toml`;
- initial registry SHA-256:
  `1d1ee24ba6ca132e3474c2e3384ae657796c7dec7fde3024169baa87d0d39a56`;
- final registry SHA-256:
  `d7e2b7ff7a5ba3f945b177cfa2888e3d02ef7b33f4d552d6e9f39e517ad47f38`.

## Full Contextual Speaker Review

The display-only packet contains all 556 reference rows and was reviewed twice:
first in chronological order, then in fixed reverse block order
`3000-3615`, `2400-3000`, `1800-2400`, `1200-1800`, `600-1200`, and
`0-600`. No script assigned correctness.

- chronological packet:
  `/tmp/orator-spec013/runtime-v21/full-01/review/by-reference.md`;
- reverse-block packet:
  `/tmp/orator-spec013/runtime-v21/full-01/review/reverse-blocks.md`;
- current judgment: 413 correct, 142 incorrect, 1 ambiguous;
- natural-turn diagnostic: `413/556 = 74.2806%`;
- ambiguous row: `ref-0022`.

| Reference interval | Correct | Incorrect | Ambiguous | Diagnostic |
|---|---:|---:|---:|---:|
| 0-600 s | 72 | 20 | 1 | 77.42% |
| 600-1200 s | 73 | 11 | 0 | 86.90% |
| 1200-1800 s | 62 | 18 | 0 | 77.50% |
| 1800-2400 s | 60 | 20 | 0 | 75.00% |
| 2400-3000 s | 80 | 49 | 0 | 62.02% |
| 3000-3600 s | 63 | 24 | 0 | 72.41% |
| 3600-3615.12 s | 3 | 0 | 0 | 100.00% |

The degradation is distributed across the session, with the largest
concentration from 2400 to 3000 s. It is not limited to startup or the final
seconds. Representative complete-turn substitutions include Shi Yi becoming
`spk_1` at 1039.60-1044.56 s, Tang Yunfeng becoming `spk_4` at
2136.6-2150.2 s, and multiple Zhu Jie turns becoming `spk_1` or `spk_3`
between 2380 and 3388 s.

The identity mapping used by this review is:

| Global ID | Reference speaker |
|---|---|
| `spk_1` | Tang Yunfeng |
| `spk_2` | Xu Zijing |
| `spk_3` | Shi Yi |
| `spk_4` | Zhu Jie |

For auditability, the reconciled incorrect row IDs are:

```text
0005 0008 0009 0013 0018 0025 0035 0037 0045 0048 0049 0061 0063 0069
0071 0073 0076 0079 0084 0089 0096 0109 0113 0128 0130 0135 0138 0160
0171 0173 0175 0182 0183 0185 0189 0193 0194 0201 0204 0214 0216 0221
0236 0239 0241 0245 0248 0250 0252 0258 0262 0278 0280 0287 0292 0293
0296 0298 0299 0304 0306 0308 0312 0313 0327 0331 0333 0334 0335 0338
0341 0343 0350 0352 0354 0356 0359 0360 0361 0363 0365 0367 0374 0375
0379 0382 0384 0385 0388 0389 0390 0396 0397 0399 0403 0405 0407 0409
0417 0420 0422 0425 0426 0429 0432 0433 0436 0442 0444 0447 0450 0452
0454 0457 0459 0461 0463 0464 0467 0471 0472 0474 0476 0478 0499 0500
0503 0504 0505 0506 0507 0509 0513 0515 0518 0521 0531 0532 0533 0535
0537 0551
```

This natural-turn review is a valid contextual diagnostic, but not the strict
speaker-time acceptance result. `test.txt` contains duplicate and backward
timestamps, and the exact audible start/end boundaries of the 556-row reference
ledger have not yet been manually adjudicated and signed. No exact
speaker-duration accuracy is claimed until that ledger is complete.

## Comparison and Decision

| Evidence set | Correct / 556 | Diagnostic |
|---|---:|---:|
| Corrected frozen v2 written-context baseline | 378 | 67.99% |
| Current v2.1 full real-WebSocket review | 413 | 74.28% |
| v2.1 official high frozen-evidence screening | 385 | 69.24% |
| v2.1 official low frozen-evidence screening | 377 | 67.81% |
| v2.1 multi-scale TitaNet frozen candidate | 418 | 75.18% |

These rows are different artifact classes and are not presented as a paired
acceptance comparison. They do show that discarding v2.1 solely because of its
120-second startup behavior would be unjustified: the current deployable v2.1
diagnostic is 35 correct turns above the corrected frozen v2 diagnostic and
five below the best reference-free multi-scale TitaNet frozen candidate. That
candidate reaches only 75.18 percent and fails the 93 percent implementation
gate. The official-profile screening also shows that neither NVIDIA profile
improves this scene after comprehensive speaker fusion. v2.1 therefore remains
the active checkpoint under the inherited profile and is designated as the
sole closing baseline, but it is not accepted. Its 74.28 percent natural-turn
result remains 15.72 percentage points
below the 90 percent industrial floor. Exact speaker-time acceptance remains
open until the audible-boundary ledger is signed and a selected runtime
candidate passes the real WebSocket gates.
