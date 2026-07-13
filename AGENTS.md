# Orator — Agent Knowledge Base

**Generated:** 2026-06-21
**Stack:** C++20 / CUDA / CMake
**Runtime:** Pure C++/CUDA, zero third-party runtime dependencies
**Build:** `cmake -S . -B build && cmake --build build -j`
**Test:** `cd build && ctest --output-on-failure`

---

## OVERVIEW

Real-time edge-deployed (Jetson Orin/Thor) auditory pipeline. Ingests mono PCM audio over WebSocket, runs diarization (Sortformer) + ASR (Qwen3-ASR) + VAD as three independent pipelines on a shared time base, and outputs a comprehensive timeline JSON.

---

## STRUCTURE

```
./
├── include/          # Public headers — one per type, layered by domain
│   ├── core/         #   Interfaces, data types, contracts
│   ├── model/        #   Model interfaces (IAsr, IDiarizer)
│   ├── pipeline/     #   Pipeline orchestration interfaces
│   ├── protocol/     #   Topic-based protocol layer
│   ├── gpu/          #   GPU memory & synchronization
│   ├── io/           #   File I/O, tokenizers
│   ├── net/          #   WebSocket transport
│   └── feature/      #   Feature extraction (mel)
├── src/              # Implementations — mirrors include/ structure
├── specs/            # SDD artifacts — spec.md, plan.md, tasks.md per feature
├── .specify/         # Constitution + governance artifacts
├── tools/            # Probes, utilities, Python oracles
├── test/             # CTest + Python integration tests
├── web/              # Web UI — index.html, app.js, style.css
└── models/           # Model weights + reference data (LFS)
```

---

## WHERE TO LOOK

| Task | Location | Notes |
|------|----------|-------|
| Add/change a model | `include/core/stages.h` + `include/model/` + `src/model/` | Implement interface, register, never edit consumers |
| Change pipeline orchestration | `include/pipeline/` + `src/pipeline/` | Workers under `AuditoryStream` controller |
| Modify time base | `include/core/time_base.h` | All pipelines inherit from `buffer_.time_base()` |
| Change transport | `include/net/` + `src/net/` | libwebsockets-based, not raw POSIX |
| Add a protocol topic | `include/protocol/` + `src/protocol/` | Topic registry + schema registry |
| GPU kernel work | `include/gpu/` + `src/gpu/` | Kernels validated against CPU reference |
| Understand project state | `specs/PROJECT_STATE.md` | Must verify claims against code (Art. VIII) |
| Constitutional rules | `.specify/memory/constitution.md` | Highest authority — read before any non-trivial work |

---

## CONVENTIONS

### Code style (Constitution Art. V)
- Google C++ Style: 2-space indent, `PascalCase` types/methods, `lower_snake_case` locals, trailing-underscore `member_` fields, `I`-prefixed interfaces, `#pragma once`
- No `new`/`delete`/`cudaMalloc` on perf paths without owning RAII wrapper
- Every CUDA call error-checked
- Functions small, single-purpose

### Layering (strict, enforced in review)
Dependencies point toward the bottom layers, with `core/` as the base:
```
protocol/  net/
    ↑       ↑
    pipeline/
    ↑
  model/
    ↑
  gpu/ io/ feature/
    ↑     ↑
      core/
```
Note: `protocol/` is consumed by both `pipeline/` (protocol timeline, session store)
and `net/` (WS envelope wrapping). It functions as a shared dependency layer below
pipeline and net, not as an outermost layer above them.

### SDD workflow (mandatory for non-trivial work)
1. Read constitution → spec (WHAT/WHY) → plan (HOW) → tasks (verifiable steps)
2. Implement only after spec/plan/tasks reviewed
3. Validate: build clean (`-Wall -Wextra`), `ctest` pass, real streaming path for perf claims
4. Update `PROJECT_STATE.md` + spec status lines in the same change (Art. VIII)

### Pipeline decoupling
- Pipelines communicate ONLY through `ComprehensiveTimeline` — no callbacks, no shared pointers, no atomic flags between pipelines
- Three consistency principles: origin → process → result (Art. III)

### File structure
- One responsibility per type and per file
- Public header exposes contract; implementation stays in `.cc`/`.cu`
- `core/` = contracts + data types only

---

## ANTI-PATTERNS (FORBIDDEN)

- **Runtime third-party dependencies** — zero tolerance (Art. I). Python/PyTorch only in `tools/` as offline oracles.
- **`as any` / `@ts-ignore` / `@ts-expect-error`** — never. This is C++/CUDA; no type escape hatches.
- **Raw `new`/`delete`/`cudaMalloc` without owning wrapper** — RAII required.
- **Pipelines reading each other's data directly** — must go through `ComprehensiveTimeline`.
- **Time codes computed with ad hoc arithmetic** — must use `TimeBase::SecondsAt()` / `SampleAt()` / `Duration()`.
- **Unvalidated numerical changes** — every model stage validated against PyTorch/NeMo oracle before considered done (Art. II).
- **Speculative features / unrelated refactors** — make only the requested change (Art. V §6).
- **Stale state docs** — verify against code before trusting (Art. VIII).
- **Shotgun debugging** — fix root cause, not symptoms. Revert to last known good state after 3 failures.

---

## GOOGLE AGENT SKILLS ROUTING

This project uses a **Google Agent Skills** routing mechanism for complex multi-step tasks. High-level skill packages are stored in `.agents/skills/`.

### How it works

1. **Skill packages** live under `.agents/skills/<skill-name>/`, each containing a `SKILL.md` entry point.
2. **Before** executing any complex or cross-cutting task — such as frontend architecture, cloud deployment, CI/CD pipeline changes, performance profiling, security audits, or large refactors — the AI **MUST**:
   - Check if `.agents/skills/` exists and contains a relevant skill directory.
   - If found, read the corresponding `SKILL.md` for specialized guidance, constraints, and step-by-step protocols.
   - Follow the skill's instructions as supplementary rules on top of this file and the Constitution.
3. **Skill categories** (when implemented) may include:
   - `frontend-arch` — Web UI architecture decisions, SPA patterns
   - `cloud-deploy` — Deployment, containerization, CI/CD
   - `perf-profiling` — GPU/CPU profiling methodology
   - `security-review` — Vulnerability auditing protocols
   - `refactor-large` — Large-scale refactoring strategies   - `model-validation` — Accuracy verification against reference oracles
### Fallback behavior
If `.agents/skills/` does not exist or no matching skill is found, proceed using this AGENTS.md + the Constitution as the sole guidance. The skills mechanism is an optional augmentation layer.

### Relationship with SDD
Google Agent Skills complement but do **not** replace the SDD workflow (constitution → spec → plan → tasks → implement). For any non-trivial work:
1. Check `.agents/skills/` for a relevant skill → read `SKILL.md`
2. Follow the SDD workflow (constitution → spec → plan → tasks → implement)
3. If the skill conflicts with the Constitution, the **Constitution wins**

---

## EXISTING SDD RULES (Spec-Driven Development)

This project follows **Spec-Driven Development (SDD)** adapted from spec-kit. These rules are authoritative:

### Truth hierarchy
1. **Code** is authoritative over all documents (Constitution Art. VIII)
2. **Constitution** (`.specify/memory/constitution.md`) — highest written authority
3. **Project state** (`specs/PROJECT_STATE.md`) — describes current code state, subordinate to code
4. **Spec artifacts** (`specs/NNN-feature/spec.md`, `plan.md`, `tasks.md`) — define work in progress
5. **Test review protocol** (`.specify/test-review-protocol.md`) — governance for manual-style evaluation

### Mandatory workflow
1. Read constitution + project state first
2. Create/update `spec.md` (WHAT/WHY, testable requirements)
3. Create/update `plan.md` (HOW: architecture, data flow, threading, risks)
4. Create/update `tasks.md` (small, ordered, independently verifiable)
5. Implement only after spec/plan/tasks are reviewed
6. Validate: build clean + `ctest` pass + real streaming path when applicable
7. Record verified lessons; sync state docs in the same change

### Constitutional hard rules
- **Zero runtime deps** (C++20/CUDA only, Art. I)
- **Accuracy first** (Art. II) — no quality trade-off without explicit approval
- **Common time base + independent pipelines** (Art. III)
- **Streaming validation through real WebSocket** (Art. IV)
- **Engineering quality mandatory** (Art. V) — readability, RAII, race-free, small functions
- **Precise terminology** — no metaphors, no jargon (Art. VI)
- **Docs match code** — status advances in same change (Art. VIII)
- **Configuration consistency** — always use `orator.toml` configuration file to ensure parameter consistency across all server starts and tests. Do not hardcode parameters in command lines or code. Follow the configuration loading order: 1) Compile-time defaults, 2) `orator.toml`, 3) Environment variables (ORATOR_*), 4) CLI arguments.

### Source files
- `.specify/memory/constitution.md` — the Constitution
- `specs/PROJECT_STATE.md` — project state with verification evidence
- `specs/NNN-feature-name/spec.md`, `plan.md`, `tasks.md` — per-feature SDD artifacts
- `.github/copilot-instructions.md` — expanded SDD prompt with validation checklist
- `.specify/test-review-protocol.md` — test evaluation governance

---

## COMMANDS

```bash
cmake -S . -B build                           # Configure
cmake --build build -j                         # Build
cd build && ctest --output-on-failure          # Test
cmake --build build -j 2>&1 | grep -E "warning:|error:" || true  # Warning check
ORATOR_CONFIG=orator.toml ./build/orator_ws 8765  # Run WS server with config file
```

---

## NOTES

- **Large files** tracked via Git LFS: `*.safetensors`, `*.npz`, `*.npy`, `*.f32`, `*.i32`, `*.pcm`
- **Build output** in `build/` and `build_debug/` — both in `.gitignore`
- **AI session runtime data** in `.omo/` — also gitignored
- **GPU telemetry** disabled by default; opt-in via `ORATOR_GPU_TELEMETRY_SEC`
- **Log level** controlled by `ORATOR_LOG_LEVEL` env var or `[debug].log_level` in `orator.toml` (0=DEBUG .. 3=ERROR)
- **Runtime config** via `orator.toml` (TOML format). Runtime behavior is typed
  through `AuditoryStream::Config`. Loading order: defaults → `orator.toml` →
  environment → CLI. Model/GPU/transport implementations do not read
  `ORATOR_*` directly. See `include/io/config_reader.h`.
- **Specs 001-004 completed and verified.** Spec 006 (Web UI MVP) implemented. Active work follows Spec 004 protocol layer.
- **VAD gate optimization** (2026-06-25): Replaced O(N²) `ProtocolTimeline::Replay(0.0)` per `ProcessSpan` call with `ProtocolTimeline` subscription → local `VadCache` (O(1) `GetAll()`). Eliminated O(N²) Replay overhead on ASR hot path, enabling real-time 1× streaming for full 3615s session. `VadCache` subscribes to `vad/speech_segment` via `ProtocolTimeline::SubscribeInternal`, populated by VAD thread's `Publish`. ASR worker reads O(1) from local `VadCache::GetAll()`.
