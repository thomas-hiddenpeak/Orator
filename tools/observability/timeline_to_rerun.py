#!/usr/bin/env python3
"""Export an Orator comprehensive-timeline JSON to a rerun recording (.rrd).

OFFLINE developer observability (Constitution Art. I: third-party libraries are
allowed under tools/, never in the C++/CUDA runtime). The Orator WS server emits
a timeline JSON (the output of tools/verify/py/ws_unified_test.py --out X.json)
whose `timeline.tracks` hold the four pipelines' results on one absolute audio
time base: diarization speaker spans, ASR utterances, VAD speech spans, and the
forced-aligner's per-unit timestamps, plus a merged `comprehensive` track.

This tool replays those onto a single rerun "audio_time" timeline so the four
pipelines can be scrubbed in alignment -- the time-aligned multimodal view that
replaces grepping the binary logs when debugging segmentation, alignment, or
speaker-boundary issues.

Usage:
    tools/.venv/bin/python tools/observability/timeline_to_rerun.py \
        --in /tmp/out.json --out /tmp/out.rrd
    # or view interactively:
    tools/.venv/bin/python tools/observability/timeline_to_rerun.py \
        --in /tmp/out.json --spawn
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

import rerun as rr
import rerun.blueprint as rrb

# Stable per-speaker colors (RGB) for the diarization rows.
_SPEAKER_COLORS = [
    (0x4C, 0x9F, 0xE0),
    (0xE0, 0x6C, 0x4C),
    (0x6C, 0xE0, 0x8A),
    (0xE0, 0xC8, 0x4C),
    (0xB0, 0x7C, 0xE0),
    (0xE0, 0x4C, 0xA8),
]


def _color(key: str) -> tuple[int, int, int]:
    """Stable RGB for an identity string (so a speaker keeps one color across
    the diarization, activity and comprehensive lanes)."""
    if key in ("L-1", ""):
        return (0x80, 0x80, 0x80)
    return _SPEAKER_COLORS[(hash(key) & 0x7FFFFFFF) % len(_SPEAKER_COLORS)]


def _ident(entry: dict) -> str:
    """Stable identity key for a diar/comprehensive entry: the global voiceprint
    id (spk_N, Spec 010) when present, else the diarizer-local label."""
    sid = entry.get("speaker_id")
    if sid:
        return str(sid)
    return f"L{int(entry.get('speaker', -1))}"


def _at(seconds: float) -> None:
    """Place subsequent rr.log calls at `seconds` on the audio timeline."""
    rr.set_time("audio_time", duration=float(seconds))


def _log_diarization(track: dict) -> None:
    entries = track.get("entries", [])
    # Baseline every identity activity lane at 0 so each step series starts low,
    # and give each identity a stable color + name (carried across all lanes).
    idents = sorted({_ident(e) for e in entries})
    for key in idents:
        rr.log(f"pipelines/diarization/{key}/active",
               rr.SeriesLines(colors=[_color(key)], names=[key]), static=True)
        _at(0.0)
        rr.log(f"pipelines/diarization/{key}/active", rr.Scalars(0.0))
    for e in entries:
        key = _ident(e)
        start, end = float(e["start"]), float(e["end"])
        _at(start)
        rr.log(
            f"pipelines/diarization/{key}",
            rr.TextLog(
                f"{key}  [{start:.2f}-{end:.2f}]  ({end - start:.2f}s)",
                level=rr.TextLogLevel.INFO,
            ),
        )
        # Step series: 1 while the identity is active, 0 otherwise -> a per-
        # identity activity bar lane that is scannable at a glance.
        rr.log(f"pipelines/diarization/{key}/active", rr.Scalars(1.0))
        _at(end)
        rr.log(f"pipelines/diarization/{key}/active", rr.Scalars(0.0))


def _log_asr(track: dict) -> None:
    for e in track.get("entries", []):
        start, end = float(e["start"]), float(e["end"])
        _at(start)
        rr.log(
            "pipelines/asr/utterance",
            rr.TextLog(f"[{start:.2f}-{end:.2f}] {e.get('text', '')}"),
        )


def _log_vad(track: dict) -> None:
    for e in track.get("entries", []):
        start, end = float(e["start"]), float(e["end"])
        _at(start)
        rr.log(
            "pipelines/vad/speech",
            rr.TextLog(f"speech [{start:.2f}-{end:.2f}] ({end - start:.2f}s)"),
        )
        _at(start)
        rr.log("pipelines/vad/active", rr.Scalars(1.0))
        _at(end)
        rr.log("pipelines/vad/active", rr.Scalars(0.0))


def _log_align(track: dict) -> None:
    for e in track.get("entries", []):
        for u in e.get("units", []):
            start = float(u["start"])
            _at(start)
            rr.log(
                f"pipelines/align/text_{e.get('text_id', 0)}",
                rr.TextLog(f"[{start:.2f}-{float(u['end']):.2f}] {u.get('text', '')}"),
            )


def _log_comprehensive(entries: list) -> None:
    """Per-speaker swimlanes plus a merged stream, keyed by global identity."""
    for e in entries:
        start = float(e["start"])
        key = _ident(e)
        line = f"[{start:.2f}-{float(e['end']):.2f}] {key}: {e.get('text', '')}"
        _at(start)
        rr.log(f"comprehensive/{key}", rr.TextLog(line))
        rr.log("comprehensive/all", rr.TextLog(line))


def _log_gpu_telemetry(samples: list) -> None:
    """Spec 011: per-pipeline scheduler health from the runtime's periodic
    gpu_telemetry samples (SerializeGpuTelemetry) -> scheduler/<name>/* lanes:
    real-time factor, compute seconds, scheduling-active, and CUDA priority."""
    for s in samples:
        if s.get("type") != "gpu_telemetry":
            continue
        t = float(s.get("time_sec", 0.0))
        _at(t)
        for p in s.get("pipelines", []):
            name = p.get("name", "?")
            rtf = p.get("real_time_factor")
            if rtf is not None:
                rr.log(f"scheduler/{name}/rtf", rr.Scalars(float(rtf)))
            comp = p.get("compute_sec")
            if comp is not None:
                rr.log(f"scheduler/{name}/compute_sec", rr.Scalars(float(comp)))
            rr.log(f"scheduler/{name}/active",
                   rr.Scalars(1.0 if p.get("stream_active") else 0.0))
            cp = p.get("cuda_priority")
            if cp is not None:
                rr.log(f"scheduler/{name}/cuda_priority", rr.Scalars(float(cp)))


def _log_cursors(samples: list, sample_rate: int) -> None:
    """Spec 011 Phase 2: per-pipeline cursor backlog from cursor_progress
    samples (SerializeCursorTelemetry). The sample is placed on the audio
    timeline at total_samples/sample_rate; position/pending are converted from
    samples to seconds so a rising `pending_sec` directly shows pipeline lag."""
    sr = float(sample_rate) if sample_rate else 16000.0
    for s in samples:
        if s.get("type") != "cursor_progress":
            continue
        total = float(s.get("total_samples", 0.0))
        _at(total / sr)
        for c in s.get("cursors", []):
            cid = c.get("id", "?")
            rr.log(f"cursors/{cid}/position_sec",
                   rr.Scalars(float(c.get("position", 0)) / sr))
            rr.log(f"cursors/{cid}/pending_sec",
                   rr.Scalars(float(c.get("pending", 0)) / sr))


def _parse_tegra(line: str) -> dict:
    """Parse one tegrastats line into device metrics.

    Handles both Orin (`GR3D_FREQ`, `VDD_GPU_SOC`/`VDD_CPU_CV`) and Thor
    (no `GR3D_FREQ`, `VDD_GPU`/`VDD_CPU_SOC_MSS`, `VIN`) formats. Power values
    are `inst/avg/max`; the instantaneous (first) value is taken.
    """
    out: dict = {}
    m = re.search(r"RAM (\d+)/(\d+)MB", line)
    if m:
        out["ram_used_mb"] = float(m.group(1))
    m = re.search(r"SWAP (\d+)/(\d+)MB", line)
    if m:
        out["swap_used_mb"] = float(m.group(1))
    m = re.search(r"CPU \[([^\]]*)\]", line)
    if m:
        cores = re.findall(r"(\d+)%@", m.group(1))
        if cores:
            out["cpu_pct"] = sum(int(c) for c in cores) / len(cores)
    m = re.search(r"GR3D_FREQ (\d+)%", line)        # Orin only; absent on Thor
    if m:
        out["gpu_pct"] = float(m.group(1))
    for label, key in (("gpu", "gpu_c"), ("cpu", "cpu_c"),
                       ("tj", "tj_c"), ("soc", "soc_c")):
        m = re.search(rf"\b{label}\w*@([\d.]+)C", line)
        if m:
            out[key] = float(m.group(1))
    for pat, key in ((r"VDD_GPU\S* (\d+)mW", "gpu_mw"),
                     (r"VDD_CPU\S* (\d+)mW", "cpu_mw"),
                     (r"(?:^| )VIN (\d+)mW", "vin_mw")):
        m = re.search(pat, line)
        if m:
            out[key] = float(m.group(1))
    return out


# Metric -> rerun entity path. Grouped so a blueprint can pull whole groups.
_DEVICE_PATHS = {
    "ram_used_mb": "device/mem/ram_used_mb",
    "swap_used_mb": "device/mem/swap_used_mb",
    "cpu_pct": "device/cpu/util_pct",
    "gpu_pct": "device/gpu/util_pct",
    "gpu_c": "device/temp/gpu_c",
    "cpu_c": "device/temp/cpu_c",
    "tj_c": "device/temp/tj_c",
    "soc_c": "device/temp/soc_c",
    "gpu_mw": "device/power/vdd_gpu_mw",
    "cpu_mw": "device/power/vdd_cpu_soc_mw",
    "vin_mw": "device/power/vin_total_mw",
}


def _log_device_series(series: list) -> int:
    """Spec 011 Phase 2 (FR5): continuous hardware telemetry. Each tegrastats
    line is parsed and logged at its `t_sec` (elapsed from the audio t0, so it
    aligns to audio_time at rate=1) onto device/* scalar lanes."""
    n = 0
    for s in series:
        line = s.get("line", "")
        if not line:
            continue
        vals = _parse_tegra(line)
        if not vals:
            continue
        _at(float(s.get("t_sec", 0.0)))
        for key, val in vals.items():
            path = _DEVICE_PATHS.get(key)
            if path:
                rr.log(path, rr.Scalars(val))
        n += 1
    return n


def _log_tegrastats(data: dict, audio_sec: float) -> None:
    """Fallback: the coarse before/after/final snapshots as device markers,
    used only when no continuous `device_series` is present in the run JSON."""
    tg = data.get("tegrastats", {})
    if not isinstance(tg, dict):
        return
    for key, t in (("before", 0.0),
                   ("after", min(3.0, audio_sec)),
                   ("final", audio_sec)):
        s = tg.get(key)
        if not isinstance(s, str):
            continue
        vals = _parse_tegra(s)
        _at(t)
        for k, val in vals.items():
            path = _DEVICE_PATHS.get(k)
            if path:
                rr.log(path, rr.Scalars(val))


def _log_session_summary(data: dict, n_gpu: int, n_cursor: int,
                         n_device: int) -> None:
    """A static run-metadata document (Spec 011 Phase 2, FR7)."""
    meta = data.get("meta", {})
    tracks = {t.get("kind"): t for t in data.get("timeline", {}).get("tracks", [])}

    def _track_line(kind: str) -> str:
        t = tracks.get(kind)
        if not t:
            return f"- {kind}: (absent)"
        return (f"- {kind}: {len(t.get('entries', []))} entries, "
                f"RTF {t.get('real_time_factor', '?')}×")

    speakers = sorted({_ident(e)
                       for e in data.get("timeline", {}).get("comprehensive", [])})
    lines = [
        "# Orator run summary",
        "",
        f"- audio: {meta.get('audio_sec', '?')} s",
        f"- streamed at: {meta.get('rate_requested', '?')}× "
        f"(stream RTF {meta.get('stream_rt_factor', '?')}×)",
        f"- source: {meta.get('pcm', '?')}",
        "",
        "## Pipelines",
        _track_line("diarization"),
        _track_line("asr"),
        _track_line("vad"),
        _track_line("align"),
        "",
        f"## Speakers (global identity): {', '.join(speakers) or '(none)'}",
        "",
        "## Telemetry captured",
        f"- gpu_telemetry samples: {n_gpu}",
        f"- cursor_progress samples: {n_cursor}",
        f"- device (tegrastats) samples: {n_device}",
    ]
    _at(0.0)
    rr.log("session/summary", rr.TextDocument("\n".join(lines),
                                              media_type=rr.MediaType.MARKDOWN))



_TRACK_LOGGERS = {
    "diarization": _log_diarization,
    "asr": _log_asr,
    "vad": _log_vad,
    "align": _log_align,
}


def _build_blueprint() -> rrb.Blueprint:
    """Engineered dashboard (Spec 011 Phase 2, FR7): the six observation
    dimensions laid out as coherent rows on the shared audio timeline."""
    return rrb.Blueprint(
        rrb.Vertical(
            rrb.Horizontal(
                rrb.TextLogView(origin="pipelines",
                                name="Pipelines (diar/asr/vad/align)"),
                rrb.TextLogView(origin="comprehensive",
                                name="Comprehensive timeline"),
            ),
            rrb.Horizontal(
                rrb.TimeSeriesView(origin="scheduler",
                                   name="Scheduler — RTF / activity / priority"),
                rrb.TimeSeriesView(origin="cursors",
                                   name="Pipeline backlog (pending_sec)"),
            ),
            rrb.Horizontal(
                rrb.TimeSeriesView(origin="device/power", name="Power (mW)"),
                rrb.TimeSeriesView(origin="device/temp", name="Temperature (°C)"),
                rrb.TimeSeriesView(origin="device/mem", name="Memory (MB)"),
            ),
            rrb.TextDocumentView(origin="session/summary", name="Run summary"),
            row_shares=[3, 2, 2, 1],
        ),
        collapse_panels=True,
    )


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--in", dest="inp", required=True, help="timeline JSON path")
    ap.add_argument("--out", dest="out", help="output .rrd path")
    ap.add_argument("--spawn", action="store_true", help="open the rerun viewer")
    ap.add_argument("--no-blueprint", action="store_true",
                    help="skip the dashboard layout (use rerun's heuristic)")
    args = ap.parse_args()

    if not args.out and not args.spawn:
        ap.error("provide --out PATH and/or --spawn")

    data = json.loads(Path(args.inp).read_text())
    timeline = data.get("timeline", {})
    tracks = timeline.get("tracks", [])
    sample_rate = int(timeline.get("sample_rate", 16000) or 16000)
    telemetry = data.get("telemetry", [])
    device_series = data.get("device_series", [])
    n_gpu = sum(1 for s in telemetry if s.get("type") == "gpu_telemetry")
    n_cursor = sum(1 for s in telemetry if s.get("type") == "cursor_progress")

    blueprint = None if args.no_blueprint else _build_blueprint()
    rr.init("orator_observability", spawn=args.spawn,
            default_blueprint=blueprint)
    if args.out:
        rr.save(args.out)

    for track in tracks:
        logger = _TRACK_LOGGERS.get(track.get("kind"))
        if logger:
            logger(track)
        else:
            print(f"  (skipped unknown track kind: {track.get('kind')})",
                  file=sys.stderr)
    _log_comprehensive(timeline.get("comprehensive", []))

    _log_gpu_telemetry(telemetry)
    _log_cursors(telemetry, sample_rate)
    n_device = _log_device_series(device_series)
    if n_device == 0:
        _log_tegrastats(data, float(data.get("meta", {}).get("audio_sec", 0.0)))
    _log_session_summary(data, n_gpu, n_cursor, n_device)

    meta = data.get("meta", {})
    print(
        f"exported {len(tracks)} tracks, {n_gpu} gpu + {n_cursor} cursor + "
        f"{n_device} device samples "
        f"(audio={meta.get('audio_sec', '?')}s, "
        f"stream_rt={meta.get('stream_rt_factor', '?')}x)"
        + (f" -> {args.out}" if args.out else " (spawned viewer)")
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
