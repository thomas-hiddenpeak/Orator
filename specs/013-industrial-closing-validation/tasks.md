# Tasks: Industrial Closing Validation

**Status**: In progress - v2.1 closing baseline selected 2026-07-15

## Phase 0: Governance

- [x] T001 Review and approve Spec 013 requirements, thresholds, and terminology.
- [x] T002 Submit the common-clock/private-cache and supplemental-test
  Constitution amendment as a separate governance commit.
- [ ] T003 Correct all remaining closure and implementation claims in
  `PROJECT_STATE.md`, active specs, READMEs, and code comments.
- [x] T004 Define the reproducibility manifest and artifact naming convention.
- [x] T005 Freeze the current unaccepted baseline commit, TOML, registry state,
  model/data hashes, and full JSON package. The `ee0dd82` full package is
  retained as diagnostic evidence and explicitly rejected by its live/terminal
  ordering contract; see `baseline-2026-07-14.md`.

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
- [x] T017 Add ID-convergence tests across partial/final events, align groups,
  revisions, terminal tracks, export, reconnect, and Web UI state. Node model
  tests, the unified client, and the real Chromium flow cover the full chain.
- [x] T017A Replace the single active WebSocket output pointer with one audio
  producer plus multiple non-mutating observers. Extend the unified client gate
  to prove that observer connection lifecycle does not reset the producer and
  that producer/observer terminal timelines are identical. The registered
  12-second real-WebSocket gate covers an observer present before production, a
  rejected concurrent producer that disconnects during the stream, and a late
  observer. The producer and both retained observers received terminal SHA-256
  `9b1f2b3c...`; the early observer's 37 business events and nine telemetry
  events matched the producer exactly. A real Chromium observer independently
  converged to 2 ASR / 5 diar entries and exported the same parsed terminal
  document as the unified client.
- [x] T018 Correct configuration precedence and migrate/remove every behavioral
  environment-only switch. Precedence and timeline gap-fill are corrected;
  migrate ASR/align/Sortformer/GPU/transport controls through typed config and
  remove legacy GEMM kernel-forcing switches before marking complete.
- [x] T019 Emit or capture the complete resolved configuration in every run
  manifest. The terminal timeline must carry the canonical resolved object and
  the unified client must persist its hash in the artifact sidecar.
- [x] T020 Remove the misleading parameter-matrix mode from the unified test
  client and retain execution/capture responsibilities only.

## Phase 2: Reference Ledger

- [x] T030 Define the immutable ledger schema and manual judgment categories.
- [ ] T031 Adjudicate all 556 reference turns in chronological order against the
  audio, including exact speech intervals and overlaps. The v2.1 commit
  `43523ba` ledger is initialized and hash-validated at 556 rows; all 556 remain
  unsigned. Seven continuous work batches cover all rows as 93 / 84 / 80 / 80 /
  129 / 87 / 3 across the six 600-second blocks and final partial block. See
  `reference-ledger-v21-2026-07-15.md`.
- [ ] T032 Classify critical turns before candidate-output review.
- [ ] T033 Perform a second complete review in reversed 600-second block order.
- [ ] T034 Resolve every disagreement and record ambiguous reference rows without
  deleting them.
- [ ] T035 Independently verify the manual time, turn, speaker, criticality, and
  offset totals.

## Phase 3: Reproducible Baseline

- [x] T040 Restore and register the real-WebSocket Python integration gate in
  CTest using the sole unified socket client plus a process-only runner.
- [x] T041 Add silence, endpoint, time-base, track-immutability, stable-ID, and
  Web UI model tests. The registered gate runs 12 s canonical speech and 30 s
  generated silence with isolated TOML storage.
- [ ] T042 Run clean build, warning check, full CTest, JavaScript checks, and
  selected sanitizer/CUDA memory checks.
- [x] T043 Run 120 s, 360 s, and 600 s real-WebSocket tests with committed TOML.
- [ ] T044 Run one full v2.1 closing-baseline WebSocket capture with continuous
  `tegrastats` and browser evidence.
- [ ] T045 Complete the 556-row baseline context review and publish all required
  score breakdowns without script-inferred judgments.
- [x] T046 Canonicalize equal-start overlapping diarization records before the
  typed/live split and retain strict live/terminal equality validation. The
  full `ee0dd82` diagnostic package exposed three order-only mismatches;
  `test_typed_evidence_flow` now covers the contract.
- [x] T047 Designate streaming v2.1 `340/1/188/188` as the sole closing
  baseline. The compile-time default, checked-in TOML, generic weight-loader
  test, active async numerical gate, README, plan, and project state all select
  v2.1. The v2 weight and obsolete CTest are deleted, and `test_config` rejects
  their reappearance. Historical reports cannot satisfy a Phase 3-7 promotion
  or acceptance task.

## Phase 4: Existing-Model Upper Bound

- [x] T050 Export Sortformer posterior evidence per natural turn from the frozen
  package.
- [x] T051 Export TitaNet per-turn similarities independently of Sortformer
  bucket labels and validate them against NeMo.
- [x] T052 Export VAD and forced-alignment boundary evidence on the same time
  base.
- [x] T053 Build an auditable constrained speaker-decision candidate over frozen
  tracks without reference-specific runtime inputs.
- [ ] T054 Perform the complete contextual review of the candidate and evaluate
  the 93 percent development gate in every fixed block. A complete provisional
  text-context pass now covers all 556 rows and is sufficient to reject the
  candidate's natural-turn gate; audible-boundary adjudication, the reversed
  pass, and signed speaker-time totals remain open.
- [x] T055 Reject the initial frozen candidate and stop tuning its threshold
  family. Its complete provisional text-context diagnostic is `378/556`
  (67.986 percent); the one ambiguous row remains a failure in the mandatory
  denominator. No policy was accepted from this profile.
- [x] T056 Add and execute a multi-chunk NeMo oracle for the exact asynchronous
  runtime model/profile and retain the FIFO mismatch as a regression test. The
  reference-free processed fixture now covers five chunks / 1502 output frames,
  and the actual v2 runtime checkpoint provenance is fixed at NVIDIA revision
  `a16aa88603f758b4e4788177c6345ba3594edef6`, source `.nemo` SHA-256
  `48bf1aee...`, and runtime safetensors SHA-256 `754e4468...`. The regenerated
  raw oracle retained SHA-256 `0fb3b6d0...`; C++ passed at 1502/1502 argmax,
  `max_abs=1.43051e-6`, and `mean_abs=9.48068e-8`.
- [x] T057 Correct asynchronous FIFO/model-input parity, pass the exact v2
  runtime oracle, and regenerate frozen Sortformer evidence before another
  runtime accuracy comparison. The corrected full-session diagnostic changed
  464/556 reference intervals but the corresponding frozen text-context
  candidate remained 378 correct / 177 incorrect / 1 ambiguous; numerical
  parity fixed the implementation defect without recovering business accuracy.
- [x] T058 Remove non-NeMo cache controls, make `spkcache_sil_frames` effective,
  and require the full TOML streaming profile before Sortformer initialization.
  Removed keys now fail configuration loading, late tuning is rejected, the
  rebuilt exact-profile oracle passes. The suite passed 61/61 at that step.
  See `sortformer-oracle-2026-07-15.md`.
- [x] T059 Evaluate the clean integrated-view upper bound without
  reference-specific runtime rules. The best provisional written-context
  diagnostic is `502/556` (90.288 percent), still below the 93 percent
  development gate and not a signed constitutional result.
- [x] T059A Reconstruct the abandoned streaming v2.1 context. The existing
  frozen-evidence first pass covers all 556 rows: async v2.1 recorded 377
  correct / 178 incorrect / 1 ambiguous, while checkpoint-native sync recorded
  357 / 198 / 1. These are historical written-context diagnostics, not a
  real-WebSocket constitutional result.
- [x] T059B Regenerate the five-chunk v2.1 asynchronous NeMo oracle using the
  active `188/340/188`, FIFO `188`, context `1+1` profile. The regenerated raw
  fixture is byte-identical to `ref_stream_async_v21_long.f32` at SHA-256
  `2635b090...`; bind the runtime numerical gate to v2.1 before streaming.
- [x] T059C Run the v2.1 candidate through the clean build and numerical suite,
  then a 120-second real-WebSocket promotion run with continuous telemetry. The
  clean suite passed 63/63 and the transport/telemetry contracts passed, but the
  contextual promotion gate failed: v2.1 lost diar evidence from 4.48-27.20 s
  and reused local slots across several 65-84 s short turns. Continue the full
  run only as the owner-requested diagnostic, not as a promoted candidate.
- [x] T059D Run full `test.mp3` at 1x through the real WebSocket path using the
  committed v2.1 TOML and retain the source-stable manifest, terminal timeline,
  events, logs, browser evidence, and `tegrastats` series. The 3615.12-second
  run completed in 3616.559 seconds with zero mechanical-contract issues;
  terminal artifact SHA-256 is `653e366b...`.
- [x] T059E Complete the full 556-row chronological and reversed-block
  contextual speaker review for the v2.1 runtime. The reconciled diagnostic is
  413 correct / 142 incorrect / 1 ambiguous (`74.2806%`), with the worst fixed
  block at 2400-3000 seconds (`62.02%`). The retained corrected-v2 result is
  reported only as a different-class frozen diagnostic; no paired acceptance
  comparison or exact speaker-time score is claimed.
- [x] T059H Validate the two NVIDIA-published v2.1 streaming profiles with exact
  multi-chunk NeMo/C++ fixtures: high latency `340/40/40/300` and low latency
  `6/7/188/144` (chunk/right/FIFO/update). Keep the legacy
  `340/1/188/188` result only as a same-profile checkpoint diagnostic. High
  latency passed 1502 frames at `max_abs=1.07288e-6`; low latency passed at
  `max_abs=1.54972e-6`, both with 1502/1502 argmax agreement. The low-latency
  gate exposed and fixed an undersized Conformer scratch buffer when `T<64`;
  the corrected first-block path also passes `compute-sanitizer` with zero
  errors.
- [x] T059I Export full-length native diar evidence for both official profiles,
  preserve the common time base, and complete the 556-row contextual comparison
  before selecting a profile for another real-WebSocket run. High latency
  recorded 385 correct / 170 incorrect / 1 ambiguous (`69.2446%`); low latency
  recorded 377 / 178 / 1 (`67.8058%`). Both fail the 90 percent screening gate,
  so neither is selected for an integrated run. The inherited v2.1 profile
  remains active at its previously measured 413 / 142 / 1 real-WebSocket
  diagnostic.
- [x] T059F Trace the current v2.1 incorrect rows through local Sortformer
  channels, global speaker-identity assignments, and the `business_speaker`
  comprehensive view. The first loss is usually in the local Sortformer slot:
  after 2400 seconds its reference-localization ceiling falls to roughly
  62-67 percent in several fixed windows, while the identity layer mostly
  propagates stable local epochs. Native TitaNet evidence independently repairs
  some complete Zhu Jie turns but is ambiguous or wrong at 1039, 2136, and
  3270-3298 seconds, so a direct whole-turn override is ineligible.
- [x] T059G Build a reference-free multi-scale rolling TitaNet evidence track
  for the captured v2.1 session. The 3/5-second, 1-second-step TOML policy
  produced 1,166 dual-scale points and 239 runs from 7,224 native spans. It
  changed nine of 936 business entries. Full contextual review of all 11
  affected reference rows found five repairs and no regression, yielding 418 /
  137 / 1 (`75.1799%`). This remains below the 93 percent implementation gate,
  so policy tuning stops and the frozen candidate is not integrated. See
  `speaker-sliding-v21-2026-07-15.md`.

## Phase 5: Model Escalation If Required

- [x] T060 Close the non-deployable model branch. The historical reference-free ERes2NetV2
  turn-voiceprint candidate reached a provisional text-context `400/556`
  (71.942 percent). Forced-alignment phrase boundaries plus independent phrase
  embeddings reached `409/556` (73.561 percent), with 9 repaired rows, no
  correct-to-incorrect row transitions, and 2 accurate-to-mostly-accurate
  downgrades. A pinned WeSpeaker ResNet293-LM audit found no additional rewrite
  that passed the strict reference-free conflict rule. These are written-context
  diagnostics, not constitutional audible scores, and cannot enter the 93
  percent model-selection gate without a native streaming deployment path.
- [x] T060A Remove the exploratory offline Sortformer/Pyannote tooling, tests,
  and report. They duplicate the role of the manually adjudicated `test.txt`
  and cannot produce a deployed runtime candidate. Retain only NeMo fixtures
  for same-checkpoint v2.1 numerical parity.
- [ ] T061 Select a model only when its frozen-evidence result exceeds the 93
  percent gate without a fixed-block or critical-turn regression.
- [ ] T062 Write model-specific SDD artifacts and trusted-oracle tolerances before
  any C++/CUDA port.
- [ ] T063 Port and validate each selected model stage numerically before runtime
  integration.

## Phase 6: v2.1 Runtime Candidate

- [ ] T070 Implement the frozen speaker-evidence and fusion contracts behind
  typed TOML fields.
- [ ] T071 Add complete audit evidence to each business-speaker decision.
- [ ] T072 Validate that raw ASR, diarization, VAD, align, and voiceprint tracks
  remain unchanged by fusion revisions.
- [ ] T073 Complete Web UI live/final convergence, microphone, reconnect,
  telemetry, and export validation in a real browser.
- [ ] T074 Run all model oracle gates for the exact accepted v2.1 TOML profile.
- [ ] T075 Pass 120 s, then 360 s, then 600 s promotion gates; stop on the first
  regression.

## Phase 7: Full v2.1 Acceptance

- [ ] T080 Run full v2.1 acceptance A with an empty isolated speaker registry.
- [ ] T081 Complete the full 556-row context and semantic review for run A.
- [ ] T082 Restart the process and run full v2.1 acceptance B with the frozen
  enrolled-registry fixture.
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
