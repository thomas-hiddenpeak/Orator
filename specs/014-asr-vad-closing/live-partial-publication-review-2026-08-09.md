# Spec 014 Live Partial Publication Review (2026-08-09)

## Scope and evaluation boundary

This transitional report covers the first bounded Spec 014 correction through
the repeated 120-second gate. It fixes duplicate publication of unchanged Live
ASR partial text. It does not change a model, decoder session, VAD decision,
endpoint, forced alignment, diarizer, speaker policy, final transcript, or TOML
behavior.

Mechanical tooling built and exercised the product, captured raw events,
verified source/time/ID/observer/telemetry contracts, and compared exact JSON
state. It did not judge ASR, speaker attribution, endpoint quality, or
hallucination. The reviewer directly read all three silence event streams and
terminal documents, then read all 18 in-scope `test.txt` contributions against
each 120-second candidate in chronological and reverse conversational context.
No program labeled a row, assigned correctness, calculated accuracy, ranked a
candidate, or issued the product verdict.

## Root cause and correction

`AsrWorker::EmitIncrementalChunk` used `inc_delivered_text_` to suppress an
unchanged typed partial, but its direct WebSocket `emit_` call sat outside that
state-change guard. Every admitted audio quantum advanced the provisional end
time and emitted the same text again even when the decoder text had not
changed.

The correction derives one `partial_changed` decision and uses it for the typed
sink, direct emitter, and delivered-state update. The state update is performed
even when one sink is absent. A changed non-empty text remains visible once and
in order; an unchanged text produces no event. Final and retract branches are
untouched, and final provides the terminal interval.

## Engineering gate

| Evidence | Result |
|---|---|
| Focused test | `test_asr_worker PASSED` |
| Focused controls | unchanged text with typed sink, unchanged text with direct-only emitter, changing text order, final IDs, VAD order, silence, short/long gap, terminal drain |
| Full build | warning-clean; no `warning:` or `error:` diagnostics |
| Full CTest | `74/74` pass in 52.88 seconds, including JavaScript and real-WebSocket tests |
| Model oracle | Not applicable; no model input, value, kernel, decoder, or numerical path changed |
| Build-log SHA-256 | `5645d6904f543cfb6e760b2443e15556606e1eae0842d93237df4344eed46c32` |
| CTest-log SHA-256 | `b175709959e8912ef3cbdc638dfbd2202600180251349df7c6d1bfad028519e7` |

## Three-run silence review

All sessions use the same 30-second, 16 kHz, signed-PCM digital-silence
fixture, 100 ms frames, source-rate WebSocket pacing, independent processes,
empty registries, isolated storage, observers, telemetry, and direct `end`.

| Run | Raw SHA-256 | Stream factor | Direct-end wait | Mechanical issues |
|---|---|---:|---:|---|
| A | `813d47b7e3f75a2dc58ff69f034115bb4ee794f12374face6ac8dd53b7617a62` | `0.991x` | `0.261 s` | none |
| B | `91fadf9f4738bc103306634b868f4925692d0d3966fcbe01bb4a25cd7a516887` | `0.991x` | `0.271 s` | none |
| C | `5ea2e06e96c129c07b014718dfaadb6162e6b997db92b25909b7f7bac88ccb15` | `0.991x` | `0.261 s` | none |

The reviewer read Run A's complete application stream: one `vad_state` with
`speech=false`, followed by four empty diarization publications. The terminal
diarization, ASR, VAD, alignment, voiceprint, business, and comprehensive
views are empty. No event contains words or asserts speech.

Run B and Run C were each read independently in the same way. Each contains
only `speech=false` and empty diarization publications, followed by empty
terminal product views. None asserts speech and none contains a substantive
live or final transcript. The candidate therefore preserves the digital-
silence hallucination conclusion. Room tone and physical microphone remain
open.

## Repeated 120-second mechanical evidence

Both runs use `test.mp3`, 100 ms frames, `1.0x`, direct `end`, independent
processes, empty registries, isolated storage, early/late observers, required
telemetry, and TOML copies that differ only in ports and artifact paths.

| Fact | Run A | Run B |
|---|---:|---:|
| Raw SHA-256 | `4d168f5285f3ff88c4ea9149ff772ae66a4de75f1a57dc48009d5ecb6d45781b` | `be26dbdd39160b8a61f490d43468a61acf0668d0c5a415d3a0c6ad3f56abcc16` |
| Manifest SHA-256 | `141345b585e2bdc9958e8a5a7250315443c6f68b5d9c45130e83d49ebc7da534` | `470ba2645f387344c6bf226bd1c898a7578e413e5bc86a99838cc87d3ac8db65` |
| Total wall / stream factor | `122.422 s / 0.980x` | `122.422 s / 0.980x` |
| Direct-end wait | `2.421 s` | `2.422 s` |
| Diar / ASR / VAD / align | `23 / 11 / 39 / 11` | `23 / 11 / 39 / 11` |
| Partial events | `96` | `96` |
| Adjacent unchanged partial states | none | none |
| Contract issue list | empty | empty |
| Producer/early/late terminal | exact within run | exact within run |

The unmodified seal baseline contains 983 partial events, including 887
adjacent repetitions of the same `text_id` and text. The candidate sequences
contain one event for each actual text transition and no adjacent unchanged
state. These counts describe a wire-state contract only.

After volatile source/compute/RTF fields are removed, the seal baseline and
both candidate runs have the same canonical product-track SHA-256:
`6fe2bbad2e826bd1e919f284ce0a719fb40d585a015b6d4be6123e7e6b7a0989`.
Their final diarization, primary-speaker, ASR, VAD, alignment, voiceprint,
business, and comprehensive JSON is therefore unchanged mechanically.

## Complete 120-second contextual review

Run A was read from `ref-0001` through the audible portion of `ref-0018`, then
from that cutoff back to `ref-0001`. Run B received the same independent two
passes. Exact final JSON equality did not substitute for either reading.

| Ref | Contextual ASR judgment | Final speaker-business judgment |
|---|---|---|
| `0001` | Opening meaning survives, but critical `RM1` remains `M一` | Zhu stabilizes after known cold-start local/unknown evidence |
| `0002` | Commitment, `40%`, and `15` survive | Substantive contribution remains Zhu |
| `0003` | Pure-Hangzhou question survives | Xu local turn remains visible |
| `0004` | Value-of-15 explanation survives | Zhu remains owner |
| `0005` | `就是杭州嘛` survives inside merged text | Known Shi micro-turn split remains |
| `0006` | Hangzhou confirmation survives | Zhu remains owner after the edge split |
| `0007` | Company/location entities remain absent from the relation question at this cutoff | Known Xu/Tang/Zhu rapid-handoff edge remains |
| `0008` | Negative Chengdu relationship answer remains malformed at this cutoff | Known rapid-handoff fragmentation remains |
| `0009` | `然后呢` survives | One inherited edge character remains split |
| `0010` | `15`, `5%`, `3.14`, and acceptance survive | Tang owns the proposal after a local onset |
| `0011` | Zhu interruption survives | Zhu owns the words; hesitation remains unknown |
| `0012` | Tang's decision survives | Tang retains the decision; next edge starts early |
| `0013` | Zhu's `不是` is present across the raw sequence | `不/是` boundary remains split |
| `0014` | `不能犹豫` survives | Tang remains owner |
| `0015` | Interruption survives; rhetorical polarity remains distorted | Zhu remains owner |
| `0016` | `你就当...为准` still contains the false leading negation at the cutoff | Tang remains owner |
| `0017` | Zhu restart survives, with the following Tang phrase joined | Known `专` handoff error remains |
| `0018` | Audible Tang continuation is exact through 120 seconds | Tang owns the substantive continuation after the inherited edge |

Both reverse readings retain this interpretation. The bounded 120-second view
still exposes critical product-name and polarity/relationship failures and
therefore does not pass the ASR critical-meaning gate. The later full-session
baseline can contextually recover some local relationship wording from wider
conversation, but that signed result also remains unchanged by this candidate.

No run introduces a new speaker identity permutation, drift pattern, long-turn
rewrite, endpoint, final-text, or business-view difference. The candidate
preserves the conditional FR50 boundary and the existing ASR limitations.

## Transitional verdict

- **Live publication contract**: passed through two independent 120-second
  real-WebSocket runs.
- **Digital silence**: passed by three independent direct contextual reviews.
- **Engineering gate**: passed.
- **ASR semantics and critical meaning**: unchanged and still below closing
  requirements; no accuracy promotion is claimed.
- **Speaker guard**: unchanged within the conditional FR50 boundary.
- **Candidate status**: accepted for the 360-second gate, not yet accepted as
  the Spec 014 release baseline.

## 360-second promotion gate

The next run uses clean commit
`a1c8d1d2924cc6a6bd68f329344d38780c51d59c`, an empty isolated registry,
the same checked-in behavior, and a TOML copy whose only differences are ports
and artifact paths.

| Fact | Result |
|---|---|
| Raw SHA-256 | `03d0426c82e7773b5888759d9612d3526b388b80b8302c1a67f220231dfe1fbe` |
| Manifest SHA-256 | `1d8cb54e45a2145e097782d20872b6056d6cdda574de6cff817fb70ca3349b82` |
| Wall / stream factor | `362.393 s / 0.993x` |
| Direct-end wait | `2.393 s` |
| Diar / ASR | `54 / 30` |
| Partial text transitions | `261` |
| Adjacent unchanged partial states | none |
| Contract / observer issues | none; producer, early, and late terminal views converge |

The reviewer read every contribution from `ref-0001` through the audible
portion of `ref-0039`, then reread the same complete evidence from the cutoff
back to the opening. Main discussion meaning remains visible, with an
approximately 80-89% semantic judgment. `RM1`, Shi Yi's name, the `70`
calculation, and the rebuilt allocation proposal remain material failures.
Wider context restores Tang's first-rule statement and the Hangzhou/Chengdu
relationship that were unsafe at the 120-second cutoff. The second portion of
`ref-0039` begins after this cutoff and is not judged as missing here.

Main speaker ownership remains usable. The known cold-start, short-turn,
overlap, and character-edge residuals remain; no new identity permutation,
drift, long-turn rewrite, or policy topology appears. The publication contract
passes 360 seconds. ASR critical meaning remains open and no accuracy promotion
is claimed.

## 600-second promotion gate

The 600-second run independently restarts the same clean commit with another
empty isolated registry and storage tree.

| Fact | Result |
|---|---|
| Raw SHA-256 | `4c754c472c34eba3030391f7d0d0dcdd5b949ef89128d00c24e9e45f51a019fd` |
| Manifest SHA-256 | `c128baeafa6b77be130a063861ac2236f2ecb5ac00fc804cab8f8a82eb5669ff` |
| Wall / stream factor | `601.614 s / 0.997x` |
| Direct-end wait | `1.614 s` |
| Diar / ASR | `98 / 52` |
| Partial text transitions | `424` |
| Adjacent unchanged partial states | none |
| Contract / observer issues | none; producer, early, and late terminal views converge |

The reviewer read all 93 contributions intersecting `0-600` chronologically,
then reread `ref-0093` back through `ref-0001` in complete context. The
approximately 80-89% semantic interpretation from the signed full-baseline
opening block remains unchanged. Reverse context recovers the Hangzhou/Chengdu
relationship, Tang's first-rule statement, the direct `50 亿` decision,
`0.7`, `28.8`, and the operative `15/25/51/Tang` allocation despite local
wording errors.

The critical failures that remain after reverse reconciliation are
`ref-0001`, `ref-0019`, `ref-0022`, `ref-0031`, `ref-0039`, `ref-0047`,
`ref-0052`, `ref-0066`, `ref-0077`, `ref-0081`, and `ref-0092`. They cover the
product/person names, missing `70`, action-right/capital-speaking mechanism,
person/value association, a lost negation, an absent independent concurrence,
`34.4` reduced to `34`, the judgment that Tang's share is too small, and a
`100` versus `1 亿` contradiction. This list is manually retained from complete
context; no executable result produced it or its interpretation.

The final business view retains the same known speaker residual classes and no
new policy regression. Publication remains one event per actual partial text
transition throughout the longer session, while every final, alignment group,
business entry, observer terminal, and source extent completes normally.

## Updated transitional verdict

- **Live publication contract**: passed engineering, three silence, repeated
  120-second, 360-second, and 600-second gates.
- **ASR semantics**: unchanged; the 600-second result remains approximately
  80-89% and fails critical meaning.
- **Speaker guard**: unchanged within the conditional FR50 boundary.
- **Candidate status**: publication correction is accepted for browser and
  physical-microphone validation before full A/B acceptance. It is not an ASR,
  endpoint, or speaker accuracy promotion.
