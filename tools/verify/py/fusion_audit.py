#!/usr/bin/env python3
"""Audit and fuse Orator timeline evidence.

This is an offline verifier/analysis utility. It reads the JSON package emitted
by tools/verify/py/ws_unified_test.py, keeps all captured pipeline tracks
immutable, and writes a candidate comprehensive timeline derived from the frozen
ASR, diarization, VAD, and forced-alignment tracks.

The script only organizes evidence and reports mechanical consistency issues.
It does not assign correctness, calculate accuracy, rank/select a candidate, or
issue a verdict. Complete contextual semantic review with manual result
verification is the only result-evaluation method.
"""

from __future__ import annotations

import argparse
import json
import math
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any


EPS = 1e-6


@dataclass
class AsrEntry:
    text_id: int
    start: float
    end: float
    text: str


@dataclass
class DiarEntry:
    start: float
    end: float
    speaker: int
    confidence: float
    speaker_id: str | None
    speaker_name: str | None


@dataclass
class AlignUnit:
    start: float
    end: float
    text: str


@dataclass
class AlignGroup:
    text_id: int
    start: float
    end: float
    units: list[AlignUnit]


@dataclass
class SpeakerTurn:
    start: float
    end: float
    speaker: int
    speaker_id: str | None
    speaker_name: str | None
    diar_overlap_sec: float
    diar_total_overlap_sec: float


def _num(value: Any, default: float = 0.0) -> float:
    if isinstance(value, (int, float)) and math.isfinite(float(value)):
        return float(value)
    return default


def _int(value: Any, default: int = -1) -> int:
    if isinstance(value, int):
        return value
    if isinstance(value, float) and value.is_integer():
        return int(value)
    return default


def _timeline(package: dict[str, Any]) -> dict[str, Any]:
    tl = package.get("timeline", package)
    if not isinstance(tl, dict):
        raise ValueError("input does not contain a timeline object")
    return tl


def _tracks(tl: dict[str, Any]) -> dict[str, dict[str, Any]]:
    tracks = {}
    for item in tl.get("tracks", []):
        if isinstance(item, dict) and isinstance(item.get("kind"), str):
            tracks[item["kind"]] = item
    return tracks


def _extract_asr(track: dict[str, Any]) -> list[AsrEntry]:
    entries = track.get("entries", [])
    out: list[AsrEntry] = []
    for idx, item in enumerate(entries if isinstance(entries, list) else []):
        if not isinstance(item, dict):
            continue
        out.append(
            AsrEntry(
                text_id=idx,
                start=_num(item.get("start")),
                end=_num(item.get("end")),
                text=str(item.get("text", "")),
            )
        )
    return out


def _extract_diar(track: dict[str, Any]) -> list[DiarEntry]:
    entries = track.get("entries", [])
    out: list[DiarEntry] = []
    for item in entries if isinstance(entries, list) else []:
        if not isinstance(item, dict):
            continue
        out.append(
            DiarEntry(
                start=_num(item.get("start")),
                end=_num(item.get("end")),
                speaker=_int(item.get("speaker")),
                confidence=_num(item.get("confidence"), 0.0),
                speaker_id=(
                    str(item["speaker_id"]) if item.get("speaker_id") else None
                ),
                speaker_name=(
                    str(item["speaker_name"]) if item.get("speaker_name") else None
                ),
            )
        )
    out.sort(key=lambda x: (x.start, x.end, x.speaker))
    return out


def _extract_align(track: dict[str, Any]) -> dict[int, AlignGroup]:
    entries = track.get("entries", [])
    out: dict[int, AlignGroup] = {}
    for item in entries if isinstance(entries, list) else []:
        if not isinstance(item, dict):
            continue
        text_id = _int(item.get("text_id"))
        if text_id < 0:
            continue
        units: list[AlignUnit] = []
        raw_units = item.get("units", [])
        for unit in raw_units if isinstance(raw_units, list) else []:
            if not isinstance(unit, dict):
                continue
            units.append(
                AlignUnit(
                    start=_num(unit.get("start")),
                    end=_num(unit.get("end")),
                    text=str(unit.get("text", "")),
                )
            )
        units.sort(key=lambda x: (x.start, x.end))
        out[text_id] = AlignGroup(
            text_id=text_id,
            start=_num(item.get("start")),
            end=_num(item.get("end")),
            units=units,
        )
    return out


def _entries_by_text_id(entries: list[dict[str, Any]]) -> dict[int, list[dict[str, Any]]]:
    grouped: dict[int, list[dict[str, Any]]] = {}
    for item in entries:
        if not isinstance(item, dict):
            continue
        text_id = _int(item.get("text_id"))
        if text_id < 0:
            continue
        grouped.setdefault(text_id, []).append(item)
    return grouped


def _overlap(a0: float, a1: float, b0: float, b1: float) -> float:
    return max(0.0, min(a1, b1) - max(a0, b0))


def _weighted_speaker(
    start: float, end: float, diar: list[DiarEntry]
) -> tuple[int, str | None, str | None, float, float]:
    """Return speaker chosen by duration-weighted overlap, then confidence."""
    best_speaker = -1
    best_id: str | None = None
    best_name: str | None = None
    best_overlap = 0.0
    best_conf = -1.0
    total_overlap = 0.0
    for seg in diar:
        if seg.end <= start + EPS:
            continue
        if seg.start >= end - EPS:
            break
        ov = _overlap(start, end, seg.start, seg.end)
        if ov <= EPS:
            continue
        total_overlap += ov
        better = ov > best_overlap + EPS
        if not better and abs(ov - best_overlap) <= EPS:
            better = seg.confidence > best_conf
        if better:
            best_speaker = seg.speaker
            best_id = seg.speaker_id
            best_name = seg.speaker_name
            best_overlap = ov
            best_conf = seg.confidence
    return best_speaker, best_id, best_name, best_overlap, total_overlap


def _attribute_interval(
    start: float, end: float, diar: list[DiarEntry]
) -> tuple[int, str | None, str | None, float, float]:
    """Attribute a diar-bounded interval, preferring tighter turns on ties."""
    best_speaker = -1
    best_id: str | None = None
    best_name: str | None = None
    best_overlap = 0.0
    best_span = 0.0
    best_conf = -1.0
    total_overlap = 0.0
    for seg in diar:
        if seg.end <= start + EPS:
            continue
        if seg.start >= end - EPS:
            break
        ov = _overlap(start, end, seg.start, seg.end)
        if ov <= EPS:
            continue
        total_overlap += ov
        span = max(0.0, seg.end - seg.start)
        better = ov > best_overlap + EPS
        if not better and ov >= best_overlap - EPS:
            better = (
                span < best_span - EPS
                or (abs(span - best_span) <= EPS and seg.confidence > best_conf)
            )
        if better:
            best_speaker = seg.speaker
            best_id = seg.speaker_id
            best_name = seg.speaker_name
            best_overlap = ov
            best_span = span
            best_conf = seg.confidence
    return best_speaker, best_id, best_name, best_overlap, total_overlap


def _same_turn_speaker(a: SpeakerTurn, b: SpeakerTurn) -> bool:
    return a.speaker == b.speaker and a.speaker_id == b.speaker_id


def _diar_turns(
    start: float,
    end: float,
    diar: list[DiarEntry],
    max_gapfill_sec: float | None,
) -> list[SpeakerTurn]:
    bounds = [start, end]
    for seg in diar:
        if seg.start > start + EPS and seg.start < end - EPS:
            bounds.append(seg.start)
        if seg.end > start + EPS and seg.end < end - EPS:
            bounds.append(seg.end)
    bounds = sorted(bounds)
    deduped: list[float] = []
    for value in bounds:
        if not deduped or abs(value - deduped[-1]) > EPS:
            deduped.append(value)

    turns: list[SpeakerTurn] = []
    for i in range(len(deduped) - 1):
        s = deduped[i]
        e = deduped[i + 1]
        if e - s <= EPS:
            continue
        speaker, speaker_id, speaker_name, best_overlap, total_overlap = (
            _attribute_interval(s, e, diar)
        )
        turn = SpeakerTurn(
            start=s,
            end=e,
            speaker=speaker,
            speaker_id=speaker_id,
            speaker_name=speaker_name,
            diar_overlap_sec=best_overlap,
            diar_total_overlap_sec=total_overlap,
        )
        if turns and _same_turn_speaker(turns[-1], turn):
            turns[-1].end = e
            turns[-1].diar_overlap_sec += best_overlap
            turns[-1].diar_total_overlap_sec += total_overlap
        else:
            turns.append(turn)

    for idx, turn in enumerate(turns):
        if turn.speaker >= 0:
            continue
        before = None
        after = None
        for seg in diar:
            if seg.end <= turn.start + EPS and (before is None or seg.end > before.end):
                before = seg
            if seg.start >= turn.end - EPS and (after is None or seg.start < after.start):
                after = seg
        if (
            before is not None
            and after is not None
            and before.speaker == after.speaker
            and before.speaker_id == after.speaker_id
            and (
                max_gapfill_sec is None
                or turn.end - turn.start <= max_gapfill_sec + EPS
            )
        ):
            turns[idx] = SpeakerTurn(
                start=turn.start,
                end=turn.end,
                speaker=before.speaker,
                speaker_id=before.speaker_id,
                speaker_name=before.speaker_name,
                diar_overlap_sec=turn.diar_overlap_sec,
                diar_total_overlap_sec=turn.diar_total_overlap_sec,
            )

    merged: list[SpeakerTurn] = []
    for turn in turns:
        if merged and _same_turn_speaker(merged[-1], turn):
            merged[-1].end = turn.end
            merged[-1].diar_overlap_sec += turn.diar_overlap_sec
            merged[-1].diar_total_overlap_sec += turn.diar_total_overlap_sec
        else:
            merged.append(turn)
    return merged


def _vad_overlap(start: float, end: float, vad: list[dict[str, Any]]) -> float:
    total = 0.0
    for seg in vad:
        if not isinstance(seg, dict):
            continue
        s = _num(seg.get("start"))
        e = _num(seg.get("end"))
        if e <= start + EPS:
            continue
        if s >= end - EPS:
            break
        total += _overlap(start, end, s, e)
    return total


def _unit_runs(
    units: list[AlignUnit],
    pause_sec: float,
    boundary_tolerance_sec: float,
    turns: list[SpeakerTurn] | None = None,
) -> list[list[AlignUnit]]:
    runs: list[list[AlignUnit]] = []
    cur: list[AlignUnit] = []
    turns = turns or []

    def boundary_near_gap(gap_start: float, gap_end: float) -> bool:
        if boundary_tolerance_sec <= 0.0:
            return False
        for idx in range(1, len(turns)):
            left = turns[idx - 1]
            right = turns[idx]
            if _same_turn_speaker(left, right):
                continue
            boundary = right.start
            if (
                boundary >= gap_start - boundary_tolerance_sec
                and boundary <= gap_end + boundary_tolerance_sec
            ):
                return True
        return False

    for unit in units:
        if cur:
            gap = unit.start - cur[-1].end
            if gap > pause_sec or boundary_near_gap(cur[-1].end, unit.start):
                runs.append(cur)
                cur = []
        cur.append(unit)
    if cur:
        runs.append(cur)
    return runs


def _fallback_from_current(
    asr: AsrEntry,
    current_by_id: dict[int, list[dict[str, Any]]],
    diar: list[DiarEntry],
    vad: list[dict[str, Any]],
) -> list[dict[str, Any]]:
    current = current_by_id.get(asr.text_id)
    if current:
        out = []
        for item in current:
            copied = dict(item)
            copied["boundary_source"] = "current_comprehensive"
            copied["speaker_source"] = "current_comprehensive"
            copied["vad_overlap_sec"] = round(
                _vad_overlap(_num(copied.get("start")), _num(copied.get("end")), vad),
                3,
            )
            out.append(copied)
        return out

    speaker, speaker_id, speaker_name, best_overlap, total_overlap = _weighted_speaker(
        asr.start, asr.end, diar
    )
    entry: dict[str, Any] = {
        "start": round(asr.start, 3),
        "end": round(asr.end, 3),
        "text_id": asr.text_id,
        "speaker": speaker,
        "text": asr.text,
        "boundary_source": "asr_span",
        "speaker_source": "diar_overlap",
        "diar_overlap_sec": round(best_overlap, 3),
        "diar_total_overlap_sec": round(total_overlap, 3),
        "vad_overlap_sec": round(_vad_overlap(asr.start, asr.end, vad), 3),
    }
    if speaker_id:
        entry["speaker_id"] = speaker_id
    if speaker_name:
        entry["speaker_name"] = speaker_name
    return [entry]


def fuse(
    asr_entries: list[AsrEntry],
    diar_entries: list[DiarEntry],
    align_groups: dict[int, AlignGroup],
    vad_entries: list[dict[str, Any]],
    current_comprehensive: list[dict[str, Any]],
    pause_sec: float,
    boundary_tolerance_sec: float,
    min_run_sec: float,
    short_flip_sec: float,
    short_flip_chars: int,
    short_flip_gap_sec: float,
    fill_unknown_sec: float,
    min_diar_coverage_ratio: float,
    min_diar_overlap_sec: float,
    diar_gapfill_max_sec: float | None,
) -> list[dict[str, Any]]:
    current_by_id = _entries_by_text_id(current_comprehensive)
    out: list[dict[str, Any]] = []

    for asr in asr_entries:
        group = align_groups.get(asr.text_id)
        if not group or not group.units:
            out.extend(_fallback_from_current(asr, current_by_id, diar_entries, vad_entries))
            continue

        turns = _diar_turns(asr.start, asr.end, diar_entries, diar_gapfill_max_sec)
        if not turns:
            out.extend(_fallback_from_current(asr, current_by_id, diar_entries, vad_entries))
            continue
        runs = _unit_runs(
            group.units,
            pause_sec,
            boundary_tolerance_sec,
            turns,
        )

        slices = ["" for _ in turns]
        align_counts = [0 for _ in turns]
        for run in runs:
            run_start = run[0].start
            run_end = run[-1].end
            if run_end < run_start:
                run_start, run_end = run_end, run_start
            if run_end - run_start < min_run_sec:
                mid = 0.5 * (run_start + run_end)
                run_start = max(asr.start, mid - min_run_sec * 0.5)
                run_end = min(asr.end, mid + min_run_sec * 0.5)
            mid = 0.5 * (run_start + run_end)
            turn_idx = 0
            while turn_idx + 1 < len(turns) and mid >= turns[turn_idx].end - EPS:
                turn_idx += 1
            slices[turn_idx] += "".join(unit.text for unit in run)
            align_counts[turn_idx] += len(run)

        for turn, text, align_count in zip(turns, slices, align_counts):
            if not text:
                continue
            entry = {
                "start": round(turn.start, 3),
                "end": round(turn.end, 3),
                "text_id": asr.text_id,
                "speaker": turn.speaker,
                "text": text,
                "boundary_source": "align_units_midpoint_turn",
                "speaker_source": "diar_turn_midpoint",
                "align_units": align_count,
                "diar_overlap_sec": round(turn.diar_overlap_sec, 3),
                "diar_total_overlap_sec": round(turn.diar_total_overlap_sec, 3),
                "vad_overlap_sec": round(
                    _vad_overlap(turn.start, turn.end, vad_entries), 3
                ),
            }
            if turn.speaker_id:
                entry["speaker_id"] = turn.speaker_id
            if turn.speaker_name:
                entry["speaker_name"] = turn.speaker_name
            out.append(entry)

    out.sort(key=lambda x: (_num(x.get("start")), _num(x.get("end")), _int(x.get("text_id"))))
    out = _coalesce_candidate(out)
    if min_diar_coverage_ratio > 0.0 or min_diar_overlap_sec > 0.0:
        out = _downgrade_low_diar_coverage(
            out,
            min_ratio=min_diar_coverage_ratio,
            min_overlap_sec=min_diar_overlap_sec,
        )
        out = _coalesce_candidate(out)
    if short_flip_sec > 0.0 or fill_unknown_sec > 0.0:
        out = _smooth_context(
            out,
            short_flip_sec=short_flip_sec,
            short_flip_chars=short_flip_chars,
            short_flip_gap_sec=short_flip_gap_sec,
            fill_unknown_sec=fill_unknown_sec,
        )
        out = _coalesce_candidate(out)
    return out


def _coalesce_candidate(entries: list[dict[str, Any]]) -> list[dict[str, Any]]:
    merged: list[dict[str, Any]] = []
    for item in entries:
        if not merged:
            merged.append(item)
            continue
        prev = merged[-1]
        same_source = (
            _int(prev.get("text_id")) == _int(item.get("text_id"))
            and _int(prev.get("speaker")) == _int(item.get("speaker"))
            and prev.get("speaker_id") == item.get("speaker_id")
            and prev.get("boundary_source") == item.get("boundary_source")
        )
        adjacent = _num(item.get("start")) - _num(prev.get("end")) <= EPS
        if same_source and adjacent:
            prev["end"] = item.get("end")
            prev["text"] = str(prev.get("text", "")) + str(item.get("text", ""))
            prev["align_units"] = _int(prev.get("align_units"), 0) + _int(
                item.get("align_units"), 0
            )
            prev["diar_overlap_sec"] = round(
                _num(prev.get("diar_overlap_sec")) + _num(item.get("diar_overlap_sec")),
                3,
            )
            prev["diar_total_overlap_sec"] = round(
                _num(prev.get("diar_total_overlap_sec"))
                + _num(item.get("diar_total_overlap_sec")),
                3,
            )
            prev["vad_overlap_sec"] = round(
                _num(prev.get("vad_overlap_sec")) + _num(item.get("vad_overlap_sec")),
                3,
            )
        else:
            merged.append(item)
    return merged


def _speaker_key(entry: dict[str, Any]) -> tuple[Any, Any]:
    return (entry.get("speaker_id"), _int(entry.get("speaker")))


def _same_speaker(a: dict[str, Any], b: dict[str, Any]) -> bool:
    return _speaker_key(a) == _speaker_key(b)


def _speaker_known(entry: dict[str, Any]) -> bool:
    return _int(entry.get("speaker")) >= 0 or bool(entry.get("speaker_id"))


def _text_chars(entry: dict[str, Any]) -> int:
    return len(str(entry.get("text", "")))


def _duration(entry: dict[str, Any]) -> float:
    return max(0.0, _num(entry.get("end")) - _num(entry.get("start")))


def _copy_speaker(dst: dict[str, Any], src: dict[str, Any], reason: str) -> None:
    if "raw_speaker" not in dst:
        dst["raw_speaker"] = dst.get("speaker")
    if "raw_speaker_id" not in dst and dst.get("speaker_id"):
        dst["raw_speaker_id"] = dst.get("speaker_id")
    if "raw_speaker_name" not in dst and dst.get("speaker_name"):
        dst["raw_speaker_name"] = dst.get("speaker_name")

    dst["speaker"] = src.get("speaker", -1)
    if src.get("speaker_id"):
        dst["speaker_id"] = src.get("speaker_id")
    else:
        dst.pop("speaker_id", None)
    if src.get("speaker_name"):
        dst["speaker_name"] = src.get("speaker_name")
    else:
        dst.pop("speaker_name", None)
    dst["speaker_source"] = "context_smooth"
    dst["speaker_smoothing"] = reason


def _clear_speaker(dst: dict[str, Any], reason: str) -> None:
    if "raw_speaker" not in dst:
        dst["raw_speaker"] = dst.get("speaker")
    if "raw_speaker_id" not in dst and dst.get("speaker_id"):
        dst["raw_speaker_id"] = dst.get("speaker_id")
    if "raw_speaker_name" not in dst and dst.get("speaker_name"):
        dst["raw_speaker_name"] = dst.get("speaker_name")
    dst["speaker"] = -1
    dst.pop("speaker_id", None)
    dst.pop("speaker_name", None)
    dst["speaker_source"] = "low_diar_coverage"
    dst["speaker_smoothing"] = reason


def _downgrade_low_diar_coverage(
    entries: list[dict[str, Any]],
    min_ratio: float,
    min_overlap_sec: float,
) -> list[dict[str, Any]]:
    """Mark weakly supported speaker attribution as unknown.

    The rule only changes the derived business view. It preserves the immutable
    diarization track and keeps raw_speaker fields for review. This is useful
    for detecting cases where a sparse diar segment would otherwise own a much
    longer ASR span.
    """
    out = [dict(e) for e in entries]
    for item in out:
        if not _speaker_known(item):
            continue
        dur = _duration(item)
        if dur <= EPS:
            continue
        overlap = _num(item.get("diar_overlap_sec"))
        ratio = overlap / dur
        if min_overlap_sec > 0.0 and overlap + EPS < min_overlap_sec:
            _clear_speaker(item, "diar_overlap_below_min")
            continue
        if min_ratio > 0.0 and ratio + EPS < min_ratio:
            _clear_speaker(item, "diar_coverage_ratio_below_min")
    return out


def _smooth_context(
    entries: list[dict[str, Any]],
    short_flip_sec: float,
    short_flip_chars: int,
    short_flip_gap_sec: float,
    fill_unknown_sec: float,
) -> list[dict[str, Any]]:
    """Contextual speaker smoothing for business-view candidates.

    This keeps the pipeline evidence intact in the output fields: any changed
    entry records its original speaker in raw_speaker/raw_speaker_id. It only
    corrects tiny local flips bounded by the same known speaker on both sides,
    or short unknown gaps bounded by the same speaker. The rule is deliberately
    opt-in and conservative; it is a candidate generator, not an accuracy score.
    """
    out = [dict(e) for e in entries]
    for i in range(1, len(out) - 1):
        prev = out[i - 1]
        cur = out[i]
        nxt = out[i + 1]
        if not _speaker_known(prev) or not _same_speaker(prev, nxt):
            continue
        gap_left = _num(cur.get("start")) - _num(prev.get("end"))
        gap_right = _num(nxt.get("start")) - _num(cur.get("end"))
        if gap_left > short_flip_gap_sec or gap_right > short_flip_gap_sec:
            continue

        dur = max(0.0, _num(cur.get("end")) - _num(cur.get("start")))
        is_unknown = not _speaker_known(cur)
        is_tiny = dur <= short_flip_sec or _text_chars(cur) <= short_flip_chars

        if is_unknown and fill_unknown_sec > 0.0 and dur <= fill_unknown_sec:
            _copy_speaker(cur, prev, "short_unknown_between_same_speaker")
        elif (
            short_flip_sec > 0.0
            and is_tiny
            and _speaker_known(cur)
            and not _same_speaker(cur, prev)
        ):
            _copy_speaker(cur, prev, "short_flip_between_same_speaker")
    return out


def business_turns(
    entries: list[dict[str, Any]], turn_gap_sec: float
) -> list[dict[str, Any]]:
    turns: list[dict[str, Any]] = []
    for idx, entry in enumerate(entries):
        if not turns:
            turns.append(_new_business_turn(entry, idx))
            continue
        prev = turns[-1]
        gap = _num(entry.get("start")) - _num(prev.get("end"))
        same = (
            prev.get("speaker_id") == entry.get("speaker_id")
            and _int(prev.get("speaker")) == _int(entry.get("speaker"))
        )
        if same and gap <= turn_gap_sec:
            prev["end"] = entry.get("end")
            prev["text"] = str(prev.get("text", "")) + str(entry.get("text", ""))
            prev["entry_count"] = _int(prev.get("entry_count"), 0) + 1
            text_id = _int(entry.get("text_id"))
            if text_id >= 0 and text_id not in prev["text_ids"]:
                prev["text_ids"].append(text_id)
            prev["source_entry_indices"].append(idx)
        else:
            turns.append(_new_business_turn(entry, idx))
    return turns


def _new_business_turn(entry: dict[str, Any], idx: int) -> dict[str, Any]:
    text_id = _int(entry.get("text_id"))
    turn: dict[str, Any] = {
        "start": entry.get("start"),
        "end": entry.get("end"),
        "speaker": entry.get("speaker", -1),
        "text": entry.get("text", ""),
        "text_ids": [text_id] if text_id >= 0 else [],
        "entry_count": 1,
        "source_entry_indices": [idx],
        "boundary_source": "business_turn_merge",
        "speaker_source": entry.get("speaker_source"),
    }
    if entry.get("speaker_id"):
        turn["speaker_id"] = entry.get("speaker_id")
    if entry.get("speaker_name"):
        turn["speaker_name"] = entry.get("speaker_name")
    return turn


def audit(
    tl: dict[str, Any],
    asr_entries: list[AsrEntry],
    diar_entries: list[DiarEntry],
    align_groups: dict[int, AlignGroup],
    candidate: list[dict[str, Any]],
    turns: list[dict[str, Any]],
) -> dict[str, Any]:
    align_text_ids = set(align_groups)
    asr_text_ids = {x.text_id for x in asr_entries}
    asr_by_id = {x.text_id: x for x in asr_entries}
    issues: list[dict[str, Any]] = []

    for text_id in sorted(align_text_ids - asr_text_ids):
        issues.append({"kind": "align_without_asr", "text_id": text_id})

    for text_id in sorted(asr_text_ids - align_text_ids):
        issues.append({"kind": "asr_without_align", "text_id": text_id})

    for text_id, group in sorted(align_groups.items()):
        asr = asr_by_id.get(text_id)
        if not asr:
            continue
        if group.start < asr.start - 0.05 or group.end > asr.end + 0.05:
            issues.append(
                {
                    "kind": "align_group_outside_asr",
                    "text_id": text_id,
                    "asr": [round(asr.start, 3), round(asr.end, 3)],
                    "align": [round(group.start, 3), round(group.end, 3)],
                }
            )
        prev_end = None
        for idx, unit in enumerate(group.units):
            if unit.end < unit.start - EPS:
                issues.append(
                    {
                        "kind": "align_unit_negative_duration",
                        "text_id": text_id,
                        "unit_index": idx,
                    }
                )
            if unit.start < asr.start - 0.05 or unit.end > asr.end + 0.05:
                issues.append(
                    {
                        "kind": "align_unit_outside_asr",
                        "text_id": text_id,
                        "unit_index": idx,
                        "asr": [round(asr.start, 3), round(asr.end, 3)],
                        "unit": [round(unit.start, 3), round(unit.end, 3)],
                    }
                )
            if prev_end is not None and unit.start < prev_end - 0.05:
                issues.append(
                    {
                        "kind": "align_unit_backtrack",
                        "text_id": text_id,
                        "unit_index": idx,
                        "previous_end": round(prev_end, 3),
                        "start": round(unit.start, 3),
                    }
                )
            prev_end = unit.end

    for idx, entry in enumerate(candidate):
        start = _num(entry.get("start"))
        end = _num(entry.get("end"))
        text_id = _int(entry.get("text_id"))
        asr = asr_by_id.get(text_id)
        if end < start - EPS:
            issues.append({"kind": "candidate_negative_duration", "index": idx})
        if asr and (start < asr.start - 0.10 or end > asr.end + 0.10):
            issues.append(
                {
                    "kind": "candidate_outside_asr",
                    "index": idx,
                    "text_id": text_id,
                    "asr": [round(asr.start, 3), round(asr.end, 3)],
                    "candidate": [round(start, 3), round(end, 3)],
                }
            )

    unknown_sec = 0.0
    for entry in candidate:
        if _int(entry.get("speaker")) < 0:
            unknown_sec += max(0.0, _num(entry.get("end")) - _num(entry.get("start")))

    return {
        "audio_sec": _num(tl.get("audio_sec")),
        "track_counts": {
            "asr": len(asr_entries),
            "diarization": len(diar_entries),
            "align": len(align_groups),
            "comprehensive_current": len(tl.get("comprehensive", [])),
            "comprehensive_candidate": len(candidate),
            "business_turns": len(turns),
        },
        "align_coverage": {
            "aligned_asr_segments": len(asr_text_ids & align_text_ids),
            "missing_align_segments": len(asr_text_ids - align_text_ids),
            "extra_align_segments": len(align_text_ids - asr_text_ids),
        },
        "candidate": {
            "unknown_speaker_sec": round(unknown_sec, 3),
            "unknown_speaker_pct_of_audio": round(
                (unknown_sec / _num(tl.get("audio_sec")) * 100.0)
                if _num(tl.get("audio_sec")) > 0
                else 0.0,
                2,
            ),
        },
        "business_turns": _business_turn_audit(tl, turns),
        "issues": issues,
        "issue_counts": _count_by_kind(issues),
    }


def _business_turn_audit(
    tl: dict[str, Any], turns: list[dict[str, Any]]
) -> dict[str, Any]:
    unknown_sec = 0.0
    for turn in turns:
        if _int(turn.get("speaker")) < 0:
            unknown_sec += max(0.0, _num(turn.get("end")) - _num(turn.get("start")))
    audio = _num(tl.get("audio_sec"))
    return {
        "count": len(turns),
        "unknown_speaker_sec": round(unknown_sec, 3),
        "unknown_speaker_pct_of_audio": round(
            (unknown_sec / audio * 100.0) if audio > 0.0 else 0.0, 2
        ),
    }


def _count_by_kind(issues: list[dict[str, Any]]) -> dict[str, int]:
    counts: dict[str, int] = {}
    for issue in issues:
        kind = str(issue.get("kind", "unknown"))
        counts[kind] = counts.get(kind, 0) + 1
    return dict(sorted(counts.items()))


def _print_summary(audit_doc: dict[str, Any], path: Path | None) -> None:
    counts = audit_doc["track_counts"]
    coverage = audit_doc["align_coverage"]
    candidate = audit_doc["candidate"]
    print("fusion audit")
    if path is not None:
        print(f"  wrote: {path}")
    print(f"  audio_sec: {audit_doc['audio_sec']:.3f}")
    print(
        "  tracks: "
        f"asr={counts['asr']} diar={counts['diarization']} "
        f"align={counts['align']} current={counts['comprehensive_current']} "
        f"candidate={counts['comprehensive_candidate']} "
        f"business_turns={counts['business_turns']}"
    )
    print(
        "  align coverage: "
        f"aligned={coverage['aligned_asr_segments']} "
        f"missing={coverage['missing_align_segments']} "
        f"extra={coverage['extra_align_segments']}"
    )
    print(
        "  candidate unknown: "
        f"{candidate['unknown_speaker_sec']:.3f}s "
        f"({candidate['unknown_speaker_pct_of_audio']:.2f}%)"
    )
    turns = audit_doc["business_turns"]
    print(
        "  business unknown: "
        f"{turns['unknown_speaker_sec']:.3f}s "
        f"({turns['unknown_speaker_pct_of_audio']:.2f}%)"
    )
    if audit_doc["issue_counts"]:
        print("  issues:")
        for kind, count in audit_doc["issue_counts"].items():
            print(f"    {kind}: {count}")
    else:
        print("  issues: none")


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, help="ws_unified_test JSON path")
    parser.add_argument("--out", help="output JSON path")
    parser.add_argument(
        "--timeline-out",
        help=(
            "optional diagnostic timeline JSON path with timeline.comprehensive "
            "replaced by the selected fusion view"
        ),
    )
    parser.add_argument(
        "--timeline-view",
        choices=("comprehensive", "business_turns"),
        default="business_turns",
        help="fusion view to write when --timeline-out is set",
    )
    parser.add_argument(
        "--pause-sec",
        type=float,
        default=None,
        help="deprecated alias for --align-snap-pause-sec",
    )
    parser.add_argument(
        "--align-snap-pause-sec",
        type=float,
        default=None,
        help=(
            "gap between alignment units still treated as one coherent speaker "
            "attribution run; default matches orator.toml"
        ),
    )
    parser.add_argument(
        "--align-boundary-split-tolerance-sec",
        type=float,
        default=0.08,
        help="force an alignment run split when a diar boundary is this close to a unit gap",
    )
    parser.add_argument(
        "--min-run-sec",
        type=float,
        default=0.02,
        help="minimum duration assigned to an alignment unit run",
    )
    parser.add_argument(
        "--short-flip-sec",
        type=float,
        default=0.0,
        help="opt-in smoothing: max duration of a short speaker flip to relabel",
    )
    parser.add_argument(
        "--short-flip-chars",
        type=int,
        default=2,
        help="opt-in smoothing: max text length of a short speaker flip to relabel",
    )
    parser.add_argument(
        "--short-flip-gap-sec",
        type=float,
        default=0.8,
        help="opt-in smoothing: max gap to surrounding same-speaker entries",
    )
    parser.add_argument(
        "--fill-unknown-sec",
        type=float,
        default=0.0,
        help="opt-in smoothing: max unknown duration to fill between same speaker",
    )
    parser.add_argument(
        "--min-diar-coverage-ratio",
        type=float,
        default=0.0,
        help=(
            "opt-in smoothing: mark speaker unknown when diar overlap covers less "
            "than this fraction of the fused entry duration"
        ),
    )
    parser.add_argument(
        "--min-diar-overlap-sec",
        type=float,
        default=0.0,
        help="opt-in smoothing: mark speaker unknown below this diar overlap duration",
    )
    parser.add_argument(
        "--diar-gapfill-max-sec",
        type=float,
        default=-1.0,
        help=(
            "max unknown diar sub-interval to fill between same speaker; "
            "negative preserves the current unlimited behavior"
        ),
    )
    parser.add_argument(
        "--business-turn-gap-sec",
        type=float,
        default=1.0,
        help="gap for merging same-speaker candidate entries into business turns",
    )
    parser.add_argument(
        "--print-summary",
        action="store_true",
        help="print a concise audit summary",
    )
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    align_snap_pause_sec = (
        args.align_snap_pause_sec
        if args.align_snap_pause_sec is not None
        else args.pause_sec
        if args.pause_sec is not None
        else 0.25
    )
    input_path = Path(args.input)
    with input_path.open("r", encoding="utf-8") as f:
        package = json.load(f)

    tl = _timeline(package)
    tracks = _tracks(tl)
    asr_entries = _extract_asr(tracks.get("asr", {}))
    diar_entries = _extract_diar(tracks.get("diarization", {}))
    align_groups = _extract_align(tracks.get("align", {}))
    vad_track = tracks.get("vad", {})
    vad_entries = vad_track.get("entries", [])
    if not isinstance(vad_entries, list):
        vad_entries = []
    vad_entries.sort(key=lambda x: _num(x.get("start")) if isinstance(x, dict) else 0.0)
    current = tl.get("comprehensive", [])
    if not isinstance(current, list):
        current = []

    candidate = fuse(
        asr_entries=asr_entries,
        diar_entries=diar_entries,
        align_groups=align_groups,
        vad_entries=vad_entries,
        current_comprehensive=current,
        pause_sec=align_snap_pause_sec,
        boundary_tolerance_sec=args.align_boundary_split_tolerance_sec,
        min_run_sec=args.min_run_sec,
        short_flip_sec=args.short_flip_sec,
        short_flip_chars=args.short_flip_chars,
        short_flip_gap_sec=args.short_flip_gap_sec,
        fill_unknown_sec=args.fill_unknown_sec,
        min_diar_coverage_ratio=args.min_diar_coverage_ratio,
        min_diar_overlap_sec=args.min_diar_overlap_sec,
        diar_gapfill_max_sec=(
            None if args.diar_gapfill_max_sec < 0.0 else args.diar_gapfill_max_sec
        ),
    )
    turns = business_turns(candidate, args.business_turn_gap_sec)
    audit_doc = audit(tl, asr_entries, diar_entries, align_groups, candidate, turns)
    out_doc = {
        "source": str(input_path),
        "parameters": {
            "align_snap_pause_sec": align_snap_pause_sec,
            "align_boundary_split_tolerance_sec": (
                args.align_boundary_split_tolerance_sec
            ),
            "min_run_sec": args.min_run_sec,
            "short_flip_sec": args.short_flip_sec,
            "short_flip_chars": args.short_flip_chars,
            "short_flip_gap_sec": args.short_flip_gap_sec,
            "fill_unknown_sec": args.fill_unknown_sec,
            "min_diar_coverage_ratio": args.min_diar_coverage_ratio,
            "min_diar_overlap_sec": args.min_diar_overlap_sec,
            "diar_gapfill_max_sec": args.diar_gapfill_max_sec,
            "business_turn_gap_sec": args.business_turn_gap_sec,
        },
        "audit": audit_doc,
        "fusion": {
            "strategy": "diar_turns_with_align_run_midpoint",
            "comprehensive": candidate,
            "business_turns": turns,
        },
    }

    out_path = Path(args.out) if args.out else None
    if out_path:
        with out_path.open("w", encoding="utf-8") as f:
            json.dump(out_doc, f, ensure_ascii=False, indent=2)

    timeline_out_path = Path(args.timeline_out) if args.timeline_out else None
    if timeline_out_path:
        selected_view = turns if args.timeline_view == "business_turns" else candidate
        diagnostic_package = dict(package)
        diagnostic_timeline = dict(tl)
        diagnostic_timeline["comprehensive"] = selected_view
        diagnostic_timeline["fusion_diagnostic"] = {
            "source": str(input_path),
            "view": args.timeline_view,
            "parameters": out_doc["parameters"],
            "strategy": out_doc["fusion"]["strategy"],
        }
        diagnostic_package["timeline"] = diagnostic_timeline
        with timeline_out_path.open("w", encoding="utf-8") as f:
            json.dump(diagnostic_package, f, ensure_ascii=False, indent=2)

    if args.print_summary or not out_path:
        _print_summary(audit_doc, out_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
