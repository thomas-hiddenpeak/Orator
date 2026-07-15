# Spec 013: Industrial Closing Validation

**Status**: In progress - v2.1 closing baseline selected 2026-07-15
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

### 1.1 Closing baseline decision

All remaining work in this spec uses streaming Sortformer v2.1 with the
checked-in `340/1/188/188` profile (chunk/right-context/FIFO/cache-update) as
the sole closing baseline. The v2 checkpoint and its CTest gate have been
removed; only prior reports and hashes remain as historical evidence. A v2
artifact cannot be selected for a new candidate, used for an acceptance run, or
satisfy any Phase 3-7 gate. This decision fixes the model line on which accuracy
recovery and formal validation continue; it does not accept the current v2.1
output.

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
- **Closing baseline**: the exact v2.1 model file and `340/1/188/188` streaming
  profile resolved from the checked-in `orator.toml`. Experiments use separate
  TOML files and do not change the closing baseline until they pass every
  promotion gate.

## 3. Verified Baseline Defects and Remediation Status

The following code-level defects were verified when this spec was approved.
Items marked resolved have implementation and focused-test evidence but remain
subject to the complete acceptance gates in this spec.

1. **Resolved in Phase 1**: `AuditoryStream` now owns one immutable `TimeBase`
   and injects it into private caches, workers, and retained audio stores. At
   finalization it reports and verifies exact sample extents for every active
   input, evidence, identity, alignment, and business-speaker track.
2. **Resolved in Phase 1**: ASR reads an immutable typed VAD snapshot from
   `ComprehensiveTimeline`, while forced alignment receives finalized typed ASR
   records through a `ComprehensiveTimeline` subscription. Protocol messages
   are emitted only after typed track commit.
3. **Resolved in Phase 1**: `ComprehensiveTimeline` is now a pure typed store.
   A registered `business_speaker` pipeline owns speaker selection, gap policy,
   align-aware text projection, and support diagnostics, and writes only its
   own revisable track. Raw ASR and alignment records reject conflicting
   same-ID deposits.
4. **Resolved for the runtime contract**: timeline gap-fill and every retained
   model/GPU/transport behavior are typed fields; startup applies defaults,
   TOML, environment, then CLI. Model, GPU, and transport layers no longer read
   process environment, and terminal packages carry the resolved configuration.
5. **Resolved in Phase 1**: finalized ASR allocates one `text_id` and reuses it
   for the typed sink and live event; the terminal ASR track serializes that ID.
   The registered real-WebSocket and Node gates plus a real Chromium run verify
   partial/final/retract, alignment, business revision, terminal track, exact
   export/load, and reconnect convergence. Full-session repeatability remains
   open.
6. **Expanded 2026-07-15**: the configured CTest suite contains 64 tests: 55
   C++ tests, eight Python unit/integration gates, and one dependency-free Node
   browser-model gate. The complete suite passes, including the real-WebSocket
   contract, official v2.1 numerical profiles, short-block GEMM, and multi-scale
   voiceprint evidence. The browser acceptance tool remains manual because
   Playwright is a tools-only dependency.
7. **Historical defect-isolation evidence only**: a pinned NVIDIA v2 NeMo oracle
   crosses five runtime chunks and repeated FIFO/cache updates. The
   regenerated 1502-frame fixture matches C++ at `max_abs=1.43051e-6` and
   1502/1502 argmax. This proved the FIFO implementation correction but cannot
   satisfy a current model or acceptance gate. Its model file and obsolete
   CTest were removed after v2.1 became the sole closing baseline.
8. **v2.1 selected as the sole closing baseline; product gate still open**: the
   compile-time default and checked-in TOML select streaming v2.1. Its inherited
   asynchronous profile passed the exact multi-chunk NeMo gate. An earlier
   936-entry full real-WebSocket artifact recorded a cut-oriented written-context
   diagnostic of 413 / 142 / 1 natural turns. NVIDIA's official high- and
   low-latency profiles also passed separate numerical gates, but historical
   full contextual diagnostics recorded only 385 / 170 / 1 and 377 / 178 / 1.
   Neither official profile proceeds to transport acceptance. The inherited
   v2.1 profile is the fixed starting point for closing work and remains below
   the 90 percent product gate.
9. Previous full-length reviews used coarse 10-minute summaries and selected
   regression windows. They did not adjudicate and sign off every reference
   turn, so they cannot supply the denominator for a 90 percent claim.
10. **Full closing-baseline system capture complete; accuracy review open**:
    clean commit `3b40245` completed the 3615.12-second v2.1 stream in 3616.442
    seconds with exact seven-pipeline extents, zero mechanical issues,
    continuous telemetry, and exact producer/persisted/Web UI/download terminal
    equality. This closes the capture requirement in T044. It does not close
    T031 or T045 because all 556 audible ledger rows remain unsigned. See
    `closing-baseline-v21-2026-07-15.md`.
11. **Clean closing-baseline written-context review complete; audible review
    open**: the exact 935-entry clean package received a full chronological
    manual pass and a reverse fixed-block manual pass. The reconciled result is
    443 correct / 112 incorrect / 1 ambiguous (`79.6763%`). Tools only arranged
    evidence and did not assign correctness. The result fails the full-session
    gate and five fixed 600-second block gates. Exact audible boundaries,
    speaker-time, offsets, criticality, and independent totals remain unsigned,
    so this is not a constitutional closing score. See
    `closing-baseline-v21-context-review-2026-07-15.md`.

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
- **FR21**: Every `business_speaker` entry must carry a reference-free,
  structured speaker-decision audit. The audit records the speaker evidence
  source, text-projection source, decision reason, selected candidate, every
  rejected overlapping candidate, candidate overlap/coverage/confidence, and
  selected-versus-best-alternative overlap and confidence margins. Live
  revisions, terminal tracks, the compatibility alias, and Web UI state must
  preserve the same audit object. Adding or revising audit evidence must not
  change a raw track or silently change the selected speaker.
- **FR22**: Frozen legacy timeline packages that predate FR21 may reconstruct
  the same audit object from their immutable diarization and business tracks.
  The reconstruction tool must not read reference annotations, alter an input
  track, or assign correctness. Discrete fields must match exactly. Legacy
  three-decimal confidence and millisecond timeline fields may use only their
  declared quantization envelopes; any runtime value outside those bounds must
  fail. New artifacts must retain round-trip raw confidence precision, while
  millisecond time-derived fields remain explicitly bounded.

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
  oracle gate. The exact accepted Sortformer model file and runtime profile must
  be compared with NeMo; the source checkpoint revision, source and converted
  model hashes, TOML hash, and oracle-fixture hash are part of that gate.
  TitaNet and ASR/align changes must be compared with their PyTorch or NeMo
  references. An asynchronous Sortformer profile must exercise at least three
  chunks, include output produced after cache compression, and compare FIFO
  inclusion, overflow transfer, cache compression, and frame probabilities. A
  synchronous `fifo_len = 0` fixture or a different Sortformer checkpoint does
  not validate an asynchronous runtime profile. Every Phase 3-7 closing run
  must resolve the v2.1 model and `340/1/188/188` baseline from TOML; a v2 run
  is historical evidence and is ineligible for promotion.
- **FR14**: Before another full model run, frozen evidence must be used to
  measure the best attainable speaker decision from independent Sortformer,
  TitaNet, VAD, ASR, and forced-alignment evidence. Reference labels may score a
  candidate but may never enter runtime decisions.
- **FR15**: If the frozen-evidence development candidate cannot reach 93 percent
  on both speaker-time and natural-turn measures, parameter tuning stops. Every
  locally available, deployable streaming model revision must be evaluated under
  the same runtime profile. Offline-only diarization models are excluded from
  candidate selection because the manually adjudicated `test.txt` already
  supplies business truth and an offline result cannot become the deployed
  runtime. This does not remove the same-checkpoint NeMo numerical oracle
  required by FR13.
- **FR16**: Silence, endpoint, and hallucination behavior must be tested through
  the real WebSocket path. No substantive final ASR text may be emitted for
  confirmed silence.

### 4.5 Product surface and operations

- **FR17**: Web UI live rows must converge exactly to the terminal business view
  without duplicate IDs, stale partials, missing revisions, or browser-side
  speaker re-splitting. One connection owns audio production for a session;
  browser and diagnostic observer connections receive the same live events and
  terminal timeline without resetting, replacing, or otherwise mutating that
  session. Connecting or disconnecting an observer must not change the common
  time base, producer ownership, track contents, or terminal result.
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

Constitution 1.6.0 retains `test.mp3` as the mandatory canonical gate and permits
additional safety and locked holdout recordings. Supplemental recordings never
replace the canonical result and must follow the provenance requirements in
Article VI.

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
- **Article VI**: full item-by-item contextual review is mandatory. Supplemental
  safety and locked holdout recordings follow the 1.6.0 provenance rules and do
  not replace `test.mp3`.
- **Article IX**: runtime parameter changes are TOML-only, and the implementation
  loading order must be corrected to defaults, TOML, environment, then CLI.
- **Articles X-XI**: this spec, its plan/tasks, code, tests, and project state must
  advance together with evidence.
