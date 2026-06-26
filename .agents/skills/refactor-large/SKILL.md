# Refactor Large — Large-Scale Refactoring Strategies

**Domain:** Large-scale refactoring, architectural changes, codebase restructuring
**Triggers:** major refactoring, architectural changes, codebase restructuring, "refactor the codebase"

---

## When to Use

- Performing large-scale refactoring that affects multiple components
- Making architectural changes that impact several layers of the codebase
- Restructuring the codebase for better maintainability or performance
- When asked: "refactor the codebase", "restructure the architecture", "large-scale refactoring"

---

## Refactoring Principles (Constitution Art. V)

### 1. Implementation Discipline
- Make the change that is requested or clearly necessary, and no more. No speculative features, and no unrequested refactoring combined with a fix.
- Validate every change: it builds with no new warnings (`-Wall -Wextra`), the full test suite passes, and any behavioral claim is supported by a run.

### 2. Organization
- Strict layering by directory: `core/` (types + interfaces), `model/` (concrete CUDA models), `feature/`, `io/`, `pipeline/` (orchestration), `net/` (transport), `gpu/` (device memory). Dependencies point inward toward `core/`; consumers depend on interfaces, never on concrete models.
- One responsibility per type and per file. Public headers expose the contract; implementation detail stays in the `.cc`/`.cu`.

### 3. Maintainability
- No duplicated logic; shared behavior is defined once. No dead code, no commented-out code, and no unused fallback path retained without a stated, written reason.
- Resources are owned through RAII (`std::unique_ptr`, `std::shared_ptr`, owning GPU buffers). No raw `new`/`delete`/`cudaMalloc` on a performance-critical path without an owning wrapper. Every CUDA call is error-checked.
- Functions are small and single-purpose. Deep nesting and long parameter lists are refactored.

---

## Refactoring Methodology

### Step 1 — Analyze impact
- Identify all components that will be affected by the refactoring
- Map dependencies between components using the strict layering model
- Identify all tests that will need to be updated or created

### Step 2 — Create spec and plan
- Create or update `spec.md` (WHAT and WHY, testable requirements)
- Create or update `plan.md` (HOW: architecture, data flow, threading, risks)
- Create or update `tasks.md` (small, ordered, independently verifiable tasks)

### Step 3 — Implement incrementally
- Make small, targeted changes that can be independently verified
- Ensure each change builds cleanly and passes tests
- Update documentation and state docs in the same change

### Step 4 — Validate thoroughly
- Build is clean with no new warnings under `-Wall -Wextra`
- Relevant tests pass (`ctest --output-on-failure`)
- For streaming behavior/perf claims: run through WebSocket path and report measured numbers with units and conditions
- For accuracy-sensitive changes: compare against reference/oracle outputs and report tolerance or quality metrics

---

## Change Guidelines

| Change Type | Approach | Notes |
|-------------|----------|-------|
| Interface changes | Update registry and implementations | Ensure backward compatibility where possible |
| Layer reorganization | Maintain strict dependency direction | Dependencies must point toward `core/` |
| Type refactoring | Preserve public contracts | Keep public headers stable where possible |
| Performance optimization | Validate against benchmarks | Ensure no accuracy degradation (Art. II) |

### Constraints
- **No speculative refactoring** — only refactor what is necessary for the requested change
- **Maintain layering** — `core/` → `model/feature/io/gpu/` → `pipeline/net/protocol/`
- **Preserve interfaces** — new capabilities should be added behind interfaces and registration
- **Update documentation** — state docs match the code: `PROJECT_STATE.md` and affected spec/tasks status lines are updated to reality

---

## Anti-Patterns

- **Unrelated refactoring combined with fixes** — make only the requested change (Art. V §6)
- **Breaking layering dependencies** — dependencies must point toward `core/`
- **Removing interfaces without replacement** — new capabilities should be added behind interfaces and registration
- **Refactoring without tests** — validate every change with the full test suite
- **Speculative features** — do not add unrequested functionality during refactoring

---

## Relationship with SDD

- Large-scale refactoring requires a complete SDD workflow: constitution → spec → plan → tasks → implement
- Refactoring tasks in `tasks.md` must be independently verifiable
- All refactoring changes must update `PROJECT_STATE.md` in the same change (Art. VIII)
- If refactoring affects model numerics, accuracy comparison against oracles is required (Art. II)