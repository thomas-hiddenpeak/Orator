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

## Phase 4: Web UI and Physical Microphone

- [~] **T040** Validate file-input Live partial/retract/final replacement,
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
  model tests and `74/74` CTest entries pass. Repeat from the exact clean commit
  and empty isolated storage before completing this task.
- [~] **T041** Review the Live-region segmentation and final comprehensive view
  in conversational context; automation must not decide endpoint correctness.
  The directly read 12-second opening preserves its reference meaning in two
  readable final rows without a stale draft or duplicate final. The known FR50
  cold-start speaker split remains visible. The clean 120-second forward and
  reverse readings preserve the previously signed 18-contribution result and
  expose no browser-only cut or attribution change, but the pacing candidate
  must repeat mechanically before completing this task. See
  `browser-persistence-review-2026-08-09.md`.
- [ ] **T042** Run physical-microphone sessions covering silence, room tone,
  short speech, continuous speech, pauses, interruption, overlap, and ordinary
  background noise.
- [ ] **T043** Complete contextual review of microphone ASR, VAD endpoint,
  hallucination, speaker, and Live/Final behavior; record hardware/browser
  provenance.
- [ ] **T044** Record Firefox and Safari/WebKit behavior when available; document
  an explicit environment limitation when unavailable.

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
