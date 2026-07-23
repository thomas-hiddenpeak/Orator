#!/usr/bin/env python3
"""Copy frozen WebSocket diar deliveries into strict identity snapshots.

The output preserves capture order and raw producer values. It does not read a
reference, summarize speaker behavior, or evaluate an identity.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import re
import sys
from pathlib import Path
from typing import Any


SPEAKER_RE = re.compile(r"^speaker_(\d+)$")
OUTPUT_COLUMNS = [
    "snapshot_index",
    "start_sec",
    "end_sec",
    "local_speaker",
    "confidence",
    "captured_speaker_id",
]


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def parse_segment(source: Any) -> tuple[float, float, int, float, str]:
    if not isinstance(source, dict):
        raise ValueError("diar segment is not an object")
    start = float(source["start"])
    end = float(source["end"])
    confidence = float(source["confidence"])
    if (not math.isfinite(start) or not math.isfinite(end) or
            not math.isfinite(confidence)):
        raise ValueError("diar segment contains a non-finite value")
    if start < 0.0 or end <= start:
        raise ValueError("diar segment has invalid bounds")
    speaker = source.get("speaker")
    if not isinstance(speaker, str):
        raise ValueError("diar segment speaker is not a string")
    match = SPEAKER_RE.fullmatch(speaker)
    if match is None:
        raise ValueError("diar segment speaker has an invalid label")
    speaker_id = source.get("speaker_id", "")
    if not isinstance(speaker_id, str):
        raise ValueError("diar segment speaker_id is not a string")
    return start, end, int(match.group(1)), confidence, speaker_id


def segment_order_key(
    segment: tuple[float, float, int, float, str],
) -> tuple[float, float, int]:
    return segment[0], segment[1], segment[2]


def diar_snapshots(package: Any) -> list[list[tuple[float, float, int,
                                                    float, str]]]:
    if not isinstance(package, dict) or not isinstance(package.get("events"),
                                                       list):
        raise ValueError("artifact has no event list")
    snapshots = []
    for event in package["events"]:
        if not isinstance(event, dict) or event.get("type") != "diar":
            continue
        raw_segments = event.get("segments")
        if not isinstance(raw_segments, list):
            raise ValueError("diar event has no segment list")
        segments = [parse_segment(source) for source in raw_segments]
        if segments != sorted(segments, key=segment_order_key):
            raise ValueError("diar event segments are not ordered")
        snapshots.append(segments)
    if not snapshots:
        raise ValueError("artifact contains no diar snapshots")
    return snapshots


def write_snapshots(
    path: Path,
    snapshots: list[list[tuple[float, float, int, float, str]]],
) -> int:
    row_count = 0
    with path.open("w", encoding="utf-8", newline="") as output:
        writer = csv.writer(output, delimiter="\t", lineterminator="\n")
        writer.writerow(OUTPUT_COLUMNS)
        for snapshot_index, segments in enumerate(snapshots):
            if not segments:
                writer.writerow([snapshot_index, "", "", "", "", ""])
                continue
            for start, end, local, confidence, speaker_id in segments:
                writer.writerow([
                    snapshot_index,
                    f"{start:.9f}",
                    f"{end:.9f}",
                    local,
                    f"{confidence:.9f}",
                    speaker_id,
                ])
                row_count += 1
    return row_count


def export_snapshots(
    artifact_path: Path,
    output_path: Path,
    manifest_path: Path,
) -> dict[str, Any]:
    snapshots = diar_snapshots(load_json(artifact_path))
    output_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    row_count = write_snapshots(output_path, snapshots)
    manifest = {
        "schema_version": 1,
        "kind": "orator_speaker_identity_snapshot_evidence",
        "scope": "mechanical_capture_order_copy",
        "source": {
            "path": str(artifact_path.resolve()),
            "sha256": sha256_file(artifact_path),
        },
        "output": {
            "path": str(output_path.resolve()),
            "sha256": sha256_file(output_path),
        },
        "snapshot_count": len(snapshots),
        "segment_row_count": row_count,
    }
    manifest_path.write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8")
    return manifest


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifact", required=True)
    parser.add_argument("--out-tsv", required=True)
    parser.add_argument("--manifest", required=True)
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    manifest = export_snapshots(
        Path(args.artifact), Path(args.out_tsv), Path(args.manifest))
    print(json.dumps({
        "kind": manifest["kind"],
        "snapshot_count": manifest["snapshot_count"],
        "segment_row_count": manifest["segment_row_count"],
        "output_sha256": manifest["output"]["sha256"],
    }, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main(sys.argv[1:]))
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError,
            csv.Error) as error:
        raise SystemExit(f"speaker identity snapshot evidence: {error}")
