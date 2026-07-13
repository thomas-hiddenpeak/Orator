# Plan: Industrial Closing Validation

## 1. Planning Decision

The project will not optimize the existing final speaker labels one parameter at
a time. It will first establish a compliant evidence contract and a complete
manual reference ledger, then determine the accuracy upper bound available from
the currently implemented models. Runtime implementation proceeds only when the
frozen-evidence candidate has sufficient margin above the 90 percent product
gate.

Work remains on `master` in small, auditable commits. Each failed experiment is
either removed or left disabled with an explicit experimental status. After
three failed solution families with the same root limitation, work returns to
the last accepted commit and the model strategy is reconsidered.

## 2. Phase 0: Governance and Baseline Reset

1. Review and approve Spec 013 before runtime implementation.
2. Mark the product as not closed in `PROJECT_STATE.md`; retain historical runs
   as component evidence only.
3. Propose a separate Constitution patch that:
   - preserves one session-owned common time base while allowing private
     per-pipeline audio stores;
   - keeps `test.mp3` mandatory but permits supplemental silence, microphone,
     and locked holdout tests for safety and industrial-readiness claims.
4. Create a reproducibility manifest format and record the current `master`
   commit, dirty state, data/model hashes, device, power mode, clocks, registry
   state, TOML hash, and tool versions.
5. Freeze a named current-candidate full JSON package for diagnosis. It is a
   baseline measurement, not an accepted result.

## 3. Phase 1: Correct the Product Contracts

### 3.1 Session clock

`AuditoryStream` will own one immutable `core::TimeBase` created when the session
audio ingest is initialized. Each private audio cache receives that clock source,
and every worker receives a copy derived from that same object. Cache and worker
constructors will no longer infer origin from their own sample-rate parameter.

End-of-stream reconciliation will cover diarization, ASR, VAD, forced alignment,
speaker evidence, and business fusion. A mismatch is a test failure, not only a
debug log.

### 3.2 Typed evidence flow

`ComprehensiveTimeline` becomes the authoritative typed evidence store:

```text
audio ingest + session clock
  -> private pipeline audio stores
  -> diar / asr / vad raw tracks
  -> align / speaker-evidence tracks read from ComprehensiveTimeline
  -> business-speaker fusion track
  -> one terminal document containing every immutable raw track and the final view
```

The protocol layer remains responsible for topic envelopes, persistence, replay,
and WebSocket transport. It is not used as a private data path between workers.
Typed incremental cursors or subscriptions on `ComprehensiveTimeline` replace
the ASR `VadCache` pointer and direct align enqueue subscription.

The current speaker selection, text projection, and gap-fill policy moves from
the container into a registered `business_speaker` fusion pipeline. The
container may align and index track records but does not choose or invent track
content.

### 3.3 Identity and view contract

ASR allocates one ID before publishing a final and reuses it in all destinations.
The ASR terminal track serializes this ID explicitly. Forced alignment and the
business view use the same ID; no tool reconstructs IDs from list position.

The Web UI consumes the server's business-view revisions and terminal track. It
does not perform another speaker split. Browser tests assert that partial,
final, revision, reconnect, and terminal states converge without duplicate or
stale rows.

### 3.4 Configuration

Startup order is corrected to:

1. compile-time defaults;
2. `orator.toml`;
3. `ORATOR_*` environment variables;
4. CLI arguments.

Every behavioral environment switch is either represented by a typed TOML
field or removed from acceptance builds. The resolved runtime configuration is
included in the run manifest. The obsolete matrix mode in
`ws_unified_test.py` is removed because it labels overrides without applying
them and ranks candidates by unknown duration rather than business accuracy.

## 4. Phase 2: Build the Full Reference Turn Ledger

The 556 timestamped reference entries are reviewed against the complete audio.
For each entry, the reviewer records:

- stable reference ID and source line;
- real speaker and contextual summary;
- audible start/end on the absolute sample clock;
- overlap participants and intervals;
- critical/non-critical classification decided before candidate output review;
- acceptable semantic equivalents;
- any reference ambiguity.

Review occurs in two complete passes: chronological and by reversed 600-second
block order. Disagreements are resolved by replaying the audio in context. No
reference row is removed to simplify scoring. Empty, duplicate, or ambiguous
source rows remain in an adjudication log.

The review tools may seek audio and display system tracks beside a ledger row.
They do not assign correctness. Acceptance totals are derived only from the
signed manual judgments and independently checked against the ledger.

## 5. Phase 3: Establish the Reproducible Current Baseline

1. Correct and register real-WebSocket integration tests in CTest.
2. Run a clean build, warning check, all CTest tests, JavaScript syntax checks,
   and selected sanitizer checks.
3. Run 120 s, 360 s, 600 s, and full-length incremental WebSocket tests using
   the checked-in `orator.toml` without runtime tuning overrides.
4. Capture all live events, terminal tracks, UI state, server logs, and
   continuous `tegrastats`.
5. Perform the full 556-row contextual review on the final business view.
6. Publish the baseline score by full session, fixed 600-second block, speaker,
   criticality, boundary offset, uncertainty, and confident-wrong attribution.

This baseline replaces every earlier selected-window or script-derived accuracy
claim for comparison purposes.

## 6. Phase 4: Determine the Existing-Model Upper Bound

All candidate work uses the frozen baseline package until a runtime candidate is
selected.

### 6.1 Independent evidence extraction

For every natural turn candidate, preserve these inputs separately:

- Sortformer per-speaker posterior coverage, top-one/top-two margin, overlap,
  local slot, and temporal continuity;
- TitaNet similarity to each enrolled global speaker, top-one/top-two margin,
  audio duration, and clean-speech quality, computed per natural turn rather
  than per Sortformer label bucket;
- VAD speech support and pause endpoints;
- forced-alignment unit boundaries and gaps;
- ASR `text_id` and final span as a content/time anchor only;
- all source timestamps on the same absolute session clock.

TitaNet evidence must not be grouped by the Sortformer label it is intended to
check. This avoids making the second model repeat the first model's assignment.

### 6.2 Candidate decision model

The first candidate is a constrained sequence decision over natural turns:

- Sortformer supplies local ownership likelihood;
- TitaNet supplies independent global-identity likelihood;
- VAD and forced alignment supply legal boundaries;
- simultaneous speech supplies cannot-link constraints;
- temporal continuity is used only when current acoustic evidence supports it;
- low-evidence cases remain uncertain instead of inheriting a neighbour.

No transcript phrase, real speaker name, known timestamp, or reference label is
part of runtime logic. Each output includes an audit record containing source
evidence, chosen speaker, rejected alternatives, confidence margin, and reason.

### 6.3 Decision gate

The frozen candidate must reach at least 93 percent on both speaker-time and
natural-turn measures, retain 100 percent critical-turn correctness, and avoid
regression in every fixed 600-second block. If it does, its policy and TOML
parameters are frozen for runtime implementation.

If it does not, stop policy and threshold tuning. Compare stronger
overlap-aware diarization or speaker-verification models offline against trusted
oracles and the same ledger. A model is ported to C++/CUDA only when the offline
candidate exceeds the 93 percent gate and has a stage-by-stage validation plan.

## 7. Phase 5: Implement and Validate the Selected Candidate

Implementation proceeds in this order:

1. common clock and typed track contract;
2. stable ID and serialization contract;
3. independent per-turn speaker-evidence track;
4. registered business-speaker fusion track;
5. TOML configuration and resolved-config manifest;
6. Web UI convergence and telemetry display;
7. unit, integration, oracle, concurrency, and failure-path tests.

Each model-affecting commit runs the relevant PyTorch/NeMo oracle before the
next layer is changed. A tolerance is never widened to accept the implementation.

## 8. Phase 6: Validation Pyramid

### 8.1 Automated and numerical gates

- clean configure/build under `-Wall -Wextra` with no warnings;
- all C++ tests and registered Python real-WebSocket tests;
- ASR, Sortformer, TitaNet, VAD, and forced-alignment oracle gates for changed
  stages and the exact accepted profile;
- stable-ID, source-clock, track immutability, revision, alignment coverage,
  endpoint, empty-input, and reset/reconnect tests;
- selected AddressSanitizer/UndefinedBehaviorSanitizer host tests and CUDA
  memory checking where supported.

### 8.2 Real streaming levels

| Level | Purpose | Promotion rule |
|---|---|---|
| 120 s | startup, live UI, initial identities, endpoint and telemetry smoke | no contract/stability failure and full contextual review passes |
| 360 s | dense early multi-speaker exchanges and revision behavior | no regression versus 120 s; all IDs and align groups reconcile |
| 600 s | first standard accuracy block and sustained real-time operation | all 90 percent gates for the block pass |
| Full | long-session identity, tail, resource behavior, full semantics | all Spec 013 gates pass over every ledger row |

Tail clips may be used for diagnosis, but they do not count as full-session
acceptance because they reset model history.

### 8.3 Full acceptance runs

Run A starts from an empty isolated speaker registry. Run B starts after a
process restart with the enrolled registry produced by the accepted fixture.
Both use the same committed TOML and model/data files. Each run must pass the
full contextual review independently; averaged scores cannot hide one failure.

`tegrastats` remains active throughout. The final report includes sample count,
missing-field rate, min/mean/95th-percentile/max for CPU, RAM, GPU utilization,
GPU memory, temperature, GPU rail power, and total system power, plus any
thermal-throttle or allocation event.

### 8.4 Web UI acceptance

Use the actual browser with both file upload and microphone input. Verify live
partial text, VAD state, revisions, speaker labels, alignment lane, telemetry,
reconnect, end-of-stream, and JSON export. Browser state at terminal completion
must match the server terminal document by `text_id`, time span, text, and
speaker identity. Screenshots and browser-console output are retained as
evidence.

## 9. Phase 7: Final Sign-Off

Create one closing report containing:

- all reproducibility manifests and artifact hashes;
- oracle results and tolerances;
- automated test results;
- the complete signed turn ledger and contextual judgments;
- all required speaker, ASR, boundary, latency, resource, and stability results;
- both full-run comparisons;
- known limitations and the exact permitted product claim.

The project is marked closed only when there are no open priority-zero or
priority-one defects, every conjunctive gate passes, `PROJECT_STATE.md` matches
the accepted commit, and the final report is reviewed. A release tag is created
only after that review.

## 10. Rollback and Experiment Discipline

- One hypothesis family is changed at a time; the TOML diff and expected effect
  are recorded before execution.
- An experiment that fails a lower test level does not progress to full length.
- Accuracy regression in any fixed block rejects the candidate even if the full
  average rises.
- Failed runtime behavior is reverted with a normal revert commit; destructive
  history rewriting is not used.
- Frozen raw evidence and the signed reference ledger are never rewritten by a
  candidate tool.

