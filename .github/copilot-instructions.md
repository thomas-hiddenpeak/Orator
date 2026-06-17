# Orator Project Prompt (Spec-Kit SDD)

You are working in the Orator repository. Follow Spec-Driven Development (SDD) adapted from spec-kit.

## Source of truth
- Constitution: `.specify/memory/constitution.md`
- Project state: `specs/PROJECT_STATE.md`
- Active SDD artifacts: `specs/NNN-feature-name/spec.md`, `plan.md`, `tasks.md`
- Test review protocol artifact: `specs/006-test-review-protocol/spec.md`

If any instruction conflicts, the Constitution wins. **The code is authoritative
over every state document** (Constitution Article VIII): when a doc and the code
disagree, the code is correct and the doc is a defect to fix. Do not act on a
documented state claim you have not confirmed against the code.

## Documentation–code consistency (Constitution Article VIII)
Stale state docs are the documented cause of context pollution and avoidable
rework. Enforce these every session:
- **Verify before trusting.** Before relying on a claim in `PROJECT_STATE.md` or a
  spec/tasks status line — at session start, and whenever a claim drives a coding
  or testing decision — confirm it against the code: locate the symbol
  (grep/usages), build, and run the relevant test. A clean build + full `ctest`
  pass is the consistency proof.
- **Status advances with the code, in the same change.** When implementation
  lands, update the matching status line (`Draft`/`Revised`/`In progress` →
  `Implemented`) and `PROJECT_STATE.md` with the commit reference, as part of the
  same change. A shipped capability left `Draft`, or a removed/renamed component
  still described as live, is a defect to correct.
- **Claims carry evidence.** A state claim names how to confirm it: defining
  symbol/file, a passing test, and the landing commit.
- **Label retained-but-inactive code.** A type that still compiles but is off the
  runtime path (reference/test-only) is documented as retained and inactive so it
  is not mistaken for live behavior.

## Required workflow (non-trivial work)
1. Understand current state and constraints.
2. Create or update `spec.md` (WHAT and WHY, testable requirements).
3. Create or update `plan.md` (HOW: architecture, data flow, threading, risks).
4. Create or update `tasks.md` (small, ordered, independently verifiable tasks).
5. Implement only after spec, plan, and tasks are reviewed/approved.
6. Validate with build + tests + real streaming path measurements when applicable.
7. Record verified lessons in `/memories/repo/` and keep docs in sync.

## Constitutional hard rules
- Runtime dependency policy: production runtime is pure C++17/CUDA only. No new runtime third-party libraries.
- Accuracy first: do not trade measurable quality for speed unless explicitly approved and validated.
- Dual pipeline model: diarization and ASR are independent pipelines sharing only input audio and one absolute time base.
- Streaming validation: validate through the real WebSocket streaming path, not whole-file shortcut runs.
- Engineering quality is mandatory: readability, layering, RAII, race-free concurrency, small focused functions.
- Terminology standard: precise engineering language, no metaphor-heavy wording in docs.

## Implementation boundaries
- Prefer minimal, targeted changes. Avoid unrelated refactors.
- Keep layering intact:
  - `core/` contracts and data types
  - `model/` concrete model implementations
  - `pipeline/` orchestration
  - `net/` transport
  - `gpu/` device-memory and synchronization utilities
- New capability should be added behind interfaces and registration, not by tightly coupling consumers to concrete models.

## Validation checklist (before claiming done)
- Build is clean with no new warnings under `-Wall -Wextra`.
- Relevant tests pass (`ctest --output-on-failure`).
- For streaming behavior/perf claims: run through WebSocket path and report measured numbers with units and conditions.
- For accuracy-sensitive changes: compare against reference/oracle outputs and report tolerance or quality metrics.
- Output contract compatibility is preserved unless the spec explicitly changes it.
- State docs match the code: `PROJECT_STATE.md` and the affected spec/tasks status lines are updated to reality with commit references (Constitution Article VIII).

## Response style for engineering updates
- Be explicit and measurable.
- Distinguish facts from assumptions.
- If blocked, state blocker, impact, and the smallest next action.
- For reviews, prioritize findings (bugs, risks, regressions, missing tests) over summaries.

## Quick start commands
- Configure: `cmake -S . -B build`
- Build: `cmake --build build -j`
- Test: `cd build && ctest --output-on-failure`
