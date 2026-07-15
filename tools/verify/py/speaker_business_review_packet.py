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
    reference_id: str
    start: float
    end: float
    speaker: str
    text: str
    interval_issue: str = ""


@dataclass
class ViewEntry:
    start: float
    end: float
    speaker: str
    text: str
    reason: str = ""
    uncertain: bool = False


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
        interval_issue = ""
        if end == start:
            interval_issue = "duplicate_source_timestamp"
        elif end < start:
            interval_issue = "backward_source_timestamp"
        out.append(RefBlock(
            reference_id=f"ref-{idx + 1:04d}",
            start=start,
            end=end,
            speaker=speaker,
            text="".join(lines),
            interval_issue=interval_issue,
        ))
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
    if package.get("kind") == "orator_frozen_speaker_candidate":
        raw_entries = package.get("track", [])
        audio_sec = _num(package.get("audio_sec"))
        if audio_sec <= 0.0:
            audio_sec = max(
                (_num(item.get("end")) for item in raw_entries
                 if isinstance(item, dict)),
                default=0.0)
    else:
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
                reason=str(item.get("decision_reason", "")),
                uncertain=bool(item.get("speaker_uncertain", False)),
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
        in_window = (
            _overlap(block.start, block.end, start, end) > 0.0
            if block.end > block.start
            else start <= block.start < end)
        if not in_window:
            continue
        issue = f" [{block.interval_issue}]" if block.interval_issue else ""
        lines.append(
            f"- `{block.reference_id}` "
            f"`{_sec_to_hms(block.start)}-{_sec_to_hms(block.end)}`{issue} "
            f"{block.speaker}: {_compact_text(block.text, max_chars)}"
        )
    return lines or ["- No reference block in this window."]


def _format_view(entries: list[ViewEntry], start: float, end: float, max_chars: int) -> list[str]:
    lines = []
    for entry in entries:
        if _overlap(entry.start, entry.end, start, end) <= 0.0:
            continue
        audit = ""
        if entry.reason:
            state = ", uncertain" if entry.uncertain else ""
            audit = f" [{entry.reason}{state}]"
        lines.append(
            f"- `{entry.start:07.3f}-{entry.end:07.3f}` "
            f"{entry.speaker}{audit}: {_compact_text(entry.text, max_chars)}"
        )
    return lines or ["- No candidate entry in this window."]


def _evidence_window(
    block: RefBlock, audio_sec: float,
) -> tuple[float, float]:
    if block.end > block.start:
        return block.start, block.end
    return max(0.0, block.start - 1.0), min(audio_sec, block.start + 2.0)


def _assignment_signature(
    entries: list[ViewEntry], start: float, end: float,
) -> list[tuple[float, float, str, bool]]:
    signature = []
    for entry in entries:
        if _overlap(entry.start, entry.end, start, end) <= 0.0:
            continue
        signature.append((
            max(start, entry.start),
            min(end, entry.end),
            entry.speaker,
            entry.uncertain,
        ))
    return signature


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


def _render_by_reference(
    timeline_path: Path,
    reference_path: Path,
    audio_sec: float,
    refs: list[RefBlock],
    entries: list[ViewEntry],
    max_chars: int,
    comparison_path: Path | None = None,
    comparison_entries: list[ViewEntry] | None = None,
    only_changed: bool = False,
) -> str:
    selected: list[tuple[RefBlock, float, float, bool | None]] = []
    for block in refs:
        evidence_start, evidence_end = _evidence_window(block, audio_sec)
        changed = None
        if comparison_entries is not None:
            changed = (
                _assignment_signature(
                    comparison_entries, evidence_start, evidence_end)
                != _assignment_signature(
                    entries, evidence_start, evidence_end)
            )
        if only_changed and not changed:
            continue
        selected.append((block, evidence_start, evidence_end, changed))

    out = [
        "# Speaker Business Review Worksheet",
        "",
        f"- Timeline: `{timeline_path}`",
        f"- Reference: `{reference_path}`",
        f"- Reference entries: `{len(refs)}`",
        f"- Displayed entries: `{len(selected)}`",
        "- Purpose: per-reference context display only; no correctness is "
        "assigned by this tool.",
    ]
    if comparison_path is not None:
        out.append(f"- Comparison timeline: `{comparison_path}`")
    out.append("")
    for block, evidence_start, evidence_end, changed in selected:
        issue = f" ({block.interval_issue})" if block.interval_issue else ""
        out.extend(
            [
                f"## {block.reference_id}",
                "",
                f"- Source interval: `{block.start:.3f}-{block.end:.3f}`"
                f"{issue}",
                f"- Reference speaker: `{block.speaker}`",
                f"- Reference text: {_compact_text(block.text, max_chars)}",
            ]
        )
        if changed is not None:
            out.extend([
                f"- Assignment changed: `{'yes' if changed else 'no'}`",
                "- Comparison evidence:",
                *_format_view(
                    comparison_entries or [], evidence_start, evidence_end,
                    max_chars),
            ])
        out.extend([
            "- Candidate evidence:",
            *_format_view(entries, evidence_start, evidence_end, max_chars),
            "",
        ])
    return "\n".join(out)


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--timeline", required=True, help="timeline JSON path")
    parser.add_argument("--reference", required=True, help="timestamped reference txt")
    parser.add_argument("--out", required=True, help="markdown output path")
    parser.add_argument(
        "--comparison-timeline",
        help="optional prior candidate for assignment-only diff display",
    )
    parser.add_argument(
        "--only-changed",
        action="store_true",
        help="with --comparison-timeline, show only changed reference rows",
    )
    parser.add_argument(
        "--by-reference",
        action="store_true",
        help="render one section for every reference row",
    )
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
    comparison_path = (
        Path(args.comparison_timeline) if args.comparison_timeline else None)
    comparison_entries = None
    if comparison_path is not None:
        comparison_audio_sec, comparison_entries = _load_view(comparison_path)
        if abs(comparison_audio_sec - audio_sec) > 1e-3:
            raise ValueError("comparison timeline audio duration differs")
    if args.only_changed and comparison_entries is None:
        raise ValueError("--only-changed requires --comparison-timeline")
    refs = _parse_ref(reference_path, audio_sec)
    windows = _parse_windows(args.windows, audio_sec, args.window_sec)
    renderer = _render_by_reference if args.by_reference else _render
    render_args = dict(
        timeline_path=timeline_path,
        reference_path=reference_path,
        audio_sec=audio_sec,
        refs=refs,
        entries=entries,
        max_chars=args.max_chars,
    )
    if args.by_reference:
        render_args.update(
            comparison_path=comparison_path,
            comparison_entries=comparison_entries,
            only_changed=args.only_changed,
        )
    if not args.by_reference:
        render_args["windows"] = windows
    out_path.write_text(
        renderer(**render_args),
        encoding="utf-8",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
