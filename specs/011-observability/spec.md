# Spec 011 — Observability Component Integration (rerun)

- **Feature**: `011-observability`
- **Status**: Implemented (2026-06-30) — Phase 1 (data flow + per-pipeline RTF)
  and Phase 2 (comprehensive dashboard: continuous hardware telemetry, cursor
  backlog, scheduler health, blueprint, methodology). Validated on a `rate=1`
  120 s run (125 gpu + 125 cursor + 126 device samples; six dashboard
  dimensions). Fixed a config bug: nested `[telemetry.cursor]` was never read.
  Live consumer + full-hour recording remain follow-ups.
- **Owner**: project owner
- **Constitution**: v1.5.0
- **Depends on**: Spec 002 (GPU-scheduling telemetry), Spec 004 (comprehensive
  timeline + protocol), Spec 009 (forced alignment), Spec 010 (speaker identity)

> WHAT the observability layer must provide and WHY. HOW (architecture, data
> flow) is in `plan.md`; ordered work is in `tasks.md`.

---

## 1. Summary

Orator runs five independent pipelines (diarization, ASR, VAD, speaker identity,
forced alignment) on one absolute audio time base and already serializes their
results. Debugging segmentation / alignment / speaker-boundary / GPU-scheduling
issues today means grepping a multi-MB JSON or the binary logs — there is no
time-aligned multimodal view.

A rerun-based **offline observability** module was introduced
(`tools/observability/timeline_to_rerun.py`, `rerun-sdk` in the tools venv) but
was never wired into the workflow, never fed the runtime's GPU/cursor telemetry,
and renders only the diarizer's per-session LOCAL speaker (not the stable GLOBAL
identity from Spec 010). This feature **enables and completes** that module: it
defines exactly what runtime data flows to the observability component, captures
the periodic telemetry the runtime already emits, and renders every pipeline —
including global speaker identity and per-pipeline real-time factor — on a single
scrubbable rerun timeline.

## 2. Constitutional constraints

- **Art. I (zero runtime third-party deps)**: rerun and any visualization
  library live ONLY under `tools/` (offline/developer tooling). The C++/CUDA
  runtime never links rerun; it only emits structured JSON/WS data it already
  produces. This feature adds NO runtime dependency.
- **Art. III (independent pipelines, one time base)**: the observability view is
  a pure consumer — it reads published results and never couples pipelines.
- **Art. IV (real streaming validation)**: telemetry and the exported view are
  validated against a real `rate=1` WebSocket run, not a whole-file shortcut.
- **Art. VIII (docs match code)**: the telemetry schema documented here matches
  `SerializeGpuTelemetry` / `SerializeCursorTelemetry`.

## 3. Data sources (runtime → observability)

All already produced by the runtime; this feature captures and renders them.

| Source | Form | Schema (owner) |
|---|---|---|
| Pipeline tracks | final timeline JSON `timeline.tracks[]` | diar/asr/vad/align + `comprehensive` (Spec 004/009/010) |
| Speaker identity | `speaker_id` on diar/comprehensive entries | Spec 010 (`spk_N`, stable global) |
| GPU-scheduling telemetry | periodic WS `{"type":"gpu_telemetry",...}` | `SerializeGpuTelemetry` — per-pipeline `real_time_factor`, `compute_sec`, `priority_index`, `class`, `stream_active`, `cuda_priority` |
| Cursor progress telemetry | periodic WS cursor message | `SerializeCursorTelemetry` |
| Run metadata | timeline header | `audio_sec`, `sample_rate`, `stream_rt`, per-track RTF |

## 4. Functional requirements

- **FR1** Capture the periodic `gpu_telemetry` (and cursor) WS messages during a
  run into the output JSON as a `telemetry` array (time-stamped samples), so the
  offline exporter has the time series without a live connection.
- **FR2** The exporter renders, on one rerun `audio_time` timeline:
  - diarization activity lanes + text, keyed by **global `speaker_id`** when
    present (fall back to local `speaker`), with stable per-identity color;
  - ASR utterances, VAD speech spans, forced-aligner per-character units, and the
    merged comprehensive turns (speaker + text);
  - **per-pipeline real-time-factor scalar lanes** from the `gpu_telemetry` time
    series (diar/asr/vad RTF over wall time);
  - device markers (RAM / GPU%) if present.
- **FR3** Configuration is via `orator.toml` `[telemetry]` (GPU push interval) and
  `[telemetry.cursor]`; both default OFF (0). Enabling them must not regress
  real-time streaming.
- **FR4** A documented one-command workflow: run with telemetry on → export to a
  `.rrd` (or `--spawn` the viewer).

## 5. Non-goals (this spec)

- A LIVE rerun consumer that connects to the WS in real time (a follow-up; the
  offline `.rrd` export is the deliverable here).
- Embedding rerun or any viz library in the C++ runtime (forbidden, Art. I).
- New runtime metrics beyond what is already serialized.

## 6. Acceptance

- Telemetry captured during a real `rate=1` run; the exported `.rrd` shows all
  five pipelines aligned on one timeline, global speaker identity, and per-
  pipeline RTF lanes; validated by opening the recording and confirming the
  multimodal alignment against the known test audio (context review, not a
  script metric). No runtime third-party dependency; streaming stays real-time;
  ctest unaffected.

---

## 7. Phase 2 — Comprehensive observability dashboard

Phase 1 wired the data flow and rendered the pipelines + per-pipeline RTF. Phase
2 turns that into an **engineered observation surface**: every dimension of a run
— pipelines, comprehensive timeline, hardware device, and pipeline/process health
— presented as coherent time series on one scrubbable timeline, organized by a
rerun **blueprint** dashboard rather than a flat entity tree.

### 7.1 Observation dimensions (what a run must show)

| Dimension | Entities | Source |
|---|---|---|
| **Pipelines** | `pipelines/diarization/<id>`, `pipelines/asr`, `pipelines/vad`, `pipelines/align/*` (text + activity step series) | timeline tracks |
| **Comprehensive timeline** | `comprehensive/<id>` per-speaker swimlanes + merged turns | timeline `comprehensive` |
| **Scheduler / process** | `scheduler/<pipeline>/{rtf,compute_sec,active,cuda_priority}` | `gpu_telemetry` |
| **Pipeline backlog** | `cursors/<pipeline>/{position_sec,pending_sec}` | `cursor_progress` |
| **Hardware device** | `device/mem/*`, `device/power/*`, `device/temp/*`, `device/cpu/util_pct`, `device/gpu/util_pct` (continuous) | continuous `tegrastats` |
| **Session summary** | static run metadata document | timeline header + meta |

### 7.2 Functional requirements (Phase 2)

- **FR5 (continuous hardware telemetry)** The capture client samples `tegrastats`
  continuously for the streamed duration (not three before/after/final
  snapshots), timestamps each line by elapsed seconds since stream start (≈ audio
  time at `rate=1`), and writes a `device_series` array to the run JSON. The
  exporter parses RAM/SWAP, CPU utilization, per-rail power (VDD_GPU,
  VDD_CPU_SOC, VIN total), and temperatures (gpu/cpu/tj/soc); `GR3D_FREQ` GPU%
  is parsed when the device exposes it (Orin) and omitted gracefully when it does
  not (Thor tegrastats has no GR3D_FREQ).
- **FR6 (process / scheduler health)** The exporter renders the scheduler time
  series (per-pipeline RTF, compute_sec, scheduling-active, cuda_priority) and
  the per-pipeline cursor backlog (`pending_sec`) so scheduling contention and
  pipeline lag are directly visible.
- **FR7 (dashboard blueprint)** The exporter ships a rerun blueprint that lays out
  the dimensions as a dashboard: a pipelines/comprehensive text-log row, a
  scheduler + backlog time-series row, a device power/temperature/memory
  time-series row, and a session-summary document.
- **FR8 (methodology & best practices)** A documented system-observation method:
  when to enable telemetry, recommended intervals, what each lane means, how to
  read contention/backlog/thermal signals, and the cross-dimension diagnostic
  workflow (e.g. backlog rising while RTF < 1 → pipeline starvation).

### 7.3 Acceptance (Phase 2)

- A real `rate=1` run with telemetry + continuous tegrastats on produces a run
  JSON with non-empty `telemetry` and `device_series`; the exported `.rrd` opens
  into the blueprint dashboard with all six dimensions populated and aligned on
  `audio_time`. Streaming stays ~1.0× real-time. No runtime dependency. Method
  doc present and consistent with the rendered lanes.
