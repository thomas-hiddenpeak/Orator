#!/usr/bin/env python3
"""Prepare speaker-business review packets without scoring accuracy.

The constitutional review for speaker attribution is context-aware and manual.
This utility only places the timestamped reference transcript beside the
candidate business view, grouped by time window. It does not compute or report
an accuracy percentage.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any


TS_RE = re.compile(r"^(\d{2}):(\d{2}):(\d{2})\s+(.+?)\s*$")


@dataclass
class RefBlock:
    start: float
    end: float
    speaker: str
    text: str


@dataclass
class ViewEntry:
    start: float
    end: float
    speaker: str
    text: str


def _num(value: Any, default: float = 0.0) -> float:
    if isinstance(value, (int, float)):
        return float(value)
    return default


def _sec_to_hms(sec: float) -> str:
    sec = max(0.0, sec)
    total = int(sec)
    h = total // 3600
    m = (total % 3600) // 60
    s = total % 60
    return f"{h:02d}:{m:02d}:{s:02d}"


def _compact_text(text: str, max_chars: int) -> str:
    compact = " ".join(text.split())
    if max_chars > 0 and len(compact) > max_chars:
        return compact[: max_chars - 1] + "..."
    return compact


def _overlap(a0: float, a1: float, b0: float, b1: float) -> float:
    return max(0.0, min(a1, b1) - max(a0, b0))


def _parse_ref(path: Path, audio_sec: float) -> list[RefBlock]:
    starts: list[tuple[float, str, list[str]]] = []
    current: tuple[float, str, list[str]] | None = None

    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        match = TS_RE.match(line)
        if match:
            if current is not None:
                starts.append(current)
            h, m, s, speaker = match.groups()
            sec = int(h) * 3600 + int(m) * 60 + int(s)
            current = (float(sec), speaker.strip(), [])
            continue
        if current is not None and line:
            current[2].append(line)

    if current is not None:
        starts.append(current)

    out: list[RefBlock] = []
    for idx, (start, speaker, lines) in enumerate(starts):
        end = starts[idx + 1][0] if idx + 1 < len(starts) else audio_sec
        if end > start:
            out.append(RefBlock(start=start, end=end, speaker=speaker, text="".join(lines)))
    return out


def _speaker_label(entry: dict[str, Any]) -> str:
    if entry.get("speaker_name"):
        return str(entry["speaker_name"])
    if entry.get("speaker_id"):
        return str(entry["speaker_id"])
    speaker = entry.get("speaker")
    if isinstance(speaker, int) and speaker >= 0:
        return f"L{speaker}"
    return "unknown"


def _load_view(path: Path) -> tuple[float, list[ViewEntry]]:
    package = json.loads(path.read_text(encoding="utf-8"))
    timeline = package.get("timeline", package)
    if not isinstance(timeline, dict):
        raise ValueError("input does not contain a timeline object")
    audio_sec = _num(timeline.get("audio_sec"))
    raw_entries = timeline.get("comprehensive", [])
    if not isinstance(raw_entries, list):
        raw_entries = []

    entries: list[ViewEntry] = []
    for item in raw_entries:
        if not isinstance(item, dict):
            continue
        start = _num(item.get("start"))
        end = _num(item.get("end"))
        if end <= start:
            continue
        entries.append(
            ViewEntry(
                start=start,
                end=end,
                speaker=_speaker_label(item),
                text=str(item.get("text", "")),
            )
        )
    entries.sort(key=lambda x: (x.start, x.end, x.speaker))
    return audio_sec, entries


def _parse_windows(value: str, audio_sec: float, window_sec: float) -> list[tuple[float, float]]:
    if value:
        out: list[tuple[float, float]] = []
        for item in value.split(","):
            if not item.strip():
                continue
            if "-" not in item:
                raise ValueError(f"invalid window: {item}")
            start_s, end_s = item.split("-", 1)
            start = float(start_s)
            end = float(end_s)
            if end > start:
                out.append((start, end))
        return out

    out = []
    start = 0.0
    while start < audio_sec:
        end = min(audio_sec, start + window_sec)
        out.append((start, end))
        start = end
    return out


def _format_ref(blocks: list[RefBlock], start: float, end: float, max_chars: int) -> list[str]:
    lines = []
    for block in blocks:
        if _overlap(block.start, block.end, start, end) <= 0.0:
            continue
        lines.append(
            f"- `{_sec_to_hms(block.start)}-{_sec_to_hms(block.end)}` "
            f"{block.speaker}: {_compact_text(block.text, max_chars)}"
        )
    return lines or ["- No reference block in this window."]


def _format_view(entries: list[ViewEntry], start: float, end: float, max_chars: int) -> list[str]:
    lines = []
    for entry in entries:
        if _overlap(entry.start, entry.end, start, end) <= 0.0:
            continue
        lines.append(
            f"- `{entry.start:07.3f}-{entry.end:07.3f}` "
            f"{entry.speaker}: {_compact_text(entry.text, max_chars)}"
        )
    return lines or ["- No candidate entry in this window."]


def _render(
    timeline_path: Path,
    reference_path: Path,
    audio_sec: float,
    refs: list[RefBlock],
    entries: list[ViewEntry],
    windows: list[tuple[float, float]],
    max_chars: int,
) -> str:
    out = [
        "# Speaker Business Review Packet",
        "",
        f"- Timeline: `{timeline_path}`",
        f"- Reference: `{reference_path}`",
        f"- Audio seconds: `{audio_sec:.3f}`",
        "- Purpose: side-by-side context review only; no script accuracy score.",
        "",
    ]
    for start, end in windows:
        out.extend(
            [
                f"## {_sec_to_hms(start)}-{_sec_to_hms(end)}",
                "",
                "### Reference",
                "",
                *_format_ref(refs, start, end, max_chars),
                "",
                "### Candidate Business View",
                "",
                *_format_view(entries, start, end, max_chars),
                "",
            ]
        )
    return "\n".join(out)


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--timeline", required=True, help="timeline JSON path")
    parser.add_argument("--reference", required=True, help="timestamped reference txt")
    parser.add_argument("--out", required=True, help="markdown output path")
    parser.add_argument(
        "--window-sec",
        type=float,
        default=600.0,
        help="default window size when --windows is not supplied",
    )
    parser.add_argument(
        "--windows",
        default="",
        help="comma-separated second ranges, for example 0-600,1800-2400",
    )
    parser.add_argument(
        "--max-chars",
        type=int,
        default=220,
        help="maximum characters per reference or candidate item",
    )
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    timeline_path = Path(args.timeline)
    reference_path = Path(args.reference)
    out_path = Path(args.out)

    audio_sec, entries = _load_view(timeline_path)
    refs = _parse_ref(reference_path, audio_sec)
    windows = _parse_windows(args.windows, audio_sec, args.window_sec)
    out_path.write_text(
        _render(
            timeline_path=timeline_path,
            reference_path=reference_path,
            audio_sec=audio_sec,
            refs=refs,
            entries=entries,
            windows=windows,
            max_chars=args.max_chars,
        ),
        encoding="utf-8",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
