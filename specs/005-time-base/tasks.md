# Tasks 005 — Reusable Common Time Base

- **Feature**: `005-time-base`
- **Spec**: [spec.md](spec.md) · **Plan**: [plan.md](plan.md)
- **Status**: In progress (owner-approved)
- **Constitution**: v1.1.0

> Ordered, independently verifiable. Origin-0 integration first (numbers
> unchanged), then the reconciliation check. Build + test each phase.

---

## Phase 1 — The reusable value type
- [ ] **T010** `core::TimeBase` (`include/core/time_base.h`, header-only):
  `SecondsAt`, `SampleAt`, `Duration`, `Derive`, `LocalSeconds`, `valid`. Pure
  value type. *(Verify: compiles.)*
- [ ] **T011** `test_time_base`: conversions exact; `Derive`+`LocalSeconds` map a
  sub-stream's local samples onto the common clock; round trips. *(Verify: AC1.)*

## Phase 2 — Buffer exposes the common base
- [ ] **T020** `SharedAudioBuffer::time_base()` + `WaitAndRead` optional
  `span_start_abs` out-param (cursor's absolute pos before the read). Existing
  callers unaffected. *(Verify: AC2; builds; existing buffer tests green.)*

## Phase 3 — Consumers inherit (origin-0, numbers unchanged)
- [ ] **T030** Endpoint pipeline uses `tb.SecondsAt(ep)`. *(Verify: identical
  endpoint times.)*
- [ ] **T031** ASR worker holds a `TimeBase`; time codes via `SecondsAt`.
  *(Verify: identical asr times on 120 s.)*
- [ ] **T032** Diarization `t_start_sec` from the base. *(Verify: identical diar
  times on 120 s.)*
- [ ] **T033** No-regression diff: 120 s + 600 s comprehensive output identical
  before/after. *(Verify: AC3.)*

## Phase 4 — Reconciliation + close-out
- [ ] **T040** `ReconcileExtent` + controller end-point check under
  `ORATOR_TIMEBASE_CHECK=1`; logs a warning on mismatch. *(Verify: AC4; passes
  clean; injected offset flags.)*
- [ ] **T050** Full build + `ctest` green `-Wall -Wextra`; race-checked.
  *(Verify: AC5.)*
- [ ] **T051** Update `/memories/repo/` + `PROJECT_STATE.md`; commit.

## Traceability

| Requirement | Tasks |
|---|---|
| FR1 TimeBase type | T010, T011 |
| FR2 derivation | T010, T011 |
| FR3 buffer exposes base | T020 |
| FR4 consumers inherit | T030, T031, T032 |
| FR5 reconciliation | T040 |

| Acceptance | Tasks |
|---|---|
| AC1 unit | T011 |
| AC2 buffer base | T020 |
| AC3 no regression | T033 |
| AC4 reconcile | T040 |
| AC5 build/tests | T050 |

## Definition of Done
`core::TimeBase` reusable value type with derivation; buffer exposes the common
base + per-read absolute start; three pipelines inherit it (no numeric
regression on 120/600 s); reconciliation check available; build + tests green;
docs updated; committed.
