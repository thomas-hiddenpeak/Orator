# Observability (offline rerun)

Spec 011. Offline visualization of an Orator run in the
[rerun](https://rerun.io) viewer. This tooling lives in `tools/` only — the
C++/CUDA runtime emits **no** rerun dependency (Constitution Art. I). The runtime
just serves its existing WS JSON; everything here is post-processing.

## Install

```bash
python -m venv tools/.venv && source tools/.venv/bin/activate
pip install -r tools/observability/requirements.txt   # rerun-sdk==0.33.1
```

## One-command workflow

1. **Run the server with telemetry enabled.** Telemetry defaults to OFF. For an
   observability run, set the intervals in your config (see `orator.toml`
   `[telemetry]` / `[telemetry.cursor]`):

   ```toml
   [telemetry]
   gpu_interval_sec = 1.0       # periodic per-pipeline RTF / scheduling samples
   [telemetry.cursor]
   interval_sec = 1.0           # periodic per-pipeline cursor progress
   ```

   ```bash
   ORATOR_CONFIG=your.toml ./build/orator_ws 8765
   ```

2. **Stream real audio at rate=1** and capture the run (timeline + the periodic
   telemetry samples) into one JSON:

   ```bash
   python tools/verify/py/ws_unified_test.py \
       --duration 3615 --rate 1.0 --port 8765 --out run.json
   ```

3. **Export to a rerun recording** (or open the viewer directly with `--spawn`):

   ```bash
   python tools/observability/timeline_to_rerun.py --in run.json --out run.rrd
   # or:  ... --spawn          # open the dashboard live
   #      ... --no-blueprint   # skip the dashboard, use rerun's heuristic layout
   rerun run.rrd
   ```

## The dashboard

The exporter ships a rerun **blueprint** that lays out the six observation
dimensions as rows on one shared `audio_time` axis (the pipelines' common time
base):

```
┌── Pipelines (diar/asr/vad/align) ──┬── Comprehensive timeline ──┐
├── Scheduler (RTF / activity) ──────┴── Pipeline backlog ────────┤
├── Power ──────────┬── Temperature ──────────┬── Memory ─────────┤
└── Run summary ────────────────────────────────────────────────-┘
```

| Dimension | Entity paths | Meaning |
|---|---|---|
| **Pipelines** | `pipelines/diarization/<id>` (+`/active`), `pipelines/asr/utterance`, `pipelines/vad/speech` (+`pipelines/vad/active`), `pipelines/align/text_<id>` | each pipeline's results; diarization keyed by **global** `speaker_id` (`spk_N`) when speaker identity is on, else the diarizer-local label |
| **Comprehensive** | `comprehensive/<id>`, `comprehensive/all` | per-speaker swimlanes + merged stream, labelled by global identity |
| **Scheduler / process** | `scheduler/<pipe>/{rtf,compute_sec,active,cuda_priority}` | per-pipeline real-time factor, compute seconds, scheduling-active, CUDA priority (from `gpu_telemetry`) |
| **Pipeline backlog** | `cursors/<pipe>/{position_sec,pending_sec}` | per-pipeline read position and unread backlog in seconds (from `cursor_progress`) |
| **Hardware device** | `device/mem/{ram_used_mb,swap_used_mb}`, `device/cpu/util_pct`, `device/gpu/util_pct`, `device/temp/{gpu,cpu,tj,soc}_c`, `device/power/{vdd_gpu,vdd_cpu_soc,vin_total}_mw` | continuous `tegrastats` time series |
| **Session summary** | `session/summary` | static run metadata (audio length, per-track counts/RTF, speakers, sample counts) |

## System observation method (best practices)

A disciplined way to read a run, dimension by dimension:

1. **Is it real-time?** Read `scheduler/*/rtf`. A pipeline's RTF is
   `audio_processed / compute_time`; RTF > 1 means it processes faster than
   real-time. The ASR pipeline is the bottleneck (RTF ≈ 1–1.3); diar/vad sit at
   50–100×.
2. **Is anything starving?** Cross-read `cursors/<pipe>/pending_sec` against the
   RTF. **Backlog rising while RTF < 1 → that pipeline is starved** (it cannot
   keep up; audio queues up). A steady, bounded `pending_sec` (≈ one window) is
   healthy; a monotonically climbing one is the early signal of a real-time
   miss before the final `stream_rt` shows it.
3. **Who spoke?** Compare `pipelines/diarization/<id>/active` (raw diarizer
   turns) with `comprehensive/<id>` (global-identity-labelled text). A speaker
   that splits across two `spk_N` lanes, or two speakers collapsing into one,
   is a voiceprint identity issue (Spec 010), not a transcription issue.
4. **Thermals / power.** `device/temp/*` and `device/power/*` over the run show
   whether sustained load throttles the device. Correlate a temperature climb
   with an RTF dip to catch thermal throttling. `device/mem/ram_used_mb` rising
   without bound flags a leak.
5. **Contention.** `scheduler/<pipe>/active` + `cuda_priority` show which
   pipelines hold the GPU concurrently and at what priority — read alongside RTF
   when a foreground pipeline's RTF drops while a background one is active.

### When to enable

Telemetry defaults OFF. Enable only for an observability run (`[telemetry]` /
`[telemetry.cursor]` in `orator.toml`). Recommended interval **1 s**: a 60-min
run adds ~3600 small messages and ~3600 tegrastats lines — negligible, and
streaming stays ~1.0× real-time (validated). Always validate through the real
`rate=1` WebSocket path (Constitution Art. IV); a `rate=0` shortcut ages audio
out of pipeline windows and distorts both backlog and enrollment.

## Notes

- Continuous device telemetry comes from `tegrastats` (Jetson; `nvidia-smi` is
  incomplete per the Constitution). On **Thor**, `tegrastats` has no
  `GR3D_FREQ`, so `device/gpu/util_pct` is omitted gracefully — use the
  scheduler RTF/active lanes as the GPU-activity proxy. On **Orin** the GPU%
  lane is present.
- `gpu_telemetry`/`cursor_progress` cover the GPU-scheduled pipelines
  (diarization, asr, vad); align/speaker run off the GPU scheduler and appear as
  pipeline tracks + in the comprehensive view.
- The `.rrd` carries the blueprint, so `rerun run.rrd` opens straight into the
  dashboard. Pass `--no-blueprint` to fall back to rerun's heuristic layout.

