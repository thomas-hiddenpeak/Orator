#!/usr/bin/env python3
"""Arrange frozen cross-pipeline evidence for contextual human review.

This utility copies reference sections, terminal timeline entries, raw
Sortformer rows, and observed local-to-global identity epochs into independent
context directories. It performs source and shape checks only. It never
assigns correctness, groups causes, scores or ranks repairs, selects behavior,
or issues a product verdict.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import re
import sys
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Any


EPSILON = 1e-9
SERIALIZED_FRAME_CONTIGUITY_SEC = 2e-6
CONTEXT_COLUMNS = [
    "context_id", "start_sec", "end_sec", "focus_refs", "control_refs",
]
TRACK_KINDS = [
    "diarization",
    "primary_speaker",
    "asr",
    "vad",
    "align",
    "speaker_voiceprint",
    "business_speaker",
]
VAD_WINDOW_COLUMNS = [
    "evidence_id", "start_sample", "end_sample", "start_sec", "end_sec",
    "speech_probability",
]
VAD_STATE_COLUMNS = [
    "evidence_id", "final", "in_speech", "observed_until_sample",
    "observed_until_sec", "active_start_sample", "active_start_sec",
    "active_stable_until_sample", "active_stable_until_sec",
    "silence_stable_until_sample", "silence_stable_until_sec",
]
REFERENCE_TS_RE = re.compile(r"^(\d{2}):(\d{2}):(\d{2})\s+(.+?)\s*$")
REFERENCE_ID_RE = re.compile(r"^(?:ref-)?(\d{4})$")


@dataclass(frozen=True)
class Context:
    context_id: str
    start: float
    end: float
    focus_refs: tuple[str, ...]
    control_refs: tuple[str, ...]


@dataclass(frozen=True)
class ReferenceBlock:
    reference_id: str
    start: float
    end: float
    speaker: str
    text: str
    interval_issue: str


@dataclass(frozen=True)
class PosteriorRow:
    frame: int
    time_sec: float
    session: int
    top1: int
    top1_prob: float
    line: str


@dataclass(frozen=True)
class RawTsvEvidence:
    columns: tuple[str, ...]
    rows: tuple[dict[str, str], ...]


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def load_raw_tsv(path: Path, expected_columns: list[str]) -> RawTsvEvidence:
    with path.open(encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source, delimiter="\t")
        if reader.fieldnames != expected_columns:
            raise ValueError(
                f"raw evidence columns differ for {path.name}")
        rows = tuple(dict(row) for row in reader)
    if not rows:
        raise ValueError(f"raw evidence is empty: {path.name}")
    return RawTsvEvidence(tuple(expected_columns), rows)


def verify_vad_window_evidence(
    evidence: RawTsvEvidence, sample_rate: int,
) -> None:
    prior_end = 0
    for index, row in enumerate(evidence.rows):
        if row["evidence_id"] != f"vad_window:{index}":
            raise ValueError("VAD window evidence IDs are not contiguous")
        start = int(row["start_sample"])
        end = int(row["end_sample"])
        start_sec = float(row["start_sec"])
        end_sec = float(row["end_sec"])
        probability = float(row["speech_probability"])
        if start != prior_end or end <= start:
            raise ValueError("VAD window sample bounds are not contiguous")
        if not all(math.isfinite(value) for value in (
                start_sec, end_sec, probability)):
            raise ValueError("VAD window evidence contains a non-finite value")
        if (abs(start_sec - start / sample_rate) > 1e-9 or
                abs(end_sec - end / sample_rate) > 1e-9):
            raise ValueError("VAD window time differs from sample bounds")
        if not 0.0 <= probability <= 1.0:
            raise ValueError("VAD window probability is outside [0,1]")
        prior_end = end


def verify_vad_state_evidence(
    evidence: RawTsvEvidence, sample_rate: int,
) -> None:
    prior_observed = -1
    for index, row in enumerate(evidence.rows):
        if row["evidence_id"] != f"vad_state:{index}":
            raise ValueError("VAD state evidence IDs are not contiguous")
        if row["final"] not in ("true", "false"):
            raise ValueError("VAD final state is not boolean")
        if (row["final"] == "true") != (index == len(evidence.rows) - 1):
            raise ValueError("VAD final state is not the last observation")
        if row["in_speech"] not in ("true", "false"):
            raise ValueError("VAD speech state is not boolean")
        observed = int(row["observed_until_sample"])
        observed_sec = float(row["observed_until_sec"])
        if observed < prior_observed:
            raise ValueError("VAD observed frontiers are not monotonic")
        if not math.isfinite(observed_sec):
            raise ValueError("VAD observed time is not finite")
        if abs(observed_sec - observed / sample_rate) > 1e-9:
            raise ValueError("VAD observed time differs from sample frontier")
        for sample_field, sec_field in (
                ("active_start_sample", "active_start_sec"),
                ("active_stable_until_sample", "active_stable_until_sec"),
                ("silence_stable_until_sample",
                 "silence_stable_until_sec")):
            frontier = int(row[sample_field])
            frontier_sec = float(row[sec_field])
            if frontier > observed:
                raise ValueError("VAD state exceeds its observed frontier")
            if not math.isfinite(frontier_sec):
                raise ValueError("VAD state time is not finite")
            expected_sec = -1.0 if frontier == -1 else frontier / sample_rate
            if frontier < -1 or abs(frontier_sec - expected_sec) > 1e-9:
                raise ValueError("VAD state time differs from sample frontier")
        prior_observed = observed


def write_json(path: Path, value: Any) -> None:
    path.write_text(
        json.dumps(value, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )


def normalize_reference_id(value: str) -> str:
    match = REFERENCE_ID_RE.fullmatch(value.strip())
    if not match:
        raise ValueError(f"invalid reference ID: {value}")
    return f"ref-{match.group(1)}"


def parse_reference_list(value: str) -> tuple[str, ...]:
    if not value.strip():
        return ()
    result = tuple(normalize_reference_id(item) for item in value.split(","))
    if len(set(result)) != len(result):
        raise ValueError("context contains duplicate reference IDs")
    return result


def load_contexts(path: Path) -> list[Context]:
    contexts: list[Context] = []
    with path.open(encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source, delimiter="\t")
        if reader.fieldnames != CONTEXT_COLUMNS:
            raise ValueError(
                "context TSV columns must be exactly: " +
                ",".join(CONTEXT_COLUMNS))
        for row in reader:
            context_id = normalize_reference_id(row["context_id"])
            start = float(row["start_sec"])
            end = float(row["end_sec"])
            focus_refs = parse_reference_list(row["focus_refs"])
            control_refs = parse_reference_list(row["control_refs"])
            if start < 0.0 or end <= start:
                raise ValueError(f"invalid context bounds: {context_id}")
            if not focus_refs:
                raise ValueError(f"context has no focus reference: {context_id}")
            if set(focus_refs) & set(control_refs):
                raise ValueError(f"focus/control overlap: {context_id}")
            contexts.append(Context(
                context_id, start, end, focus_refs, control_refs))
    if not contexts:
        raise ValueError("context TSV is empty")
    if len({item.context_id for item in contexts}) != len(contexts):
        raise ValueError("context IDs are not unique")
    if contexts != sorted(contexts, key=lambda item: (
            item.start, item.end, item.context_id)):
        raise ValueError("contexts are not in chronological order")
    return contexts


def load_reference(path: Path, audio_sec: float) -> list[ReferenceBlock]:
    starts: list[tuple[float, str, list[str]]] = []
    current: tuple[float, str, list[str]] | None = None
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        match = REFERENCE_TS_RE.match(line)
        if match:
            if current is not None:
                starts.append(current)
            hour, minute, second, speaker = match.groups()
            start = int(hour) * 3600 + int(minute) * 60 + int(second)
            current = (float(start), speaker.strip(), [])
        elif current is not None and line:
            current[2].append(line)
    if current is not None:
        starts.append(current)

    blocks: list[ReferenceBlock] = []
    for index, (start, speaker, lines) in enumerate(starts):
        end = starts[index + 1][0] if index + 1 < len(starts) else audio_sec
        issue = ""
        if end == start:
            issue = "duplicate_source_timestamp"
        elif end < start:
            issue = "backward_source_timestamp"
        blocks.append(ReferenceBlock(
            reference_id=f"ref-{index + 1:04d}",
            start=start,
            end=end,
            speaker=speaker,
            text="".join(lines),
            interval_issue=issue,
        ))
    if not blocks:
        raise ValueError("reference transcript is empty")
    return blocks


def interval_intersects(
    start: float, end: float, other_start: float, other_end: float,
) -> bool:
    if end > start:
        return min(end, other_end) - max(start, other_start) > EPSILON
    return other_start - EPSILON <= start < other_end + EPSILON


def block_intersects(block: ReferenceBlock, context: Context) -> bool:
    return (
        interval_intersects(block.start, block.end, context.start, context.end)
        or context.start - EPSILON <= block.start < context.end + EPSILON
    )


def entry_intersects(entry: dict[str, Any], start: float, end: float) -> bool:
    entry_start = float(entry.get("start", 0.0))
    entry_end = float(entry.get("end", entry_start))
    return interval_intersects(entry_start, entry_end, start, end)


def timeline_and_tracks(package: Any) -> tuple[dict[str, Any], dict[str, list[Any]]]:
    if not isinstance(package, dict):
        raise ValueError("artifact package is not an object")
    timeline = package.get("timeline", package)
    if not isinstance(timeline, dict):
        raise ValueError("artifact does not contain a timeline object")
    tracks: dict[str, list[Any]] = {}
    for track in timeline.get("tracks", []):
        if not isinstance(track, dict) or not isinstance(track.get("kind"), str):
            continue
        kind = track["kind"]
        if kind in tracks:
            raise ValueError(f"duplicate timeline track: {kind}")
        entries = track.get("entries", [])
        if not isinstance(entries, list):
            raise ValueError(f"timeline track entries are not a list: {kind}")
        tracks[kind] = entries
    missing = [kind for kind in TRACK_KINDS if kind not in tracks]
    if missing:
        raise ValueError("timeline tracks missing: " + ",".join(missing))
    if not isinstance(timeline.get("comprehensive"), list):
        raise ValueError("timeline comprehensive view is not a list")
    return timeline, tracks


def verify_artifact_manifest(
    artifact_path: Path, manifest_path: Path, package: dict[str, Any],
) -> tuple[dict[str, Any], str]:
    manifest = load_json(manifest_path)
    if not isinstance(manifest, dict):
        raise ValueError("artifact manifest is not an object")
    actual_sha256 = sha256_file(artifact_path)
    expected_sha256 = manifest.get("artifact", {}).get("sha256")
    if actual_sha256 != expected_sha256:
        raise ValueError("artifact SHA-256 differs from its manifest")
    package_config = package.get("meta", {}).get("resolved_config_sha256")
    manifest_config = manifest.get("resolved_config_sha256")
    if package_config and package_config != manifest_config:
        raise ValueError("resolved configuration identity differs")
    return manifest, actual_sha256


def load_posterior(
    path: Path,
) -> tuple[str, list[PosteriorRow], float, list[str]]:
    with path.open(encoding="utf-8", newline="") as source:
        raw_lines = source.read().splitlines()
    if len(raw_lines) < 3:
        raise ValueError("Sortformer posterior requires at least two rows")
    header = next(csv.reader([raw_lines[0]]))
    required = {
        "frame", "time_sec", "session", "top1", "top1_prob", "top2",
        "top2_prob", "margin", "active_count",
    }
    missing = sorted(required - set(header))
    speaker_columns = sorted(
        name for name in header if re.fullmatch(r"spk\d+", name))
    if missing or not speaker_columns:
        detail = ",".join(missing) if missing else "speaker channels"
        raise ValueError("Sortformer posterior columns missing: " + detail)
    index = {name: position for position, name in enumerate(header)}
    rows: list[PosteriorRow] = []
    for raw_line in raw_lines[1:]:
        values = next(csv.reader([raw_line]))
        if len(values) != len(header):
            raise ValueError("Sortformer posterior row width differs")
        row = PosteriorRow(
            frame=int(values[index["frame"]]),
            time_sec=float(values[index["time_sec"]]),
            session=int(values[index["session"]]),
            top1=int(values[index["top1"]]),
            top1_prob=float(values[index["top1_prob"]]),
            line=raw_line,
        )
        if row.frame != len(rows):
            raise ValueError("Sortformer posterior frames are not contiguous")
        if rows and row.time_sec <= rows[-1].time_sec:
            raise ValueError("Sortformer posterior times are not monotonic")
        rows.append(row)
    frame_period = rows[1].time_sec - rows[0].time_sec
    if frame_period <= 0.0:
        raise ValueError("Sortformer posterior frame period is invalid")
    return raw_lines[0], rows, frame_period, speaker_columns


def compress_primary_runs(
    rows: list[PosteriorRow],
    frame_period: float,
    speakers_per_session: int,
    threshold: float,
) -> list[dict[str, Any]]:
    runs: list[dict[str, Any]] = []
    current: dict[str, Any] | None = None

    def flush() -> None:
        nonlocal current
        if current is not None:
            runs.append({
                "start": current["start"],
                "end": current["end"],
                "speaker": current["speaker"],
                "confidence": (
                    current["confidence_total"] / current["frame_count"]),
            })
        current = None

    for row in rows:
        if row.top1_prob + EPSILON < threshold:
            flush()
            continue
        local_speaker = row.session * speakers_per_session + row.top1
        frame_end = row.time_sec + frame_period
        if (current is None or current["speaker"] != local_speaker or
                abs(current["end"] - row.time_sec) >
                SERIALIZED_FRAME_CONTIGUITY_SEC):
            flush()
            current = {
                "start": row.time_sec,
                "end": frame_end,
                "speaker": local_speaker,
                "confidence_total": row.top1_prob,
                "frame_count": 1,
            }
        else:
            current["end"] = frame_end
            current["confidence_total"] += row.top1_prob
            current["frame_count"] += 1
    flush()
    return runs


def verify_primary_producer(
    timeline: dict[str, Any],
    tracks: dict[str, list[Any]],
    posterior: list[PosteriorRow],
    frame_period: float,
    speaker_columns: list[str],
) -> dict[str, Any]:
    threshold = float(
        timeline.get("resolved_config", {})
        .get("speaker_fusion", {})
        .get("frame_activity_threshold", 0.0))
    if not 0.0 < threshold <= 1.0:
        raise ValueError("resolved primary frame threshold is invalid")
    audio_sec = float(timeline["audio_sec"])
    if abs(posterior[0].time_sec) > 1e-6:
        raise ValueError("Sortformer posterior does not start at time zero")
    posterior_end = posterior[-1].time_sec + frame_period
    if abs(posterior_end - audio_sec) > 1e-3:
        raise ValueError("Sortformer posterior extent differs from timeline")

    compressed = compress_primary_runs(
        posterior, frame_period, len(speaker_columns), threshold)
    primary = tracks["primary_speaker"]
    if len(compressed) != len(primary):
        raise ValueError("posterior top-1 run count differs from primary track")
    for index, (source, produced) in enumerate(zip(compressed, primary)):
        if not isinstance(produced, dict):
            raise ValueError(f"primary track entry is invalid at {index}")
        if int(produced.get("speaker", -1)) != source["speaker"]:
            raise ValueError(f"posterior local slot differs at primary run {index}")
        for field in ("start", "end", "confidence"):
            if abs(float(produced.get(field, 0.0)) - source[field]) > 1e-6:
                raise ValueError(
                    f"posterior {field} differs at primary run {index}")
    return {
        "frame_activity_threshold": threshold,
        "primary_run_count": len(primary),
        "local_slot_and_order_match": True,
        "bounds_and_mean_tolerance": 1e-6,
    }


def derive_identity_epochs(
    diarization: list[Any], audio_sec: float,
) -> list[dict[str, Any]]:
    observations: dict[int, list[dict[str, Any]]] = defaultdict(list)
    for entry in diarization:
        if not isinstance(entry, dict):
            continue
        local_speaker = int(entry.get("speaker", -1))
        speaker_id = entry.get("speaker_id")
        if local_speaker < 0 or not isinstance(speaker_id, str) or not speaker_id:
            continue
        observations[local_speaker].append(entry)

    epochs: list[dict[str, Any]] = []
    for local_speaker in sorted(observations):
        entries = sorted(observations[local_speaker], key=lambda entry: (
            float(entry.get("start", 0.0)), float(entry.get("end", 0.0))))
        epoch_start = 0.0
        speaker_id = str(entries[0]["speaker_id"])
        for entry in entries[1:]:
            next_id = str(entry["speaker_id"])
            if next_id == speaker_id:
                continue
            transition = float(entry.get("start", 0.0))
            if transition <= epoch_start:
                raise ValueError("identity observations are not chronological")
            epochs.append({
                "local_speaker": local_speaker,
                "start_sec": epoch_start,
                "end_sec": transition,
                "speaker_id": speaker_id,
            })
            epoch_start = transition
            speaker_id = next_id
        epochs.append({
            "local_speaker": local_speaker,
            "start_sec": epoch_start,
            "end_sec": audio_sec,
            "speaker_id": speaker_id,
        })
    if not epochs:
        raise ValueError("no local identity observations are available")
    return epochs


def write_epochs(path: Path, epochs: list[dict[str, Any]]) -> None:
    with path.open("w", encoding="utf-8", newline="") as destination:
        writer = csv.writer(destination, delimiter="\t", lineterminator="\n")
        writer.writerow(["local_speaker", "start_sec", "end_sec", "speaker_id"])
        for epoch in epochs:
            writer.writerow([
                epoch["local_speaker"],
                f'{epoch["start_sec"]:.9f}',
                f'{epoch["end_sec"]:.9f}',
                epoch["speaker_id"],
            ])


def indexed_entries(
    entries: list[Any], start: float, end: float,
) -> list[dict[str, Any]]:
    return [
        {"index": index, "entry": entry}
        for index, entry in enumerate(entries)
        if isinstance(entry, dict) and entry_intersects(entry, start, end)
    ]


def source_text_ids(
    timeline: dict[str, Any], tracks: dict[str, list[Any]], context: Context,
) -> set[int]:
    entries = (
        tracks["asr"] + tracks["align"] + tracks["business_speaker"] +
        timeline["comprehensive"]
    )
    return {
        int(entry.get("text_id", -1))
        for entry in entries
        if isinstance(entry, dict)
        and entry_intersects(entry, context.start, context.end)
        and int(entry.get("text_id", -1)) >= 0
    }


def voiceprint_entries(
    entries: list[Any], text_ids: set[int], context: Context,
) -> list[dict[str, Any]]:
    selected = []
    for index, entry in enumerate(entries):
        if not isinstance(entry, dict):
            continue
        text_id = int(entry.get("text_id", -1))
        if (text_id >= 0 and text_id in text_ids) or entry_intersects(
                entry, context.start, context.end):
            selected.append({"index": index, "entry": entry})
    return selected


def context_tracks(
    timeline: dict[str, Any], tracks: dict[str, list[Any]], context: Context,
) -> list[dict[str, Any]]:
    text_ids = source_text_ids(timeline, tracks, context)
    output = []
    for kind in TRACK_KINDS:
        if kind == "speaker_voiceprint":
            entries = voiceprint_entries(tracks[kind], text_ids, context)
        else:
            entries = indexed_entries(
                tracks[kind], context.start, context.end)
        output.append({"kind": kind, "entries": entries})
    return output


def context_business(
    timeline: dict[str, Any], tracks: dict[str, list[Any]], context: Context,
) -> dict[str, Any]:
    return {
        "comprehensive": indexed_entries(
            timeline["comprehensive"], context.start, context.end),
        "business_speaker": indexed_entries(
            tracks["business_speaker"], context.start, context.end),
    }


def context_epochs(
    epochs: list[dict[str, Any]], context: Context,
) -> list[dict[str, Any]]:
    return [
        epoch for epoch in epochs
        if interval_intersects(
            float(epoch["start_sec"]), float(epoch["end_sec"]),
            context.start, context.end)
    ]


def context_posterior(
    rows: list[PosteriorRow], frame_period: float, context: Context,
) -> list[PosteriorRow]:
    return [
        row for row in rows
        if interval_intersects(
            row.time_sec, row.time_sec + frame_period,
            context.start, context.end)
    ]


def context_reference(
    blocks: list[ReferenceBlock], context: Context,
) -> list[ReferenceBlock]:
    selected = [block for block in blocks if block_intersects(block, context)]
    selected_ids = {block.reference_id for block in selected}
    required_ids = set(context.focus_refs) | set(context.control_refs)
    missing = sorted(required_ids - selected_ids)
    if missing:
        raise ValueError(
            f"context {context.context_id} excludes named references: " +
            ",".join(missing))
    return selected


def render_reference(
    blocks: list[ReferenceBlock], context: Context,
) -> str:
    lines = [
        "# Reference Context",
        "",
        f"- Context: `{context.context_id}`",
        f"- Common-clock bounds: `{context.start:.3f}-{context.end:.3f}`",
        "- Focus references: " + ", ".join(
            f"`{item}`" for item in context.focus_refs),
        "- Control references: " + ", ".join(
            f"`{item}`" for item in context.control_refs),
        "- Purpose: complete source display for human contextual review only.",
        "",
    ]
    focus = set(context.focus_refs)
    controls = set(context.control_refs)
    for block in blocks:
        role = "focus" if block.reference_id in focus else (
            "control" if block.reference_id in controls else "context")
        issue = f" ({block.interval_issue})" if block.interval_issue else ""
        lines.extend([
            f"## {block.reference_id}",
            "",
            f"- Role: `{role}`",
            f"- Source interval: `{block.start:.3f}-{block.end:.3f}`{issue}",
            f"- Reference speaker: `{block.speaker}`",
            f"- Reference text: {block.text}",
            "",
        ])
    return "\n".join(lines)


def write_context_tsv(path: Path, context: Context) -> None:
    with path.open("w", encoding="utf-8", newline="") as destination:
        writer = csv.writer(destination, delimiter="\t", lineterminator="\n")
        writer.writerow(CONTEXT_COLUMNS)
        writer.writerow([
            context.context_id,
            f"{context.start:.3f}",
            f"{context.end:.3f}",
            ",".join(context.focus_refs),
            ",".join(context.control_refs),
        ])


def write_posterior(
    path: Path, header: str, rows: list[PosteriorRow],
) -> None:
    lines = [header] + [row.line for row in rows]
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_raw_tsv(path: Path, evidence: RawTsvEvidence,
                  rows: list[dict[str, str]]) -> None:
    with path.open("w", encoding="utf-8", newline="") as destination:
        writer = csv.DictWriter(
            destination, fieldnames=evidence.columns, delimiter="\t",
            lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def context_vad_windows(
    evidence: RawTsvEvidence, context: Context,
) -> list[dict[str, str]]:
    return [
        row for row in evidence.rows
        if interval_intersects(
            float(row["start_sec"]), float(row["end_sec"]),
            context.start, context.end)
    ]


def context_vad_states(
    evidence: RawTsvEvidence, context: Context,
) -> list[dict[str, str]]:
    observed = [float(row["observed_until_sec"]) for row in evidence.rows]
    selected = {
        index for index, value in enumerate(observed)
        if context.start - EPSILON <= value <= context.end + EPSILON
    }
    preceding = [
        index for index, value in enumerate(observed)
        if value < context.start - EPSILON
    ]
    following = [
        index for index, value in enumerate(observed)
        if value > context.end + EPSILON
    ]
    if preceding:
        selected.add(preceding[-1])
    if following:
        selected.add(following[0])
    return [evidence.rows[index] for index in sorted(selected)]


def write_content_manifest(root: Path) -> Path:
    manifest_path = root / "content.sha256"
    rows = []
    for path in sorted(item for item in root.rglob("*") if item.is_file()):
        if path == manifest_path:
            continue
        relative = path.relative_to(root).as_posix()
        rows.append(f"{sha256_file(path)}  {relative}")
    manifest_path.write_text("\n".join(rows) + "\n", encoding="utf-8")
    return manifest_path


def export_packet(
    artifact_path: Path,
    artifact_manifest_path: Path,
    reference_path: Path,
    contexts_path: Path,
    posterior_path: Path,
    out_dir: Path,
    vad_window_path: Path | None = None,
    vad_state_path: Path | None = None,
) -> dict[str, Any]:
    if (vad_window_path is None) != (vad_state_path is None):
        raise ValueError("raw VAD window and state evidence must be paired")
    contexts = load_contexts(contexts_path)
    package = load_json(artifact_path)
    timeline, tracks = timeline_and_tracks(package)
    artifact_manifest, artifact_sha256 = verify_artifact_manifest(
        artifact_path, artifact_manifest_path, package)
    audio_sec = float(timeline.get("audio_sec", 0.0))
    if audio_sec <= 0.0:
        raise ValueError("timeline audio_sec is invalid")
    blocks = load_reference(reference_path, audio_sec)
    header, posterior, frame_period, speaker_columns = load_posterior(
        posterior_path)
    producer_check = verify_primary_producer(
        timeline, tracks, posterior, frame_period, speaker_columns)
    vad_windows = None
    vad_states = None
    if vad_window_path is not None and vad_state_path is not None:
        vad_windows = load_raw_tsv(vad_window_path, VAD_WINDOW_COLUMNS)
        vad_states = load_raw_tsv(vad_state_path, VAD_STATE_COLUMNS)
        sample_rate = int(timeline.get("sample_rate", 0))
        if sample_rate <= 0:
            raise ValueError("timeline sample rate is invalid")
        verify_vad_window_evidence(vad_windows, sample_rate)
        verify_vad_state_evidence(vad_states, sample_rate)
    epochs = derive_identity_epochs(tracks["diarization"], audio_sec)
    reference_by_context = {
        context.context_id: context_reference(blocks, context)
        for context in contexts
    }

    out_dir.mkdir(parents=True, exist_ok=False)
    write_epochs(out_dir / "local-identity-epochs.tsv", epochs)
    for context in contexts:
        context_dir = out_dir / context.context_id
        context_dir.mkdir()
        write_context_tsv(context_dir / "context.tsv", context)
        (context_dir / "reference-sections.md").write_text(
            render_reference(reference_by_context[context.context_id], context),
            encoding="utf-8")
        write_json(
            context_dir / "typed-tracks.json",
            context_tracks(timeline, tracks, context),
        )
        write_json(
            context_dir / "business-view.json",
            context_business(timeline, tracks, context),
        )
        write_epochs(
            context_dir / "local-identity-epochs.tsv",
            context_epochs(epochs, context),
        )
        write_posterior(
            context_dir / "sortformer-frames.csv",
            header,
            context_posterior(posterior, frame_period, context),
        )
        if vad_windows is not None and vad_states is not None:
            write_raw_tsv(
                context_dir / "vad-window-probabilities.tsv", vad_windows,
                context_vad_windows(vad_windows, context))
            write_raw_tsv(
                context_dir / "vad-endpoint-states.tsv", vad_states,
                context_vad_states(vad_states, context))

    packet_manifest = {
        "schema_version": 1,
        "kind": "orator_speaker_residual_evidence_packet",
        "scope": "display_only_cross_pipeline_evidence",
        "governance": (
            "Source arrangement and mechanical validation only; contextual "
            "human review is required for every product judgment."),
        "sources": {
            "artifact": {
                "path": str(artifact_path.resolve()),
                "sha256": artifact_sha256,
            },
            "artifact_manifest": {
                "path": str(artifact_manifest_path.resolve()),
                "sha256": sha256_file(artifact_manifest_path),
                "artifact_id": artifact_manifest.get("artifact_id", ""),
                "git": artifact_manifest.get("git", {}),
                "resolved_config_sha256": artifact_manifest.get(
                    "resolved_config_sha256", ""),
                "source_audio": artifact_manifest.get("source_audio", {}),
            },
            "reference": {
                "path": str(reference_path.resolve()),
                "sha256": sha256_file(reference_path),
            },
            "contexts": {
                "path": str(contexts_path.resolve()),
                "sha256": sha256_file(contexts_path),
            },
            "sortformer_posterior": {
                "path": str(posterior_path.resolve()),
                "sha256": sha256_file(posterior_path),
            },
        },
        "audio_sec": audio_sec,
        "sample_rate": int(timeline.get("sample_rate", 0)),
        "frame_period_sec": frame_period,
        "posterior_first_frame_sec": posterior[0].time_sec,
        "posterior_last_frame_sec": posterior[-1].time_sec,
        "posterior_speaker_columns": speaker_columns,
        "posterior_primary_producer_check": producer_check,
        "identity_epoch_derivation": (
            "Observed diarization local-speaker/speaker-id transitions; raw "
            "diarization entries remain in each typed-tracks file."),
        "track_kinds": TRACK_KINDS,
        "contexts": [
            {
                "context_id": item.context_id,
                "start_sec": item.start,
                "end_sec": item.end,
                "focus_refs": list(item.focus_refs),
                "control_refs": list(item.control_refs),
            }
            for item in contexts
        ],
    }
    if vad_window_path is not None and vad_state_path is not None:
        packet_manifest["sources"]["vad_window_probabilities"] = {
            "path": str(vad_window_path.resolve()),
            "sha256": sha256_file(vad_window_path),
        }
        packet_manifest["sources"]["vad_endpoint_states"] = {
            "path": str(vad_state_path.resolve()),
            "sha256": sha256_file(vad_state_path),
        }
    write_json(out_dir / "packet-manifest.json", packet_manifest)
    content_manifest = write_content_manifest(out_dir)
    packet_manifest["content_manifest_sha256"] = sha256_file(content_manifest)
    return packet_manifest


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifact", required=True)
    parser.add_argument("--artifact-manifest", required=True)
    parser.add_argument("--reference", required=True)
    parser.add_argument("--contexts", required=True)
    parser.add_argument("--sortformer-frames", required=True)
    parser.add_argument("--out-dir", required=True)
    parser.add_argument("--vad-window-probabilities")
    parser.add_argument("--vad-endpoint-states")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    result = export_packet(
        artifact_path=Path(args.artifact),
        artifact_manifest_path=Path(args.artifact_manifest),
        reference_path=Path(args.reference),
        contexts_path=Path(args.contexts),
        posterior_path=Path(args.sortformer_frames),
        out_dir=Path(args.out_dir),
        vad_window_path=(Path(args.vad_window_probabilities)
                         if args.vad_window_probabilities else None),
        vad_state_path=(Path(args.vad_endpoint_states)
                        if args.vad_endpoint_states else None),
    )
    print(json.dumps({
        "kind": result["kind"],
        "scope": result["scope"],
        "out_dir": str(Path(args.out_dir).resolve()),
        "content_manifest_sha256": result["content_manifest_sha256"],
    }, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main(sys.argv[1:]))
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError,
            csv.Error) as error:
        raise SystemExit(f"speaker residual evidence packet: {error}")
