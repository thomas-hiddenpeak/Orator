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


def _at(seconds: float) -> None:
    """Place subsequent rr.log calls at `seconds` on the audio timeline."""
    rr.set_time("audio_time", duration=float(seconds))


def _log_diarization(track: dict) -> None:
    for e in track.get("entries", []):
        spk = int(e.get("speaker", -1))
        start, end = float(e["start"]), float(e["end"])
        _at(start)
        rr.log(
            f"diarization/speaker_{spk}",
            rr.TextLog(
                f"speaker {spk}  [{start:.2f}-{end:.2f}]  ({end - start:.2f}s)",
                level=rr.TextLogLevel.INFO,
            ),
        )
        # A step series that marks which speaker is active at segment starts.
        rr.log("metrics/active_speaker", rr.Scalars(float(spk)))


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
        spk = int(e.get("speaker", -1))
        _at(start)
        rr.log(
            "comprehensive",
            rr.TextLog(
                f"[{start:.2f}-{float(e['end']):.2f}] spk{spk}: {e.get('text', '')}"
            ),
        )


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
    _log_comprehensive(timeline.get("comprehensive", []))

    meta = data.get("meta", {})
    print(
        f"exported {len(tracks)} tracks "
        f"(audio={meta.get('audio_sec', '?')}s, "
        f"stream_rt={meta.get('stream_rt_factor', '?')}x)"
        + (f" -> {args.out}" if args.out else " (spawned viewer)")
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
