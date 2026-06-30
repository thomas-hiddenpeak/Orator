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
   # or:  ... --spawn
   rerun run.rrd
   ```

## What you get

All lanes share one absolute `audio_time` axis (the pipelines' shared time base).

| Entity                       | Meaning |
|------------------------------|---------|
| `diarization/<id>`           | speaker turns, keyed by **global** `speaker_id` (`spk_N`) when speaker identity is on, else the diarizer-local label |
| `activity/<id>`              | step series, 1 while that identity is speaking |
| `asr` / `align`              | transcription text and per-character forced-alignment units |
| `vad`                        | speech/non-speech segments |
| `comprehensive`              | merged per-segment text labelled by global identity |
| `gpu/<pipeline>/rtf`         | per-pipeline real-time factor over wall time (from `gpu_telemetry`) |
| `gpu/<pipeline>/active`      | per-pipeline scheduling-active step series |
| `metrics/<track>/rtf`        | per-track real-time-factor summary |
| `device/ram_used_mb`, `device/gpu_pct` | coarse tegrastats markers |

## Notes

- Keep telemetry intervals OFF (0) for production; enable only for an
  observability run. At 1 s a 60-min run adds ~3600 tiny messages — negligible,
  and streaming stays ~1.0× real-time (validated).
- `gpu_telemetry` only covers the GPU-scheduled pipelines (diarization, asr,
  vad); align/speaker run off the GPU scheduler and appear as timeline tracks.
