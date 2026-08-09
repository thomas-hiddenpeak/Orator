# Spec 014: Ordered Tasks

`[ ]` pending, `[~]` in progress, `[x]` complete. A task is complete only when
its named evidence exists. Mechanical checks never assign product correctness.

## Phase 0: Governance and Evidence Inventory

- [x] **T001** Read Constitution v1.7.0, the test-review protocol, the
  model-validation skill, Spec 013 gates, and current `PROJECT_STATE.md`.
- [x] **T002** Record owner approval on 2026-08-09 to freeze speaker behavior and
  proceed with ASR/VAD, endpoint, microphone, and Web UI closing work.
- [x] **T003** Verify clean `master` at `1417334`, synchronized with
  `origin/master`; identify FR50 `a6f0d33` as the frozen speaker behavior.
- [x] **T004** Search for the historical FR50 raw A/B artifacts. Record that the
  documented `/tmp/orator-spec013/release-a6f0d33-fr50-precompute/` tree is no
  longer present and no alternate raw full JSON was found under `/home/rm01`.
- [x] **T005** Create and review `spec.md`, `plan.md`, and `tasks.md`; cross-link
  Spec 013 and synchronize `PROJECT_STATE.md` without advancing a product gate.

## Phase 1: Current-Commit Seal

- [x] **T010** Configure and build current `master`; retain the complete build
  log and confirm no new `warning:` or `error:` diagnostics. Clean build at
  `41c8999` has no warning/error diagnostic.
- [x] **T011** Run the complete registered CTest suite and retain its output.
  All `74/74` tests pass in `53.08 s`.
- [x] **T012** Run the existing JavaScript/Web UI checks and retain their output.
  Registered `test_web_model` passes as part of the complete suite.
- [x] **T013** Create `artifacts/spec014/baseline-1417334/` and record source,
  input, config, model, binary, device, and registry provenance. Persistent
  manifests bind every required input and the clean worktree.
- [x] **T014** Run two independent 120-second, `1.0x`, 100 ms frame,
  real-WebSocket captures with required observers and telemetry using only the
  checked-in TOML. Private copies differ only in ports and isolated paths; both
  runs pass mechanical contracts and have byte-identical normalized product
  tracks.
- [x] **T015** Read every in-scope `test.txt` contribution and complete runtime
  evidence for both 120-second runs chronologically and in reverse context.
  Reconcile all ASR, endpoint, Live/Final, and speaker observations manually.
  The readings preserve the FR50 speaker boundary, expose two critical
  negation/relationship failures, and confirm duplicate unchanged Live partials.
- [x] **T016** Write the current-commit seal report. Stop if current HEAD cannot
  preserve the documented FR50 behavior boundary. See
  `current-commit-seal-2026-08-09.md`; Phase 2 is authorized without advancing
  an ASR or speaker product gate.

## Phase 2: Silence and Full Baseline

- [x] **T020** Run three independent 30-second digital-silence sessions through
  the production WebSocket and preserve all raw events, tracks, logs, manifests,
  and telemetry. Runs A/B/C use separate clean processes, isolated storage and
  registries, `1.0x` pacing, observers, direct `end`, and persistent artifacts.
- [x] **T021** Review every silence event and terminal document directly; record
  the hallucination conclusion without using an automated label, count, or
  verdict. All three independent readings find no speech assertion or
  substantive live/final transcript. See
  `digital-silence-review-2026-08-09.md`; microphone room tone remains open.
- [x] **T022** Run one clean full-length `test.mp3` baseline at `1.0x`, direct
  `end`, empty isolated registry, and required telemetry; retain it under
  `artifacts/spec014/baseline-1417334/full-a/`. The 3615.120-second run at
  clean `96b8347` completed at `0.993x` with 26.310-second direct-end latency.
- [x] **T023** Verify only mechanical contracts: hashes, resolved config,
  transport completion, exact sample extents, typed IDs, alignment coverage,
  observer convergence, terminal timing, telemetry coverage, and stability.
  All named contracts pass; this does not assign product correctness.
- [x] **T024** Complete the full chronological contextual review of all 556
  `test.txt` contributions against raw ASR, VAD, align, business, and event
  evidence. The complete source-ordered worksheet and reviewer notes are
  retained with the immutable run.
- [x] **T025** Repeat the complete review in reverse fixed-window order,
  reconcile every disagreement, and manually derive/check the ASR semantic,
  critical-meaning, endpoint, hallucination, repetition, omission, and speaker
  conclusions. Every one of the 556 contributions was reread; no executable
  result supplied a label, total, percentage, comparison, or verdict.
- [x] **T026** Write the signed baseline report and freeze the exact first defect
  class plus accepted control contexts before any behavior change. See
  `full-baseline-context-review-2026-08-09.md`; unchanged partial WebSocket
  publication is first, and final ASR meaning follows as an independent class.

## Phase 3: Evidence-Driven ASR/VAD Corrections

- [x] **T030** Trace the selected defect through publication, typed VAD
  frontiers, admitted PCM samples, decoder-session boundaries, forced alignment,
  business projection, and Web UI state on the common sample clock. The direct
  `emit_` path sits outside the existing `inc_delivered_text_` state-change
  guard; typed partials already suppress the unchanged decoder text.
- [x] **T031** Specify one reference-free correction and its abstention/control
  boundary. Choose code correction or one-variable isolated TOML candidate; do
  not change speaker/diarizer behavior. One direct partial event is permitted
  per distinct non-empty text state for the active `text_id`; final and retract
  publication remain independent. This is a code defect and needs no TOML
  parameter.
- [x] **T032** Add or strengthen focused engineering tests for the root cause.
  If model values can change, run and record the trusted numerical oracle.
  `test_asr_worker` covers unchanged typed/direct publication, direct-only
  publication, changed-text order, final IDs, VAD order, silence, gaps, and
  terminal drain. No model value changed, so no numerical oracle applies.
- [x] **T033** Implement the smallest correction. Keep all tunable behavior in
  typed TOML and leave the checked-in TOML unchanged until promotion. Typed and
  direct partial sinks now share one `partial_changed` branch; no parameter or
  final/retract/model/endpoint path changed.
- [x] **T034** Pass warning-clean build, complete CTest, and applicable UI tests.
  The complete build is warning-clean and all `74/74` tests pass, including the
  registered JavaScript and real-WebSocket tests.
- [x] **T035** Pass three independent silence sessions by direct contextual
  review. Each complete event stream and terminal document contains no speech
  assertion or substantive live/final transcript.
- [x] **T036** Pass two 120-second real-WebSocket captures and complete
  forward/reverse contextual review. Both remove unchanged Live repeats, retain
  exact canonical final product tracks, and preserve all prior ASR and speaker
  judgments. See `live-partial-publication-review-2026-08-09.md`.
- [x] **T037** Pass one 360-second real-WebSocket capture and complete
  forward/reverse contextual review. Clean `a1c8d1d` runs at `0.993x`, has no
  unchanged partial publication or mechanical issue, and preserves the prior
  ASR/speaker interpretation across all 39 in-scope contributions.
- [x] **T038** Pass one 600-second real-WebSocket capture and complete
  forward/reverse contextual review. All 93 contributions were read in both
  directions; publication passes while ASR remains approximately 80-89% with
  the signed critical residuals unchanged. See
  `live-partial-publication-review-2026-08-09.md`.
- [ ] **T039** Remove or explicitly archive a rejected candidate before starting
  another hypothesis. After three failed implementations, restore the accepted
  baseline and revise the root-cause analysis.

### Phase 3B: Final ASR Meaning

- [x] **T045** Freeze the accepted Live-publication correction as the control and
  re-read the immutable full-baseline critical contexts with their complete
  ASR, VAD, forced-alignment, and final speaker evidence. Select one causal
  defect class by direct contextual review; no script, metric, or query may
  label, rank, or select the work. Complete direct forward/reverse rereading
  selects bounded system-prompt-conditioned lexical substitution; long sessions
  and tail drift are rejected as a common explanation.
- [x] **T046** Trace the selected defect through admitted PCM, typed VAD
  frontiers, decoder-session boundaries, final ASR records, alignment units,
  and comprehensive revisions on the common sample clock. Distinguish endpoint
  ownership from decoder semantic loss before changing behavior. `text_id=133`
  exposes the prompt phrase in Live output before finalization; downstream
  alignment and business projection preserve it. See
  `final-asr-prompt-causality-review-2026-08-09.md`.
- [x] **T047** Specify one reference-free correction and explicit abstention and
  control contexts. Keep every tunable value in `orator.toml`, preserve the
  frozen Sortformer v2.1/FR50 behavior, and add focused tests. Run the trusted
  numerical oracle before product review if any model-stage behavior changes.
  Candidate `asr-empty-system-prompt` changes only the checked-in TOML value.
  Its config-contract test passes, the warning-clean build completes, and all
  `74/74` registered tests pass in `52.74 s`. Complete forward/reverse review of
  the clean 102-second focused legal context rejects it: neither repeated legal
  term recovers, while a name and neighboring option-pool discussion regress.
  The candidate is removed and the pre-candidate prompt restored before T048;
  the restored tree is warning-clean and all `74/74` tests pass in `52.79 s`.
- [ ] **T048** Pass warning-clean build, complete CTest, three independent
  silence reviews, and two independent 120-second real-WebSocket contextual
  reviews. Remove or archive the candidate if its controls fail.
- [ ] **T049** Only after T048 passes, run 360-second and 600-second real-
  WebSocket gates with complete chronological and reverse contextual review of
  ASR meaning, endpoint behavior, and final speaker ownership. Do not authorize
  a full run or Phase 5 from mechanical evidence.

### Phase 3C: Native Streaming Boundary Revalidation

- [x] **T061** Pin the official Qwen3-ASR reference revision and repair the
  offline oracle's stale repository, model, audio, and artifact paths. Add
  mechanical tests for path resolution and provenance. The oracle may emit raw
  tensors, token IDs, and transcripts for reviewer inspection; it may not score,
  compare, rank, or label transcript correctness. The repaired check resolves
  official commit `7c6daf7`, the project model, canonical audio, and TOML; its
  new standard-library contract test passes. A new official GPU forward remains
  unavailable because both local Python tool environments contain CPU Torch.
- [x] **T062** Extend and run the encoder locality probe with an eight-second
  known control and the production one-second append unit, both sliced from the
  same full mel and full windowed encode. Record numerical tensor evidence and
  inspect prompt, rollback, decode, and tail behavior against the pinned
  official source. This task may identify an implementation mismatch but may
  not make a product-accuracy claim. The 800-frame control matches exactly;
  every tested 100-frame slice differs from the same full encode, with maximum
  absolute difference `0.1759`. Source inspection identifies accumulated-audio
  re-encoding versus frozen one-second appends as the demonstrated mismatch.
  See `streaming-encoder-boundary-review-2026-08-09.md`.
- [x] **T063** If T062 demonstrates a concrete mismatch, specify and implement
  one reference-free, TOML-owned correction while freezing VAD, prompt,
  alignment, Sortformer v2.1, FR50 fusion, and the common time base. Pass the
  applicable numerical oracle, warning-clean build, and complete CTest before
  producing product output. Candidate `asr-final-full-context-decode` retains
  exact segment PCM, leaves one-second Live provisional, and makes a TOML-owned
  complete-context decode authoritative only at Final. Focused tests prove the
  enabled and disabled paths, exact PCM, Final replacement, and empty-Final
  retraction. The full build is warning-clean and all `75/75` CTest entries pass
  in `53.95` seconds.
- [ ] **T064** Run one focused real-WebSocket context plus explicit neighboring
  controls and complete chronological and reverse semantic review against the
  human-listened reference. Reject and remove the candidate on a new critical
  regression; otherwise return to T048 and the duration ladder.

## Phase 4: Web UI and Physical Microphone

- [x] **T040** Validate file-input Live partial/retract/final replacement,
  terminal convergence, reconnect, persistence, reload, and export in real
  desktop and mobile Chromium using Playwright screenshots and DOM evidence.
  The first clean 12-second run passes Live population, terminal convergence,
  exact export, and screenshots, then fails persisted reload. Evidence proves
  an empty `Clear` reset can overwrite the just-finalized document because
  resets save empty sessions and IDs have only second-level time resolution.
  Clean commit `b0eadbe` then passes terminal/load/export/reconnect mechanics
  at 120 seconds, but the terminal `wall_clock_ok=false`: relative 60 ms browser
  timers accumulate event-loop delay and make the path approximately 123.121
  seconds. The absolute-deadline candidate retains 60 ms frames; all nine Web
  model tests and `74/74` CTest entries pass. Clean `d09b13b` then passes the
  complete 120-second browser flow from empty isolated storage. Independent
  timing records 120.322 seconds from selection through decode and source
  completion, 0.477 seconds for automatic Flush, and 2.165 seconds from Flush
  to terminal End. Exact export/load/reconnect and desktop/mobile review pass.
- [x] **T041** Review the Live-region segmentation and final comprehensive view
  in conversational context; automation must not decide endpoint correctness.
  The directly read 12-second opening preserves its reference meaning in two
  readable final rows without a stale draft or duplicate final. The known FR50
  cold-start speaker split remains visible. The clean 120-second forward and
  reverse readings preserve the previously signed 18-contribution result and
  expose no browser-only cut, stale draft, duplicate final, or attribution
  change. See
  `browser-persistence-review-2026-08-09.md`.
- [ ] **T042** Run physical-microphone sessions covering silence, room tone,
  short speech, continuous speech, pauses, interruption, overlap, and ordinary
  background noise. The available Jetson APE analog source has completed direct
  capture probes and one real-Chromium 30-second room-tone session at clean
  documentation commit `d2ac5d1`. No USB source is present, and controlled
  playback does not reach the capture endpoint as sustained speech. Short
  speech, continuous speech, pauses, interruption, overlap, and voiced
  background-noise cases therefore remain open; fake-device evidence cannot
  complete this task.
- [ ] **T043** Complete contextual review of microphone ASR, VAD endpoint,
  hallucination, speaker, and Live/Final behavior; record hardware/browser
  provenance. All 89 room-tone WebSocket log lines and the terminal/browser
  state were read chronologically and in reverse. The reviewer finds no
  substantive speech assertion in the no-deliberate-speech context, but no
  spoken reference exists and active-speech endpoint/ASR/speaker behavior is
  not evaluated. See `physical-microphone-review-2026-08-09.md`.
- [x] **T044** Record Firefox and Safari/WebKit behavior when available; document
  an explicit environment limitation when unavailable. Chromium 148 is the only
  executable browser available. Ubuntu's Firefox command is an uninstalled-snap
  launcher, Playwright has no Firefox/WebKit bundle, the installed WebKitGTK
  library has no browser or driver, and Safari is unavailable on Linux. No
  unsupported compatibility result is claimed.

## Phase 5: Full Candidate Acceptance

- [ ] **T050** Freeze the candidate commit and checked-in TOML only after Phase 3
  and Phase 4 pass; record all immutable hashes.
- [ ] **T051** Run full A with an empty isolated registry at `1.0x`, direct
  `end`, observers, continuous `tegrastats`, and required telemetry.
- [ ] **T052** Complete all 556 contributions chronologically and in reverse
  windows for ASR meaning, endpointing, and final speaker ownership; manually
  reconcile and verify all reported results.
- [ ] **T053** Restart the process and run full B with only Run A's frozen
  registry, then repeat the complete dual review independently.
- [ ] **T054** Verify the Spec 014 acceptance table and applicable Spec 013 gates
  manually, while recording mechanical and numerical evidence separately.
- [ ] **T055** Execute the locked holdout only after its provenance and reference
  construction are frozen; report it separately from `test.mp3`.
- [ ] **T056** Write the final Spec 014 report, update Spec 013 T084-T086 and
  `PROJECT_STATE.md`, obtain report review, and create a release tag only when
  every applicable Spec 013 gate is closed.

## Deferred Speaker Reopening Condition

- [ ] **T060** Reopen speaker optimization only when new independently useful
  business data or a deployable orthogonal signal such as MOSS/TSE exists. That
  work requires a separate SDD package and cannot be mixed into Spec 014.
