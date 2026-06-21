# Orator — specs/ SDD Artifacts

Spec-Driven Development artifacts. Every non-trivial change follows the SDD workflow.

**Do NOT repeat root AGENTS.md — read it first.**

---

## STRUCTURE

```
specs/
├── PROJECT_STATE.md          # Current project state (verify against code!)
├── 001-streaming-pipeline/   # Completed
├── 002-gpu-scheduling/       # Completed
├── 003-sliding-window-asr/   # Implemented
├── 004-comprehensive-timeline/ # Unified spec (time base + timeline + protocol) — implemented
└── 006-web-ui/               # Draft (Web UI MVP)
```

Each feature directory contains three files: `spec.md` (WHAT/WHY), `plan.md` (HOW), `tasks.md` (steps).

---

## WORKFLOW

1. **Read Constitution** (`.specify/memory/constitution.md`) — highest authority
2. **Confirm project state** against actual code (Art. VIII)
3. Create/update `spec/NNN-feature/`:
   - `spec.md` — testable requirements, no implementation detail
   - `plan.md` — architecture, threading, data flow, risks
   - `tasks.md` — small ordered steps, each independently verifiable
4. **Review** all three before implementing
5. **Implement** — then validate (build + test + real streaming path)
6. **Sync state** — update `PROJECT_STATE.md` + spec/tasks status lines in the same commit

---

## STATUS CONVENTION

Status lines in task lists use: `Draft` → `Revised` → `In progress` → `Implemented`. Status is updated **in the same change** that lands the code. A shipped capability left at `Draft` is a consistency defect.
