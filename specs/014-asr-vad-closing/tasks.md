# Spec 014: Ordered Tasks

`[ ]` pending, `[~]` in progress, `[x]` complete. A task is complete only when
its named evidence exists. Mechanical checks never assign product correctness.
Under Constitution 1.8.0, every historical shortened-run "accept" or "reject"
entry in this task log is retained only as a diagnostic record. It has no
current product-decision authority unless a complete canonical run and full
chronological/reverse contextual review also support it.

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
- [x] **T064** Run one focused real-WebSocket context plus explicit neighboring
  controls and complete chronological and reverse semantic review against the
  human-listened reference. Reject and remove the candidate on a new critical
  regression; otherwise return to T048 and the duration ladder. Clean commit
  `5b6ba51` captures the exact 102-second context at `0.979x`, with an unchanged
  clean worktree and complete provenance. Direct review finds useful repairs,
  but also changes “雷总也不说话了” into “雷总也是出汗了” and “签了独家吗”
  into “签了总项目”. Reverse review confirms both are new critical assertions.
  The candidate is rejected before silence or duration gates, its TOML switch
  is restored to false, and its implementation remains evidence-only. See
  `streaming-encoder-boundary-review-2026-08-09.md`.

### Phase 3D: Trained-Window Causal Control

- [x] **T065** Restore the accepted streaming Final as the exact control and
  expose one TOML-owned acoustic append window without changing Live publication
  code, VAD, prompt, decoder rollback, alignment, Sortformer v2.1, FR50 fusion,
  or the common time base. The candidate value is the model-defined 800 mel
  frames (eight seconds); the current 100-frame value is the control. Typed
  `[asr].stream_window_mel_frames` now flows through TOML, the resolved snapshot,
  `AuditoryStream::Config`, and `AsrConfig`; both parser and Qwen reject every
  value except the evidenced 100/800 pair. The checked-in value remains 100.
- [x] **T066** Prove mechanically that the configured 800-frame path uses the
  already accepted complete-window numerical contract, then pass focused tests,
  warning-clean build, and complete CTest. Neither tensors nor tests may assign
  transcript correctness. Config/model tests prove 800-frame propagation and
  validation. The repeated 16-second numerical probe again gives exact equality
  for both 800-frame slices and reproduces SHA-256 `b420acc5...9784`; the build
  has no warning/error diagnostic and all `75/75` tests pass in `52.85` seconds.
- [x] **T067** Capture the same complete 102-second real-WebSocket context under
  the isolated 800-frame candidate and read every Live and Final state in
  chronological and reverse conversational context against `test.txt`. Reject
  on any new critical meaning or unusable Live behavior; do not start a duration
  ladder from mechanical evidence. The capture commit activates only the
  800-frame window; all other candidate controls remain frozen. Clean commit
  `291c63d` streams the exact 102-second input at `0.99x`; direct terminal wait
  is `1.061` seconds and all provenance is stable. Complete forward/reverse and
  comprehensive-view review finds useful lexical repairs, but two long business
  statements are critically truncated, “十五” becomes “十五个月”, short segments
  repeat the rejected “出汗了/总项目” assertions, and the first eight-second Live
  state still displays “语音识别”. The candidate is rejected.
- [x] **T068** Only if T067 demonstrates a semantic advantage without a control
  regression, specify a second candidate that retains the accepted one-second
  provisional Live view and replays the model-defined trained-window stream only
  for Final. Repeat engineering and focused contextual gates before returning to
  T048. Otherwise restore the 100-frame control and revise the causal analysis.
  T067 fails the prerequisite, so no replay implementation is authorized. The
  checked-in TOML returns to 100 frames; the typed 100/800 evidence boundary is
  retained, and the next hypothesis must address decoder continuation and short-
  tail behavior without treating encoder locality as transcript correctness.
  The restored configuration passes a warning-clean build and all `75/75` tests
  in `52.75` seconds.

### Phase 3E: Official Accumulated-Audio Streaming

- [x] **T069** After the third failed candidate, restore the accepted control
  and re-audit the pinned official streaming state machine before changing code.
  The official source confirms `audio_accum` is fully re-fed every two seconds,
  the first two chunks use an empty prefix, later chunks roll back five tokens,
  the residual tail is appended without padding, and the streaming example uses
  `max_new_tokens=32`. This explains why applying that budget only every eight
  seconds caused T067's long-segment truncation. See
  `official-streaming-state-review-2026-08-09.md`.
- [x] **T070** Add a TOML-owned `kv_append` / `accumulated_redecode` mode plus
  typed chunk and rollback values. Implement the official state transition in
  `Qwen3Asr`, keep `kv_append` checked in, and add source-contract/config/model
  tests. Do not alter VAD, prompt, segment cap, alignment, Sortformer, speaker
  policy, or publication semantics. The inactive implementation passes focused
  `test_config`, `test_qwen3`, `test_asr_worker`, and `test_registration`; all
  typed values appear in the resolved configuration.
- [x] **T071** Under the checked-in control, pass focused tests, warning-clean
  build, complete CTest, and applicable retained numerical gates. Commit and
  push the inactive implementation before producing candidate output. The
  focused tests pass `4/4`, complete CTest passes `75/75` in `53.14` seconds,
  and a subsequent clean-first build emits no warning or error. Inactive
  implementation commit `2acae3a` is pushed to `master` with `kv_append`
  checked in and no candidate output.
- [x] **T072** Change only TOML to `accumulated_redecode`, repeat the engineering
  gates, commit the clean candidate, and stream the identical 102-second focused
  input through the production WebSocket at 1.0x/100 ms with observer and
  telemetry evidence. The candidate is now active in TOML; focused tests pass
  `4/4`, complete CTest passes `75/75` in `50.92` seconds, and a clean-first
  build emits no warning or error. Clean pushed commit `a6ba893` captures the
  exact 102-second source at `0.99x` with direct terminal return in `1.024`
  seconds and complete source/time/telemetry/observer evidence.
- [x] **T073** Read every Live, Final, and comprehensive-view contribution in
  chronological and reverse context against the complete human reference. Stop
  on a real-time failure, new critical meaning, omission, or unusable Live state;
  otherwise return to T048. No code may compare or label transcript output.
  Both complete readings reject the candidate: long-segment continuation is
  restored, but a correct negation in Live is replaced by a false Final action,
  the exact exclusive-signing question regresses, and one Live state invents an
  investment instruction. `kv_append` is restored. See
  `official-accumulated-2s-review-2026-08-09.md`.

### Phase 3F: One-Second Accumulated Cadence

- [x] **T074** Derive one bounded follow-up from the complete Phase 3E reading.
  At two seconds, short segments can finalize before the two unfixed chunks are
  complete, discarding a correct Live prefix and reproducing the rejected fresh
  full-context result. The official API makes chunk duration configurable. One
  one-second accumulated candidate is authorized; no parameter sweep is.
- [x] **T075** Under restored `kv_append`, pass focused config and full CTest,
  commit and push the rejected-candidate record, and verify no process remains.
  `test_config` passes, complete CTest passes `75/75` in `52.89` seconds, the
  build emits no warning or error, and no server/client/tegrastats process
  remains. Rejection/restoration commit `1310b1f` is pushed to `master`.
- [x] **T076** Activate `accumulated_redecode` with
  `stream_chunk_ms = 1000`, leaving rollback, VAD, prompt, segment cap, align,
  v2.1, FR50, time base, and publication unchanged. Repeat engineering gates
  and commit the clean candidate before output. The TOML candidate is active;
  focused tests pass `4/4`, complete CTest passes `75/75` in `52.24` seconds,
  and the build emits no warning or error. Clean candidate commit `244006e` is
  pushed before output.
- [x] **T077** Stream the identical 102-second focus at 1.0x/100 ms with source,
  time-base, telemetry, observer, and terminal evidence. Exact source/config/
  binary provenance remains fixed, all seven tracks reconcile at 1,632,000
  samples, the run reports `0.988x`, direct terminal return is 1.280 seconds,
  and both telemetry sources and observers are complete.
- [x] **T078** Read every Live, Final, and comprehensive-view contribution
  chronologically and in reverse. Restore `kv_append` on any new critical
  meaning, omission, or unusable Live state; otherwise return to T048. Both
  complete readings reject the candidate: the false short Final, missing
  exclusive-signing term, legal/numeric substitutions, and visible Live
  hallucinations remain. Restore the control and do not start T048. See
  `official-accumulated-1s-review-2026-08-09.md`.

### Phase 3G: Decoder-State Root Cause

- [x] **T079** Under restored `kv_append` and dormant 2000 ms accumulated
  values, pass focused config, complete CTest, warning review, commit/push the
  Phase 3F rejection record, and verify no capture process remains. Focused
  `test_config` passes, complete CTest passes `75/75` in `52.75` seconds, the
  build emits no warning or error, and no capture process remains.
  Rejection/restoration commit `3924fb8` is pushed to `master`.
- [x] **T080** Specify and implement one opt-in, bounded raw decoder-state trace
  for the inactive accumulated path. Record only audio extent, chunk/Final
  state, raw and retained token IDs, rollback boundary, text prefix,
  continuation, and token budget; keep it disabled in checked-in TOML. Typed
  `[asr].stream_state_trace` and `stream_state_trace_path` flow through resolved
  config to an `io/` JSONL writer. Each accumulated decode row records exact
  common-clock samples and token state without reference text or judgments.
- [x] **T081** Pass focused and complete engineering gates under the inactive
  control, then commit/push before evidence. Focused config/JSON/Qwen tests pass
  `3/3`, complete CTest passes `75/75` in `52.90` seconds, and compilation emits
  no warning or error. Checked-in TOML remains `kv_append` with tracing false
  and an empty path; this change is the clean inactive implementation source.
- [x] **T082** After T081 is pushed, capture the identical 102-second focus with
  trace enabled only through an isolated `orator.toml`. Retain exact source,
  config, binary, run, trace, time-base, observer, and telemetry provenance.
  Clean commit `b92f6ba` produces 91 trace rows across eight segments; all
  source/config/binary hashes remain stable, all seven extents close at
  1,632,000 samples, observers match, and required telemetry coverage passes.
- [x] **T083** Read every trace transition against the pinned official source
  contract and the complete human conversation in chronological and reverse
  context. No code may compare text or select a candidate. Authorize one new
  runtime hypothesis only if a concrete mismatch is established; otherwise
  stop the accumulated branch at `kv_append`. All 91 rows have been read in
  both directions. Accumulated state transitions match the pinned source. The
  native first-three-token EOS ban is absent from official vLLM sampling and is
  the sole authorized mismatch; cadence, rollback, VAD, prompt, and fusion stay
  frozen. See `decoder-state-root-cause-review-2026-08-10.md`.

### Phase 3H: Official Greedy Termination

- [x] **T084** Commit and push the T082/T083 evidence report and Phase 3H SDD
  while checked-in behavior remains `kv_append`, dormant 2000 ms, and
  `ban_steps = 3`. Transitional evidence commit `5340cd0` is pushed to
  `master`.
- [x] **T085** Reproduce the exact Phase 3F one-second accumulated state and set
  only `ban_steps = 0` relative to that artifact. Keep trace false and every
  VAD, prompt, segment, model, alignment, speaker, time-base, and publication
  value fixed. Pass focused configuration tests, complete CTest, and a
  warning-clean build, then commit/push before product evidence. The focused
  config test passes, complete CTest passes `75/75` in `51.84` seconds, and the
  build emits no warning or error. The clean candidate is committed and pushed
  by this checkpoint before T086 starts.
- [x] **T086** Stream the identical 102-second WAV through the real WebSocket at
  1x with early/late observers, direct `end`, continuous telemetry, exact
  provenance, and full common-time-base reconciliation. Mechanical tools MUST
  NOT compare transcript text or issue a verdict. Clean commit `a9ebea7` runs
  at `0.987x`, returns terminal state in 1.330 seconds, closes all seven extents
  at 1,632,000 samples, matches both observers, and passes telemetry coverage.
- [x] **T087** Read every Live, Final, and comprehensive contribution in complete
  chronological and reverse context against `test.txt`. All 81 Live, eight
  Final, and 28 comprehensive entries were read in both directions. The
  candidate repairs no focused critical context and retains the unsupported
  investment Live. Under Constitution 1.8.0 these are local diagnostic findings
  only; the former product rejection and longer-gate prohibition are
  superseded. See `official-greedy-termination-review-2026-08-10.md`.
- [x] **T088** Under the restored control, pass focused configuration, complete
  CTest, warning review, and process cleanup; update state documentation and
  commit/push the historical diagnostic record. Focused `test_config`
  passes, complete CTest passes `75/75` in `52.86` seconds, compilation emits no
  warning or error, and no server, client, or `tegrastats` process remains.

### Phase 3I: Full-Length Phase 3H Re-evaluation

- [x] **T089** Amend evaluation governance to Constitution 1.8.0 and classify
  every shortened or focused run as diagnostic only. Product verdicts, global
  accuracy statements, candidate ranking, and decisions to stop evaluation
  require complete `test.mp3` plus all 556 `test.txt` contributions reviewed
  chronologically and in reverse context. Amendment commit `d07b7af` is pushed.
- [x] **T090** Reactivate the exact Phase 3H candidate in `orator.toml`:
  `accumulated_redecode`, 1000 ms, `ban_steps = 0`, trace false. Keep every
  other output-affecting value fixed, update exact configuration assertions,
  pass focused configuration tests, complete CTest, and a warning-clean build,
  then commit and push the candidate before capture. The exact configuration
  test passes, the complete suite passes `75/75` in `51.93` seconds, and the
  clean build emits no warning or error diagnostic. Commit/push completes
  before T091 starts.
- [x] **T091** Run all 3615.120 seconds of `test.mp3` at 1.0x/100 ms through
  the production WebSocket with an empty isolated registry, early and late
  observers, direct `end`, continuous runtime telemetry, continuous
  `tegrastats`, immutable provenance, and exact common-time-base
  reconciliation. Retain all raw evidence; automation performs mechanical
  validation only. Candidate commit `5f98db1` completes the run at `0.992x`,
  returns the terminal document after `27.883 s`, reconciles all seven tracks
  at 57,841,920 samples, and retains raw evidence under
  `artifacts/spec014/candidates/asr-official-greedy-no-eos-ban/full-a/`.
- [x] **T092** Read every one of the 556 `test.txt` contributions and all
  relevant Live, Final, diarization, forced-alignment, speaker, and
  comprehensive records in chronological context. Assign every semantic and
  speaker judgment directly without executable evaluation or aggregation. All
  556 Final/comprehensive contexts and all 2,664 Live events are read directly
  from start to terminal time.
- [x] **T093** Repeat the complete review in reverse fixed contextual windows,
  reconcile every disagreement manually, and independently recheck every
  manually derived total and band. All 556 reference contexts and all 2,664
  Live events are reread from terminal time to zero in fixed windows; the
  reverse pass does not change the full-session disposition. The forward and
  reverse unjudged reference worksheets retain SHA-256
  `28f346d3...0646` and `dd4f1a40...6355` respectively.
- [x] **T094** Manually record the full ASR semantic band, all six complete
  600-second bands, final 15.120-second context, critical meanings, Live/Final
  effect, endpoint observations, and frozen-speaker guard. Only this complete
  review may accept/reject the candidate or state an overall change. The
  complete review places the run in the 70-79% band, all six complete blocks
  below 90%, final 15.120 seconds at 80-89%, and the speaker view inside the
  conditional FR50 boundary. Critical meaning and Live presentation fail; the
  candidate is not an overall improvement and is rejected. See
  `official-greedy-full-context-review-2026-08-10.md`.
- [x] **T095** Retain or restore the TOML candidate strictly from T092-T094,
  rerun focused configuration, complete CTest, warning review, and process
  cleanup, then update the report and project state and commit/push the result.
  The full review rejects the candidate and restores `kv_append`, dormant
  2000 ms accumulated cadence, and `ban_steps = 3`. The rebuilt exact
  configuration test passes, the clean build emits no warning or error, and
  complete CTest passes `75/75` in `52.93` seconds. Documentation and clean
  process-state verification complete in the same transition.

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
