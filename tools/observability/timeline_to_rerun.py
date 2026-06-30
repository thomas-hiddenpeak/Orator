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

# Stable per-speaker colors (RGB) for the diarization rows.
_SPEAKER_COLORS = [
    (0x4C, 0x9F, 0xE0),
    (0xE0, 0x6C, 0x4C),
    (0x6C, 0xE0, 0x8A),
    (0xE0, 0xC8, 0x4C),
    (0xB0, 0x7C, 0xE0),
    (0xE0, 0x4C, 0xA8),
]


def _color(speaker: int) -> tuple[int, int, int]:
    if speaker < 0:
        return (0x80, 0x80, 0x80)
    return _SPEAKER_COLORS[speaker % len(_SPEAKER_COLORS)]


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
    # Baseline every identity lane at 0 so the activity step series starts low.
    idents = sorted({_ident(e) for e in entries})
    for key in idents:
        _at(0.0)
        rr.log(f"activity/{key}", rr.Scalars(0.0))
    for e in entries:
        key = _ident(e)
        start, end = float(e["start"]), float(e["end"])
        _at(start)
        rr.log(
            f"diarization/{key}",
            rr.TextLog(
                f"{key}  [{start:.2f}-{end:.2f}]  ({end - start:.2f}s)",
                level=rr.TextLogLevel.INFO,
            ),
        )
        # Step series: 1 while the identity is active, 0 otherwise -> a per-
        # identity activity bar lane that is scannable at a glance.
        rr.log(f"activity/{key}", rr.Scalars(1.0))
        _at(end)
        rr.log(f"activity/{key}", rr.Scalars(0.0))


def _log_asr(track: dict) -> None:
    for e in track.get("entries", []):
        start, end = float(e["start"]), float(e["end"])
        _at(start)
        rr.log(
            "asr/utterance",
            rr.TextLog(f"[{start:.2f}-{end:.2f}] {e.get('text', '')}"),
        )


def _log_vad(track: dict) -> None:
    for e in track.get("entries", []):
        start, end = float(e["start"]), float(e["end"])
        _at(start)
        rr.log(
            "vad/speech",
            rr.TextLog(f"speech [{start:.2f}-{end:.2f}] ({end - start:.2f}s)"),
        )
        _at(start)
        rr.log("metrics/vad_active", rr.Scalars(1.0))
        _at(end)
        rr.log("metrics/vad_active", rr.Scalars(0.0))


def _log_align(track: dict) -> None:
    for e in track.get("entries", []):
        for u in e.get("units", []):
            start = float(u["start"])
            _at(start)
            rr.log(
                f"align/text_{e.get('text_id', 0)}",
                rr.TextLog(f"[{start:.2f}-{float(u['end']):.2f}] {u.get('text', '')}"),
            )


def _log_comprehensive(entries: list) -> None:
    for e in entries:
        start = float(e["start"])
        key = _ident(e)
        _at(start)
        rr.log(
            "comprehensive",
            rr.TextLog(
                f"[{start:.2f}-{float(e['end']):.2f}] {key}: {e.get('text', '')}"
            ),
        )


def _log_gpu_telemetry(samples: list) -> None:
    """Spec 011: per-pipeline real-time-factor + scheduling lanes from the
    runtime's periodic gpu_telemetry samples (SerializeGpuTelemetry)."""
    for s in samples:
        if s.get("type") != "gpu_telemetry":
            continue
        t = float(s.get("time_sec", 0.0))
        _at(t)
        for p in s.get("pipelines", []):
            name = p.get("name", "?")
            rtf = p.get("real_time_factor")
            if rtf is not None:
                rr.log(f"gpu/{name}/rtf", rr.Scalars(float(rtf)))
            rr.log(f"gpu/{name}/active",
                   rr.Scalars(1.0 if p.get("stream_active") else 0.0))


def _parse_tegra(line: str) -> dict:
    """Pull a few metrics out of one tegrastats snapshot line."""
    out: dict = {}
    m = re.search(r"RAM (\d+)/(\d+)MB", line)
    if m:
        out["ram_used_mb"] = float(m.group(1))
    m = re.search(r"GR3D_FREQ (\d+)%", line)
    if m:
        out["gpu_pct"] = float(m.group(1))
    return out


def _log_tegrastats(data: dict, audio_sec: float) -> None:
    """Log the coarse before/after/final device snapshots as scalar markers.

    tegrastats here is only three wall-clock snapshots (not a true time series),
    so these are placed approximately along the audio timeline as trend markers.
    """
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
        if "ram_used_mb" in vals:
            rr.log("device/ram_used_mb", rr.Scalars(vals["ram_used_mb"]))
        if "gpu_pct" in vals:
            rr.log("device/gpu_pct", rr.Scalars(vals["gpu_pct"]))


_TRACK_LOGGERS = {
    "diarization": _log_diarization,
    "asr": _log_asr,
    "vad": _log_vad,
    "align": _log_align,
}


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--in", dest="inp", required=True, help="timeline JSON path")
    ap.add_argument("--out", dest="out", help="output .rrd path")
    ap.add_argument("--spawn", action="store_true", help="open the rerun viewer")
    args = ap.parse_args()

    if not args.out and not args.spawn:
        ap.error("provide --out PATH and/or --spawn")

    data = json.loads(Path(args.inp).read_text())
    timeline = data.get("timeline", {})
    tracks = timeline.get("tracks", [])

    rr.init("orator_timeline", spawn=args.spawn)
    if args.out:
        rr.save(args.out)

    for track in tracks:
        logger = _TRACK_LOGGERS.get(track.get("kind"))
        if logger:
            logger(track)
        else:
            print(f"  (skipped unknown track kind: {track.get('kind')})",
                  file=sys.stderr)
        # Per-track real-time factor summary marker (Spec 011 T2.3).
        rtf = track.get("real_time_factor")
        if rtf is not None:
            _at(0.0)
            rr.log(f"metrics/{track.get('kind', '?')}/rtf", rr.Scalars(float(rtf)))
    _log_comprehensive(timeline.get("comprehensive", []))

    telemetry = data.get("telemetry", [])
    _log_gpu_telemetry(telemetry)

    meta = data.get("meta", {})
    _log_tegrastats(data, float(meta.get("audio_sec", 0.0)))
    print(
        f"exported {len(tracks)} tracks, "
        f"{sum(1 for s in telemetry if s.get('type') == 'gpu_telemetry')} gpu samples "
        f"(audio={meta.get('audio_sec', '?')}s, "
        f"stream_rt={meta.get('stream_rt_factor', '?')}x)"
        + (f" -> {args.out}" if args.out else " (spawned viewer)")
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
