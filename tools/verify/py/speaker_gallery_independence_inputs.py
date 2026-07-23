#!/usr/bin/env python3
"""Copy immutable voiceprint queries for gallery-independence review.

Context bounds select rows for display only. The tool does not compare scores,
label a speaker, or evaluate a product result.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any


CONTEXT_COLUMNS = [
    "context_id", "start_sec", "end_sec", "focus_refs", "control_refs",
]
QUERY_COLUMNS = [
    "evidence_id", "kind", "text_id", "source_start", "source_end",
    "start", "end",
]
INCLUSIVE_EVIDENCE_COLUMNS = QUERY_COLUMNS + [
    "embedding_available", "session_gallery_complete",
    "robust_gallery_complete", "session_scores", "robust_scores",
]


@dataclass(frozen=True)
class Context:
    context_id: str
    start_sec: float
    end_sec: float


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_contexts(path: Path) -> list[Context]:
    contexts = []
    with path.open(encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source, delimiter="\t")
        if reader.fieldnames != CONTEXT_COLUMNS:
            raise ValueError("context columns differ from the governed schema")
        for row in reader:
            start = float(row["start_sec"])
            end = float(row["end_sec"])
            if (not row["context_id"] or not math.isfinite(start) or
                    not math.isfinite(end) or start < 0.0 or end <= start):
                raise ValueError("context has invalid bounds")
            contexts.append(Context(row["context_id"], start, end))
    if not contexts:
        raise ValueError("context table is empty")
    return contexts


def timeline_from(package: Any) -> dict[str, Any]:
    if not isinstance(package, dict):
        raise ValueError("artifact is not an object")
    timeline = package.get("timeline", package)
    if not isinstance(timeline, dict):
        raise ValueError("artifact has no timeline")
    return timeline


def voiceprint_entries(timeline: dict[str, Any]) -> list[dict[str, Any]]:
    matches = [
        track for track in timeline.get("tracks", [])
        if isinstance(track, dict) and track.get("kind") == "speaker_voiceprint"
    ]
    if len(matches) != 1 or not isinstance(matches[0].get("entries"), list):
        raise ValueError("timeline requires one speaker_voiceprint track")
    return matches[0]["entries"]


def intersects(start: float, end: float, context: Context) -> bool:
    return min(end, context.end_sec) > max(start, context.start_sec)


def selected_queries(
    entries: list[dict[str, Any]], contexts: list[Context],
) -> list[dict[str, str]]:
    selected = []
    seen = set()
    for entry in entries:
        if not isinstance(entry, dict):
            raise ValueError("voiceprint entry is not an object")
        evidence_id = entry.get("evidence_id")
        kind = entry.get("evidence_kind")
        if not isinstance(evidence_id, str) or not evidence_id:
            raise ValueError("voiceprint evidence_id is invalid")
        if evidence_id in seen:
            raise ValueError("voiceprint evidence_id is duplicated")
        seen.add(evidence_id)
        if not isinstance(kind, str) or not kind:
            raise ValueError("voiceprint evidence kind is invalid")
        start = float(entry["start"])
        end = float(entry["end"])
        if (not math.isfinite(start) or not math.isfinite(end) or
                start < 0.0 or end <= start):
            raise ValueError("voiceprint query has invalid bounds")
        if not any(intersects(start, end, context) for context in contexts):
            continue
        text_id = int(entry["text_id"])
        source_start = int(entry["source_start"])
        source_end = int(entry["source_end"])
        if ((text_id >= 0 and source_end <= source_start) or
                (text_id < 0 and (source_start != 0 or source_end != 0))):
            raise ValueError("voiceprint query has an invalid source range")
        selected.append({
            "evidence_id": evidence_id,
            "kind": kind,
            "text_id": str(text_id),
            "source_start": str(source_start),
            "source_end": str(source_end),
            "start": f"{start:.9f}",
            "end": f"{end:.9f}",
        })
    if not selected:
        raise ValueError("no voiceprint query intersects the contexts")
    return selected


def write_queries(path: Path, rows: list[dict[str, str]]) -> None:
    with path.open("w", encoding="utf-8", newline="") as output:
        writer = csv.DictWriter(
            output, fieldnames=QUERY_COLUMNS, delimiter="\t",
            lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def parse_scores(value: str) -> list[tuple[str, float]]:
    if not value:
        return []
    scores = []
    for item in value.split(","):
        speaker_id, separator, raw_score = item.rpartition(":")
        score = float(raw_score)
        if (not separator or not speaker_id or not math.isfinite(score)):
            raise ValueError("replayed score list is invalid")
        scores.append((speaker_id, score))
    return scores


def verify_inclusive_replay(
    entries: list[dict[str, Any]],
    selected: list[dict[str, str]],
    replayed_path: Path,
    tolerance: float = 1e-6,
) -> dict[str, Any]:
    expected_by_id = {entry["evidence_id"]: entry for entry in entries}
    with replayed_path.open(encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source, delimiter="\t")
        if ((reader.fieldnames or [])[:len(INCLUSIVE_EVIDENCE_COLUMNS)] !=
                INCLUSIVE_EVIDENCE_COLUMNS):
            raise ValueError("replayed evidence columns differ")
        replayed = list(reader)
    if len(replayed) != len(selected):
        raise ValueError("replayed evidence count differs from query input")

    maximum_score_delta = 0.0
    for query, row in zip(selected, replayed):
        if any(row[column] != query[column] for column in QUERY_COLUMNS):
            raise ValueError("replayed query metadata differs from input")
        expected = expected_by_id[query["evidence_id"]]
        for field in (
                "embedding_available", "session_gallery_complete",
                "robust_gallery_complete"):
            expected_value = "1" if bool(expected.get(field, False)) else "0"
            if row[field] != expected_value:
                raise ValueError("replayed evidence availability differs")
        for field in ("session_scores", "robust_scores"):
            actual_scores = parse_scores(row[field])
            expected_scores = [
                (str(item["speaker_id"]), float(item["score"]))
                for item in expected.get(field, [])
            ]
            if ([item[0] for item in actual_scores] !=
                    [item[0] for item in expected_scores]):
                raise ValueError("replayed evidence speaker IDs differ")
            for (_, actual), (_, expected_score) in zip(
                    actual_scores, expected_scores):
                if not math.isfinite(expected_score):
                    raise ValueError("terminal evidence score is not finite")
                delta = abs(actual - expected_score)
                maximum_score_delta = max(maximum_score_delta, delta)
                if delta > tolerance:
                    raise ValueError("replayed inclusive score differs")
    return {
        "row_count": len(replayed),
        "score_tolerance": tolerance,
        "maximum_score_delta": maximum_score_delta,
    }


def export_inputs(
    artifact_path: Path,
    contexts_path: Path,
    output_path: Path,
    manifest_path: Path,
    replayed_evidence_path: Path | None = None,
) -> dict[str, Any]:
    package = json.loads(artifact_path.read_text(encoding="utf-8"))
    contexts = load_contexts(contexts_path)
    entries = voiceprint_entries(timeline_from(package))
    queries = selected_queries(entries, contexts)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    write_queries(output_path, queries)
    manifest = {
        "schema_version": 1,
        "kind": "orator_speaker_gallery_independence_inputs",
        "scope": "display_only_query_selection",
        "sources": {
            "artifact": {
                "path": str(artifact_path.resolve()),
                "sha256": sha256_file(artifact_path),
            },
            "contexts": {
                "path": str(contexts_path.resolve()),
                "sha256": sha256_file(contexts_path),
            },
        },
        "output": {
            "path": str(output_path.resolve()),
            "sha256": sha256_file(output_path),
        },
        "context_count": len(contexts),
        "query_count": len(queries),
    }
    if replayed_evidence_path is not None:
        manifest["sources"]["replayed_evidence"] = {
            "path": str(replayed_evidence_path.resolve()),
            "sha256": sha256_file(replayed_evidence_path),
        }
        manifest["inclusive_replay_contract"] = verify_inclusive_replay(
            entries, queries, replayed_evidence_path)
    manifest_path.write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8")
    return manifest


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifact", required=True)
    parser.add_argument("--contexts", required=True)
    parser.add_argument("--out-tsv", required=True)
    parser.add_argument("--manifest", required=True)
    parser.add_argument("--replayed-evidence")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    manifest = export_inputs(
        Path(args.artifact), Path(args.contexts), Path(args.out_tsv),
        Path(args.manifest),
        (Path(args.replayed_evidence) if args.replayed_evidence else None))
    print(json.dumps({
        "kind": manifest["kind"],
        "context_count": manifest["context_count"],
        "query_count": manifest["query_count"],
        "output_sha256": manifest["output"]["sha256"],
    }, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main(sys.argv[1:]))
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError,
            csv.Error) as error:
        raise SystemExit(f"speaker gallery independence inputs: {error}")
