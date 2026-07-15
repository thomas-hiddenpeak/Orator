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

The WebSocket session has one audio producer and any number of observers. The
first connection that sends audio owns production until it ends, resets, or
disconnects. Opening an observer never calls `AuditoryStream::Reset()` and all
stream-generated events are broadcast to every connected observer. A second
connection that attempts to send audio while a producer is active receives an
explicit error and its bytes are not ingested. The unified WebSocket client
opens a concurrent observer in the registered integration gate and requires
the producer and observer to receive identical terminal timeline content. A
real browser is also connected before a promotion stream to verify that live
rows and telemetry update while the unified client remains the producer.

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

The migration keeps environment overrides only at the process entry point,
where they are parsed into `AuditoryStream::Config` before any model is
constructed. Model, GPU, and transport implementations never call `getenv()`.
The accepted typed controls are:

- `[asr]`: system prompt, EOS-ban steps, decode batch, profiling, encoder
  attention mode, and CUDA-graph enablement;
- `[align]`: profiling;
- `[debug]`: log level, time-base diagnostics, Sortformer progress, GPU
  scheduling, and the optional WebSocket text-frame log path.

Legacy kernel-forcing variables used only for local A/B experiments are
removed rather than promoted to product configuration. The terminal timeline
contains a canonical `resolved_config` object after defaults, TOML,
environment, and CLI resolution. The unified client copies that object into a
sidecar run manifest and hashes the canonical representation.

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

## 5. Phase 3: Establish the Reproducible v2.1 Closing Baseline

The closing line is fixed to streaming Sortformer v2.1 with the checked-in
`340/1/188/188` profile. Compile-time defaults and `orator.toml` select the same
weight file and profile. The v2 checkpoint and its obsolete CTest are removed;
only prior reports and hashes remain to explain historical implementation
findings. They cannot enter candidate selection or acceptance totals.

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

This v2.1 baseline replaces every earlier selected-window, v2, or script-derived
accuracy claim for closing purposes. The existing `413/556` contextual result
is the starting diagnostic, not an accepted score.

## 6. Phase 4: Determine the Existing-Model Upper Bound

All candidate work uses the frozen v2.1 baseline package until a runtime
candidate is selected. Evidence from v2 may explain a regression but cannot
decide a candidate.

Before comparing business candidates, the exact Sortformer execution profile is
checked against NeMo. The gate binds the source checkpoint revision and hash,
converted runtime weight hash, TOML hash, processed-input hash, and output
fixture hash. For an asynchronous profile, the oracle fixture crosses at least
three chunks, retains output produced after repeated cache compression, and
verifies that model input is
`speaker cache + FIFO + current chunk`, that no frame is discarded before FIFO
overflow transfer, and that the official cache-update period controls the
transfer. A mismatch is a model-port defect and is corrected before threshold
tuning or model replacement continues.

The 2026-07-15 correction first isolated an implementation defect with NVIDIA
v2. The exact v2 runtime profile now passes a five-chunk NeMo oracle at
`max_abs=1.43051e-6`; unsupported local cache controls were removed, and stale
TOMLs containing them fail loading. The corresponding full-session diagnostic
changed assignments throughout the recording without improving the frozen
written-context candidate, so model/fusion escalation continues without
treating FIFO parity as an accuracy fix. This is historical remediation
evidence, not the closing model gate. See
`sortformer-oracle-2026-07-15.md`.

The first v2.1 comparison changed only the model weights and retained the legacy
Orator asynchronous profile (`188/340/188`, FIFO `188`, context `1+1`). This is
useful for isolating checkpoint differences, but it is not the v2.1 deployment
upper bound. NVIDIA's published v2.1 model card recommends two different
streaming profiles, all values in 80 ms frames:

- high latency: chunk/right/FIFO/update `340/40/40/300`;
- low latency: chunk/right/FIFO/update `6/7/188/144`.

Both official profiles must receive separate multi-chunk NeMo/C++ numerical
gates and full-session frozen diar evidence before another integrated
real-WebSocket run. The high-latency profile is an accuracy diagnostic unless
its 30.4-second input-buffer latency is explicitly accepted; the 1.04-second
low-latency profile is the deployment candidate when its compute throughput and
contextual accuracy pass. The checkpoint-native synchronous profile remains a
separate diagnostic and cannot validate either asynchronous candidate.

The 2026-07-15 screening completed these gates. High latency passed numerical
parity but recorded 385 / 170 / 1 natural turns; low latency passed numerical
parity but recorded 377 / 178 / 1. Both are below the inherited v2.1
real-WebSocket diagnostic of 413 / 142 / 1 and below the 90 percent gate, so
neither official profile advances to another integrated run. Work returns to
reference-free multi-pipeline evidence fusion on the active inherited v2.1
profile. The owner decision on 2026-07-15 designates that profile as the sole
closing baseline because it is the strongest deployable v2.1 result. This does
not waive the 90 percent product gate or the 93 percent frozen-candidate
promotion gate.

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
Before runtime integration, generate a reference-free rolling TitaNet track at
multiple TOML-defined window sizes. Windows are paired by their absolute centre
time, ranked only against identities active in the captured session, and
accepted only when the configured number of scales agree and independently
pass the configured score and margin gates. The resulting points and runs are
evidence only: they do not read `test.txt`, assign correctness, or mutate any
captured pipeline track.

### 6.2 Candidate decision model

The first candidate is a constrained sequence decision over natural turns:

- Sortformer supplies local ownership likelihood;
- TitaNet supplies independent global-identity likelihood;
- VAD and forced alignment supply legal boundaries;
- simultaneous speech supplies cannot-link constraints;
- temporal continuity is used only when current acoustic evidence supports it;
- low-evidence cases remain uncertain instead of inheriting a neighbour.

Rolling voiceprint transitions may split or rewrite the business view only at
VAD or forced-alignment boundaries on the same session clock. Known-speaker
overrides require sustained multi-scale evidence; filling an unknown span may
use a separately configured lower gate. A boundary that cannot be supported by
the alignment/VAD tracks remains unchanged.

The 2026-07-15 T059G frozen experiment implements this decision model with
3-second and 5-second native TitaNet windows at a 1-second step. Existing
business-span edges are preserved for whole-span decisions; candidate-strength
partial rewrites require a forced-alignment pause. The policy changes nine of
936 source entries. Full contextual review of all 11 affected reference rows
finds five repairs and no regression, for 418 / 137 / 1 (`75.1799%`). This fails
the 93 percent gate below, so the experiment is not integrated and no further
policy tuning is permitted. See `speaker-sliding-v21-2026-07-15.md`.

No transcript phrase, real speaker name, known timestamp, or reference label is
part of runtime logic. Each output includes an audit record containing source
evidence, chosen speaker, rejected alternatives, confidence margin, and reason.

### 6.3 Decision gate

The frozen candidate must reach at least 93 percent on both speaker-time and
natural-turn measures, retain 100 percent critical-turn correctness, and avoid
regression in every fixed 600-second block. If it does, its policy and TOML
parameters are frozen for runtime implementation.

If it does not, stop policy and threshold tuning. First compare every already
ported, deployable streaming checkpoint under the same profile against trusted
oracles and the same ledger. A replacement model is considered only when it has
a feasible native deployment path, exceeds the 93 percent gate, and has a
stage-by-stage validation plan. Offline-only diarization models are excluded
from the model-selection path; the manually adjudicated reference already
supplies acceptance truth.
Model escalation is also paused whenever the current runtime profile fails its
trusted oracle, because an unfaithful port cannot establish the existing-model
upper bound.

The 2026-07-15 owner review removed an exploratory offline Sortformer/Pyannote
branch from the work product. It duplicated the role of `test.txt` and could
not produce a native streaming deployment candidate. NeMo remains only as the
same-checkpoint numerical oracle required to validate the v2.1 C++/CUDA port.

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
