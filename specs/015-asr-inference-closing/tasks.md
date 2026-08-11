# Spec 015: Tasks

All tasks are pending unless marked otherwise. Product-output evaluation is
contextual and reviewer-owned; tools stop at numerical or mechanical evidence.

## Phase 0 - Freeze and Baseline

- [x] **T001** Record clean baseline commit `c235830`, worktree, root TOML,
  official-source revision, model hashes, binary hash, and device provenance.
- [x] **T002** Define and record the frozen speaker/non-ASR path and resolved
  configuration manifest.
- [x] **T003** Add a mechanical changed-path and resolved-config guard that
  fails when a Spec 015 candidate modifies the frozen control surface.
- [x] **T004** Run a warning-clean build and complete CTest under the unchanged
  baseline; verify no capture process remains.
- [x] **T005** Freeze the Phase 3I full report and artifact provenance as the
  immutable ASR product baseline. Do not rerun audio in this task.

## Phase 1 - Numerical Oracle

- [x] **T006** Verify the pinned official Qwen3-ASR checkout is clean at
  `7c6daf77a2421100f5fb066495372c00129d39ff` and freeze required source files.
- [ ] **T007** Restore a usable offline official PyTorch oracle environment;
  prefer CUDA, with bounded CPU execution as the documented fallback.
- [ ] **T008** Add an offline source-equivalent adapter for the official vLLM
  eight-second encoder attention contract if vLLM itself cannot run on Jetson.
- [ ] **T009** Extend oracle output to raw PCM, mel, encoder states, prompt and
  position IDs, per-step logits/argmax IDs, cache metadata, and complete token
  sequences without transcript judgment.
- [ ] **T010** Generate and hash the deterministic synthetic and fixed
  source-order fixture families under `models/reference/asr/spec015/`.
- [ ] **T011** Record precision and tolerance contracts before native
  implementation changes; retain existing tolerances unchanged.

## Phase 2 - First-Divergence Repair

- [ ] **T012** Validate full and incremental mel at 2/8/16/24 seconds,
  amplitude changes, retained boundaries, and unpadded final tails.
- [ ] **T013** Prove or reject the current running-global-maximum exactness
  claim; repair only the feature owner if it fails.
- [ ] **T014** Validate 100-frame diagnostic and official 800-frame encoder
  boundaries, long accumulated inputs, and partial tails.
- [ ] **T015** Validate official attention-window ownership against the native
  `windowed_encoder` behavior and repair the first encoder delta only.
- [ ] **T016** Validate exact prompt IDs, special-token order, language/context
  conditioning, audio embedding replacement, position IDs, and cache positions.
- [ ] **T017** Validate decoder logits and selected ID at every greedy step,
  including EOS, token limit, repeat guard, and split-prefill behavior.
- [ ] **T018** Validate complete token sequences for fresh 2/8/16/24-second
  inputs, accumulated updates, rollback, and final-tail states.
- [ ] **T019** Register every accepted fixture in CTest and rerun all earlier
  stage gates after each repair.
- [ ] **T020** Write the first-divergence report. Do not authorize a product
  candidate while any unexplained numerical delta remains.

## Phase 3 - Native Parity Stream

- [ ] **T021** Specify the exact official-equivalent native stream state from
  the completed parity evidence, including memory and endpoint ownership.
- [ ] **T022** Implement the inactive mode using exact accumulated PCM,
  validated features/encoder/decoder, rollback, and final-tail behavior.
- [ ] **T023** Carry every runtime-selectable ASR behavior through typed TOML,
  resolved configuration, tests, and captured provenance.
- [ ] **T024** Add reset, repeated-session, tail, common-clock, invalid-config,
  disabled-mode, and memory-bound tests.
- [ ] **T025** Pass all numerical fixtures, warning-clean build, complete CTest,
  frozen-path guard, and repeated component stability checks.
- [ ] **T026** Commit the inactive implementation before producing candidate
  output; root TOML remains the current control.
- [ ] **T027** Activate one exact parity candidate in an isolated TOML and run
  only short mechanical WebSocket checks for transport, time, memory,
  telemetry, and terminal completeness. Do not issue a transcript verdict.

## Phase 4 - Full Candidate A

- [ ] **T028** Freeze the clean candidate commit, isolated TOML, source/model/
  binary hashes, empty registry, observers, pacing, and telemetry plan.
- [ ] **T029** Process complete `test.mp3` at 1.0x/100 ms through the production
  WebSocket and retain mechanically valid raw evidence.
- [ ] **T030** Read all 556 reference contexts with Final ASR and endpoint
  evidence chronologically; assign every semantic judgment directly.
- [ ] **T031** Read all Live events chronologically for visible unsupported
  assertions, rewrite behavior, and Final convergence.
- [ ] **T032** Repeat the complete 556-context and complete Live review in
  reverse contextual windows and manually reconcile disagreements.
- [ ] **T033** Manually derive and recheck the whole-session band, every complete
  600-second block, critical meaning, endpoint, Live/Final, and frozen-speaker
  guard results.
- [ ] **T034** Accept or reject Candidate A only from T030-T033. Restore the
  root TOML after failure and write the immutable full report.

## Phase 5 - Conditional Endpoint Correction

- [ ] **T035** Enter this phase only when Candidate A's complete review locates
  material loss in ASR decoder-session ownership after numerical parity.
- [ ] **T036** Trace frozen VAD evidence, acoustic lead/trail, decoder state,
  and ASR publication on the common clock without reading speaker output in
  runtime.
- [ ] **T037** Specify one ASR-local endpoint hypothesis that separates
  acoustic padding from semantic state and changes no shared VAD value.
- [ ] **T038** Implement the typed TOML policy and focused mechanical tests;
  keep diarization, speaker, aligner, timeline, and time base frozen.
- [ ] **T039** Repeat numerical, engineering, frozen-path, and short mechanical
  gates. Short transcripts remain diagnostic only.
- [ ] **T040** Return to T028-T034 for a complete full-length product decision.

## Phase 6 - Run B and Product Surface

- [ ] **T041** After Candidate A passes, restart with only A's frozen registry
  and capture complete full Run B under identical ASR conditions.
- [ ] **T042** Independently repeat the complete chronological and reverse
  contextual ASR, Live, endpoint, and frozen-speaker guard review for Run B.
- [ ] **T043** Run and directly review three independent digital-silence
  sessions for substantive final hallucination.
- [ ] **T044** Validate ASR partial/retract/final convergence, persistence,
  reload, reconnect, export, and desktop/mobile Chromium rendering.
- [ ] **T045** Validate physical microphone room tone, short/continuous speech,
  pauses, interruption, overlap, and voiced background noise when functional
  capture hardware is available; otherwise record the explicit blocker.
- [ ] **T046** Confirm all Spec 015 acceptance gates manually and mechanically
  within their permitted boundaries.

## Phase 7 - Closing Handoff

- [ ] **T047** Make the accepted ASR values the checked-in root TOML baseline
  and remove or permanently disable obsolete experimental ASR modes.
- [ ] **T048** Run the locked holdout with frozen provenance and report it
  separately from canonical `test.mp3`.
- [ ] **T049** Write the Spec 015 closing report and synchronize Spec 014,
  applicable Spec 013 status, `PROJECT_STATE.md`, README/configuration docs,
  and tests with verified code.
- [ ] **T050** Obtain final report review and create a release tag only when all
  applicable project gates are complete.
