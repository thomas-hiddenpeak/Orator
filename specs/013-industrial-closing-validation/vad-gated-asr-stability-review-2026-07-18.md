# VAD-Gated ASR Stability Review

**Date**: 2026-07-18

**Scope**: T117-T121 / FR28

**Reference**: `test/data/reference/test.txt`

**Candidate input**: `test/data/audio/test.mp3`

**Decision boundary**: 120-second promotion only; no full-length accuracy claim

## Governance

All commands used in this review were limited to capture, hashing, structural
comparison, numerical validation, and evidence arrangement. No program, script,
query, formula, metric, or algorithm assigned speaker correctness, aggregated a
product result, selected a candidate, or issued the promotion decision.

The contextual findings below were produced by reading the candidate business
view beside the human-listened `test.txt`, first in chronological order and
then from the last in-scope contribution back to the first. Mechanical hashes
establish determinism only.

## T117: Producer-Divergence Diagnosis

The two rejected T116 full captures were exported into the production typed
replay format. Each package was replayed twice without changing an input.

| Evidence | Run A | Run B |
|---|---|---|
| Source timeline SHA-256 | `b444d55a72a0bf224325530dd8cc409baa96069d26846bc2212969ef91ea25eb` | `67e3791f08f00c13f4d768881b9fe55624245d30407809b9f7ac9d0ce7113f65` |
| Diarization track | `7f2a78abc0a1e26e9ebfb5782894c642f88398e81a3e52ec86318799f71d338f` | same |
| Primary-speaker track | `bff99424c79440cde425a2177b00278338559d0bd78f810b9d462a1e08c8d4d9` | same |
| ASR track | `6d719d5cf62df0bb1230f3a3c0031bfd70ca2c46f2591d32c4cf31c045e45d32` | `69fb58264828b56a74952cd7aae330a0832a245d01d58285660483d5d865cdae` |
| Alignment track | `c973cce8c8347eda93bac70f1147fd757bdef89e7026903a2603866a8c632391` | `2ddf845a265b9e839718a3ae652d389fe5c917f06814d6511e49138c6984f148` |
| Voiceprint track | `ad8805ef7aa3733d500026cd832b58c864023de5ad95a2eea3738f09f0385f38` | `38259eee703e117fa18a5b3c7a4384438af6f95d0d77d065dbac9701c242fe69` |
| Business replay, both repeats | `9c76bff0bda29ad99bd3cb5ede00c8f200fbb5407f1408df4ff275229f8603fb` | `0ca4f5d962eb623c733c1629943efc6b3eac30e7196690ffc2c847ad2cc876d7` |

The first producer difference is ASR `text_id=49`. Run A opens the segment at
`658.7`, while Run B opens it at `658.8`; its text also differs. The finalized
VAD evidence around the segment is unchanged. Forced alignment first differs
on the same ASR source, and the first voiceprint difference is the evidence
derived from that changed alignment. The first final business-view difference
is therefore also inside `text_id=49`, beginning at `658.78/658.80`.

The code path explains the observation. `AsrWorker` previously consumed any
audio beyond the currently published VAD horizon immediately. Its next decoder
reset was therefore whichever 100 ms input frame the ASR worker observed before
the independent VAD worker advanced. The parsed `asr.vad_lead_ms` setting did
not govern this fallback. The ASR segment then changed alignment units,
ASR-derived voiceprint windows, and final speaker projection. The identical
diarization and primary tracks rule out Sortformer variance as the source of
this first divergence.

### Contextual Review of the First Divergence

The complete implicated conversation is `ref-0098` through `ref-0103`. It was
read in both directions.

| Reference | Human-listened context | Run A / Run B contextual result |
|---|---|---|
| `ref-0098` | Xu Zijing asks what absolute approval means | Both retain Xu Zijing for the recognizable contribution. |
| `ref-0099` | Shi Yi says the two of them can decide | Both contain the same mixed attribution; the ASR wording changes but does not create the first A/B speaker difference. |
| `ref-0100` | Tang Yunfeng says there is no need to hold a meeting | Both retain Tang Yunfeng. |
| `ref-0101` | Shi Yi jokes about changing the company to sell excrement | Both retain Shi Yi; Run B preserves more of the intended wording. |
| `ref-0102` | Tang Yunfeng repeats that no meeting is needed | Run A assigns the recognizable phrase to Tang Yunfeng; Run B assigns it to Shi Yi. Complete context makes Run B wrong here. |
| `ref-0103` | Shi Yi continues the joke | Both retain Shi Yi. |

The reverse read confirms that `ref-0102` is a real business-level consequence
of the producer scheduling difference, not a display-only boundary change.
T117 therefore identifies ASR/VAD publication order as the root producer
variance and justifies a scheduling-invariant VAD gate before another speaker
fusion rule.

## FR28 Implementation

FR28 introduces a typed `VadStateResult` with the observed frontier, padded
active onset, stable active frontier, and confirmed-silence frontier. `GpuVad`
derives it from its existing endpoint state and deposits it only through
`ComprehensiveTimeline`.

`AsrWorker` now owns undecided audio until typed VAD evidence makes it stable.
It applies TOML `asr.vad_lead_ms`, consumes active speech in TOML
`asr.vad_gate_chunk_ms` quanta, preserves the existing trailing-gap policy, and
skips confirmed silence. During shutdown, the controller freezes terminal VAD
state before draining ASR, while the aligner remains active for the resulting
finals. No worker callback, shared model pointer, or inter-worker cursor wait
was added.

Focused tests exercise pre-published, late-published, and active-then-final VAD
orders over identical samples. Reset positions, chunk sizes, fed samples,
events, and finals are exactly equal. Tests also cover lead retention,
confirmed silence, short and long gaps, terminal tails, timeline monotonicity,
configuration serialization, and VAD frontier invariants.

Engineering evidence before the real-streaming gate:

- warning-clean C++/CUDA build;
- VAD numerical/CPU-oracle gate passed;
- all `69/69` registered CTest entries passed;
- checked-in TOML explicitly freezes `asr.vad_gate_chunk_ms = 100`.

These facts do not evaluate transcript or speaker accuracy.

## T121: Independent 120-Second Real-WebSocket Runs

Both runs used the production server, incremental 100 ms WebSocket frames at
1.0x pacing, Sortformer v2.1, isolated empty registries and storage, direct
`end`, observers, runtime telemetry, and `tegrastats`. Behavioral TOML values
were identical.

| Item | Run A | Run B |
|---|---:|---:|
| Audio | 120.000 s | 120.000 s |
| Total wall time | 121.11 s | 121.11 s |
| Stream rate | 0.991x | 0.991x |
| Direct terminal wait | 1.115 s | 1.113 s |
| ASR compute RTF | 2.369x | 2.380x |
| Final ASR segments | 10 | 10 |
| Final diar intervals | 23 | 23 |

All seven typed tracks end at exactly `1,920,000` samples with no gap. Observer
state converges. Artifact-level hashes differ because their paths, timestamps,
and telemetry are different; the canonical entry hashes for every product
track are identical:

| Track | Canonical entry SHA-256 |
|---|---|
| diarization | `f36a3c4cc33af9d7640c30185a677b55848e53e2a46caea504bc4e983f5717e3` |
| primary speaker | `4f4c1d396e00b991ba391a01f487dc18b22d5d5f03049a4134b342aaa4675dba` |
| ASR | `d7708c71b0fa864ff321e0011dea71b5049f9ad0ee8ee92f97e600a7b4383989` |
| VAD | `b9de53a5d41833b8d7b5e149eef59499ace449cf7e388ae9eb0b629f175e12e5` |
| alignment | `79a4226606f562679e1d99760f1fa76221757d5643c4a90467a6191c0f3e19c3` |
| speaker voiceprint | `37517e5f3dc66819f61f5a7bb8ace1921282415f10551d2defa5c3eb0985b570` |
| business speaker | `6da393dfea40c560163bebb7951689d9fabeecdfe051742c574496812f9ff42d` |

Exact equality here is scheduling-determinism evidence, not an accuracy score.

### Forward Contextual Review

The in-scope reference contains `ref-0001` through `ref-0018`.

- `ref-0001`-`ref-0002`: FR28 removes the pre-reference `0-3 s` transcript
  previously emitted from undecided audio and preserves the dominant Zhu Jie
  monologue. The short cold-start identity fragment and missing early diar
  support remain visible; final full-session gallery rewriting is not available
  in an empty-registry 120-second run.
- `ref-0003`-`ref-0008`: Xu Zijing, Zhu Jie, Shi Yi, and Tang Yunfeng alternate
  rapidly. FR28 changes ASR boundaries and exposes more of one Xu Zijing tail,
  but the natural-turn speaker sequence and its existing short-fragment errors
  do not regress relative to the prior candidate.
- `ref-0009`: Zhu Jie's short `然后呢` remains split across local evidence and
  remains an existing speaker error. FR28 does not repair or worsen it.
- `ref-0010`-`ref-0018`: The Tang Yunfeng/Zhu Jie exchange retains the same
  contextual speaker sequence. Changed endpoints affect fillers and partial
  words, not who owns the recognizable natural turns.

### Reverse Contextual Review

Reading `ref-0018` back through `ref-0001` confirms the same result. The final
Tang Yunfeng stretch, the alternating interruption sequence, the four-speaker
exchange, and the opening Zhu Jie monologue remain coherent in the same order.
No A/B disagreement exists, and no natural-turn speaker regression is
introduced by the stable VAD gate. The review does not claim that the known
early cold-start and micro-turn errors are correct.

## Decision

FR28 is retained for the next promotion stage because it removes the proven
ASR/VAD scheduling race, passes the engineering gates, produces identical
product tracks in two independent real-WebSocket runs, removes the
pre-reference blank-audio transcript, and introduces no contextual speaker
regression in the complete 120-second gate.

This is not a full-length speaker result and does not alter the corrected T111
`514/556` frozen comparison. A clean 600-second run and then independent full-length A/B
captures remain mandatory. Any full candidate must receive complete 556-item
forward and reverse contextual semantic review before the speaker baseline or
closure status can advance.

## T122: Clean 600-Second Gate

Transitional commit `1d511a946b291347d0d52eea0ee17e137cee65f0` ran
`600.000 s` of `test.mp3` through the production WebSocket with 100 ms frames
at 1.0x pacing, direct `end`, an isolated empty registry, observers, runtime
telemetry, and `tegrastats`. The run completed in `603.064 s`; direct terminal
wait was `3.064 s`. All seven tracks ended at exactly `9,600,000` samples with
zero extent gaps, observer terminal hashes converged, telemetry cadence passed,
and no mechanical contract issue was recorded. The captured artifact is
`/tmp/orator-spec013/release-1d511a9-t122/fr28-600-ws.json`, SHA-256
`ac312cbcb132f5b827e275e5d97a6615197f659399962d6fa1b4bf3251cba2c9`.

The reviewer then read every in-scope human contribution, `ref-0001` through
`ref-0093`, first chronologically and then from the last contribution back to
the first. The review preserves the known short-turn defects already present
in the accepted evidence, but finds two new business-speaker regressions:

- In `ref-0037`, Tang Yunfeng's continuation `不能再等了` is assigned to Zhu
  Jie. The raw Sortformer interval has the same local-channel error as the
  earlier evidence, but the earlier comprehensive view corrected it using
  ASR-derived phrase voiceprint evidence.
- In `ref-0073`, Shi Yi answers Tang Yunfeng with `我可以否决了，对，45`.
  The new view assigns the recognizable response to Tang Yunfeng, breaking the
  question/answer exchange and the following Shi Yi calculation.

This semantic result rejects the 600-second promotion. No full capture was
started, and T111 remains unchanged.

## T124: Deterministic Trailing-Context Correction

FR28 had preserved the absolute clock while feeding only VAD speech regions.
Short natural pauses inside one trailing group were therefore removed from the
decoder input, while forced alignment still operated on the original source
clock. A closing source also ended at the padded VAD boundary instead of
retaining the configured trailing alignment context. The two tracks were
mechanically valid but no longer described the same acoustic sequence at turn
boundaries.

The successor keeps an undecided gap pending until typed VAD establishes one
of two outcomes. Speech returning within TOML `asr.vad_trail_sec` receives
every intervening sample in the same fixed-quantum decoder session. A confirmed
long gap closes at the trailing source-clock bound without sending terminal
silence-only audio to the decoder. Those unconsumed samples remain available
for the next TOML lead, so pre-published and endpoint-first VAD schedules retain
identical decoder calls, reset positions, samples, events, finals, and source
bounds without replaying audio.

The warning/error build scan is empty and all `69/69` registered CTest entries
pass. Focused tests include pre-published, late-published, active-then-final,
endpoint-first long-gap, short-gap, confirmed-silence, and truncated terminal
tail schedules. This is engineering evidence only. Blank-audio and new
120/600-second real-WebSocket gates remain mandatory before the correction can
be retained for a full run.
