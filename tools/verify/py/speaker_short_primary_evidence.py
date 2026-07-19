#!/usr/bin/env python3
"""Export reference-free short-primary/source-clock evidence.

The tool reads frozen runtime tracks and raw model evidence only. It lists
short primary runs whose canonical identity differs from at least one current
business piece associated through forced alignment. It never reads a human
reference, assigns correctness, ranks a topology, or selects product behavior.
"""

import argparse
import bisect
import csv
import hashlib
import json
import os


EPSILON = 1e-9
TIME_TOLERANCE_SEC = 1e-6


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_json(path):
    with open(path, encoding="utf-8") as source:
        return json.load(source)


def timeline_from(package):
    timeline = package.get("timeline", package)
    if not isinstance(timeline, dict):
        raise ValueError("timeline package is not an object")
    return timeline


def tracks_by_kind(timeline):
    return {
        track["kind"]: track.get("entries", [])
        for track in timeline.get("tracks", [])
        if isinstance(track, dict) and isinstance(track.get("kind"), str)
    }


def overlap(start, end, other_start, other_end):
    return max(0.0, min(end, other_end) - max(start, other_start))


def identity_key(entry):
    speaker_id = entry.get("speaker_id")
    if isinstance(speaker_id, str) and speaker_id:
        return f"id:{speaker_id}"
    return f"local:{int(entry.get('speaker', -1))}"


def read_frames(path):
    rows = []
    with open(path, encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source)
        required = {
            "frame", "time_sec", "session", "top1", "top1_prob",
            "top2", "top2_prob", "margin", "active_count",
        }
        missing = sorted(required - set(reader.fieldnames or []))
        if missing:
            raise ValueError("Sortformer frame CSV missing: " +
                             ",".join(missing))
        speaker_columns = sorted(
            (name for name in reader.fieldnames or []
             if name.startswith("spk") and name[3:].isdigit()),
            key=lambda value: int(value[3:]))
        if not speaker_columns:
            raise ValueError("Sortformer frame CSV has no speaker columns")
        for source in reader:
            rows.append({
                "frame": int(source["frame"]),
                "time_sec": float(source["time_sec"]),
                "session": int(source["session"]),
                "top1": int(source["top1"]),
                "top1_prob": float(source["top1_prob"]),
                "top2": int(source["top2"]),
                "top2_prob": float(source["top2_prob"]),
                "margin": float(source["margin"]),
                "active_count": int(source["active_count"]),
                "channels": {
                    name: float(source[name]) for name in speaker_columns
                },
            })
    if len(rows) < 2:
        raise ValueError("Sortformer frame CSV requires at least two rows")
    for index, row in enumerate(rows):
        if row["frame"] != index:
            raise ValueError("Sortformer frame indices are not contiguous")
        if index and row["time_sec"] <= rows[index - 1]["time_sec"]:
            raise ValueError("Sortformer frame times are not monotonic")
    frame_period = rows[1]["time_sec"] - rows[0]["time_sec"]
    return rows, frame_period


def parse_scores(value):
    scores = []
    if not value:
        return scores
    for item in value.split(","):
        speaker_id, separator, score = item.rpartition(":")
        if not separator or not speaker_id:
            raise ValueError("invalid primary evidence score list")
        scores.append({"speaker_id": speaker_id, "score": float(score)})
    return scores


def read_primary_evidence(path):
    rows = []
    with open(path, encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source, delimiter="\t")
        required = {
            "evidence_id", "kind", "start", "end",
            "embedding_available", "session_gallery_complete",
            "robust_gallery_complete", "session_scores", "robust_scores",
        }
        missing = sorted(required - set(reader.fieldnames or []))
        if missing:
            raise ValueError("primary evidence TSV missing: " +
                             ",".join(missing))
        for index, source in enumerate(reader):
            expected_id = f"primary_run:{index}"
            if source["evidence_id"] != expected_id:
                raise ValueError("primary evidence IDs are not contiguous")
            if source["kind"] != "primary_run":
                raise ValueError("primary evidence has an unexpected kind")
            rows.append({
                "evidence_id": source["evidence_id"],
                "start_sec": float(source["start"]),
                "end_sec": float(source["end"]),
                "embedding_available": source["embedding_available"] == "1",
                "session_gallery_complete": (
                    source["session_gallery_complete"] == "1"),
                "robust_gallery_complete": (
                    source["robust_gallery_complete"] == "1"),
                "session_scores": parse_scores(source["session_scores"]),
                "robust_scores": parse_scores(source["robust_scores"]),
            })
    return rows


def read_identity_epochs(path):
    rows = []
    with open(path, encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source, delimiter="\t")
        required = {
            "local_speaker", "start_sec", "end_sec", "speaker_id",
        }
        missing = sorted(required - set(reader.fieldnames or []))
        if missing:
            raise ValueError("identity epoch TSV missing: " +
                             ",".join(missing))
        for source in reader:
            start = float(source["start_sec"])
            end = float(source["end_sec"])
            if start < 0.0 or end <= start or not source["speaker_id"]:
                raise ValueError("identity epoch has invalid values")
            rows.append({
                "local_speaker": int(source["local_speaker"]),
                "start_sec": start,
                "end_sec": end,
                "speaker_id": source["speaker_id"],
            })
    rows.sort(key=lambda item: (
        item["local_speaker"], item["start_sec"], item["end_sec"]))
    return rows


def validate_primary_evidence(primary, evidence):
    if len(primary) != len(evidence):
        raise ValueError("primary evidence count does not match primary track")
    for index, (entry, item) in enumerate(zip(primary, evidence)):
        if (abs(float(entry["start"]) - item["start_sec"]) >
                TIME_TOLERANCE_SEC or
                abs(float(entry["end"]) - item["end_sec"]) >
                TIME_TOLERANCE_SEC):
            raise ValueError(
                f"primary evidence bounds differ at primary_run:{index}")


def alignment_relations(primary, align):
    start = float(primary["start"])
    end = float(primary["end"])
    relations = []
    for group in align:
        text_id = int(group.get("text_id", -1))
        units = group.get("units", [])
        for index, unit in enumerate(units):
            unit_start = float(unit.get("start", 0.0))
            unit_end = float(unit.get("end", unit_start))
            if unit_end > unit_start + EPSILON:
                if overlap(start, end, unit_start, unit_end) <= EPSILON:
                    continue
                relations.append({
                    "kind": "positive_unit",
                    "text_id": text_id,
                    "unit_index": index,
                    "start_sec": unit_start,
                    "end_sec": unit_end,
                    "text": str(unit.get("text", "")),
                })
            elif (start - TIME_TOLERANCE_SEC <= unit_start <=
                  end + TIME_TOLERANCE_SEC):
                relations.append({
                    "kind": "zero_duration_unit",
                    "text_id": text_id,
                    "unit_index": index,
                    "start_sec": unit_start,
                    "end_sec": unit_end,
                    "text": str(unit.get("text", "")),
                })
        for index, (before, after) in enumerate(zip(units, units[1:])):
            gap_start = float(before.get("end", before.get("start", 0.0)))
            gap_end = float(after.get("start", 0.0))
            if (gap_end <= gap_start + EPSILON or
                    overlap(start, end, gap_start, gap_end) <= EPSILON):
                continue
            relations.append({
                "kind": "alignment_gap",
                "text_id": text_id,
                "before_unit_index": index,
                "after_unit_index": index + 1,
                "start_sec": gap_start,
                "end_sec": gap_end,
                "before_text": str(before.get("text", "")),
                "after_text": str(after.get("text", "")),
            })
    relations.sort(key=lambda item: (
        item["start_sec"], item["end_sec"], item["text_id"], item["kind"]))
    return relations


def relation_overlaps_business(relation, entry):
    if int(entry.get("text_id", -1)) != relation["text_id"]:
        return False
    start = float(entry["start"])
    end = float(entry["end"])
    if relation["kind"] == "zero_duration_unit":
        point = relation["start_sec"]
        return (start - TIME_TOLERANCE_SEC <= point <=
                end + TIME_TOLERANCE_SEC)
    return overlap(
        relation["start_sec"], relation["end_sec"], start, end) > EPSILON


def associated_business(primary, relations, business):
    start = float(primary["start"])
    end = float(primary["end"])
    text_ids = {item["text_id"] for item in relations}
    selected = []
    for index, entry in enumerate(business):
        text_id = int(entry.get("text_id", -1))
        if text_id not in text_ids:
            continue
        direct = overlap(
            start, end, float(entry["start"]), float(entry["end"])) > EPSILON
        related = any(
            relation_overlaps_business(relation, entry)
            for relation in relations)
        if direct or related:
            selected.append((index, entry))
    return selected


def indexed_entries(entries, indices):
    return [
        {"index": index, **entries[index]}
        for index in sorted(set(indices)) if 0 <= index < len(entries)
    ]


def raw_frames_for(primary, frames, frame_period):
    times = [item["time_sec"] for item in frames]
    start = float(primary["start"])
    end = float(primary["end"])
    left = max(0, bisect.bisect_left(times, start) - 1)
    right = min(len(frames), bisect.bisect_left(times, end) + 1)
    return [
        item for item in frames[left:right]
        if (overlap(start, end, item["time_sec"],
                    item["time_sec"] + frame_period) > EPSILON or
            item["time_sec"] < start or item["time_sec"] >= end)
    ]


def overlapping_entries(entries, start, end):
    return [
        {"index": index, **entry}
        for index, entry in enumerate(entries)
        if overlap(start, end, float(entry.get("start", 0.0)),
                   float(entry.get("end", entry.get("start", 0.0)))) >
        EPSILON
    ]


def voiceprint_entries(entries, text_ids, start, end):
    selected = []
    for index, entry in enumerate(entries):
        same_source = int(entry.get("text_id", -1)) in text_ids
        intersects = overlap(
            start, end, float(entry.get("start", 0.0)),
            float(entry.get("end", entry.get("start", 0.0)))) > EPSILON
        if same_source or intersects:
            selected.append({"index": index, **entry})
    return selected


def epochs_for(primary, epochs):
    local = int(primary.get("speaker", -1))
    start = float(primary["start"])
    end = float(primary["end"])
    return [
        item for item in epochs
        if item["local_speaker"] == local and
        overlap(start, end, item["start_sec"], item["end_sec"]) > EPSILON
    ]


def build_inventory(timeline, frames, frame_period, primary_evidence,
                    identity_epochs):
    tracks = tracks_by_kind(timeline)
    required = {
        "diarization", "primary_speaker", "asr", "vad", "align",
        "speaker_voiceprint", "business_speaker",
    }
    missing = sorted(required - set(tracks))
    if missing:
        raise ValueError("timeline tracks missing: " + ",".join(missing))
    primary = tracks["primary_speaker"]
    validate_primary_evidence(primary, primary_evidence)
    short_max_sec = float(
        timeline.get("resolved_config", {})
        .get("speaker_fusion", {}).get("short_max_sec", 0.0))
    if short_max_sec <= 0.0:
        raise ValueError("resolved speaker_fusion.short_max_sec is missing")

    asr_by_id = {
        int(entry.get("text_id", -1)): entry for entry in tracks["asr"]
    }
    align_by_id = {
        int(entry.get("text_id", -1)): entry for entry in tracks["align"]
    }
    records = []
    for index, entry in enumerate(primary):
        start = float(entry["start"])
        end = float(entry["end"])
        if end <= start or end - start > short_max_sec + EPSILON:
            continue
        relations = alignment_relations(entry, tracks["align"])
        if not relations:
            continue
        associated = associated_business(
            entry, relations, tracks["business_speaker"])
        conflicts = [
            (business_index, business_entry)
            for business_index, business_entry in associated
            if identity_key(business_entry) != identity_key(entry)
        ]
        if not conflicts:
            continue

        text_ids = sorted({relation["text_id"] for relation in relations})
        primary_control_indices = [index - 1, index, index + 1]
        business_control_indices = []
        for business_index, _ in associated:
            business_control_indices.extend([
                business_index - 1, business_index, business_index + 1])
        for business_index, business_entry in enumerate(
                tracks["business_speaker"]):
            if int(business_entry.get("text_id", -1)) in text_ids:
                business_control_indices.append(business_index)

        context_start = min(
            [start] + [item["start_sec"] for item in relations])
        context_end = max([end] + [item["end_sec"] for item in relations])
        records.append({
            "record_id": f"short_primary:{index}",
            "primary_index": index,
            "primary": entry,
            "primary_evidence": primary_evidence[index],
            "identity_epochs": epochs_for(entry, identity_epochs),
            "alignment_relations": relations,
            "asr_sources": [
                asr_by_id[text_id] for text_id in text_ids
                if text_id in asr_by_id
            ],
            "align_sources": [
                align_by_id[text_id] for text_id in text_ids
                if text_id in align_by_id
            ],
            "associated_business": [
                {"index": business_index, **business_entry}
                for business_index, business_entry in associated
            ],
            "identity_conflicts": [
                {"index": business_index, **business_entry}
                for business_index, business_entry in conflicts
            ],
            "controls": {
                "primary": indexed_entries(
                    primary, primary_control_indices),
                "business": indexed_entries(
                    tracks["business_speaker"], business_control_indices),
            },
            "raw_sortformer_frames": raw_frames_for(
                entry, frames, frame_period),
            "activity": overlapping_entries(
                tracks["diarization"], context_start, context_end),
            "vad": overlapping_entries(
                tracks["vad"], context_start, context_end),
            "voiceprint": voiceprint_entries(
                tracks["speaker_voiceprint"], set(text_ids),
                context_start, context_end),
        })
    return records, short_max_sec


def records_sha256(records):
    encoded = json.dumps(
        records, ensure_ascii=False, sort_keys=True,
        separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def export_inventory(timeline_path, frames_path, primary_evidence_path,
                     identity_epochs_path, out_path):
    package = load_json(timeline_path)
    timeline = timeline_from(package)
    frames, frame_period = read_frames(frames_path)
    primary_evidence = read_primary_evidence(primary_evidence_path)
    identity_epochs = read_identity_epochs(identity_epochs_path)
    records, short_max_sec = build_inventory(
        timeline, frames, frame_period, primary_evidence, identity_epochs)
    output = {
        "schema_version": 1,
        "kind": "orator_short_primary_evidence",
        "scope": "reference_free_mechanical_identity_conflicts",
        "sources": {
            "timeline": {
                "path": os.path.abspath(timeline_path),
                "sha256": sha256_file(timeline_path),
            },
            "sortformer_frames": {
                "path": os.path.abspath(frames_path),
                "sha256": sha256_file(frames_path),
            },
            "primary_evidence": {
                "path": os.path.abspath(primary_evidence_path),
                "sha256": sha256_file(primary_evidence_path),
            },
            "identity_epochs": {
                "path": os.path.abspath(identity_epochs_path),
                "sha256": sha256_file(identity_epochs_path),
            },
        },
        "audio_sec": float(timeline["audio_sec"]),
        "sample_rate": int(timeline["sample_rate"]),
        "short_max_sec": short_max_sec,
        "frame_period_sec": frame_period,
        "record_count": len(records),
        "records_sha256": records_sha256(records),
        "records": records,
    }
    parent = os.path.dirname(os.path.abspath(out_path))
    os.makedirs(parent, exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as destination:
        json.dump(output, destination, ensure_ascii=False, indent=2)
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--timeline", required=True)
    parser.add_argument("--sortformer-frames", required=True)
    parser.add_argument("--primary-evidence", required=True)
    parser.add_argument("--identity-epochs", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()
    result = export_inventory(
        args.timeline, args.sortformer_frames, args.primary_evidence,
        args.identity_epochs, args.out)
    print(json.dumps({
        "record_count": result["record_count"],
        "records_sha256": result["records_sha256"],
        "out": os.path.abspath(args.out),
    }, ensure_ascii=False))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, csv.Error) as error:
        raise SystemExit(f"speaker short primary evidence: {error}")
