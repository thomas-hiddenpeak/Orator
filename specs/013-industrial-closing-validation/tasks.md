# Tasks: Industrial Closing Validation

**Status**: In progress - spec/plan approved 2026-07-13

## Phase 0: Governance

- [x] T001 Review and approve Spec 013 requirements, thresholds, and terminology.
- [x] T002 Submit the common-clock/private-cache and supplemental-test
  Constitution amendment as a separate governance commit.
- [ ] T003 Correct all remaining closure and implementation claims in
  `PROJECT_STATE.md`, active specs, READMEs, and code comments.
- [ ] T004 Define the reproducibility manifest and artifact naming convention.
- [ ] T005 Freeze the current unaccepted baseline commit, TOML, registry state,
  model/data hashes, and full JSON package.

## Phase 1: Contract Compliance

- [x] T010 Create one session-owned immutable `TimeBase` and inject it into every
  audio store and worker.
- [x] T011 Make end-of-stream extent reconciliation cover every registered track
  and fail tests on any mismatch.
- [x] T012 Add typed incremental evidence reads/subscriptions to
  `ComprehensiveTimeline` without content inference.
- [x] T013 Replace the ASR `VadCache` pointer path with VAD evidence read through
  `ComprehensiveTimeline`.
- [x] T014 Replace forced aligner's direct protocol subscription with finalized
  ASR evidence read through `ComprehensiveTimeline`.
- [x] T015 Move speaker choice, text projection, and gap policy into a registered
  `business_speaker` pipeline and keep raw tracks immutable.
- [x] T016 Fix finalized ASR `text_id` allocation/emission order and serialize IDs
  explicitly in the ASR terminal track.
- [ ] T017 Add ID-convergence tests across partial/final events, align groups,
  revisions, terminal tracks, export, reconnect, and Web UI state. The unified
  client now gates final events, align groups, business revisions, and terminal
  tracks; partial replacement, export, reconnect, and Web UI state remain open.
- [ ] T018 Correct configuration precedence and migrate/remove every behavioral
  environment-only switch. Precedence and timeline gap-fill are corrected;
  remaining lower-level switches are still open.
- [ ] T019 Emit or capture the complete resolved configuration in every run
  manifest.
- [x] T020 Remove the misleading parameter-matrix mode from the unified test
  client and retain execution/capture responsibilities only.

## Phase 2: Reference Ledger

- [ ] T030 Define the immutable ledger schema and manual judgment categories.
- [ ] T031 Adjudicate all 556 reference turns in chronological order against the
  audio, including exact speech intervals and overlaps.
- [ ] T032 Classify critical turns before candidate-output review.
- [ ] T033 Perform a second complete review in reversed 600-second block order.
- [ ] T034 Resolve every disagreement and record ambiguous reference rows without
  deleting them.
- [ ] T035 Independently verify the manual time, turn, speaker, criticality, and
  offset totals.

## Phase 3: Reproducible Baseline

- [ ] T040 Restore and register real-WebSocket Python integration tests in CTest.
- [ ] T041 Add silence, endpoint, time-base, track-immutability, stable-ID, and
  Web UI model tests.
- [ ] T042 Run clean build, warning check, full CTest, JavaScript checks, and
  selected sanitizer/CUDA memory checks.
- [ ] T043 Run 120 s, 360 s, and 600 s real-WebSocket tests with committed TOML.
- [ ] T044 Run one full current-baseline WebSocket capture with continuous
  `tegrastats` and browser evidence.
- [ ] T045 Complete the 556-row baseline context review and publish all required
  score breakdowns without script-inferred judgments.

## Phase 4: Existing-Model Upper Bound

- [ ] T050 Export Sortformer posterior evidence per natural turn from the frozen
  package.
- [ ] T051 Export TitaNet per-turn similarities independently of Sortformer
  bucket labels and validate them against NeMo.
- [ ] T052 Export VAD and forced-alignment boundary evidence on the same time
  base.
- [ ] T053 Build an auditable constrained speaker-decision candidate over frozen
  tracks without reference-specific runtime inputs.
- [ ] T054 Perform the complete contextual review of the candidate and evaluate
  the 93 percent development gate in every fixed block.
- [ ] T055 Freeze the candidate policy if it passes; otherwise record the failed
  evidence ceiling and stop threshold tuning.

## Phase 5: Model Escalation If Required

- [ ] T060 Benchmark stronger overlap-aware diarization and speaker-verification
  candidates offline only if T054 fails.
- [ ] T061 Select a model only when its frozen-evidence result exceeds the 93
  percent gate without a fixed-block or critical-turn regression.
- [ ] T062 Write model-specific SDD artifacts and trusted-oracle tolerances before
  any C++/CUDA port.
- [ ] T063 Port and validate each selected model stage numerically before runtime
  integration.

## Phase 6: Runtime Candidate

- [ ] T070 Implement the frozen speaker-evidence and fusion contracts behind
  typed TOML fields.
- [ ] T071 Add complete audit evidence to each business-speaker decision.
- [ ] T072 Validate that raw ASR, diarization, VAD, align, and voiceprint tracks
  remain unchanged by fusion revisions.
- [ ] T073 Complete Web UI live/final convergence, microphone, reconnect,
  telemetry, and export validation in a real browser.
- [ ] T074 Run all model oracle gates for the exact accepted TOML profile.
- [ ] T075 Pass 120 s, then 360 s, then 600 s promotion gates; stop on the first
  regression.

## Phase 7: Full Acceptance

- [ ] T080 Run full acceptance A with an empty isolated speaker registry.
- [ ] T081 Complete the full 556-row context and semantic review for run A.
- [ ] T082 Restart the process and run full acceptance B with the frozen enrolled
  registry fixture.
- [ ] T083 Complete the full 556-row context and semantic review for run B.
- [ ] T084 Verify every Spec 013 accuracy, boundary, alignment, latency,
  telemetry, stability, and repeatability gate independently for both runs.
- [ ] T085 Execute the supplemental locked holdout suite after the Constitution
  amendment if an industrial-readiness claim is requested.
- [ ] T086 Write the final closing report with manifests, hashes, complete ledger,
  metrics, limitations, and exact product claim.
- [ ] T087 Synchronize specs, tasks, `PROJECT_STATE.md`, README, and code comments
  in the accepted commit.
- [ ] T088 Review the final report, close all priority-zero/priority-one defects,
  and create the release tag only after sign-off.
