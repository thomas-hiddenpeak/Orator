# Spec 014: Implementation and Validation Plan

> This document defines HOW Spec 014 is executed. Requirements and acceptance
> gates are in `spec.md`; ordered work is in `tasks.md`.

## 1. Execution Strategy

The work is divided into two independent controls:

1. **Frozen speaker control**: keep the FR50 speaker model, configuration,
   registry sequence, and fusion behavior unchanged. Speaker output remains a
   mandatory regression view because ASR boundaries affect forced alignment and
   derived business entries.
2. **ASR/VAD candidate line**: establish the current behavior from raw evidence,
   isolate one defect class at a time, and promote only candidates that pass the
   duration ladder and complete contextual review.

No ASR parameter is changed before the current-commit seal and baseline review
identify an actual defect and its source boundary.

## 2. Baseline Anchors

| Anchor | Value |
|---|---|
| Code start | `1417334` on clean `master` |
| Speaker behavior | FR50 implementation `a6f0d33` |
| Diarizer | streaming Sortformer v2.1, `340/1/188/188` |
| Canonical audio | `test/data/audio/test.mp3`, 16 kHz mono source |
| Canonical reference | human-listened `test/data/reference/test.txt` |
| Runtime config | checked-in `orator.toml` until a candidate is accepted |
| Transport | production WebSocket, 100 ms PCM frames, `1.0x` pacing |
| Registry order | empty isolated A, process restart, frozen A registry for B |

The historical FR50 reports and hashes remain valid records, but the missing
raw `/tmp` JSON means they cannot supply a new ASR review. The first current
full capture becomes the raw baseline for this spec; it does not retroactively
replace FR50's speaker acceptance result.

## 3. Artifact Layout and Provenance

All new artifacts are retained under the gitignored persistent tree:

```text
artifacts/spec014/
  baseline-1417334/
    build/
    silence-a/
    silence-b/
    silence-c/
    ws-120-a/
    ws-120-b/
    full-a/
    review/
  candidates/<candidate-id>/
    config/
    oracle/
    silence/
    ws-120/
    ws-360/
    ws-600/
    full-a/
    full-b/
    review/
  browser/
  microphone/
  holdout/
```

Each run directory contains raw unified-client JSON, terminal timeline,
manifest, registry copy when applicable, server log, continuous `tegrastats`,
and a text record of the exact command. Review records cite hashes of immutable
inputs and output files. Large runtime artifacts remain ignored by Git; signed
review documents are committed.

## 4. Current-Commit Seal

The seal establishes that documentation and code agree before behavior changes:

1. configure and build the existing `build/` tree with warnings enabled;
2. run the complete registered CTest suite;
3. run the Web UI JavaScript checks already registered by the project;
4. start `orator_ws` from the checked-in TOML with no behavioral override;
5. stream the first 120 seconds through `ws_unified_test.py` at `1.0x`, with
   observer and required telemetry capture;
6. perform complete forward and reverse contextual review of every in-scope
   reference contribution and the final comprehensive view; and
7. record whether current HEAD preserves the FR50 behavior boundary.

Build/test code may establish engineering contracts only. The reviewer, not a
test, decides transcript and speaker correctness.

## 5. Silence Baseline

The existing registered integration driver creates a 30-second 16 kHz PCM
digital-silence fixture and sends it through the unified WebSocket client. Run
three independent server sessions with isolated storage. Preserve every partial,
retract, final, VAD state, typed track, and terminal document.

The reviewer reads all emitted content and decides whether it asserts
substantive speech. A zero-entry count can be recorded as a mechanical fact but
cannot by itself issue the hallucination verdict. Later microphone testing adds
room tone and ambient noise without replacing these three digital-silence runs.

## 6. Canonical ASR Baseline Capture

If no immutable FR50 full JSON is recovered, run one new clean full baseline:

```text
test.mp3 -> 100 ms PCM frames -> production WebSocket -> AuditoryStream
         -> VAD typed evidence -> AsrWorker -> ASR typed records
         -> forced alignment -> business speaker revisions
         -> terminal comprehensive timeline
```

The unified client records transport, terminal, telemetry, source, binary, and
config evidence. The run does not evaluate accuracy.

### 6.1 Review presentation

Evidence may be displayed in fixed windows `0-600`, `600-1200`, `1200-1800`,
`1800-2400`, `2400-3000`, `3000-3600`, and `3600-3615.12`. Within each window,
preserve source order and show:

- complete `test.txt` context;
- raw finalized ASR records and their `text_id`/time spans;
- partial/retract/final event history;
- VAD segments and endpoint state;
- forced-alignment units;
- final business-speaker entries; and
- relevant warnings and terminal state.

An evidence-display tool may copy and order these fields but may not align a
reference row to an output by a correctness rule, label it, calculate a metric,
rank defects, or select a configuration.

### 6.2 Required manual passes

1. Read all seven windows chronologically.
2. Read the same complete evidence from the final window back to the first.
3. Reconcile every disagreement directly from conversational context.
4. Manually record semantic, critical-meaning, hallucination, endpoint,
   repetition, omission, and cross-speaker-join conclusions.
5. Independently verify every manually derived total and percentage without a
   formula or executable aggregation.

The baseline report identifies the first defect class to investigate. It does
not authorize a fix solely because a parameter appears correlated with an
error.

## 7. Root-Cause and Candidate Method

Defects are investigated in the following order because each later layer
depends on the earlier one:

1. **Publication correctness**: stale partials, missing retracts, duplicate
   finals, inconsistent IDs, or terminal/UI divergence.
2. **Silence admission**: VAD/frontier state permits unsupported audio to reach
   the decoder or permits unsupported final publication.
3. **Endpoint construction**: lead, stable feed quantum, trailing interval,
   segment cap, or terminal finalization cuts or joins source audio incorrectly.
4. **Decoder behavior**: repetition, omitted meaning, or hallucinated meaning
   remains when admitted audio and endpoint bounds are contextually correct.
5. **Presentation segmentation**: runtime output is correct but the business
   timeline or Live region displays incoherent boundaries.

For each defect class:

- freeze the exact contexts and accepted controls before implementation;
- trace typed evidence on the common sample clock;
- specify one reference-free runtime contract shared by more than one material
  context when possible;
- implement the smallest code correction or isolated TOML candidate;
- run numerical oracles when model values can change; and
- execute the promotion ladder, stopping at the first contextual regression.

After three failed implementations for the same hypothesis, return to the last
accepted baseline and revise the root-cause model before another candidate.

## 8. Configuration Method

The checked-in TOML remains unchanged during diagnosis. Candidate TOMLs are
complete copies stored under the ignored artifact tree, with one intentional
behavioral difference recorded per experiment. The server is started by
selecting that config file; commands do not override individual ASR, VAD,
speaker, aligner, or timeline values.

Potentially relevant existing typed fields include:

- `[asr].vad_gate`, `vad_lead_ms`, `vad_gate_chunk_ms`, `vad_trail_sec`,
  `vad_min_overlap_sec`, `segment_sec`, `max_audio_tokens`, `max_new_tokens`,
  `ban_steps`, and `decode_batch`;
- `[vad].threshold`, `min_speech_ms`, `min_silence_ms`, and `speech_pad_ms`;
- `[align]` and `[timeline]` fields only when evidence proves the defect lies in
  those owners.

This list is diagnostic scope, not authorization to tune all fields. Speaker and
diarizer fields remain frozen.

## 9. Promotion Ladder

### 9.1 Engineering gate

- warning-clean build;
- complete CTest;
- applicable JavaScript checks;
- model-stage numerical oracle when applicable;
- deterministic IDs, sample extents, and typed publication order.

### 9.2 Product gates

1. Three independent 30-second silence runs.
2. Two independent 120-second runs and complete forward/reverse review.
3. One 360-second run and complete forward/reverse review.
4. One 600-second run and complete forward/reverse review.
5. Full A with empty registry and complete chronological/reverse review.
6. Process restart, frozen-registry full B, and the same complete review.

Each duration is authorized only after the previous result is reviewed. Code
may verify transport and structural contracts, never the product verdict.

## 10. Speaker Regression Boundary

The speaker policy, Sortformer/TitaNet configuration, and registry procedure are
held constant. Raw diarization and voiceprint tracks should therefore remain
unchanged for equivalent input and scheduling conditions, but that mechanical
fact is insufficient: changed ASR boundaries alter forced alignment and may
change business-speaker projection.

Every full ASR candidate is reviewed against all 556 contributions for both ASR
meaning and final speaker ownership. The FR50 manually signed speaker result is
the comparison boundary. No script compares, labels, counts, or ranks the
speaker result.

## 11. Web UI and Microphone Validation

After a runtime candidate passes the 600-second gate:

1. start the local UI served by `orator_ws`;
2. use Playwright for deterministic file-input, reconnect, persistence, export,
   desktop, and mobile mechanical checks;
3. inspect screenshots and DOM state for overlap, clipping, stale partials, and
   incoherent Live/Final replacement;
4. execute physical microphone sessions with explicit browser permission;
5. review silence, room tone, short speech, pause, interruption, overlap, and
   background-noise contexts; and
6. verify that terminal/load/export rebuilds the same accepted transcript.

Browser automation verifies mechanics. Contextual transcript and endpoint
correctness remain reviewer judgments.

### 11.1 Session-persistence correction boundary

The first clean Chromium file-input run reaches terminal convergence, exact
download, screenshots, and a visible persisted-session row, but `End` followed
by `Clear` cannot reload that row. The retained files show the root cause below
the Web UI: `AuditoryStream::Reset()` saves even when the fresh session has
received no samples, while the generated session ID has only second-level time
resolution plus the process ID. A same-second empty reset can therefore replace
the just-finalized non-empty document.

The bounded correction is a storage-lifecycle contract, not a model or endpoint
candidate:

1. snapshot the current session sample extent before stopping workers;
2. persist on reset only when that extent is nonzero;
3. generate an opaque ID from microsecond wall time, process ID, and a
   monotonically increasing per-process sequence; and
4. retain the existing atomic file write, timeline schema, load RPC, browser
   state, TOML, and all ASR/VAD/diarization/speaker behavior.

Focused tests must prove that an empty reset creates no saved row, two rapid
non-empty resets create distinct loadable rows, and a non-empty row remains
unchanged after a subsequent empty reset. The complete real Chromium flow is
then repeated from a fresh isolated storage tree before microphone work starts.

### 11.2 Browser file-pacing correction boundary

The exact clean-commit 120-second repeat passes terminal/load/export/reconnect
mechanics and complete contextual reading, but its terminal document records
`wall_clock_ok=false`. The first-sample wall clock and persisted-file time bound
the path to approximately 123.121 seconds. The existing file sender schedules
every 60 ms chunk with another relative 60 ms timeout; event-loop delay is
therefore accumulated over the whole file and consumes the narrow allowance
left after normal terminal processing.

The browser transport correction must:

1. retain the exact 60 ms PCM frames and byte coverage;
2. schedule each next frame against an absolute deadline derived from the
   stream start and bytes already sent;
3. expose the pure delay calculation for dependency-free JavaScript tests;
4. use no runtime dependency and change no backend or model behavior; and
5. repeat the clean 120-second browser flow, record source streaming, automatic
   Flush, and terminal End separately, and complete forward/reverse contextual
   reading.

The pure test verifies that per-callback lateness is not added to every future
deadline and that a late callback requests immediate catch-up rather than a new
full-frame delay. The terminal `wall_clock_ok` field is a direct-end production
gate: an interactive file flow deliberately performs a nonterminal Flush before
the user sends End, so that combined field is retained as evidence but cannot
judge browser source pacing. Browser mechanics remain distinct from product
accuracy.

### 11.3 Physical-input availability boundary

Physical-microphone acceptance requires proof that voiced acoustic input reaches
the browser capture source. Device enumeration alone is insufficient. Before a
speech scenario is reviewed, retain the source identity and a direct capture,
inspect the captured waveform, and use a known voiced source to establish that
the environment can deliver sustained speech into that source.

If the host exposes a source but cannot deliver a working acoustic signal:

1. retain the probes and one real-browser room-tone session as bounded evidence;
2. complete contextual review of every emitted event and terminal state;
3. leave active-speech microphone tasks open and state the hardware limitation;
4. record Firefox and Safari/WebKit availability without installing an
   unrelated browser stack or substituting a fake microphone;
5. do not start Phase 5 full-candidate acceptance; and
6. return to the next already-proven ASR semantic defect class using the frozen
   full baseline and the staged silence/120/360/600 gates.

When functional capture hardware becomes available, resume short speech,
continuous speech, pause, interruption, overlap, and ordinary background-noise
sessions before candidate freeze. This availability boundary does not waive
FR10 and does not authorize a model, endpoint, VAD, or speaker parameter change.

## 12. Final Acceptance and Handoff

When full A/B pass:

- freeze the accepted commit, checked-in TOML, model hashes, and registry;
- execute the locked holdout only after its provenance is finalized;
- write the Spec 014 final report with complete evidence and limitations;
- update Spec 013 T084/T085/T086 evidence status and `PROJECT_STATE.md`; and
- create a release tag only after independent report review and every required
  Spec 013 gate is closed.

## 13. First Bounded Correction: Partial Publication

The signed full baseline freezes publication correctness as the first defect
class. `AsrWorker::EmitIncrementalChunk` already suppresses an unchanged typed
partial by comparing `inc_live_text_` with `inc_delivered_text_`, but its direct
WebSocket `emit_` call currently sits outside that state-change branch. As each
new audio quantum advances the provisional end time, the same non-empty text is
therefore emitted again even though the accepted partial state did not change.

The correction is deliberately below all model and endpoint policy:

1. derive one `partial_changed` decision from the active `text_id`'s text;
2. when exposed and changed, mirror the same new partial to the typed sink and
   direct WebSocket emitter, then record it as delivered even if one sink is
   absent;
3. when exposed and unchanged, publish nothing; and
4. leave final, retract, VAD, decoder, alignment, speaker, time-base, and TOML
   behavior unchanged.

A focused `AsrWorker` test must feed two decoder quanta that return identical
text and prove that the typed sink and direct event stream each receive one
partial before the final. Existing changing-text, final-ID, VAD-order, silence,
short-gap, long-gap, and terminal-drain cases remain controls. The real
WebSocket gate then verifies that repeated unchanged Live events are absent
without using that mechanical observation to judge transcript correctness.

## 14. Final ASR Prompt Candidate

Complete forward and reverse contextual rereading rejects long decoder sessions
or tail drift as the common source of final ASR meaning loss. The clearest
common-clock trace is `text_id=133`: VAD admits both repetitions of
`一致行动的人`, the decoder publishes `语音识别` while the segment is still Live,
forced alignment assigns times to those already-finalized characters, and the
business-speaker view copies them into speaker-bounded entries. The configured
system prompt itself contains `语音识别`.

The first final-meaning candidate therefore changes only
`[asr].system_prompt` in `orator.toml` from the historical Chinese instruction
to an empty string. The model's local chat template supports empty system text,
and its README transcription examples do not add a custom system instruction.
The candidate carries no `test.txt` vocabulary and leaves the language hint,
segment cap, decoder limits, VAD, alignment, FR50 speaker behavior, and all
model code unchanged.

Validation order is:

1. assert checked-in TOML resolution and run the warning-clean build plus full
   CTest;
2. review a focused real-WebSocket excerpt containing the complete repeated
   legal-term exchange, without treating that one phrase as promotion evidence;
3. complete three independent silence reviews and two independent canonical
   120-second forward/reverse context reviews;
4. only after those controls pass, complete 360-second and 600-second forward
   and reverse context reviews; and
5. reject and remove the candidate on any new critical meaning, hallucination,
   endpoint, or frozen-speaker regression.

No full candidate run is authorized by the diagnosis or focused excerpt. See
`final-asr-prompt-causality-review-2026-08-09.md`.

The clean `5accc5f` focused run rejects this candidate before step 3. Both
repetitions of the legal term remain unusable, while a name and the neighboring
option-pool discussion regress under otherwise identical decoder boundaries.
The checked-in TOML is restored to the pre-candidate prompt and T048 does not
start. The next hypothesis must account for the fact that system conditioning
changes ambiguous decoding but neither the historical instruction nor an empty
instruction reliably preserves domain terms.

## 15. Streaming Encoder Equivalence Diagnosis

The next phase returns to model-integration evidence before another transcript
candidate. The historical T011 probe establishes that an independently encoded
eight-second attention window matches the corresponding slice of a full
windowed encode. Production later reduced `kStreamWindowMel` from 800 to 100 for
Live latency, but no retained numerical artifact extends T011's equivalence
claim to that one-second block. Since `n_window_infer=800`, a full trained
attention window contains eight 100-frame convolution chunks.

The diagnosis proceeds without evaluating product output:

1. pin and record the official Qwen3-ASR source revision used by the oracle;
2. repair `tools/reference/asr_oracle.py` so its repository, model, input, and
   output paths resolve from the project instead of stale locations;
3. extend the existing encoder probe to compare independent 800-frame and
   100-frame slices against the same full-window encoding, retaining tensor
   differences as numerical implementation evidence only;
4. trace official accumulated-audio/prefix-rollback/final-tail behavior against
   `Qwen3Asr::StreamChunk`, `StreamDecodeStep`, and `StreamFinalize`;
5. select a runtime correction only when a concrete implementation mismatch is
   demonstrated independently of `test.txt`; and
6. keep model behavior frozen until the correction passes its numerical oracle,
   warning-clean build, and complete CTest.

If the one-second append is non-equivalent, the preferred correction separates
encoder context from Live publication cadence: preserve the model's trained
acoustic window while continuing to expose stable partial state at an ergonomic
interval. The exact behavior must be TOML-owned; no audio-specific term,
timestamp, or reference hint may enter runtime code. Only after the numerical
contract passes does a focused real-WebSocket capture receive complete forward
and reverse contextual semantic review. That review, not the probe, decides
whether a candidate can enter FR9.

The completed T061/T062 control now confirms that boundary: complete 800-frame
windows match exactly, while all tested 100-frame standalone slices diverge
from the same full encode and reach a maximum absolute difference of `0.1759`.
Source history traces the unsupported reduction to `d7010a5`. The first bounded
candidate is therefore `asr-final-full-context-decode`: retain the exact
TOML-bounded segment PCM, keep the existing one-second path for provisional
Live text, and regenerate only Final through the existing full-context native
model path. `[asr].final_full_context_decode` owns activation and
`[asr].final_max_new_tokens` owns its independent decode budget. See
`streaming-encoder-boundary-review-2026-08-09.md`.

Complete forward and reverse review rejects that first correction. It repairs
parts of the option-pool discussion and removes the provisional prompt phrase
and late unsupported money assertion from Final, but it also creates new
critical statements about a named participant and the type of signed business
agreement. The checked-in switch therefore returns to false; the implementation
is retained only as inactive evidence and no silence or duration ladder is
authorized.

Phase 3D isolates the demonstrated boundary before adding another compound
design. The runtime exposes the acoustic append window through typed TOML,
with 100 mel frames as the restored control and the model-defined 800 mel frames
as the only candidate value. Everything else, including decoder rollback and
Final behavior, remains fixed. The same 102-second context is sufficient for
the causal review because it contains the repaired and regressed neighboring
business statements. Only a complete chronological and reverse reading can
decide whether the trained window contributes useful product evidence.

An eight-second Live update cadence is not an acceptable final UI design by
itself. If, and only if, the isolated control improves Final meaning without a
critical regression, the next implementation will preserve the accepted
one-second provisional Live path and replay the exact bounded PCM through the
trained-window streaming path at Final. This sequencing separates model-window
evidence from presentation latency and avoids selecting between transcripts by
code.

The isolated T067 result fails that prerequisite. Complete contextual review
finds that the 800-frame path contributes useful long-segment vocabulary but
does not provide a uniformly safer transcript: short residual segments repeat
the rejected complete-context errors, two long Finals stop before critical
business clauses, and the first Live state still contains the configured prompt
phrase. The missing clauses are absent before forced alignment and speaker
fusion, so downstream evidence cannot reconstruct them. Phase 3D therefore
restores the 100-frame control and does not implement the proposed Final replay.

The revised causal analysis separates three facts: 800-frame encoder locality
is numerically valid; an eight-second decode cadence with a 32-token continuation
budget can leave long contributions unfinished; and a short residual encoded as
one complete block can still be semantically worse than the legacy 100-frame
stream. A later candidate must address decoder continuation and short-tail
handling as explicit model contracts. It may not select between alternative
transcripts by code or infer correctness from tensor parity.

## 16. Risks and Controls

| Risk | Control |
|---|---|
| Missing historical raw artifacts | Recapture a current clean baseline and retain it under `artifacts/spec014/` |
| ASR change regresses speaker business view | Freeze speaker configuration and perform complete dual-purpose full review |
| VAD tuning repairs one phrase but loses another | One-variable TOML candidate, staged duration gates, complete context and controls |
| Script-derived accuracy enters the process | Tools are limited to capture, hashes, schema checks, and unjudged evidence display |
| Repeated full runs consume time without a supported hypothesis | Full run only after silence, 120, 360, and 600 gates pass |
| UI appears correct while terminal state differs | Compare event, typed, terminal, persisted, exported, and rendered states by stable `text_id` |
| Enumerated audio source has no working transducer | Preserve direct and controlled-playback probes, leave voiced microphone gates open, and never substitute fake-device evidence |
| Temporary artifacts disappear | Store raw evidence under persistent gitignored project artifacts, cite hashes in committed reports |
| A Live-latency change invalidates trained encoder context | Prove 100-frame and 800-frame locality separately; decouple publication cadence from acoustic windowing when needed |
| Numerical parity is mistaken for transcript correctness | Limit oracle claims to tensors, tokens, and deterministic state; require complete contextual review on the production WebSocket path |

## 17. Constitution Check

This plan preserves zero runtime dependencies, one common time base, independent
typed pipelines, production-WebSocket validation, TOML-owned behavior, trusted
model oracles, complete contextual result review, and synchronized SDD/state
documentation. No constitutional exception is required.
