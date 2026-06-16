# Spec 005 — Reusable Common Time Base

- **Feature**: `005-time-base`
- **Status**: Implemented, verified, committed (84fba90) and pushed
- **Created**: 2026-06-16
- **Owner**: project owner
- **Constitution**: v1.1.0

> WHAT to change and WHY. The type and integration are in `plan.md`.

---

## 1. Summary

Make the project's common time base an explicit, reusable abstraction
(`core::TimeBase`) instead of an implicit convention. Today three independent
stream consumers (diarization, ASR, endpoint) each count their own samples from
0 and convert to seconds with ad hoc `sample / sample_rate`; their times align
only because each happens to start at 0. As more consumers appear — some
producing data only intermittently — this coincidence is fragile. A single
shared, instantiable, derivable time base guarantees alignment by construction:
one origin, one conversion, and a derivation for sub-streams that begin at an
arbitrary absolute sample.

## 2. Background and Problem

- Each pipeline derives time codes from a local counter (`vad_.base_sample()`,
  `inc_abs_pos_`, diarization `t_start_sec = 0`, endpoint `ep`) and divides by
  the sample rate independently. Alignment is by the coincidence of all starting
  at 0, not by a shared mechanism (start-point reliability is implicit).
- There is no run-time check that a consumer's position equals its position on
  the common clock, and no end-point reconciliation against the stream total.
- Future consumers will be intermittent (data sometimes present, sometimes not);
  they need a precise way to anchor local counts onto the common clock.

## 3. Goals

- **G1** One reusable `core::TimeBase` value type: absolute-sample↔seconds
  conversion, instantiable per session, copyable, no threads/buffers.
- **G2** Derivation: a sub-stream that begins at absolute sample S can derive a
  child base anchored at S and report local counts on the common clock —
  serving intermittent consumers.
- **G3** `SharedAudioBuffer` exposes the common base (start-point reliability:
  every consumer inherits the same origin instead of assuming 0).
- **G4** Each existing pipeline reports times through the base; with origin 0 the
  numbers are byte-identical (no regression), making the mechanism explicit.
- **G5** An end-point reconciliation check (diagnostic) confirms a consumer's
  processed extent aligns with the common clock total.

## 4. Non-Goals

- **NG1** Wall-clock or multi-session/global time.
- **NG2** Engine numerics or buffer cursor rework.
- **NG3** Changing any emitted time value (origin-0 integration is identity).

## 5. Functional Requirements

- **FR1** `core::TimeBase` provides `SecondsAt(abs)`, `SampleAt(sec)`,
  `Duration(n)`, `valid()`, and accessors; default-constructed is invalid.
- **FR2** `Derive(anchor_abs)` returns a child base on the same clock; for it,
  `LocalSeconds(i) == SecondsAt(anchor + i)`.
- **FR3** `SharedAudioBuffer::time_base()` returns the session base, and a read
  can report the absolute start of the returned span.
- **FR4** Diarization, ASR, and endpoint pipelines convert via the base; with
  origin 0 the outputs are unchanged.
- **FR5** `ReconcileExtent(processed, common_total)` returns the signed sample
  gap; the controller can log a mismatch under a debug flag.

## 6. Acceptance Criteria

- **AC1** `test_time_base`: conversions exact; `Derive`/`LocalSeconds` map a
  sub-stream's local samples onto the common clock; round trips. (FR1, FR2)
- **AC2** The buffer exposes the base and a per-read absolute start that advances
  by the consumed count. (FR3)
- **AC3** 120 s + 600 s comprehensive output is identical before/after the
  consumer integration (origin-0 identity). (FR4, NG3)
- **AC4** With the reconciliation check enabled, a clean run reports zero gap;
  an injected offset is flagged. (FR5)
- **AC5** Build clean `-Wall -Wextra`; tests pass; threaded path race-free.

## 7. Constitution Check

- **I**: header-only value type + std lib; no new deps.
- **II**: no numeric change (origin-0 identity).
- **III**: strengthens independent pipelines on one comprehensive timeline by
  making the shared clock explicit.
- **IV**: validated on the real streaming path (120/600 s diff).
- **V**: small, documented, race-free.
- **VI/VII**: standard terms; spec→plan→tasks.

## 8. Risks

- **R1** Alignment regression → integrate origin-0 first (identity), diff
  120/600 s before/after; non-zero anchors only after.
- **R2** Over-abstraction → pure value type; consumers opt in.
