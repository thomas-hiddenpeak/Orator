# Spec 014 Current-Commit Seal (2026-08-09)

## Scope and authority

This report completes T010-T016 for the unchanged runtime that Spec 014
inherits from `1417334`. The captures ran from clean commit
`41c8999f05f7b4a7c54d15dc3e580bd4e613eacb`; the only changes between those
commits are the Spec 014 documentation package and its Spec 013/project-state
links. No runtime source, model, or checked-in TOML behavior changed.

`test/data/reference/test.txt` is the authoritative human-listened reference.
The two outputs below were each read against all 18 audible contributions in
chronological order and then again from the cutoff back to the opening context.
The judgments in this report were made from the complete conversation. No
compiled code, script, query, formula, metric, hash, or algorithm assigned
correctness, aggregated accuracy, ranked a defect, selected a candidate, or
issued the product decision. Automation only captured and displayed raw
evidence, checked mechanical contracts, and recorded provenance.

This is a current-commit seal, not an ASR acceptance result. It authorizes the
Spec 014 silence and full-baseline captures while preserving every Spec 013
gate.

## Reproducible source

| Item | Value |
|---|---|
| Runtime anchor | `1417334` |
| Capture commit | `41c8999f05f7b4a7c54d15dc3e580bd4e613eacb`, clean |
| Runtime delta from anchor | none; documentation only |
| Input | `test/data/audio/test.mp3`, first `120.000 s` |
| Input SHA-256 | `b7c25d1c349b02d654b6a406bc29039749e4240a4109dda4fcc905285b14b18b` |
| Human reference SHA-256 | `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86` |
| Checked-in TOML SHA-256 | `d00150ae376d802af0fcf8c0a89aa3fae1e0abb2bf5d10601c55cefd570a40db` |
| Server binary SHA-256 | `222e5b55e6e5ea62a1ea7600d676616e044af93c0b22bdba0fd7b9d0a3cbdc84` |
| Sortformer v2.1 SHA-256 | `d036020b6b93977098929d417b1b106a952ec02cc38cafc9d3315ae0ec4d90b8` |
| Qwen3-ASR directory SHA-256 | `5d5911665ea78eb50ec6703a75c37002e80274398a8fdfa45c6b1ee319671f5b` |
| Silero VAD SHA-256 | `13f1f0c5d61411445c4f0d75bc4ee1a6895ec2551edb0d1d60d692d97122d2c0` |
| Forced aligner directory SHA-256 | `91f73de6a92eb279b14a52f0b13b1108f6cd104138c88a5687a29874ff52125d` |
| TitaNet directory SHA-256 | `e771eb2e2cbfde9977fda1f537c662006aeba8daacb56e3abe0716182e867a9a` |
| Transport | production WebSocket, 100 ms PCM frames, `1.0x`, direct `end` |
| Registry | separate isolated empty registry for each run |
| Device | Jetson AGX Thor, MAXN |

Each run used a private TOML copy. A complete diff against the checked-in file
changes only WebSocket/UI ports and registry/storage/session paths. Model,
pipeline, VAD, ASR, endpoint, speaker, fusion, telemetry, and all other
behavioral values are identical to the checked-in TOML. The original project
speaker registry was not read or modified.

Raw artifacts and full manifests are retained under the gitignored tree
`artifacts/spec014/baseline-1417334/ws-120-a/` and
`artifacts/spec014/baseline-1417334/ws-120-b/`.

## Engineering seal

The clean build log is retained at
`artifacts/spec014/baseline-1417334/build/build-clean-41c8999.log`, SHA-256
`3b8d1933eb3bb1f27d1bc31e3b0a42e3a1d698a5dcd349358486fc12dcfefbba`.
It contains no `warning:` or `error:` diagnostic.

The complete CTest log is retained at
`artifacts/spec014/baseline-1417334/build/ctest-clean-41c8999.log`, SHA-256
`9f27e41b86cf86aad71a9c2224d2b4b2c6a1ae43fca3b3410418f63cd4733f97`.
All `74/74` registered tests pass in `53.08 s`. This includes the registered
JavaScript `test_web_model` contract and real-WebSocket integration test.
These results establish engineering consistency only.

## Real-WebSocket evidence

| Mechanical fact | Run A | Run B |
|---|---:|---:|
| Raw artifact SHA-256 | `e483c8f06f88bb26f1cd4a2ef01eb2391b16b272ca5256180fdaa2165d8fea74` | `d5b6c3a0d3f989d90010a040bf30a4c55959b094fe35d9d35a0e19d47fa944eb` |
| Stream factor | `0.980x` | `0.980x` |
| Total wall time | `122.425 s` | `122.423 s` |
| Direct-end wait | `2.421 s` | `2.423 s` |
| Diar / primary entries | `23 / 27` | `23 / 27` |
| ASR / VAD / align entries | `11 / 39 / 11` | `11 / 39 / 11` |
| Business entries | `35` | `35` |
| Runtime / tegrastats samples | `119 / 122` | `119 / 122` |
| Required telemetry-field coverage | `100%` | `100%` |
| Runtime / tegrastats cadence | `99.167% / 100%` | `99.167% / 100%` |
| Contract issue list | empty | empty |
| Producer/early/late terminal hashes | equal within run | equal within run |

All seven active tracks close at the exact `1,920,000`-sample input extent on
the common 16 kHz clock. Time-base, wall-clock, terminal, observer, telemetry,
and reconciliation flags pass. Run A and Run B normalized product tracks are
mechanically identical at SHA-256
`50fd452fb5893ad124b44d0e7be115a727d5775a0c8d6e3b73ecc974b417016d`.
This equality establishes repeatability only and did not make any contextual
judgment.

Thor's `tegrastats` line does not expose a GPU-utilization percentage. The
runtime independently supplied GPU utilization from `nvidia-smi`; both runs
also captured unified-memory use, GPU frequency, `VDD_GPU`, `VIN` system
power, CPU, RAM, and temperature with complete required-field coverage.

## Complete contextual review

Run A was read from `ref-0001` through `ref-0018`, then from `ref-0018` back to
`ref-0001`. Run B received the same two independent passes. The byte-identical
product tracks did not substitute for either reading. `ref-0018` was judged
only on audio audible before the exact 120-second cutoff.

| Ref | Speaker | Contextual ASR/endpoint judgment | Final speaker-business observation |
|---|---|---|---|
| `0001` | Zhu | Opening meaning is mostly retained, but critical product name `RM1` becomes `M一`; one natural contribution is split across several ASR IDs. | Known cold start: an opening fragment is Tang and later material is unknown before Zhu stabilizes. |
| `0002` | Zhu | Commitment, `40%`, and `15` are retained; `这件事情` becomes noncritical `这些事情`. | Substantive contribution is Zhu. |
| `0003` | Xu | The pure-Hangzhou question is retained; the trailing fragment is incomplete. | Local slot 2 carries the question without a mature global identity. |
| `0004` | Zhu | The explanation of the value of `15` is retained. | Zhu is retained. |
| `0005` | Shi | `就是杭州嘛` is present inside the merged ASR source. | Known short-turn failure: the phrase is divided between Xu's local slot and Zhu. |
| `0006` | Zhu | The Hangzhou confirmation is retained. | Zhu is retained after the preceding edge split. |
| `0007` | Xu | `校区现在跟成都没关系嘛` loses `校区` and `成都`; only a malformed relation question remains. | Xu's local slot carries the start and Zhu carries the trailing question edge. |
| `0008` | Tang | Critical relation answer `对，跟成都没关系` becomes `对，跟什么都没有，成都关系`; the negated business relation is not preserved. | The reply is fragmented across Tang-local, Xu-local, and Zhu spans; this is an inherited rapid-handoff defect. |
| `0009` | Zhu | `然后呢` is retained. | One leading character is local slot 3 and the remainder is Zhu, an inherited edge split. |
| `0010` | Tang | `15`, `5%`, and `3.14` plus the acceptance decision are retained. | Substantive proposal is Tang; its onset is still local-only. |
| `0011` | Zhu | `我还没说完。然后呢，额` is retained across two ASR IDs. | Zhu owns the substantive words; the hesitation is unknown. |
| `0012` | Tang | `就这么定了` is retained. | Tang is retained, but forced alignment appends the next turn's leading `不`. |
| `0013` | Zhu | `不是` is present in the raw ASR sequence. | Forced alignment leaves `不` on Tang and only `是` on Zhu; context recovers the turn but the character boundary is wrong. |
| `0014` | Tang | `不能犹豫，我跟你们说` is retained. | Tang is retained. |
| `0015` | Zhu | The interruption and desire to finish are retained; rhetorical `不知道` becomes `知道`. | Zhu is retained. |
| `0016` | Tang | Critical instruction `你就当你最前面说的话为准` becomes `不是当...为准`; the instruction's polarity is not preserved. | Tang is retained. |
| `0017` | Zhu | Opening `不是` is omitted and the following Tang phrase is joined into this ASR ID. | `就是专` is incorrectly attached to Zhu before the handoff. |
| `0018` | Tang | Audible portion through `拿钱过来就是以最快` is retained; closure at exactly 120 seconds is expected cutoff, not endpoint truncation. | Tang owns the substantive audible continuation after the inherited boundary error. |

The reverse readings preserve the same interpretation. In particular, later
context does not repair the lost Chengdu negation in `ref-0007`/`ref-0008`, and
the later repetition of Tang's `当...为准` instruction confirms that
`ref-0016` changed a business instruction rather than harmless wording.
Likewise, reading backward from Tang's long `ref-0018` continuation confirms
that `就是专治你` starts with Tang; the `专` split is not a new speaker.

The 120-second sample therefore exposes critical ASR meaning failures and does
not satisfy the Spec 014 critical-meaning gate. It also retains the documented
FR50 speaker-policy boundary: cold-start unknown/wrong evidence, the Shi
micro-turn loss, and rapid-handoff character splits remain, but neither run
introduces a new speaker identity permutation, accumulating drift, or a new
speaker policy. Because the historical FR50 raw JSON is unavailable, this is a
contextual boundary comparison against the signed FR50 report, not a bytewise
comparison with the missing artifact.

## Endpoint and Live findings

The 39 finalized VAD segments mechanically support all 11 finalized ASR IDs.
No final ID is missing from alignment, and no terminal ASR record lies beyond
received audio. The cutoff at `120.000 s` is exact. These contracts do not make
the endpoint judgment.

Contextually, the endpoint path still groups several independent rapid turns
into long decoder sessions, notably `56.124-80.164 s` and
`90.620-106.916 s`. Forced alignment and business projection recover many
speaker handoffs, but they cannot restore the two changed negations or all
single-character boundaries. The full baseline must determine whether this is
local to these interactions or a session-wide endpoint class before any TOML
value changes.

The raw Live stream has exactly one final event for each ID and no retract in
these speech runs. Final WebSocket IDs/text converge with the typed ASR track,
alignment groups, terminal timeline, and both observers. No stale draft remains
in the terminal model contract.

A separate delivery defect is nevertheless confirmed. For `text_id=0`, the
server emits 48 partial events while the visible text changes only five times;
the same unchanged-event pattern occurs throughout the run. Code inspection
locates the asymmetry in `AsrWorker::EmitIncrementalChunk`: the typed sink is
guarded by `inc_live_text_ != inc_delivered_text_`, but the WebSocket `emit_`
path runs outside that guard. The browser applies every duplicate and schedules
a render; animation-frame coalescing limits same-frame work but does not remove
the approximately 100 ms repeated updates or repeated auto-scroll. This does
not alter final text, but it is a real Live-region transport/rendering defect
and remains open for a focused engineering fix and browser validation.

## Seal decision

T010-T016 are complete. The unchanged current runtime is reproducible, passes
its engineering and real-WebSocket mechanical contracts, and preserves the
documented FR50 speaker behavior boundary. Phase 2 is authorized.

No ASR, VAD, endpoint, speaker, or Web UI product gate is closed by this seal.
The immediate sequence remains:

1. three independent digital-silence real-WebSocket sessions and direct review
   of every emitted event;
2. one full current-config `test.mp3` baseline retained under
   `artifacts/spec014/`;
3. complete 556-contribution chronological and reverse contextual review; and
4. only then freeze the first reference-free correction class.

The Live duplicate-partial correction is bounded and does not require a TOML
parameter. It may be implemented as an engineering defect after the baseline
captures, but it cannot be presented as an ASR or endpoint accuracy repair.
