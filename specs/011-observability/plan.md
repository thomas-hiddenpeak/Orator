# Spec 011 — Plan (Observability Component Integration)

> HOW. WHAT/WHY is in `spec.md`; ordered work is in `tasks.md`.

## 1. Architecture

```
  C++/CUDA runtime (no rerun)                tools/ (offline, rerun allowed)
  ┌───────────────────────────┐             ┌──────────────────────────────┐
  │ AuditoryStream            │   WS JSON   │ ws_unified_test.py            │
  │  - final timeline JSON ───┼────────────▶│  --out run.json               │
  │  - gpu_telemetry (periodic)│            │  (now also records the        │
  │  - cursor telemetry        │            │   periodic telemetry samples) │
  └───────────────────────────┘             └───────────────┬──────────────┘
                                                             │ run.json
                                                             ▼
                                            ┌──────────────────────────────┐
                                            │ timeline_to_rerun.py          │
                                            │  tracks + speaker_id + align  │
                                            │  + gpu_telemetry RTF lanes    │
                                            │  -> run.rrd  /  --spawn viewer │
                                            └──────────────────────────────┘
```

The runtime is unchanged except for being RUN with telemetry enabled (config).
All visualization is offline tooling. One absolute `audio_time` axis ties every
lane together (the same time base the pipelines already share, Art. III).

## 2. Data flow & schemas

- **Final timeline JSON** (existing): `timeline.tracks[]` (kinds
  diarization/asr/vad/align) + `timeline.comprehensive[]`; entries carry
  `speaker` (local int) and, when speaker identity is on, `speaker_id` (global
  `spk_N`). Per-track `real_time_factor` / `compute_sec`.
- **gpu_telemetry** (existing periodic WS, schema = `SerializeGpuTelemetry`):
  `{"type":"gpu_telemetry","time_sec":T,"pipelines":[{"name","priority_index",
  "class","stream_active","cuda_priority","compute_sec","real_time_factor"}...]}`.
  Captured per push as a sample at wall/audio time `T`.
- **cursor telemetry** (existing periodic WS, schema = `SerializeCursorTelemetry`):
  per-pipeline cursor progress; captured the same way.

## 3. Components & changes

1. **Capture (`tools/verify/py/ws_unified_test.py`)**: collect every received WS
   message whose `type` is `gpu_telemetry` (and the cursor type) into a
   `telemetry: [...]` list and write it alongside `timeline` in `--out` JSON.
   No change to timing/streaming; purely an additional sink on the existing
   message loop.
2. **Render (`tools/observability/timeline_to_rerun.py`)**:
   - diarization/comprehensive lanes keyed by `speaker_id or f"L{speaker}"`;
     stable color per identity string.
   - new `_log_gpu_telemetry`: for each sample, log `gpu/<name>/rtf` and
     `gpu/<name>/active` scalars at `time_sec` → per-pipeline RTF lanes.
   - keep align/asr/vad/comprehensive loggers; add per-track RTF summary scalars.
3. **Config (`orator.toml`)**: document recommended telemetry intervals for an
   observability run (e.g. `[telemetry].gpu_push_interval_sec = 1.0`,
   `[telemetry.cursor].interval_sec = 1.0`); defaults stay 0 (off).
4. **Docs**: `tools/observability/` usage + the one-command workflow; sync
   PROJECT_STATE / spec status.

## 4. Threading / performance

- Telemetry threads already exist and are interval-gated; at 1 s interval over a
  60-min run that is ~3600 small messages — negligible. Validate the stream still
  runs at ~1.0× real-time with telemetry on.
- Capture is on the Python client side (no runtime cost).

## 5. Risks

- Large `.rrd` for a full hour with 13k+ align units: acceptable (~1–2 MB
  observed); the exporter streams logs, no unbounded memory.
- rerun-sdk API drift: pin `rerun-sdk==0.33.1` (already pinned).

## 6. Validation

- Real `rate=1` all-features run with telemetry on → `run.json` carries a
  non-empty `telemetry` array → export → `.rrd` opens with five pipeline lanes,
  global speaker identity, and per-pipeline RTF lanes aligned on `audio_time`.
- Context review of the recording against the known test audio (Test Review
  Protocol), not a script metric.
