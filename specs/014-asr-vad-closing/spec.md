# Spec 014: ASR, VAD Endpoint, and Live Transcript Closing

- **Feature**: `014-asr-vad-closing`
- **Status**: Phase 1 and Phase 2 complete; bounded Live publication candidate
  passes engineering, three-run silence, repeated 120-second, 360-second, and
  600-second gates; Phase 4 Chromium file input and physical-endpoint room tone
  pass their bounded reviews, but active physical-microphone scenarios remain
  blocked by unavailable effective capture hardware; Phase 3B causal review
  traces one prompt-conditioned decoder factor and rejects/removes the first
  empty-system-prompt candidate; Phase 3C is revalidating the native streaming
  encoder and decoder boundaries against the official implementation; the
  resulting TOML-owned full-context Final candidate passes its engineering gate
  but fails complete focused contextual review and is disabled; Phase 3D now
  exposes a typed TOML acoustic window, passes its engineering and exact
  eight-second numerical gates under the restored 100-frame control; complete
  forward/reverse review rejects the isolated 800-frame candidate and restores
  that control; Phase 3E adds the official accumulated-audio state transition
  behind a typed TOML mode and passes its control-side engineering gate; complete
  forward/reverse review rejects the two-second accumulated candidate and
  restores `kv_append`; Phase 3F also rejects the bounded one-second accumulated
  cadence after complete forward/reverse review and restores the control;
  Phase 3G adds a default-off raw decoder-state trace, passes its engineering
  gate, and completes an exact 91-row forward/reverse root-cause review; the
  accumulated state transition matches the pinned official source, while one
  concrete initial-token EOS-suppression mismatch authorizes a single Phase 3H
  candidate; that candidate now passes focused and complete engineering gates
  and awaits exact contextual evidence; ASR semantic closing and full-candidate
  acceptance remain open
- **Owner**: project owner
- **Constitution**: v1.7.0
- **Depends on**: Spec 003 (streaming ASR), Spec 004 (typed comprehensive
  timeline), Spec 006 (Web UI), Spec 009 (forced alignment), and Spec 013
  (conjunctive industrial closing gates)

> This document defines WHAT must be established and WHY. Implementation and
> validation design are in `plan.md`; ordered work is in `tasks.md`.

---

## 1. Summary

Orator has a repeatable speaker-business baseline, but the combined product is
not closed. The native Qwen3-ASR implementation has numerical oracle evidence,
while full-session contextual semantic accuracy, silence hallucination,
endpoint behavior, live/final convergence, physical-microphone behavior, and
the corresponding Web UI evidence remain open.

Speaker optimization has reached the evidence available from the current
Sortformer v2.1, TitaNet, voice activity detection (VAD), and forced-alignment
tracks. Further speaker work is deferred until a new independent signal or new
business data exists. This spec therefore freezes the accepted speaker behavior
and advances ASR, VAD endpointing, and transcript presentation without weakening
any Spec 013 gate.

## 2. Verified Starting State

The following facts define the start of this work:

1. The clean repository and implementation starting point is `master` commit
   `1417334` (FR60). FR51-FR60 contain evidence audits and diagnostic support;
   they did not promote a new speaker product policy.
2. The frozen speaker product behavior is FR50 at commit `a6f0d33`, with
   streaming Sortformer v2.1 profile `340/1/188/188`, the checked-in TOML
   behavior, and the empty-registry A / frozen-registry B sequence.
3. FR50's complete contextual review remains the speaker reference result. Its
   speaker-time gates pass, but critical and confidently wrong attribution gates
   remain open. This is a conditional baseline, not canonical speaker closure.
4. The original FR50 raw full-session JSON files were recorded under `/tmp` and
   are no longer present on 2026-08-09. Reports and hashes remain, but they are
   not a substitute for raw ASR evidence. A current baseline capture is required
   unless an immutable byte-identical copy is recovered before execution.
5. Qwen3-ASR mel, encoder, and decoder numerical gates have prior evidence. They
   establish implementation parity only; no full contextual ASR product result
   is currently signed.
6. The clean current-commit seal at documentation commit `41c8999` passes a
   warning-clean build, all `74/74` registered tests, and two independent
   120-second real-WebSocket captures. Complete forward/reverse contextual
   review preserves the FR50 speaker-policy boundary but identifies critical
   ASR meaning failures and duplicate unchanged Live partial delivery. See
   `current-commit-seal-2026-08-09.md`. No product gate is advanced by that
   bounded seal.
7. Three independent 30-second digital-silence sessions at clean commit
   `f1d0e05` pass their mechanical contracts and direct event-by-event review.
   Each output makes no speech assertion and contains no substantive live or
   final transcript. See `digital-silence-review-2026-08-09.md`. Physical
   microphone room tone and background-noise behavior remain open.
8. One persistent 3615.120-second current-config baseline at clean commit
   `96b8347` passes its real-WebSocket, common-time-base, observer, telemetry,
   forced-alignment, stability, and direct-end mechanical contracts. Complete
   chronological and reverse-window review of all 556 reference contributions
   manually judges ASR semantics in the 70-79% band, below every complete
   600-second block gate, with multiple unrecovered critical-meaning failures.
   The same review preserves the conditional FR50 speaker boundary and finds
   unchanged Live partials repeatedly published over WebSocket. See
   `full-baseline-context-review-2026-08-09.md`.
9. Clean commit `d09b13b` passes the complete 120-second real-Chromium file
   flow, including Live/Final convergence, persistence, reload, export,
   reconnect, and desktop/mobile review. The complete forward/reverse context
   reading finds no browser-only semantic or speaker change. See
   `browser-persistence-review-2026-08-09.md`.
10. Clean documentation commit `d2ac5d1` completes one real-Chromium
    physical-endpoint room-tone session. Complete chronological and reverse
    event review finds no substantive speech assertion in the no-deliberate-
    speech context. The host exposes only the Jetson APE board analog source;
    direct and playback/capture probes do not establish a working microphone
    signal, so active-speech microphone requirements remain open. See
    `physical-microphone-review-2026-08-09.md`.
11. Firefox is an unusable snap launcher on this host, no Firefox snap or
    Playwright Firefox exists, and no WebKit browser/driver is installed.
    Safari is unavailable on Linux. This records the target-environment limit;
    it is not a cross-browser behavior result.
12. Complete direct rereading of the frozen full baseline and a common-clock
    trace of `ref-0226`/`ref-0227` show that the repeated legal phrase is already
    replaced by text copied from the configured system prompt in decoder Live
    partials and the final ASR record. Alignment and the business-speaker view
    preserve that text rather than creating it. One reference-free candidate,
    an empty `[asr].system_prompt` with every other value frozen, was tested in
    complete focused context, failed to recover the legal term, introduced new
    material regressions, and was removed before longer gates. See
    `final-asr-prompt-causality-review-2026-08-09.md`.
13. Code and history inspection after that rejection finds an unresolved model-
    integration contract. The original numerical probe validates standalone
    800-mel-frame (eight-second) encoder windows, while the production stream
    appends independently encoded 100-mel-frame (one-second) blocks. The model
    attention window spans eight such convolution chunks. No retained oracle
    evidence currently proves that the one-second block is numerically
    equivalent to its full-window slice. This is a migration question, not an
    ASR product verdict.

## 3. Objective and Claim Boundary

This spec must produce a configuration and exact implementation for which:

- final ASR text preserves conversational meaning over the complete canonical
  `test.mp3` session;
- critical numbers, negations, names, decisions, and commitments are preserved;
- silence and room-tone inputs do not produce substantive transcripts;
- VAD endpoint decisions do not truncate, duplicate, or incorrectly join
  speech in a way that changes business meaning;
- incremental partial, retract, final, typed-track, business-view, persisted,
  exported, and Web UI representations converge;
- changes to ASR or VAD do not silently regress the frozen FR50 speaker-business
  behavior in the final comprehensive view; and
- the accepted path is validated through incremental production WebSocket input
  on the session common time base.

Completion of this spec supplies the ASR, silence, endpoint, live/final, browser,
and microphone evidence required by Spec 013. It cannot close Spec 013 while its
speaker, holdout, report-review, or release gates remain open.

## 4. Definitions

- **Final ASR record**: one immutable typed ASR record with a stable `text_id`,
  absolute common-clock interval, and finalized transcript text.
- **Partial transcript**: a revisable live ASR event for the current `text_id`.
- **Retract event**: removal of a previously exposed partial whose finalized VAD
  evidence does not support publication.
- **Endpoint**: the source-clock boundary at which one decoder session is
  finalized because VAD evidence, the configured trailing interval, the segment
  cap, or end-of-stream permits closure.
- **Substantive hallucination**: transcript content that asserts speech meaning
  when the reviewed input contains no speech. Correctness is decided only by
  contextual review, never by an automated text or count rule.
- **Critical ASR meaning**: a number, percentage, negation, proper name,
  decision, commitment, owner, responsibility, or other content whose change
  alters the business interpretation.
- **Speaker regression guard**: the requirement that an ASR/VAD candidate retain
  the frozen speaker policy and receive complete contextual review of the final
  business view before promotion.

## 5. Functional Requirements

### FR1 - Immutable baseline and provenance

Every baseline or candidate capture MUST record the Git commit, worktree state,
input hash, `orator.toml` hash, model hashes, server-binary hash, resolved
configuration, registry provenance, frame size, pacing, device conditions, and
terminal command timing. Acceptance artifacts MUST be retained under the
gitignored repository `artifacts/spec014/` tree rather than temporary storage.

### FR2 - Current-commit seal

Before ASR behavior changes, commit `1417334` MUST pass a warning-clean build,
the complete registered CTest suite, applicable JavaScript checks, and a
120-second real-WebSocket run with required telemetry and observer behavior.
The 120-second output MUST receive complete chronological and reverse-context
review against every in-scope `test.txt` contribution. Any unexpected speaker
or product difference stops ASR work until reconciled.

### FR3 - Silence and non-speech behavior

The exact candidate MUST be exercised by three independent real-WebSocket runs
of generated digital silence. A later physical-microphone gate MUST include
reviewed room tone. Automation may capture raw events, tracks, audio extents,
and hashes; only contextual review may decide whether any emitted content is a
substantive hallucination. The Spec 013 gate remains zero substantive final
transcripts in each accepted silence run.

### FR4 - Canonical full-session ASR baseline

The canonical baseline MUST use `test/data/audio/test.mp3` streamed
incrementally through the production WebSocket at `1.0x` with the checked-in
TOML. Because the prior raw FR50 artifacts are unavailable, at least one new
clean full baseline capture is required before selecting a fix. The reviewer
MUST read every reference contribution and all corresponding raw and final
system evidence in complete conversation, then repeat the review in reverse
fixed-window order and reconcile every disagreement.

The review MUST separately record semantic preservation, critical meaning,
omission, insertion, repetition, cross-speaker joining, endpoint truncation,
and live/final presentation defects. No code may label a row, map correctness,
calculate accuracy, rank a defect, choose a parameter, or issue a verdict.

### FR5 - Endpoint correctness

Finalized ASR intervals MUST remain on the common time base and within received
audio. A candidate MUST preserve speech across short pauses when the context is
one utterance, and MUST finalize across a confirmed endpoint when continued
decoder context would merge independent contributions. Endpoint behavior is
accepted only by full contextual semantic review. Mechanical tests may verify
sample bounds, monotonicity, frontier ordering, and deterministic feed quanta.

### FR6 - Live/final convergence

For each `text_id`, WebSocket partial/retract/final events, typed ASR records,
forced-alignment groups, business-speaker revisions, terminal JSON, persisted
session replay, exported JSON, and Web UI state MUST converge without a stale
partial or duplicate final. Exact ID/schema/state equality is a mechanical
contract. Whether the resulting cut is semantically appropriate remains a
contextual judgment.

### FR7 - Speaker-business preservation

ASR/VAD work MUST NOT change Sortformer, TitaNet, speaker registry, speaker
fusion, or speaker-model parameters during this spec. A code defect that makes
such a change unavoidable requires a separately reviewed amendment to this
spec. Because ASR boundaries feed forced alignment and the derived business
view, every full candidate MUST receive complete contextual speaker review in
addition to raw ASR review. A candidate with a new critical or material speaker
regression is rejected.

### FR8 - Configuration ownership

Every tunable runtime behavior MUST be represented by a typed field resolved in
the order defaults, `orator.toml`, environment, then command line. Candidate
values MUST live in isolated TOML files during screening. Only an accepted
candidate may update the checked-in `orator.toml`. Behavioral values MUST NOT be
hardcoded in commands, source, scripts, or reference-specific rules.

### FR9 - Promotion ladder

Each behavior-changing candidate MUST pass, in order:

1. focused unit and numerical-oracle gates applicable to the changed stage;
2. warning-clean build and complete CTest;
3. three independent silence runs;
4. repeated 120-second real-WebSocket runs;
5. one 360-second real-WebSocket run;
6. one 600-second real-WebSocket run; and
7. full empty-registry A and restarted frozen-registry B runs.

Each product-output gate receives the required complete contextual review before
the next duration is authorized. A rejected candidate is removed or left only
as explicitly inactive evidence; its TOML does not become the baseline.

### FR10 - Physical microphone and browser evidence

The accepted candidate MUST be tested with a physical microphone for silence,
room tone, short responses, continuous speech, pauses, interruption, overlap,
and ordinary background noise. A real browser MUST verify microphone capture,
Live-region replacement, retract/final behavior, end-of-stream convergence,
reconnect, session persistence, reload, and exact export. Desktop and mobile
Chromium evidence is mandatory; Firefox and Safari/WebKit evidence is recorded
when supported by the target environment.

### FR11 - Industrial-readiness handoff

After canonical configuration freeze, the locked holdout defined by Spec 013
MUST be executed before any general industrial-readiness claim. Holdout evidence
is reported separately and never replaces `test.mp3`.

### FR12 - Documentation and release handoff

The accepted commit, configuration, artifacts, hashes, complete manual review,
known limitations, microphone/browser evidence, and holdout status MUST be
recorded in this spec, its tasks, `PROJECT_STATE.md`, and the Spec 013 final
report. No release tag is created until all applicable Spec 013 gates are signed.

### FR13 - Native streaming model-boundary parity

Before a second ASR product candidate is authorized, the project MUST pin the
official Qwen3-ASR reference revision, repair the offline oracle's repository
and model paths, and revalidate every native streaming assumption affected by
the one-second production step. At minimum this covers prompt tokenization,
audio-encoder window locality, accumulated audio ordering, prefix rollback,
greedy decoding, and final-tail handling.

Numerical probes may compare tensors, token IDs, and deterministic decoder
state because those are implementation contracts. They MUST NOT evaluate
transcript meaning, calculate product accuracy, rank candidate output, or issue
an acceptance verdict. Any runtime correction must be reference-free, expose
its tunable value through `orator.toml`, preserve the frozen FR50 speaker line,
and pass FR9 before full-session acceptance.

The first correction, complete-segment full-context Final replacement, is
rejected after complete chronological and reverse review of its focused
context. It removes some provisional prompt-conditioned text but introduces new
critical business assertions. It therefore remains only as explicitly disabled
evidence. The next authorized step is a single-variable causal control using
the model's 800-mel-frame trained attention window while restoring the accepted
streaming Final policy. That control is diagnostic and cannot be promoted until
its own complete contextual review passes.

### FR14 - Official accumulated-audio streaming state

After the empty-prompt, full-context Final, and 800-frame append candidates all
fail focused contextual review, no further encoder-window or prompt candidate is
authorized until the native runtime can exercise the official Qwen3-ASR
streaming state transition. That mode MUST accumulate exact PCM within the
existing VAD-bounded segment, decode every official two-second chunk from all
audio accumulated so far, apply the official first-two-chunk empty-prefix and
five-token rollback policy, and flush an unpadded residual tail.

Activation, chunk duration, unfixed-chunk count, and unfixed-token count MUST be
typed TOML values captured in the resolved configuration. The default remains
the restored KV-append control until the accumulated mode passes its engineering
gate. The mode MUST remain bounded by the existing segment cap and common time
base, MUST NOT inspect reference text or choose between transcript candidates,
and MUST leave VAD, prompt, alignment, Sortformer v2.1, and FR50 fusion fixed.

### FR15 - Bounded accumulated cadence correction

Complete Phase 3E review establishes that the two-second accumulated candidate
can preserve long-segment continuation while still replacing a correct early
Live negation during short-segment Final and regressing a critical question. A
single follow-up MAY set `stream_chunk_ms = 1000` in accumulated mode so a short
segment reaches prefix rollback before Final. This is a cadence hypothesis, not
an accepted accuracy improvement.

The candidate MUST differ from Phase 3E output behavior only by chunk duration.
It MUST retain `stream_unfixed_chunks = 2`, `stream_unfixed_tokens = 5`, the
100-frame dormant KV control value, and every VAD, prompt, segment, model,
alignment, speaker, time-base, and publication value. No sweep or automated
candidate comparison is permitted. Complete forward/reverse contextual review
of the same focused source decides whether longer gates are authorized.

The exact one-second capture passes source, real-time, time-base, telemetry,
observer, and engineering contracts. Complete contextual review rejects it: a
correct Live negation is still replaced by a false Final action, the critical
exclusive-signing term never appears, legal and numeric substitutions remain,
and additional unusable Live text is exposed. The checked-in control MUST return
to `kv_append` with the dormant 2000 ms accumulated value. No duration or
rollback sweep is authorized.

### FR16 - Decoder-state root-cause evidence

After the full-context Final, 800-frame append, two-second accumulated, and
one-second accumulated hypotheses fail complete focused review, the next phase
MUST NOT introduce another product-output candidate. It MAY add opt-in raw
diagnostic capture for the inactive accumulated implementation, limited to each
decode step's common-time-base audio extent, chunk identifier, tokenized input
prefix, rollback boundary, generated continuation, token budget, and Final-tail
state.

The trace MUST be disabled in the checked-in product configuration, bounded by
the existing VAD segment cap, and free of reference text, correctness labels,
candidate ranking, or parameter selection. Review MUST compare the native state
transition with the pinned official source contract and the already captured
conversation. A new runtime candidate is authorized only after a concrete
implementation mismatch or model-state hypothesis is documented in spec,
plan, and tasks; otherwise this branch stops at the restored control.

The checked-in field names are `[asr].stream_state_trace` and
`[asr].stream_state_trace_path`. Enabling the switch MUST require
`accumulated_redecode` and a non-empty path in the same resolved configuration.
The default MUST remain false with an empty path, and inactive execution MUST
not create a trace file or capture additional decode tokens.

The exact Phase 3G trace contains 91 rows across all eight focused VAD segments.
Complete chronological and reverse review establishes that native accumulation,
unfixed-prefix handling, five-token rollback, Final-tail handling, and state
replacement match the pinned official implementation. It also distinguishes
correct Live text overwritten at Final from critical terms that never appear in
any generated state. See
`decoder-state-root-cause-review-2026-08-10.md`.

### FR17 - Official greedy termination candidate

The pinned official vLLM path uses greedy sampling with `temperature = 0.0` and
a maximum token budget. It does not suppress EOS for an initial minimum token
count. The native decoder suppresses both EOS IDs during the first
`[asr].ban_steps` argmax positions, and the checked-in value is `3`. This is the
single concrete decoder-contract mismatch identified by T083.

One candidate MAY reproduce the exact rejected Phase 3F one-second accumulated
state and set `ban_steps = 0`. Relative to that immutable evidence state, no
other output-affecting value may change: mode MUST be `accumulated_redecode`,
chunk duration 1000 ms, two unfixed chunks, five rollback tokens, 32 streaming
tokens, and the same VAD, prompt, segment cap, model, alignment, Sortformer
v2.1, FR50 fusion, time base, and publication policy. Trace MUST remain disabled
for the product run.

The candidate MUST pass focused configuration tests, complete CTest, and a
warning-clean build before it is committed and pushed. It then MUST stream the
identical 102-second source through the real WebSocket. Every Live, Final, and
comprehensive contribution MUST be read in chronological and reverse context
against the complete human reference. Mechanical tools may verify only source,
configuration, timing, time-base, observer, persistence, telemetry, and schema
contracts.

Any new critical assertion, loss of a preserved commitment, unusable endpoint
behavior, or failure to repair the focused critical contexts rejects the
candidate and restores `kv_append`, dormant 2000 ms, and `ban_steps = 3`. It
MUST NOT authorize a second decoder parameter. Only a complete contextual pass
may authorize 360-second, 600-second, or full evidence gates.

## 6. Acceptance Gates

The following are inherited from Spec 013 and are conjunctive:

| Area | Required result |
|---|---|
| Full ASR semantics | At least 90.0% by complete contextual semantic review |
| Fixed 600-second blocks | At least 90.0% ASR semantic preservation in every complete block; final 15.12 seconds reported separately |
| Critical ASR meaning | 100% preservation of critical numbers, negations, names, decisions, and commitments |
| Silence hallucination | Zero substantive final transcripts in each of three independent silence runs |
| Forced alignment | Every final ASR ID represented exactly once; units monotonic, in bounds, and text-reconstructing |
| Common time base | All active tracks reconcile to the exact input sample extent |
| Live/final convergence | Exact ID and content convergence across runtime events, typed tracks, business revisions, persistence, export, and Web UI |
| Real-time behavior | Full stream speed at least `0.98x` at `1.0x` pacing; terminal timeline within 30 seconds after direct `end` |
| Stability | No crash, out-of-memory condition, CUDA error, data race finding, or unbounded backlog |
| Telemetry | Continuous `tegrastats`; required GPU, memory, and power fields in at least 95% of load samples |
| Speaker guard | Frozen speaker policy and no newly accepted contextual regression in the final business view |
| Engineering | Warning-clean build and all registered tests pass |
| Repeatability | Full A and B independently pass every applicable gate |

All semantic percentages, totals, labels, comparisons, and verdicts are derived
and checked manually from complete context. Executable output may report only
mechanical and numerical facts.

## 7. Non-Goals

- Improving or retuning Sortformer, TitaNet, speaker fusion, or registry policy.
- Introducing MOSS, UniSE, another diarizer, or another runtime model.
- Character error rate, word error rate, edit distance, lexical matching, or an
  automated endpoint score as a product acceptance method.
- Reference-specific words, timestamps, speaker names, or rules in runtime code.
- Quantization, performance tuning unrelated to a verified correctness blocker,
  or a broad Web UI redesign unrelated to transcript operation.
- Claiming industrial readiness from `test.mp3` alone.

## 8. Constitution Check

- **Article I**: no new runtime dependency is introduced. Offline capture and
  evidence-display tools remain under `tools/`.
- **Article II**: accuracy has priority; any model-stage change requires its
  trusted numerical oracle before product review.
- **Article III**: VAD, ASR, alignment, and business projection communicate only
  through typed `ComprehensiveTimeline` records on one session time base.
- **Article IV**: every product result comes from incremental production
  WebSocket input and the terminal comprehensive document.
- **Article VI**: only complete contextual semantic review evaluates ASR,
  endpoint, hallucination, speaker attribution, or the final view. Automation
  stops at capture, display, hashes, schemas, timing, and numerical contracts.
- **Article IX**: all tunable behavior is TOML-owned and resolved through the
  required precedence.
- **Articles X-XI**: spec, plan, tasks, code, tests, state, and evidence status
  advance together.
