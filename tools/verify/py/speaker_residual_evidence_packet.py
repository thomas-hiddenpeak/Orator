#!/usr/bin/env python3
"""Arrange frozen cross-pipeline evidence for contextual human review.

This utility copies reference sections, terminal timeline entries, raw
Sortformer rows, optional auxiliary streaming-context rows, every raw pairwise
common-clock intersection with accepted speaker tracks, and observed local-to-
global identity epochs into independent context directories. It performs
source and shape checks only. It never assigns correctness, derives an
identity mapping, groups causes, scores or ranks repairs, selects behavior, or
issues a product verdict.
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
IDENTITY_REFERENCE_COLUMNS = [
    "evidence_id", "local_speaker", "epoch_start_sec", "speaker_id",
    "source_start_sec", "source_end_sec", "embedding_start_sample",
    "embedding_end_sample", "embedding_start_sec", "embedding_end_sec",
    "quality",
]
GALLERY_INDEPENDENCE_COLUMNS = [
    "evidence_id", "kind", "text_id", "source_start", "source_end",
    "start", "end", "embedding_available", "session_gallery_complete",
    "robust_gallery_complete", "session_scores", "robust_scores",
    "query_embedding_start_sample", "query_embedding_end_sample",
    "nonoverlap_embedding_available",
    "nonoverlap_session_gallery_complete",
    "nonoverlap_robust_gallery_complete", "nonoverlap_session_scores",
    "nonoverlap_robust_scores", "intersecting_reference_ids",
]
AUX_MAIN_INTERSECTION_COLUMNS = [
    "aux_segment_index", "aux_start_sec", "aux_end_sec", "aux_session",
    "aux_local_speaker", "aux_confidence", "aux_mean_margin",
    "main_track", "main_entry_index", "main_start_sec", "main_end_sec",
    "main_local_speaker", "main_speaker_id", "main_confidence",
    "intersection_start_sec", "intersection_end_sec",
    "intersection_duration_sec",
]
ACCEPTED_SPEAKER_TRACKS = ("diarization", "primary_speaker")
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
class SegmentRow:
    source_index: int
    start_sec: float
    end_sec: float
    session: int
    local_speaker: int
    confidence: float
    mean_margin: float
    line: str


@dataclass(frozen=True)
class MainSpeakerSpan:
    source_index: int
    start_sec: float
    end_sec: float
    local_speaker: int
    speaker_id: str
    confidence: float


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


def parse_evidence_bool(value: str, field: str) -> bool:
    if value not in ("0", "1"):
        raise ValueError(f"{field} is not a zero/one boolean")
    return value == "1"


def parse_evidence_scores(value: str, field: str) -> None:
    seen = set()
    if not value:
        return
    for item in value.split(","):
        speaker_id, separator, raw_score = item.rpartition(":")
        score = float(raw_score)
        if (not separator or not speaker_id or speaker_id in seen or
                not math.isfinite(score)):
            raise ValueError(f"{field} contains an invalid score")
        seen.add(speaker_id)


def verify_identity_references(
    evidence: RawTsvEvidence, sample_rate: int,
) -> dict[str, tuple[int, int]]:
    references = {}
    prior_key = None
    for row in evidence.rows:
        match = re.fullmatch(r"identity_ref:(\d+):(\d+):(\d+)",
                             row["evidence_id"])
        if match is None or row["evidence_id"] in references:
            raise ValueError("identity reference ID is invalid or duplicated")
        key = tuple(int(value) for value in match.groups())
        if prior_key is not None and key <= prior_key:
            raise ValueError("identity references are not ordered")
        local_speaker = int(row["local_speaker"])
        if local_speaker != key[0] or not row["speaker_id"]:
            raise ValueError("identity reference owner is invalid")
        epoch_start = float(row["epoch_start_sec"])
        source_start = float(row["source_start_sec"])
        source_end = float(row["source_end_sec"])
        embed_start = int(row["embedding_start_sample"])
        embed_end = int(row["embedding_end_sample"])
        embed_start_sec = float(row["embedding_start_sec"])
        embed_end_sec = float(row["embedding_end_sec"])
        quality = float(row["quality"])
        values = (
            epoch_start, source_start, source_end, embed_start_sec,
            embed_end_sec, quality,
        )
        if not all(math.isfinite(value) for value in values):
            raise ValueError("identity reference contains a non-finite value")
        if (epoch_start < 0.0 or source_start < epoch_start - 1e-6 or
                source_end <= source_start or embed_start < 0 or
                embed_end <= embed_start or quality < 0.0):
            raise ValueError("identity reference bounds are invalid")
        if (abs(embed_start_sec - embed_start / sample_rate) > 1e-9 or
                abs(embed_end_sec - embed_end / sample_rate) > 1e-9):
            raise ValueError("identity reference sample/time bounds differ")
        if (embed_start_sec < source_start - 1e-6 or
                embed_end_sec > source_end + 1e-6):
            raise ValueError("identity embedding lies outside its source")
        references[row["evidence_id"]] = (embed_start, embed_end)
        prior_key = key
    return references


def verify_gallery_independence_evidence(
    evidence: RawTsvEvidence,
    references: dict[str, tuple[int, int]],
) -> None:
    seen = set()
    for row in evidence.rows:
        evidence_id = row["evidence_id"]
        if not evidence_id or evidence_id in seen or not row["kind"]:
            raise ValueError("gallery query ID is invalid or duplicated")
        seen.add(evidence_id)
        start = float(row["start"])
        end = float(row["end"])
        if (not math.isfinite(start) or not math.isfinite(end) or
                start < 0.0 or end <= start):
            raise ValueError("gallery query bounds are invalid")
        embedding_available = parse_evidence_bool(
            row["embedding_available"], "embedding_available")
        parse_evidence_bool(
            row["session_gallery_complete"],
            "session_gallery_complete")
        parse_evidence_bool(
            row["robust_gallery_complete"], "robust_gallery_complete")
        nonoverlap_available = parse_evidence_bool(
            row["nonoverlap_embedding_available"],
            "nonoverlap_embedding_available")
        parse_evidence_bool(
            row["nonoverlap_session_gallery_complete"],
            "nonoverlap_session_gallery_complete")
        parse_evidence_bool(
            row["nonoverlap_robust_gallery_complete"],
            "nonoverlap_robust_gallery_complete")
        if embedding_available != nonoverlap_available:
            raise ValueError("inclusive/nonoverlap embedding availability differs")
        for field in (
                "session_scores", "robust_scores",
                "nonoverlap_session_scores", "nonoverlap_robust_scores"):
            parse_evidence_scores(row[field], field)
        query_start = int(row["query_embedding_start_sample"])
        query_end = int(row["query_embedding_end_sample"])
        if embedding_available:
            if query_start < 0 or query_end <= query_start:
                raise ValueError("gallery query sample bounds are invalid")
        elif query_start != 0 or query_end != 0:
            raise ValueError("unavailable gallery query has sample bounds")
        listed = tuple(
            item for item in row["intersecting_reference_ids"].split(",")
            if item)
        if len(set(listed)) != len(listed):
            raise ValueError("gallery query repeats an intersecting reference")
        expected = []
        if embedding_available:
            for reference_id, (reference_start, reference_end) in references.items():
                if min(query_end, reference_end) > max(
                        query_start, reference_start):
                    expected.append(reference_id)
        if listed != tuple(expected):
            raise ValueError("gallery query/reference intersections differ")


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


def verify_posterior_extent(
    rows: list[PosteriorRow], frame_period: float, audio_sec: float,
    label: str,
) -> None:
    if abs(rows[0].time_sec) > 1e-6:
        raise ValueError(f"{label} posterior does not start at time zero")
    posterior_end = rows[-1].time_sec + frame_period
    if abs(posterior_end - audio_sec) > 1e-3:
        raise ValueError(f"{label} posterior extent differs from timeline")


def load_segments(path: Path, audio_sec: float) -> tuple[str, list[SegmentRow]]:
    with path.open(encoding="utf-8", newline="") as source:
        raw_lines = source.read().splitlines()
    if len(raw_lines) < 2:
        raise ValueError("auxiliary Sortformer segments are empty")
    header = next(csv.reader([raw_lines[0]]))
    required = {
        "start_sec", "end_sec", "duration_sec", "session",
        "local_speaker", "confidence", "mean_margin",
    }
    missing = sorted(required - set(header))
    if missing:
        raise ValueError(
            "auxiliary Sortformer segment columns missing: " +
            ",".join(missing))
    index = {name: position for position, name in enumerate(header)}
    rows: list[SegmentRow] = []
    prior_start = -1.0
    for raw_line in raw_lines[1:]:
        values = next(csv.reader([raw_line]))
        if len(values) != len(header):
            raise ValueError("auxiliary Sortformer segment row width differs")
        start = float(values[index["start_sec"]])
        end = float(values[index["end_sec"]])
        duration = float(values[index["duration_sec"]])
        session = int(values[index["session"]])
        local_speaker = int(values[index["local_speaker"]])
        confidence = float(values[index["confidence"]])
        mean_margin = float(values[index["mean_margin"]])
        if not all(math.isfinite(value) for value in (
                start, end, duration, confidence, mean_margin)):
            raise ValueError("auxiliary Sortformer segment is not finite")
        if (start < 0.0 or end <= start or end > audio_sec + 1e-3 or
                abs((end - start) - duration) > 1e-5 or session < 0 or
                local_speaker < 0 or start + EPSILON < prior_start):
            raise ValueError("auxiliary Sortformer segment is invalid")
        rows.append(SegmentRow(
            source_index=len(rows),
            start_sec=start,
            end_sec=end,
            session=session,
            local_speaker=local_speaker,
            confidence=confidence,
            mean_margin=mean_margin,
            line=raw_line,
        ))
        prior_start = start
    return raw_lines[0], rows


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
    verify_posterior_extent(
        posterior, frame_period, audio_sec, "Sortformer")

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


def context_segments(
    rows: list[SegmentRow], context: Context,
) -> list[SegmentRow]:
    return [
        row for row in rows
        if interval_intersects(
            row.start_sec, row.end_sec, context.start, context.end)
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


def retained_reference_blocks(
    blocks: list[ReferenceBlock], row: dict[str, str],
) -> list[ReferenceBlock]:
    start = float(row["source_start_sec"])
    end = float(row["source_end_sec"])
    selected = [
        index for index, block in enumerate(blocks)
        if (interval_intersects(start, end, block.start, block.end) or
            start - EPSILON <= block.start <= end + EPSILON)
    ]
    if not selected:
        following = [
            index for index, block in enumerate(blocks)
            if block.start > end + EPSILON
        ]
        anchor = following[0] if following else len(blocks) - 1
        selected = [anchor]
    first = max(0, selected[0] - 1)
    last = min(len(blocks), selected[-1] + 2)
    return blocks[first:last]


def render_retained_reference(
    row: dict[str, str], blocks: list[ReferenceBlock],
) -> str:
    lines = [
        "# Retained Identity Reference Context",
        "",
        f"- Evidence: `{row['evidence_id']}`",
        f"- Local speaker: `{row['local_speaker']}`",
        f"- Identity epoch start: `{row['epoch_start_sec']}`",
        f"- Assigned global identity: `{row['speaker_id']}`",
        "- Source bounds: "
        f"`{row['source_start_sec']}-{row['source_end_sec']}`",
        "- Embedded bounds: "
        f"`{row['embedding_start_sec']}-{row['embedding_end_sec']}`",
        f"- Existing selection quality: `{row['quality']}`",
        "- Purpose: raw provenance display for contextual review only.",
        "",
    ]
    for block in blocks:
        issue = f" ({block.interval_issue})" if block.interval_issue else ""
        lines.extend([
            f"## {block.reference_id}",
            "",
            f"- Source interval: `{block.start:.3f}-{block.end:.3f}`{issue}",
            f"- Reference speaker: `{block.speaker}`",
            f"- Reference text: {block.text}",
            "",
        ])
    return "\n".join(lines)


def render_retained_reference_sequence(
    rows: list[dict[str, str]], blocks: list[ReferenceBlock], reverse: bool,
) -> str:
    ordered = sorted(rows, key=lambda row: (
        float(row["source_start_sec"]), float(row["source_end_sec"]),
        row["evidence_id"]), reverse=reverse)
    direction = "Reverse" if reverse else "Chronological"
    sections = [
        f"# Retained Identity References: {direction}",
        "",
        "This file orders raw retained-reference provenance for contextual "
        "review only. It contains no automated speaker judgment.",
    ]
    for row in ordered:
        sections.extend([
            "",
            "---",
            "",
            render_retained_reference(
                row, retained_reference_blocks(blocks, row)),
        ])
    return "\n".join(sections)


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


def write_segments(
    path: Path, header: str, rows: list[SegmentRow],
) -> None:
    lines = [header] + [row.line for row in rows]
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_dict_tsv(
    path: Path, columns: list[str] | tuple[str, ...],
    rows: list[dict[str, str]],
) -> None:
    with path.open("w", encoding="utf-8", newline="") as destination:
        writer = csv.DictWriter(
            destination, fieldnames=columns, delimiter="\t",
            lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def write_raw_tsv(path: Path, evidence: RawTsvEvidence,
                  rows: list[dict[str, str]]) -> None:
    write_dict_tsv(path, evidence.columns, rows)


def load_main_speaker_spans(
    tracks: dict[str, list[Any]],
) -> dict[str, list[MainSpeakerSpan]]:
    parsed: dict[str, list[MainSpeakerSpan]] = {}
    for track_name in ACCEPTED_SPEAKER_TRACKS:
        spans: list[MainSpeakerSpan] = []
        for entry_index, entry in enumerate(tracks[track_name]):
            if not isinstance(entry, dict):
                raise ValueError(
                    f"accepted {track_name} entry is not an object")
            try:
                main_start = float(entry.get("start", 0.0))
                main_end = float(entry.get("end", main_start))
                main_confidence = float(entry.get("confidence", 0.0))
            except (TypeError, ValueError) as error:
                raise ValueError(
                    f"accepted {track_name} entry is invalid") from error
            raw_local_speaker = entry.get("speaker", -1)
            main_speaker_id = entry.get("speaker_id", "")
            if (not isinstance(raw_local_speaker, int) or
                    isinstance(raw_local_speaker, bool)):
                raise ValueError(f"accepted {track_name} entry is invalid")
            if (not all(math.isfinite(value) for value in (
                    main_start, main_end, main_confidence)) or
                    main_start < 0.0 or main_end <= main_start or
                    raw_local_speaker < 0 or
                    not isinstance(main_speaker_id, str)):
                raise ValueError(f"accepted {track_name} entry is invalid")
            spans.append(MainSpeakerSpan(
                source_index=entry_index,
                start_sec=main_start,
                end_sec=main_end,
                local_speaker=raw_local_speaker,
                speaker_id=main_speaker_id,
                confidence=main_confidence,
            ))
        parsed[track_name] = spans
    return parsed


def main_speaker_row(
    segment: SegmentRow, track_name: str, span: MainSpeakerSpan | None,
) -> dict[str, str]:
    row = {
        "aux_segment_index": str(segment.source_index),
        "aux_start_sec": f"{segment.start_sec:.9f}",
        "aux_end_sec": f"{segment.end_sec:.9f}",
        "aux_session": str(segment.session),
        "aux_local_speaker": str(segment.local_speaker),
        "aux_confidence": f"{segment.confidence:.9f}",
        "aux_mean_margin": f"{segment.mean_margin:.9f}",
        "main_track": track_name,
        "main_entry_index": "",
        "main_start_sec": "",
        "main_end_sec": "",
        "main_local_speaker": "",
        "main_speaker_id": "",
        "main_confidence": "",
        "intersection_start_sec": "",
        "intersection_end_sec": "",
        "intersection_duration_sec": "",
    }
    if span is None:
        return row

    intersection_start = max(segment.start_sec, span.start_sec)
    intersection_end = min(segment.end_sec, span.end_sec)
    if intersection_end - intersection_start <= EPSILON:
        raise ValueError("non-intersecting accepted row was selected")
    row.update({
        "main_entry_index": str(span.source_index),
        "main_start_sec": f"{span.start_sec:.9f}",
        "main_end_sec": f"{span.end_sec:.9f}",
        "main_local_speaker": str(span.local_speaker),
        "main_speaker_id": span.speaker_id,
        "main_confidence": f"{span.confidence:.9f}",
        "intersection_start_sec": f"{intersection_start:.9f}",
        "intersection_end_sec": f"{intersection_end:.9f}",
        "intersection_duration_sec": (
            f"{intersection_end - intersection_start:.9f}"),
    })
    return row


def auxiliary_main_intersections(
    segments: list[SegmentRow],
    main_spans: dict[str, list[MainSpeakerSpan]],
) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    for segment in segments:
        for track_name in ACCEPTED_SPEAKER_TRACKS:
            matched = False
            for span in main_spans[track_name]:
                if not interval_intersects(
                        segment.start_sec, segment.end_sec,
                        span.start_sec, span.end_sec):
                    continue
                rows.append(main_speaker_row(segment, track_name, span))
                matched = True
            if not matched:
                rows.append(main_speaker_row(segment, track_name, None))
    return rows


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


def context_gallery_evidence(
    evidence: RawTsvEvidence, context: Context,
) -> list[dict[str, str]]:
    return [
        row for row in evidence.rows
        if interval_intersects(
            float(row["start"]), float(row["end"]),
            context.start, context.end)
    ]


def context_identity_references(
    evidence: RawTsvEvidence, context: Context,
) -> list[dict[str, str]]:
    return [
        row for row in evidence.rows
        if interval_intersects(
            float(row["source_start_sec"]),
            float(row["source_end_sec"]), context.start, context.end)
    ]


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
    identity_reference_path: Path | None = None,
    gallery_independence_path: Path | None = None,
    auxiliary_posterior_path: Path | None = None,
    auxiliary_segment_path: Path | None = None,
) -> dict[str, Any]:
    if (vad_window_path is None) != (vad_state_path is None):
        raise ValueError("raw VAD window and state evidence must be paired")
    if ((identity_reference_path is None) !=
            (gallery_independence_path is None)):
        raise ValueError(
            "identity references and gallery evidence must be paired")
    if ((auxiliary_posterior_path is None) !=
            (auxiliary_segment_path is None)):
        raise ValueError(
            "auxiliary posterior and segment evidence must be paired")
    contexts = load_contexts(contexts_path)
    package = load_json(artifact_path)
    timeline, tracks = timeline_and_tracks(package)
    artifact_manifest, artifact_sha256 = verify_artifact_manifest(
        artifact_path, artifact_manifest_path, package)
    audio_sec = float(timeline.get("audio_sec", 0.0))
    if audio_sec <= 0.0:
        raise ValueError("timeline audio_sec is invalid")
    sample_rate = int(timeline.get("sample_rate", 0))
    if sample_rate <= 0:
        raise ValueError("timeline sample rate is invalid")
    blocks = load_reference(reference_path, audio_sec)
    header, posterior, frame_period, speaker_columns = load_posterior(
        posterior_path)
    producer_check = verify_primary_producer(
        timeline, tracks, posterior, frame_period, speaker_columns)
    auxiliary_header = None
    auxiliary_posterior = None
    auxiliary_frame_period = None
    auxiliary_speaker_columns = None
    auxiliary_segment_header = None
    auxiliary_segments = None
    main_speaker_spans = None
    if (auxiliary_posterior_path is not None and
            auxiliary_segment_path is not None):
        (auxiliary_header, auxiliary_posterior, auxiliary_frame_period,
         auxiliary_speaker_columns) = load_posterior(
             auxiliary_posterior_path)
        verify_posterior_extent(
            auxiliary_posterior, auxiliary_frame_period, audio_sec,
            "auxiliary Sortformer")
        auxiliary_segment_header, auxiliary_segments = load_segments(
            auxiliary_segment_path, audio_sec)
        main_speaker_spans = load_main_speaker_spans(tracks)
    vad_windows = None
    vad_states = None
    if vad_window_path is not None and vad_state_path is not None:
        vad_windows = load_raw_tsv(vad_window_path, VAD_WINDOW_COLUMNS)
        vad_states = load_raw_tsv(vad_state_path, VAD_STATE_COLUMNS)
        verify_vad_window_evidence(vad_windows, sample_rate)
        verify_vad_state_evidence(vad_states, sample_rate)
    identity_references = None
    gallery_independence = None
    if (identity_reference_path is not None and
            gallery_independence_path is not None):
        identity_references = load_raw_tsv(
            identity_reference_path, IDENTITY_REFERENCE_COLUMNS)
        gallery_independence = load_raw_tsv(
            gallery_independence_path, GALLERY_INDEPENDENCE_COLUMNS)
        reference_bounds = verify_identity_references(
            identity_references, sample_rate)
        verify_gallery_independence_evidence(
            gallery_independence, reference_bounds)
    epochs = derive_identity_epochs(tracks["diarization"], audio_sec)
    reference_by_context = {
        context.context_id: context_reference(blocks, context)
        for context in contexts
    }

    out_dir.mkdir(parents=True, exist_ok=False)
    write_epochs(out_dir / "local-identity-epochs.tsv", epochs)
    if auxiliary_segments is not None and main_speaker_spans is not None:
        write_dict_tsv(
            out_dir / "aux-main-common-clock-intersections.tsv",
            AUX_MAIN_INTERSECTION_COLUMNS,
            auxiliary_main_intersections(
                auxiliary_segments, main_speaker_spans),
        )
    if identity_references is not None:
        write_raw_tsv(
            out_dir / "identity-retained-references.tsv",
            identity_references, list(identity_references.rows))
        reference_context_root = out_dir / "retained-reference-contexts"
        reference_context_root.mkdir()
        for row in identity_references.rows:
            reference_dir = reference_context_root / row["evidence_id"].replace(
                ":", "_")
            reference_dir.mkdir()
            write_raw_tsv(
                reference_dir / "identity-reference.tsv",
                identity_references, [row])
            (reference_dir / "reference-sections.md").write_text(
                render_retained_reference(
                    row, retained_reference_blocks(blocks, row)),
                encoding="utf-8")
        reference_rows = list(identity_references.rows)
        (out_dir / "retained-references-chronological.md").write_text(
            render_retained_reference_sequence(
                reference_rows, blocks, reverse=False),
            encoding="utf-8")
        (out_dir / "retained-references-reverse.md").write_text(
            render_retained_reference_sequence(
                reference_rows, blocks, reverse=True),
            encoding="utf-8")
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
        if (auxiliary_header is not None and
                auxiliary_posterior is not None and
                auxiliary_frame_period is not None and
                auxiliary_segment_header is not None and
                auxiliary_segments is not None and
                main_speaker_spans is not None):
            write_posterior(
                context_dir / "aux-sortformer-frames.csv",
                auxiliary_header,
                context_posterior(
                    auxiliary_posterior, auxiliary_frame_period, context),
            )
            write_segments(
                context_dir / "aux-sortformer-segments.csv",
                auxiliary_segment_header,
                context_segments(auxiliary_segments, context),
            )
            write_dict_tsv(
                context_dir / "aux-main-common-clock-intersections.tsv",
                AUX_MAIN_INTERSECTION_COLUMNS,
                auxiliary_main_intersections(
                    context_segments(auxiliary_segments, context),
                    main_speaker_spans),
            )
        if vad_windows is not None and vad_states is not None:
            write_raw_tsv(
                context_dir / "vad-window-probabilities.tsv", vad_windows,
                context_vad_windows(vad_windows, context))
            write_raw_tsv(
                context_dir / "vad-endpoint-states.tsv", vad_states,
                context_vad_states(vad_states, context))
        if (identity_references is not None and
                gallery_independence is not None):
            write_raw_tsv(
                context_dir / "identity-retained-references.tsv",
                identity_references,
                context_identity_references(identity_references, context))
            write_raw_tsv(
                context_dir / "gallery-independence-evidence.tsv",
                gallery_independence,
                context_gallery_evidence(gallery_independence, context))

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
    if (auxiliary_posterior_path is not None and
            auxiliary_segment_path is not None and
            auxiliary_posterior is not None and
            auxiliary_frame_period is not None and
            auxiliary_speaker_columns is not None):
        packet_manifest["sources"]["auxiliary_sortformer_posterior"] = {
            "path": str(auxiliary_posterior_path.resolve()),
            "sha256": sha256_file(auxiliary_posterior_path),
        }
        packet_manifest["sources"]["auxiliary_sortformer_segments"] = {
            "path": str(auxiliary_segment_path.resolve()),
            "sha256": sha256_file(auxiliary_segment_path),
        }
        packet_manifest["auxiliary_streaming_context"] = {
            "role": "display_only_correlated_model_context",
            "frame_period_sec": auxiliary_frame_period,
            "first_frame_sec": auxiliary_posterior[0].time_sec,
            "last_frame_sec": auxiliary_posterior[-1].time_sec,
            "speaker_columns": auxiliary_speaker_columns,
            "identity_mapping": "not_assigned_by_packet",
            "common_clock_intersections": (
                "all_pairwise_rows_with_explicit_track_absence"),
        }
    if (identity_reference_path is not None and
            gallery_independence_path is not None):
        packet_manifest["sources"]["identity_retained_references"] = {
            "path": str(identity_reference_path.resolve()),
            "sha256": sha256_file(identity_reference_path),
        }
        packet_manifest["sources"]["gallery_independence_evidence"] = {
            "path": str(gallery_independence_path.resolve()),
            "sha256": sha256_file(gallery_independence_path),
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
    parser.add_argument("--identity-retained-references")
    parser.add_argument("--gallery-independence-evidence")
    parser.add_argument("--aux-sortformer-frames")
    parser.add_argument("--aux-sortformer-segments")
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
        identity_reference_path=(
            Path(args.identity_retained_references)
            if args.identity_retained_references else None),
        gallery_independence_path=(
            Path(args.gallery_independence_evidence)
            if args.gallery_independence_evidence else None),
        auxiliary_posterior_path=(
            Path(args.aux_sortformer_frames)
            if args.aux_sortformer_frames else None),
        auxiliary_segment_path=(
            Path(args.aux_sortformer_segments)
            if args.aux_sortformer_segments else None),
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
