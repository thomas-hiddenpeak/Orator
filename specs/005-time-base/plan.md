# Plan 005 — Reusable Common Time Base

- **Feature**: `005-time-base`
- **Spec**: [spec.md](spec.md)
- **Status**: Draft
- **Constitution**: v1.1.0

> HOW to satisfy [spec.md](spec.md). Standard terminology.

---

## 1. The `core::TimeBase` value type

A lightweight, copyable value type (no data buffers, no threads). One shared
clock per session; sub-streams derive from it.

```
class TimeBase {
  int  sample_rate_   = 0;   // 0 => invalid
  long origin_sample_ = 0;   // absolute sample of t = 0 on the common clock
  long anchor_sample_ = 0;   // where a derived sub-stream's local data begins
 public:
  TimeBase() = default;                         // invalid
  TimeBase(int sample_rate, long origin = 0);

  bool   valid()        const;                  // sample_rate_ > 0
  int    sample_rate()  const;
  long   origin_sample()const;
  long   anchor_sample()const;

  double SecondsAt(long abs_sample) const;      // (abs - origin) / rate
  long   SampleAt(double seconds)   const;      // round(seconds*rate) + origin
  double Duration(long n_samples)   const;      // n / rate

  TimeBase Derive(long anchor_abs_sample) const;// same rate+origin, set anchor
  double LocalSeconds(long local_sample) const; // SecondsAt(anchor + local)
};
```

- `SecondsAt`/`SampleAt` are the single conversion used everywhere (replaces ad
  hoc `sample / sr`).
- `Derive` + `LocalSeconds` serve a consumer that counts its own samples from 0
  but must report on the common clock — including one that produces data only
  intermittently: when its data begins at absolute sample S, it holds
  `base.Derive(S)` and reports `LocalSeconds(i)`.
- Pure value type; header-only at `include/core/time_base.h` (all inline; no .cc).

## 2. `SharedAudioBuffer` exposes the common base (FR3)

- Add `core::TimeBase time_base() const { return core::TimeBase(sample_rate_, 0); }`
  (origin 0 = stream start; matches today).
- `WaitAndRead` gains an optional out-param for the absolute start of the
  returned span: `bool WaitAndRead(int cursor, std::vector<float>* out, long*
  span_start_abs = nullptr)`. The start is the cursor's absolute position
  *before* the read (the buffer already tracks `cursors_[cursor]`). Existing
  callers pass nothing and are unaffected.

## 3. Consumer integration (FR4, origin-0 first → numbers unchanged)

Each consumer thread obtains `buffer_.time_base()` and the per-read absolute
start; it derives its time codes from the base. Because today every consumer
starts at absolute 0, the numbers are identical — this step makes the mechanism
explicit without changing outputs (G6/AC3), and enables non-zero anchors later.

- **Endpoint** (simplest, already uses absolute `ep`): replace `ep / sr` with
  `tb.SecondsAt(ep)`.
- **ASR worker**: the worker holds a `TimeBase` (set by the controller). Replace
  `(base_sample + begin) / sr` and `inc_*_sample / sr` with `tb.SecondsAt(...)`.
  The sample positions are already absolute on the common clock (the VAD/inc
  counters start at 0 = origin 0), so values are unchanged.
- **Diarization**: `t_start_sec` becomes `tb.SecondsAt(diar_origin_abs)` where
  `diar_origin_abs` is the absolute sample of the first frame (0 today). The
  worker passes the base into `FramesToSegments` (or sets `frames.t_start_sec`
  from it). Values unchanged.

## 4. Reconciliation check (FR5, debug-gated)

A small helper `TimeBase::ReconcileExtent(processed_samples, common_total)`
returns the gap in samples; the controller logs a warning if non-zero under an
env flag (`ORATOR_TIMEBASE_CHECK=1`). On Finalize each worker reports its
processed extent; the controller reconciles against `buffer_.total_samples()`.
This is diagnostic only (no behavior change), and would flag an injected offset
(AC4).

## 5. Validation

- **Unit (AC1)**: `test_time_base` — conversions, `Derive`/`LocalSeconds`, round
  trips.
- **Buffer (AC2)**: a check that the per-read absolute start advances by the
  consumed count (extend an existing buffer test or the probe).
- **No regression (AC3)**: 120 s + 600 s comprehensive timeline diff vs the
  pre-change output — identical times.
- **Reconcile (AC4)**: enable the check; passes clean; injecting an offset flags.
- **Build/tests (AC5)**: `ctest` green, `-Wall -Wextra` clean.

## 6. Risks and Mitigations

- **R1 alignment regression** → integrate origin-0 first (numbers identical),
  diff 120/600 s before/after; only then use non-zero anchors.
- **R2 over-abstraction** → `TimeBase` is a pure value type; consumers opt in;
  no threads, no ownership.

## 7. Out of Scope

Wall-clock/multi-session time, engine numerics, buffer cursor rework.
