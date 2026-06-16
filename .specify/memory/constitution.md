# Orator Constitution

The mandatory principles that govern every change to this project. Specs,
plans, tasks, and code are all subordinate to this document. When any artifact
conflicts with the Constitution, the Constitution takes precedence. Amending it
is a deliberate, recorded action (see *Amendment Process*), not an undocumented
change.

- **Version**: 1.2.1
- **Ratified**: 2026-06-12
- **Last amended**: 2026-06-16

---

## Article I — Zero Runtime Dependencies (Pure C++/CUDA)

1. The shipped runtime is **C++17 and CUDA only**. No third-party runtime
   libraries may be linked into the product (`orator`, `orator_ws`, the
   `orator_core` library, or any artifact that runs in production).
2. Permitted, and explicitly NOT considered third-party dependencies:
   - The C++ standard library and libc.
   - The CUDA toolkit runtime libraries (`cudart`, `cuBLAS`, `cuFFT`, …).
   - OS facilities reached through libc/POSIX (sockets, threads, mmap). The
     WebSocket server is implemented directly on POSIX sockets for this
     reason — networking is an OS facility, not a dependency.
3. Vendored sources (e.g. `third_party/minimp3`) are allowed **only** in offline
   tooling and tests, never on a runtime path.
4. Python, PyTorch, and similar tools are permitted **only** as offline oracles
   and dump utilities under `tools/`. They never enter the build of a runtime
   artifact.
5. Adding any new dependency requires a Constitution amendment. There is no
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

## Article III — Independent Pipelines on a Comprehensive Timeline

1. Diarization (speaker separation) and ASR are **independent pipelines**.
   Neither reads the other's results; neither blocks on the other.
2. Their only shared inputs are the **input audio stream** and a **single
   absolute time base** (stream seconds). Audio entering the system is the sole
   point of coupling: after it is written to the shared buffer, each pipeline
   processes it at its own rate and in any order.
3. The terminal output of the system is **one comprehensive timeline**: a single
   document with a shared time axis that contains one **track** per pipeline.
   Each track holds that pipeline's time-ordered entries (for example a speaker
   diarization track and an ASR transcript track). The timeline always carries
   the diarization track and, when ASR is enabled, the ASR track.
4. The timeline is **extensible by adding tracks**. A new pipeline contributes a
   new track; existing tracks and the document schema are unchanged. Combining
   tracks (attributing transcript text to a speaker by time alignment) is
   performed by a separate component (`pipeline::ComprehensiveTimeline`) and does
   not alter how each pipeline produces its track.
5. The pipelines are decoupled from concrete models by interfaces
   (`core::IDiarizer`, `core::IAsr`, …) plus a registry. Replacing a model is a
   registration/configuration change, not an edit to a consumer.

## Article IV — Streaming Validation Through the Real Transport

1. The system is a **real-time streaming** system. The primary execution path
   is: audio arrives incrementally → shared buffer → pipelines consume it
   continuously.
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

## Article VI — Documentation and Terminology Standards

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

## Article VII — Process: Spec-Driven Development

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

## Article VIII — Documentation–Code Consistency

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

**Version 1.2.1 · Ratified 2026-06-12 · Last amended 2026-06-16**
