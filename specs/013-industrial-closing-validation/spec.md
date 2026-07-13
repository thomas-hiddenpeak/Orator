# Spec 013: Industrial Closing Validation

**Status**: In progress - approved 2026-07-13
**Created**: 2026-07-13
**Scope**: Re-establish a truthful product baseline, recover full-session business
accuracy, and define the evidence required before Orator may be declared closed.

## 1. Objective

Orator is not currently closed. Previous work established model-stage numerical
fidelity, real-time full-session stability, forced-alignment coverage, and useful
speaker-evidence diagnostics. It did not establish a repeatable, full-session
business accuracy result of at least 90 percent.

This spec defines two separate claims:

1. **Canonical-scene acceptance**: the complete product passes the required
   full-length evaluation on `test/data/audio/test.mp3` against
   `test/data/reference/test.txt`.
2. **Industrial readiness**: the same accepted configuration also passes a
   locked, independently recorded holdout set that was not used for tuning.

Passing the canonical scene is mandatory and cannot be replaced by a simpler
recording. It is necessary, but by itself it is not sufficient evidence for a
general industrial-readiness claim.

## 2. Definitions

- **Final business view**: the terminal user-facing track that states who said
  what and when. Raw ASR, diarization, voiceprint, voice activity detection
  (VAD), and forced-alignment tracks are evidence, not the acceptance target.
- **Reference turn ledger**: an immutable, manually adjudicated list covering
  every one of the 556 timestamped reference turns. Each row records the real
  speaker, audible start/end, overlap, business meaning, criticality, and any
  uncertainty in the reference.
- **Natural business turn**: one speaker's contextually complete contribution,
  including short replies and interruptions when they change or confirm a
  business position.
- **Critical turn**: a turn containing a decision, commitment, number,
  percentage, negation, ownership position, responsibility, or other content
  whose speaker identity changes the business interpretation.
- **Correct speaker time**: reference speech duration attributed to the correct
  real speaker in the final business view. Overlapping speakers are represented
  as separate reference intervals.
- **Uncertain output**: an output marked `unknown`, `speaker_uncertain`, or an
  equivalent state. It is safer than a confident wrong attribution, but it does
  not count as correct for the 90 percent gate.

## 3. Verified Baseline Defects

The following code-level defects block closure independently of model accuracy:

1. `AuditoryStream::common_time_base()` and
   `PipelineAudioCache::time_base()` construct `TimeBase(sample_rate, 0)` values
   on demand. Production does not obtain one session clock from the configured
   common-clock owner required by Constitution Article III.
2. ASR reads VAD through a shared `VadCache`, and forced alignment receives ASR
   finals through `ProtocolTimeline` subscriptions. These are pipeline data
   paths outside `ComprehensiveTimeline`.
3. `ComprehensiveTimeline` currently performs speaker selection, gap filling,
   and text projection. The Constitution defines it as a pure container and
   alignment layer that does not infer or back-fill pipeline content.
4. `ORATOR_TIMELINE_NO_GAPFILL` and several model/debug environment switches
   bypass typed TOML configuration. `ws_main.cc` applies CLI values before TOML,
   while Article IX requires CLI to be the final override.
5. A finalized ASR event increments `inc_text_id_` before emitting its live
   message, so the live final ID can differ from the timeline/align ID. The
   serialized ASR track also omits `text_id`.
6. The configured CTest suite contains 47 C++ tests. CMake defines a Python test
   registration helper, but no Python WebSocket tests are registered and the
   documented wrapper directory is absent.
7. `test_diar_stream` verifies a short stored NeMo fixture using lower-level
   defaults. It does not establish numerical parity for the accepted full
   runtime TOML profile over the complete session.
8. Previous full-length reviews used coarse 10-minute summaries and selected
   regression windows. They did not adjudicate and sign off every reference
   turn, so they cannot supply the denominator for a 90 percent claim.

## 4. Requirements

### 4.1 Governance and reproducibility

- **FR1**: The project state must explicitly say that closure is open until all
  gates in this spec pass in one release candidate.
- **FR2**: Every run must record commit ID, dirty-worktree state, `orator.toml`
  hash, resolved configuration, model/data hashes, device identity, power mode,
  clock state, speaker-registry fixture hash, test-client version, and output
  artifact hash.
- **FR3**: Runtime tuning may be changed only in `orator.toml`. Test-client
  duration, pacing, and output path are test-harness controls, not runtime model
  parameters.
- **FR4**: A result obtained with an unrecorded environment or CLI override is
  invalid for acceptance.

### 4.2 Architecture contracts

- **FR5**: One immutable session time base must be created once by the audio
  ingest owner and passed to every pipeline and audio store. No pipeline or
  cache may construct an independent origin.
- **FR6**: Pipelines may consume another pipeline's evidence only through typed
  tracks in `ComprehensiveTimeline`. The protocol layer may serialize, retain,
  and transport track updates, but it may not be the private pipeline data bus.
- **FR7**: Evidence combination must be an explicit registered fusion pipeline
  that writes its own revisable `business_speaker` track. Raw tracks remain
  immutable. `ComprehensiveTimeline` remains a container/alignment layer.
- **FR8**: ASR final, forced-alignment group, revision, terminal ASR track, final
  business view, and Web UI state must use the same stable `text_id`.

### 4.3 Reference and evaluation

- **FR9**: The reference turn ledger must cover all 556 timestamped turns. No
  sampling, selected-window substitution, or script-inferred correctness is
  permitted.
- **FR10**: Accuracy judgments must be made item by item in conversational
  context. Tools may capture, index, and present evidence, but may not assign
  correctness labels or select the winning configuration.
- **FR11**: Every reported percentage must be reproducible from signed ledger
  rows. Boundary offsets and overlapping speech must remain visible.
- **FR12**: Diagnostic model metrics, including local-slot mapping scores,
  diarization error rate, character error rate, unknown duration, and embedding
  similarity, must not replace the final business-view gates.

### 4.4 Model and pipeline validation

- **FR13**: Every model stage affected by a candidate must pass its trusted
  oracle gate. The exact accepted Sortformer runtime profile must be compared
  with NeMo; TitaNet and ASR/align changes must be compared with their PyTorch or
  NeMo references.
- **FR14**: Before another full model run, frozen evidence must be used to
  measure the best attainable speaker decision from independent Sortformer,
  TitaNet, VAD, ASR, and forced-alignment evidence. Reference labels may score a
  candidate but may never enter runtime decisions.
- **FR15**: If the frozen-evidence development candidate cannot reach 93 percent
  on both speaker-time and natural-turn measures, parameter tuning stops and the
  next action is model augmentation or replacement validated offline first.
- **FR16**: Silence, endpoint, and hallucination behavior must be tested through
  the real WebSocket path. No substantive final ASR text may be emitted for
  confirmed silence.

### 4.5 Product surface and operations

- **FR17**: Web UI live rows must converge exactly to the terminal business view
  without duplicate IDs, stale partials, missing revisions, or browser-side
  speaker re-splitting.
- **FR18**: Full tests must capture `tegrastats` continuously and report CPU,
  RAM, GPU utilization, GPU memory, temperature, rail/system power, and thermal
  or allocation failures. GPU utilization must be present during GPU load.
- **FR19**: Closure validation must use the real incremental WebSocket path at
  the constitutional 120 s, 360 s, 600 s, and full-length levels.
- **FR20**: Final acceptance requires two independent full-length runs: one with
  an empty isolated speaker registry and one after process restart with the
  frozen enrolled-registry fixture.

## 5. Acceptance Gates

All gates are conjunctive. A single failure keeps the spec open.

| Area | Required result |
|---|---|
| Full speaker-time accuracy | At least 90.0% on the final business view |
| Full natural-turn speaker accuracy | At least 90.0% over all ledger turns |
| Fixed 600 s speaker blocks | At least 90.0% in every block; final 15.12 s reported separately |
| Per-speaker recall | At least 90.0% for each real speaker by both time and turn count |
| Critical speaker turns | 100% correct; uncertain and missing count as failures |
| Confident wrong attribution | 0 critical turns and at most 2.0% of all turns |
| Boundary offsets | Median absolute offset <= 0.25 s; 95th percentile <= 0.80 s; every offset > 1.0 s annotated |
| ASR semantic accuracy | At least 90.0% full-length and in every fixed 600 s block |
| Critical ASR meaning | 100% preservation of critical numbers, negations, names, and decisions after allowed semantic equivalence |
| Silence hallucination | 0 substantive final transcripts in each of three independent silence runs |
| Forced alignment | 100% of final ASR IDs aligned; no missing/extra IDs; units monotonic, in bounds, and text-reconstructing |
| Time base | One source clock; all track extents reconcile exactly to the common sample count |
| Live/final identity | Exact `text_id` and content convergence across WS events, tracks, revisions, and Web UI |
| Real-time behavior | Full stream speed >= 0.98x at 1.0x input pacing; terminal timeline <= 30 s after end-of-stream |
| Stability | No crash, out-of-memory, CUDA error, data race finding, or unbounded backlog |
| Telemetry | Continuous `tegrastats`; required GPU/memory/power fields present for at least 95% of load samples |
| Engineering | Clean build with no warnings; all C++ and registered real-WebSocket integration tests pass |
| Repeatability | Both required full runs independently pass every applicable gate |
| Documentation | Spec/tasks/project state/final report match the accepted code and artifacts |

The 93 percent frozen-evidence target is a development margin. The product gate
remains 90 percent on the real runtime output.

## 6. Industrial-Readiness Evidence

Constitution Article VI currently permits only `test.mp3` for non-unit pipeline
tests. Before a general industrial-readiness claim, a separately reviewed
Constitution amendment must retain `test.mp3` as the mandatory canonical gate
while permitting additional safety and locked holdout recordings.

After that amendment, industrial readiness additionally requires:

- at least five independent sessions and three total hours not used for tuning;
- at least eight distinct speakers across two-, three-, and four-speaker sessions;
- microphone, far-field, background-noise, interruption, overlap, and silence
  conditions;
- the same 90 percent full business-view gates, with no session below 90 percent;
- no critical wrong attribution and no silence hallucination.

Without this holdout evidence, the allowed statement is only: "accepted on the
Orator canonical full-session scene." It must not be generalized to all
industrial audio.

## 7. Non-Goals

- Previous numerical-oracle and stability evidence is not discarded; it remains
  component evidence and must be rechecked where the accepted candidate changes
  behavior.
- Script-derived rankings do not choose model parameters.
- Text content, speaker identities, or reference-specific lexical rules are not
  hardcoded for `test.mp3`.
- Accuracy is not improved by converting wrong attribution to `unknown`; that
  only changes the safety profile and still receives no accuracy credit.

## 8. Constitution Check

- **Article I**: no new runtime dependency is permitted.
- **Article II**: model changes require stage-level trusted-oracle comparison;
  the 90 percent business gate is additional, not a replacement.
- **Article III**: current time-base ownership, direct pipeline subscriptions,
  and inference inside `ComprehensiveTimeline` must be corrected before
  acceptance.
- **Article IV**: every acceptance result comes from incremental WebSocket input
  and the terminal comprehensive document.
- **Article VI**: full item-by-item contextual review is mandatory. Additional
  industrial holdouts require the explicit amendment described in Section 6.
- **Article IX**: runtime parameter changes are TOML-only, and the implementation
  loading order must be corrected to defaults, TOML, environment, then CLI.
- **Articles X-XI**: this spec, its plan/tasks, code, tests, and project state must
  advance together with evidence.
