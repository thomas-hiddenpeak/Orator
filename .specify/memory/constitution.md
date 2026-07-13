# Orator Constitution

The mandatory principles that govern every change to this project. Specs,
plans, tasks, and code are all subordinate to this document. When any artifact
conflicts with the Constitution, the Constitution takes precedence. Amending it
is a deliberate, recorded action (see *Amendment Process*), not an undocumented
change.

- **Version**: 1.6.0
- **Ratified**: 2026-06-12
- **Last amended**: 2026-07-13

---

## Article I — Zero Runtime Dependencies (Pure C++/CUDA)

1. The shipped runtime is **C++20 and CUDA only**. No third-party runtime
   libraries may be linked into the product (`orator`, `orator_ws`, the
   `orator_core` library, or any artifact that runs in production).
2. Permitted, and explicitly NOT considered third-party dependencies:
   - The C++ standard library and libc.
   - The CUDA toolkit **base only**: `cudart` (the CUDA runtime API) plus the
     core CUDA C++ language, intrinsics, and PTX. Pinned to **CUDA 13.3**.
   - OS facilities reached through libc/POSIX (sockets, threads, mmap).
   - A **named, closed carve-out** of two already-integrated boundary-
     infrastructure libraries: `libwebsockets` (WebSocket transport) and
     `tomlplusplus` (config parsing). This list is **closed**: no additional
     external library may be added, and neither may grow into compute/operator
     territory.
3. **NOT permitted on any runtime path.** Operator/compute libraries must be
   referenced, then reimplemented in-project: `cuBLAS`, `cuBLASLt`, `cuFFT`,
   `cuDNN`, `CUTLASS`, `Thrust`, `CUB`, and any equivalent. Rationale: depending
   on a closed operator library leaves no room for the kernel-level
   modifications the project requires — epilogue fusion, CUDA-graph
   capturability, allocation-free / stream-explicit execution, and full
   numerical audit. (Migration of the one remaining such dependency, `cuBLAS` in
   `asr_gemm`, is tracked under Spec 002.)
4. Vendored sources (e.g. `third_party/minimp3`) are allowed **only** in offline
   tooling and tests, never on a runtime path.
5. Python, PyTorch, and similar tools are permitted **only** as offline oracles
   and dump utilities under `tools/`. They never enter the build of a runtime
   artifact.
6. Adding any new dependency requires a Constitution amendment. There is no
   "temporary" exception.

**Rationale**: the deployment target is a Jetson edge device; a self-contained
binary with a fixed, auditable surface is a hard product requirement.

## Article II — Accuracy Is the Primary Metric

1. Correctness outranks speed, memory, and convenience. A faster or simpler
   change that measurably degrades transcription/diarization quality is rejected
   by default.
2. Every model stage MUST be numerically validated against a trusted reference
   (the PyTorch oracle for ASR, NeMo for diarization) before it is considered
   done. Validated tolerances are recorded in the relevant spec and in
   `/memories/repo/`.
3. Numerical-fidelity changes (precision reduction, quantization, kernel
   fusion, approximations) are **opt-in and gated**: they require an explicit
   accuracy comparison against the prior verified output, and they are not
   merged without sign-off. Quantization in particular is deferred until
   explicitly scheduled by the project owner.
4. A change that cannot be validated against a reference is not complete; it is
   an unvalidated experiment, and must be labeled as such.

## Article III — Common Time Base and Independent Pipelines

The common time base is the foundational mechanism that enables all pipelines to
exist as independent units and coordinate through the comprehensive timeline.
Every registered pipeline MUST acquire, maintain, and report time data through
the common time base — without exception.

### 3.1 — Common Time Base as the Foundation

1. A **common time base** (`core::TimeBase`) is the shared clock for the entire
   system. It is defined exactly once per session by the audio-ingest owner,
   anchored at that session's origin (absolute sample 0 = stream start), and
   retained as one immutable session value. The ingest implementation MAY fan
   audio into private per-pipeline stores so one consumer cannot retain another
   consumer's memory; storage ownership does not create another time origin.
2. The common time base is a **mandatory prerequisite** for every registered
   pipeline and audio store. Each MUST receive its time base from the session
   audio-ingest owner, not construct an independent instance. Deriving a time
   base from `params.sample_rate`, a cache's sample rate, or any other local
   parameter is a violation even when the resulting numbers happen to match.
3. A pipeline holds the common time base as a member field (e.g.
   `core::TimeBase tb_`) received from that canonical source and derives all time
   codes from it. An immutable value copy is allowed; an independently
   constructed origin is not. The time base is never created on demand per
   operation.

### 3.2 — Three Consistency Principles

Every registered pipeline MUST satisfy the following three consistency
requirements with respect to the common time base. These are enforced in review;
a pipeline that violates any one of them is rejected.

4. **Origin consistency (起点一致性)**: Every pipeline derives its time origin
   from the session audio-ingest owner's common time base. The absolute sample
   position of t = 0 is identical across all pipelines by construction, not by
   repeating equivalent constructor arguments. A pipeline MUST NOT set its own
   origin independently.
5. **Process consistency (过程一致性)**: During internal processing, a pipeline
   MUST compute all time codes through the common time base's conversion methods
   (`SecondsAt`, `SampleAt`, `Duration`). A pipeline MUST NOT derive time codes
   through ad hoc arithmetic (`sample / sample_rate`, local counters, manual
   offset addition). Every time value produced internally — segment boundaries,
   frame start times, window anchors, cursor positions — MUST pass through the
   common time base. A sub-stream anchored at an arbitrary absolute sample MUST
   use `TimeBase::Derive()` and `LocalSeconds()`.
6. **Result consistency (结果一致性)**: Every datum a pipeline reports to the
   comprehensive timeline MUST carry absolute start and end times (in seconds)
   on the common time base. These time codes MUST be computed through the
   pipeline's own common time base instance. A pipeline MUST NOT report time
   codes derived by any other mechanism. End-of-stream reconciliation
   (`TimeBase::ReconcileExtent`) MUST confirm the pipeline's processed extent
   equals the common clock total.

### 3.3 — Pipeline Independence and the Comprehensive Timeline

7. Pipelines are **independent units**. One pipeline MUST NOT read another
   pipeline's results. One pipeline MUST NOT block on another pipeline's progress.
8. Pipelines communicate **only through the comprehensive timeline**
   (`pipeline::ComprehensiveTimeline`). A pipeline deposits its data (with time
   codes on the common base) into the comprehensive timeline and reads another
   pipeline's data from the same source. Direct data exchange between pipelines
   — callbacks, shared pointers, atomic flags, or any mechanism that bypasses
   the comprehensive timeline — is a violation.
9. The comprehensive timeline is a **pure container and alignment layer**. It
   never modifies, infers, substitutes, or back-fills a pipeline's content. Each
   pipeline is solely responsible for its own output content AND its own accurate
   time codes on the common base.
10. The terminal output of the system is **one comprehensive timeline document**:
    a single structure with a shared time axis containing one track per pipeline.
    Adding a pipeline adds a track; existing tracks and the document schema are
    unchanged.

### 3.4 — Model Decoupling

11. Pipelines are decoupled from concrete models by interfaces
    (`core::IDiarizer`, `core::IAsr`, ...) plus a registry. Replacing a model is
    a registration or configuration change, not an edit to a consumer.

## Article IV — Streaming Validation Through the Real Transport

1. The system is a **real-time streaming** system. The primary execution path
   is: audio arrives incrementally → the session ingest assigns absolute sample
   positions and fans out to pipeline-owned stores → pipelines consume it
   continuously on the one common time base.
2. Tests MUST exercise the complete end-to-end path and assert on the **actual
   terminal output** (the comprehensive timeline JSON). A test that bypasses the
   streaming path does not validate streaming behavior.
3. Streaming behavior is validated by sending audio **through the WebSocket
   transport as an incremental stream** (optionally faster than real time),
   not by passing a complete recording to a single non-streaming call.
   Non-streaming whole-recording tools may exist for component debugging, but
   they do not constitute streaming validation.
4. Performance is reported only from the real streaming path. Real-time factors
   are measured on the streaming pipeline under stated clock and thermal
   conditions. A measurement taken from a non-streaming whole-recording run is
   never reported as a streaming result.

## Article V — Engineering Quality Is a Requirement

This project is delivered to a high standard of code quality. The following are
acceptance criteria, enforced in review:

- Readability, organization, maintainability, extensibility, concurrency safety.
- No metaphors or jargon in documentation; precise engineering language only.
- No speculative features or unrelated refactors; make only the requested change.

## Article VI — Testing Principles

### 6.1 — Test Audio and Reference Standard

1. Canonical pipeline acceptance tests MUST use `test.mp3` as the audio source
   and `test.txt` as the reference. Supplemental recordings never replace this
   mandatory gate.
2. Product-safety tests MAY additionally use generated silence, confirmed-silent
   excerpts, controlled noise, and live microphone input to validate endpoint,
   hallucination, transport, and device behavior for which `test.txt` is not an
   applicable reference.
3. A general industrial-readiness claim MUST additionally use locked holdout
   recordings that were not used for implementation or tuning. Their source,
   consent/status, acoustic conditions, speakers, reference construction, and
   hashes MUST be recorded. Holdout results are reported separately from the
   canonical `test.mp3` result.
4. Actual pipeline testing MAY use different input pacing speeds, but acceptance
   reports state the speed and include real-time pacing where required.
5. When comparing accuracy results, NO script analysis MUST assign correctness
   or select a candidate, and NO temporary command-line scripts may be created
   for that purpose. Results MUST be compared item by item with the reference in
   conversational context. Tools may capture, index, and display evidence, but
   they do not make the accuracy judgment.

### 6.2 — Test Levels and Device Metrics

1. Tests are divided into 120s, 360s, 600s, and full-length tests.
2. During testing, device operation metrics such as power, CPU, GPU, and RAM usage MUST be observed in addition to business metrics.
3. Since the device is Jetson, support for `nvidia-smi` is incomplete; related data MUST be obtained through `tegrastats`.

### 6.3 — Speaker Diarization Accuracy Evaluation

1. When comparing results placed in context, the speaker separation accuracy statistics MUST provide the accuracy of the total comparison of speaker segmentation time blocks.
2. If there are offsets, the offsets MUST be annotated.
3. Other metrics of speaker segmentation MUST also be annotated and their meanings explained.

### 6.4 — ASR Accuracy Evaluation

1. The ASR accuracy comparison MUST use semantic comparison, meaning character-level comparison MUST NOT be used.
2. For example: the number 1 and the Chinese character "一" (one) should be considered equivalent.

### 6.5 — Unified Test Script Requirement

1. There MUST be only one Python script for the test WebSocket client globally, using a unified script for testing to ensure semantic consistency of test parameters. This script is for test execution only, not for result comparison or accuracy analysis.
2. If test parameter meanings or test conditions/methods change, the values of the same state before and after the change MUST be recorded and compared.

---

## Article VII — Amendment Process

1. **Readability**
   - Google C++ Style: 2-space indent, `PascalCase` types/methods,
     `lower_snake_case` locals, trailing-underscore `member_` fields,
     `I`-prefixed interfaces, `#pragma once` headers.
   - Every translation unit and public header opens with a documentation comment
     stating its purpose and its role in the system. Comments explain the
     reason for a decision, not a restatement of the code; they justify
     non-obvious choices and cite the contract or reference being implemented.
   - Names state their intent. No abbreviations that require decoding.

2. **Organization**
   - Strict layering by directory: `core/` (types + interfaces), `model/`
     (concrete CUDA models), `feature/`, `io/`, `pipeline/` (orchestration),
     `net/` (transport), `gpu/` (device memory). Dependencies point inward
     toward `core/`; consumers depend on interfaces, never on concrete models.
   - One responsibility per type and per file. Public headers expose the
     contract; implementation detail stays in the `.cc`/`.cu`.

3. **Maintainability**
   - No duplicated logic; shared behavior is defined once. No dead code, no
     commented-out code, and no unused fallback path retained without a stated,
     written reason.
   - Resources are owned through RAII (`std::unique_ptr`, `std::shared_ptr`,
     owning GPU buffers). No raw `new`/`delete`/`cudaMalloc` on a
     performance-critical path without an owning wrapper. Every CUDA call is
     error-checked.
   - Functions are small and single-purpose. Deep nesting and long parameter
     lists are refactored.

4. **Extensibility**
   - New models/stages are added by implementing an interface and registering
     it — never by editing consumers. New behavior is added behind the existing
     contracts wherever possible.
   - Configuration is explicit and typed (`*Config` structs), not unnamed
     constants distributed through the code. Tunable parameters carry documented
     defaults.

5. **Concurrency discipline**
   - Shared state crosses threads only through clearly-owned, documented
     synchronization primitives. Every shared field states which lock guards it.
   - No data races: this is verified, not assumed. Thread lifecycles
     (start, stop, join) are deterministic and owned by a single controller.

6. **Implementation discipline**
   - Make the change that is requested or clearly necessary, and no more. No
     speculative features, and no unrequested refactoring combined with a fix.
   - Validate every change: it builds with no new warnings (`-Wall -Wextra`),
     the full test suite passes, and any behavioral claim is supported by a run.

## Article VIII — Documentation and Terminology Standards

All project documents (the Constitution, specs, plans, tasks, READMEs, code
comments, and commit messages) MUST be written so that a reader unfamiliar with
the recent conversation can understand them months later. The following are
enforced in review:

1. **Use standard, precise engineering terminology.** Name the actual mechanism,
   data structure, or measurement. Examples: "mutex", "condition variable",
   "read cursor", "absolute sample index", "real-time factor", "end-of-stream
   flag".
2. **No metaphors, analogies, or figurative language.** Do not describe behavior
   with imagery (for example racing, draining, barriers as physical objects,
   anthropomorphism such as "the worker waits its turn"). Describe the mechanism
   directly (for example "blocks on the condition variable until new samples are
   available or the end-of-stream flag is set").
3. **No jargon, slang, or insider shorthand.** Do not use informal phrases whose
   meaning depends on context that will be lost over time (for example "whoever
   finishes first", "just in case", "bolted on", "spike", "magic constant").
   State the precise condition or fact instead.
4. **Define terms once and use them consistently.** A concept has exactly one
   name across all documents and code. Do not introduce synonyms for the same
   thing.
5. **Quantify rather than characterize.** Prefer measured numbers with units and
   stated conditions over qualitative words. Do not assert a property (for
   example correctness or performance) without the evidence or the method used
   to obtain it.
6. **Spell out an acronym or abbreviation at first use** in each document, then
   use it consistently.

A document that relies on metaphor or undefined jargon is rejected in review and
rewritten in plain, standard terminology.

## Article IX — Configuration Consistency

1. **Configuration consistency**: always use `orator.toml` configuration file to ensure parameter consistency across all server starts and tests. Do not hardcode parameters in command lines or code. Follow the configuration loading order: 1) Compile-time defaults, 2) `orator.toml`, 3) Environment variables (ORATOR_*), 4) CLI arguments.

## Article X — Process: Spec-Driven Development

1. Work of non-trivial scope follows the SDD flow, adapted from spec-kit:
   `constitution → spec → plan → tasks → implement`. Each artifact lives under
   `specs/NNN-feature-name/` and is reviewed before the next phase begins.
2. The **spec** captures WHAT and WHY (behavior, contracts, acceptance criteria)
   with no premature implementation detail. The **plan** captures HOW (design,
   data flow, threading). **Tasks** are small, ordered, independently verifiable
   steps.
3. Specs are testable: every requirement maps to a way to verify it. Ambiguities
   are resolved (or explicitly marked) before planning.
4. Memory under `/memories/repo/` records verified facts and lessons; it is kept
   in sync with reality and consulted before acting. The same synchronization
   duty applies to every state document (`specs/PROJECT_STATE.md`, each spec's
   and tasks' status line): see Article VIII.

## Article XI — Documentation–Code Consistency

State documents describe the project; the code IS the project. When the two
disagree, the code is authoritative and the document is a defect to be corrected
immediately. The following are enforced in review and before acting on any
state claim:

1. **Code is the source of truth; state docs are subordinate.** `PROJECT_STATE.md`,
   spec/tasks status lines, and `/memories/` describe the state of the code. A
   document that contradicts the code is wrong by definition and is fixed, not
   trusted. A reader must never act on a documented state claim that the code
   does not support.
2. **Every state claim is verifiable, and carries its evidence.** A claim that a
   capability is implemented states HOW to confirm it against the code: the
   defining symbol/file, a passing test, and the commit reference that landed
   it. A claim with no verification path is not allowed in a state document.
3. **Status advances with the code, in the same change.** When a task or spec's
   implementation lands, its status line is advanced in that same change
   (`Draft`/`Revised`/`In progress` → `Implemented`) with the commit reference.
   A shipped capability left marked `Draft`/`In progress`, or a removed/renamed
   component still described as live, is a consistency defect.
4. **Verify before trusting, especially on resume.** Before relying on a state
   document — at the start of a work session, or whenever a claim drives a
   testing or coding decision — confirm it against the code (locate the symbol,
   build, run the relevant test). A clean build with the full test suite passing
   is the consistency proof. Reasoning from an unverified stale document is the
   documented cause of avoidable rework and is not acceptable.
5. **Retained-but-inactive components are labeled as such.** A type that still
   exists and compiles but is no longer on the runtime path (for example a
   reference or test-only artifact) is documented explicitly as retained and
   inactive, so it is not mistaken for live behavior.

## Governance

### Amendment Process
- Amendments are proposed as an explicit change to this file, with a bumped
  version and a one-line rationale in the change description, and are committed
  as their own deliberate act.
- Versioning is semantic for governance:
  - **MAJOR**: a principle is removed or fundamentally redefined.
  - **MINOR**: a new principle/article or materially expanded guidance.
  - **PATCH**: clarifications and wording that do not change meaning.

### Compliance
- Every spec, plan, and task set states how it complies with the Constitution
  (a "Constitution Check"). A violation must be resolved, or justified and
  recorded, before implementation proceeds.
- Reviews reject changes that violate an Article without a recorded amendment.
- When guidance is silent, Articles II (accuracy) and V (quality) take
  precedence over other considerations.

**Version 1.6.0 · Ratified 2026-06-12 · Last amended 2026-07-13**
