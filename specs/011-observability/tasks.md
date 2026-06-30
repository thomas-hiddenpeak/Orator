# Spec 011 — Tasks (Observability Component Integration)

Ordered, independently verifiable. `[ ]` todo · `[~]` in progress · `[x]` done.
Gate each on: no runtime third-party dep, streaming stays real-time, ctest green.

## Phase 1 — Capture runtime telemetry
- [x] **T1.1** `ws_unified_test.py`: collect received WS messages of
  `type == "gpu_telemetry"` (and the cursor telemetry type) into a `telemetry`
  list; write it alongside `timeline` in the `--out` JSON. No timing change.
- [x] **T1.2** Document an observability run config (telemetry intervals) and run
  the real `rate=1` all-features stream with telemetry on; confirm a non-empty
  `telemetry` array and that `stream_rt` stays ~1.0×.
  Result: 125 gpu_telemetry samples over 120 s, stream_rt=0.964×.

## Phase 2 — Render in rerun
- [x] **T2.1** `timeline_to_rerun.py`: key diarization + comprehensive lanes by
  global `speaker_id` (fallback local `speaker`); stable color per identity.
- [x] **T2.2** `_log_gpu_telemetry`: per `gpu_telemetry` sample, log
  `gpu/<name>/rtf` + `gpu/<name>/active` scalars at `time_sec` → per-pipeline RTF
  lanes over wall time.
- [x] **T2.3** Per-track RTF summary scalars from `timeline.tracks[].real_time_factor`.

## Phase 3 — Workflow + docs
- [x] **T3.1** One-command workflow doc in `tools/observability/README.md`.
- [x] **T3.2** Sync `PROJECT_STATE.md` + this spec status; `CHANGELOG.md` entry.

## Phase 4 — Acceptance
- [x] **T4.1** Real `rate=1` run (120 s probe), telemetry on → 125 gpu_telemetry
  samples → exported `run.rrd` (4 tracks + 125 gpu samples). GPU RTF lanes for
  diarization/asr/vad render; diar/comprehensive keyed by global `spk_N`;
  per-track RTF + device markers present. No runtime dep; streaming ~1.0×.
  Full 60-min acceptance recording is a follow-up.

## Out of scope (follow-up)
- Live rerun consumer (WS → rerun in real time).
- Additional runtime metrics beyond what is already serialized.
