# Orator Constitution

The non-negotiable principles that govern every change to this project. Specs,
plans, tasks, and code are all subordinate to this document. When any artifact
conflicts with the Constitution, the Constitution wins. Amending it is a
deliberate, recorded act (see *Amendment Process*), never an implicit drift.

- **Version**: 1.0.0
- **Ratified**: 2026-06-12
- **Last amended**: 2026-06-12

---

## Article I — Zero Runtime Dependencies (Pure C++/CUDA)

1. The shipped runtime is **C++17 and CUDA only**. No third-party runtime
   libraries may be linked into the product (`orator`, `orator_ws`, the
   `orator_core` library, or any artifact that runs in production).
2. Permitted, and explicitly NOT considered third-party dependencies:
   - The C++ standard library and libc.
   - The CUDA toolkit runtime libraries (`cudart`, `cuBLAS`, `cuFFT`, …).
   - OS facilities reached through libc/POSIX (sockets, threads, mmap). The
     WebSocket server is implemented from scratch on raw POSIX sockets for this
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
4. A change that cannot be validated against a reference is not "done"; it is a
   spike, and must be labeled as such.

## Article III — Two Independent Pipelines on One Timeline

1. Diarization (speaker separation) and ASR are **independent pipelines**.
   Neither reads the other's results; neither blocks on the other.
2. Their only shared contract is the **input audio stream** and a **single
   absolute time base** (stream seconds). Audio entering the system is the sole
   coupling point: after it lands in the shared buffer, the two businesses
   proceed at their own pace ("whoever finishes first is fine").
3. Both pipelines emit onto **one unified timeline**. The terminal output of the
   system is always a timeline carrying BOTH speaker-separation segments AND ASR
   transcript content on the shared clock.
4. The pipelines are decoupled from concrete models by interfaces
   (`core::IDiarizer`, `core::IAsr`, `core::ITimelineMerger`, …) plus a registry.
   Swapping a model is a registration/config change, never an edit to a consumer.

## Article IV — Streaming Is Real, Tests Prove It

1. The system is a **real-time streaming** system. The authoritative path is:
   audio arrives incrementally → shared buffer → pipelines consume continuously.
2. Tests MUST exercise the real end-to-end path and assert on the **real
   terminal output** (the unified timeline JSON). Convenience shortcuts that
   bypass the streaming path are not acceptable as validation of streaming
   behavior.
3. Specifically: streaming behavior is validated by pushing audio **through the
   WebSocket transport as an incremental stream** (optionally at an accelerated
   real-time multiple), never by handing a whole clip to a single offline call.
   Offline whole-clip tools may exist for component debugging, but they do not
   count as streaming validation.
4. Performance is reported honestly and from the real path. Real-time factors
   are measured on the streaming pipeline under stated clock/thermal conditions,
   and clip-based numbers are never presented as if they were streaming numbers.

## Article V — Engineering Quality Is a Requirement, Not a Preference

This project is delivered with high engineering taste and very high code
quality. The following are acceptance criteria, enforced in review:

1. **Readability**
   - Google C++ Style: 2-space indent, `PascalCase` types/methods,
     `lower_snake_case` locals, trailing-underscore `member_` fields,
     `I`-prefixed interfaces, `#pragma once` headers.
   - Every translation unit and public header opens with a doc comment stating
     its purpose and its place in the system. Comments explain **why**, not
     **what**; they justify non-obvious decisions and cite the contract or
     reference being honored.
   - Names reveal intent. No abbreviations that a newcomer must decode.

2. **Organization**
   - Strict layering by directory: `core/` (types + interfaces), `model/`
     (concrete CUDA models), `feature/`, `io/`, `pipeline/` (orchestration),
     `net/` (transport), `gpu/` (device memory). Dependencies point inward
     toward `core/`; consumers depend on interfaces, never on concrete models.
   - One responsibility per type and per file. Public headers expose the
     contract; implementation detail stays in the `.cc`/`.cu`.

3. **Maintainability**
   - No duplicated logic; shared behavior is factored once. No dead code, no
     commented-out code, no "fallback" kept "just in case" without a stated
     reason.
   - Resources are RAII-owned (`std::unique_ptr`, `std::shared_ptr`, owning GPU
     buffers). No raw `new`/`delete`/`cudaMalloc` on a hot path that isn't
     wrapped by an owner. Every CUDA call is error-checked.
   - Functions are small and single-purpose. Deep nesting and long parameter
     lists are refactored, not tolerated.

4. **Extensibility**
   - New models/stages are added by implementing an interface and registering
     it — never by editing consumers. New behavior is added behind the existing
     contracts wherever possible.
   - Configuration is explicit and typed (`*Config` structs), not magic
     constants scattered through code. Tunable knobs carry documented defaults.

5. **Concurrency discipline** (applies as the threaded pipeline lands)
   - Shared state crosses threads only through clearly-owned, documented
     synchronization primitives. Every shared field states which lock guards it.
   - No data races: this is verified, not assumed. Thread lifecycles
     (start/stop/join) are deterministic and owned by a single controller.

6. **Implementation discipline**
   - Make the change that is requested or clearly necessary — nothing more. No
     speculative features, no unrequested refactors riding along with a fix.
   - Validate every change: it builds clean (`-Wall -Wextra`, no new warnings),
     the full test suite passes, and any behavioral claim is backed by a run.

## Article VI — Process: Spec-Driven Development

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
   in sync with reality and consulted before acting.

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
- When guidance is silent, Articles II (accuracy) and V (quality) are the
  tie-breakers.

**Version 1.0.0 · Ratified 2026-06-12 · Last amended 2026-06-12**
