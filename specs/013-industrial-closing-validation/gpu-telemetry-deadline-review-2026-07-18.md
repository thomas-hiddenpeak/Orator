# GPU Telemetry Absolute-Deadline Review - 2026-07-18

## Scope and claim boundary

This record closes T112 as an engineering timing task. It verifies the GPU
telemetry timer, payload coverage, and real-WebSocket mechanical contract. It
does not evaluate speaker correctness, revise the accepted T111 contextual
result, or advance T102/T084.

## Root cause

The previous `AuditoryStream` worker called `wait_for(interval)` before every
sample. `SerializeGpuTelemetry()` then ran the GPU and power probes, so that
probe and emission latency shifted every subsequent wait. The accumulated
delay explains how a full Run B attempt reached only `94.965%` runtime cadence
despite retaining all required fields.

## Implemented scheduling contract

Transitional experimental commit `d610de36ed1303ad58fd72bca293eb0642490a30`
adds `PeriodicDeadline` and changes only the GPU telemetry worker:

- the first deadline is one configured interval after worker start;
- the worker waits on `steady_clock` with the existing interruptible stop
  condition;
- completion advances from the previous deadline, not from completion time;
- expired periods are skipped to the first future deadline, so an overrun
  cannot create a catch-up burst;
- the checked-in one-second TOML interval, zero-disabled behavior, serializer,
  payload, emitter, and lifecycle join are unchanged.

Cursor telemetry and every speaker, ASR, VAD, alignment, and fusion path are
unchanged.

## Clean-build validation

The clean `master` worktree at `d610de36ed13` rebuilt without compiler
warnings. All `69/69` configured CTest entries passed, including the new
deterministic deadline test for first cadence, normal probe latency,
before-deadline preservation, exact-deadline completion, multi-period overrun,
and invalid intervals.

## Real-WebSocket validation

The unified client streamed the first `120.000` seconds of `test.mp3` at 1.0x
through the production incremental WebSocket path with direct `end` and
continuous `tegrastats`. The test TOML differs from root `orator.toml` only in
its isolated registry and storage paths; `telemetry.gpu_interval_sec` remains
`1.0`.

| Mechanical observation | Recorded value |
|---|---:|
| Source state | clean and unchanged at `d610de36ed13` |
| Stream rate | `0.993x` |
| Direct terminal wait | `0.800 s` |
| Runtime GPU samples | `119` |
| Runtime sample cadence | `99.167%` |
| Continuous tegrastats samples | `120` |
| Tegrastats cadence | `100%` |
| GPU-utilization field coverage | `100%` |
| GPU-memory field coverage | `100%` |
| System-power field coverage | `100%` |
| CPU/RAM/temperature field coverage | `100%` each |
| First/last runtime sample time | `1.1 / 119.1 s` |
| Adjacent runtime sample steps outside `0.9-1.1 s` | `0` |

The 118 adjacent runtime sample steps are all one second apart within floating
point representation. Runtime GPU utilization reached `98%`; runtime GPU
memory reached `60725.4 MB`; runtime system power reached `75.01 W`.
`tegrastats` independently recorded GPU rail power up to `24.776 W` and system
power up to `65.992 W`.

## Frozen evidence

| Item | SHA-256 |
|---|---|
| Timeline package | `74b5c5dee718702e31420b852bbe41d1493e8c32f811eca539da595a0c7eef85` |
| Manifest | `72ac9dbead2b6c953fe6561b94367db70841b7860d632b4de7c2183bf9b6240d` |
| Isolated TOML | `13b642f4eda23cc4666682d060209e295ce87c16b21adc72c45426cfca3b7ee6` |
| Resolved configuration | `339e5cfd07812d50dfd553b73d7e7c73fc74fb2d6d81f97072cc4ca7ee938ee0` |
| `orator_ws` binary | `d554feaa1564f0b75c8bcf55becb2d8e49d1c892d7d1734258a5a34fb995a8c0` |
| `test.mp3` | `b7c25d1c349b02d654b6a406bc29039749e4240a4109dda4fcc905285b14b18b` |

The artifact ID is
`orator-20260718T061156Z-d610de36ed13-120.000s`. Files are retained under
`/tmp/orator-spec013/release-d610de3-t112/`.

## Outcome

T112 is complete. Absolute-deadline scheduling removes cumulative probe drift
without changing any behavioral TOML value or telemetry field. This evidence
is mechanical only; T102 audible adjudication and every remaining T084 speaker
gate stay open.
