#!/usr/bin/env python3
"""Build a label-free Spec 013 speaker evidence package.

The output projects immutable Sortformer frame posteriors, independent
per-turn and per-diar-segment TitaNet registry similarities, VAD support, and
forced-alignment units onto the frozen business-turn intervals. It never reads
reference annotations and never assigns a correct speaker.
"""

import argparse
import bisect
import csv
import hashlib
import json
import math
import os
import tomllib


EPSILON = 1e-9


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
    tracks = {}
    for track in timeline.get("tracks", []):
        if isinstance(track, dict) and isinstance(track.get("kind"), str):
            tracks[track["kind"]] = track.get("entries", [])
    return tracks


def read_frames(path):
    rows = []
    with open(path, encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source)
        speaker_columns = sorted(
            (name for name in reader.fieldnames or []
             if name.startswith("spk")),
            key=lambda value: int(value[3:]))
        if not speaker_columns:
            raise ValueError("Sortformer frame CSV has no speaker columns")
        for row in reader:
            rows.append({
                "frame": int(row["frame"]),
                "time_sec": float(row["time_sec"]),
                "session": int(row["session"]),
                "top1": int(row["top1"]),
                "top1_prob": float(row["top1_prob"]),
                "top2": int(row["top2"]),
                "top2_prob": float(row["top2_prob"]),
                "margin": float(row["margin"]),
                "active_count": int(row["active_count"]),
                "probs": [float(row[name]) for name in speaker_columns],
            })
    if not rows:
        raise ValueError("Sortformer frame CSV is empty")
    for index, row in enumerate(rows):
        if row["frame"] != index:
            raise ValueError("Sortformer frame indices are not contiguous")
        if index and row["time_sec"] <= rows[index - 1]["time_sec"]:
            raise ValueError("Sortformer frame times are not monotonic")
    frame_period = (
        rows[1]["time_sec"] - rows[0]["time_sec"]
        if len(rows) > 1 else 0.08)
    return rows, speaker_columns, frame_period


def read_titanet(path):
    rows = {}
    with open(path, encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source, delimiter="\t")
        score_columns = [
            name for name in reader.fieldnames or []
            if name.startswith("score_")
        ]
        for row in reader:
            evidence_id = row["evidence_id"]
            if evidence_id in rows:
                raise ValueError(f"duplicate TitaNet evidence ID {evidence_id}")
            scores = {
                name.removeprefix("score_"): float(row[name])
                for name in score_columns if row.get(name)
            }
            rows[evidence_id] = {
                "source_start_sec": float(row["start_sec"]),
                "source_end_sec": float(row["end_sec"]),
                "source_duration_sec": float(row["duration_sec"]),
                "status": row["status"],
                "embed_start_sec": (
                    float(row["embed_start_sec"])
                    if row.get("embed_start_sec") else None),
                "embed_end_sec": (
                    float(row["embed_end_sec"])
                    if row.get("embed_end_sec") else None),
                "best_id": row.get("best_id") or None,
                "best_score": (
                    float(row["best_score"])
                    if row.get("best_score") else None),
                "second_id": row.get("second_id") or None,
                "second_score": (
                    float(row["second_score"])
                    if row.get("second_score") else None),
                "margin": (
                    float(row["margin"]) if row.get("margin") else None),
                "scores": scores,
            }
    return rows


def read_diar_segments(path):
    entries = []
    with open(path, encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source)
        required = {
            "start_sec", "end_sec", "session", "local_speaker",
            "confidence",
        }
        missing = sorted(required - set(reader.fieldnames or []))
        if missing:
            raise ValueError(
                "external diar segment CSV missing: " + ",".join(missing))
        for source_index, row in enumerate(reader):
            start = float(row["start_sec"])
            end = float(row["end_sec"])
            if start < 0.0 or end <= start:
                raise ValueError(
                    f"invalid external diar segment row {source_index + 2}")
            entries.append({
                "start": start,
                "end": end,
                "speaker": int(row["local_speaker"]),
                "speaker_id": None,
                "confidence": float(row["confidence"]),
                "source_session": int(row["session"]),
                "source_mean_margin": (
                    float(row["mean_margin"])
                    if row.get("mean_margin") else None),
                "source_index": source_index,
            })
    entries.sort(key=lambda item: (
        item["start"], item["end"], item["speaker"],
        item["source_index"]))
    return entries


def candidate_resolved_config(timeline, config_path):
    resolved = {
        section: dict(values) if isinstance(values, dict) else values
        for section, values in timeline.get("resolved_config", {}).items()
    }
    if not config_path:
        return resolved
    with open(config_path, "rb") as source:
        candidate = tomllib.load(source)
    for section, values in candidate.items():
        if not isinstance(values, dict):
            continue
        current = resolved.setdefault(section, {})
        if isinstance(current, dict):
            current.update(values)
        else:
            resolved[section] = dict(values)
    return resolved


def overlap(start, end, other_start, other_end):
    return max(0.0, min(end, other_end) - max(start, other_start))


def summarize_frames(rows, times, frame_period, start, end, onset):
    left = max(0, bisect.bisect_right(times, start - frame_period) - 1)
    right = bisect.bisect_left(times, end)
    speaker_count = len(rows[0]["probs"])
    probability_sum = [0.0] * speaker_count
    active_duration = [0.0] * speaker_count
    top1_duration = [0.0] * speaker_count
    active_top1_duration = [0.0] * speaker_count
    total_weight = 0.0
    margin_sum = 0.0
    overlap_duration = 0.0
    any_active_duration = 0.0
    used_indices = []
    for index in range(left, right):
        row = rows[index]
        weight = overlap(
            start, end, row["time_sec"], row["time_sec"] + frame_period)
        if weight <= EPSILON:
            continue
        used_indices.append(index)
        total_weight += weight
        margin_sum += row["margin"] * weight
        if row["active_count"] >= 1:
            any_active_duration += weight
            if 0 <= row["top1"] < speaker_count:
                active_top1_duration[row["top1"]] += weight
        if row["active_count"] >= 2:
            overlap_duration += weight
        if 0 <= row["top1"] < speaker_count:
            top1_duration[row["top1"]] += weight
        for speaker, probability in enumerate(row["probs"]):
            probability_sum[speaker] += probability * weight
            if probability >= onset:
                active_duration[speaker] += weight
    if not used_indices or total_weight <= EPSILON:
        return {
            "frame_start_index": None,
            "frame_end_index": None,
            "frame_coverage_sec": 0.0,
            "mean_probs": [0.0] * speaker_count,
            "active_duration_sec": [0.0] * speaker_count,
            "top1_duration_sec": [0.0] * speaker_count,
            "active_top1_duration_sec": [0.0] * speaker_count,
            "mean_margin": 0.0,
            "any_active_duration_sec": 0.0,
            "overlap_duration_sec": 0.0,
        }
    return {
        "frame_start_index": used_indices[0],
        "frame_end_index": used_indices[-1] + 1,
        "frame_coverage_sec": total_weight,
        "mean_probs": [value / total_weight for value in probability_sum],
        "active_duration_sec": active_duration,
        "top1_duration_sec": top1_duration,
        "active_top1_duration_sec": active_top1_duration,
        "mean_margin": margin_sum / total_weight,
        "any_active_duration_sec": any_active_duration,
        "overlap_duration_sec": overlap_duration,
    }


def interval_metrics(start, end, entries):
    clipped = []
    evidence = []
    for index, entry in enumerate(entries):
        item_start = float(entry.get("start", 0.0))
        item_end = float(entry.get("end", item_start))
        amount = overlap(start, end, item_start, item_end)
        if amount <= EPSILON:
            continue
        clipped.append([max(start, item_start), min(end, item_end)])
        evidence.append({
            "evidence_id": f"vad:{index}",
            "start_sec": item_start,
            "end_sec": item_end,
            "overlap_sec": amount,
        })
    clipped.sort()
    merged = []
    for item_start, item_end in clipped:
        if not merged or item_start > merged[-1][1] + EPSILON:
            merged.append([item_start, item_end])
        else:
            merged[-1][1] = max(merged[-1][1], item_end)
    coverage = sum(item_end - item_start for item_start, item_end in merged)
    gaps = []
    cursor = start
    for item_start, item_end in merged:
        if item_start > cursor:
            gaps.append(item_start - cursor)
        cursor = max(cursor, item_end)
    if cursor < end:
        gaps.append(end - cursor)
    return {
        "coverage_sec": coverage,
        "coverage_ratio": coverage / (end - start),
        "max_gap_sec": max(gaps, default=0.0),
        "island_count": len(merged),
        "evidence": evidence,
    }


def alignment_evidence(align_by_id, text_id, start, end):
    group = align_by_id.get(text_id)
    if group is None:
        return {"text_id": text_id, "units": [], "pause_gaps": []}
    selected = []
    for index, unit in enumerate(group.get("units", [])):
        unit_start = float(unit.get("start", 0.0))
        unit_end = float(unit.get("end", unit_start))
        is_point_unit = abs(unit_end - unit_start) <= EPSILON
        point_in_interval = (
            unit_start >= start - EPSILON and unit_start < end - EPSILON)
        if (is_point_unit and not point_in_interval) or (
                not is_point_unit and
                overlap(start, end, unit_start, unit_end) <= EPSILON):
            continue
        selected.append({
            "evidence_id": f"align:{text_id}:{index}",
            "start_sec": unit_start,
            "end_sec": unit_end,
            "text": str(unit.get("text", "")),
        })
    pause_gaps = []
    for before, after in zip(selected, selected[1:]):
        gap = after["start_sec"] - before["end_sec"]
        if gap > EPSILON:
            pause_gaps.append({
                "start_sec": before["end_sec"],
                "end_sec": after["start_sec"],
                "duration_sec": gap,
            })
    return {"text_id": text_id, "units": selected,
            "pause_gaps": pause_gaps}


def diar_segment_evidence(entries, titanet, start, end):
    selected = []
    for index, entry in enumerate(entries):
        amount = overlap(
            start, end, float(entry["start"]), float(entry["end"]))
        if amount <= EPSILON:
            continue
        evidence_id = f"diarization:{index}"
        if evidence_id not in titanet:
            raise ValueError(
                f"TitaNet diar evidence missing {evidence_id}")
        selected.append({
            "evidence_id": evidence_id,
            "start_sec": float(entry["start"]),
            "end_sec": float(entry["end"]),
            "local_speaker": int(entry["speaker"]),
            "baseline_speaker_id": entry.get("speaker_id"),
            "confidence": float(entry.get("confidence", 0.0)),
            "overlap_sec": amount,
            "titanet": titanet[evidence_id],
        })
    return selected


def write_diar_spans(path, entries):
    parent = os.path.dirname(os.path.abspath(path))
    os.makedirs(parent, exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="") as output:
        writer = csv.writer(output, delimiter="\t", lineterminator="\n")
        writer.writerow(["evidence_id", "start_sec", "end_sec"])
        for index, entry in enumerate(entries):
            writer.writerow([
                f"diarization:{index}",
                float(entry["start"]),
                float(entry["end"]),
            ])


def write_business_spans(path, entries):
    parent = os.path.dirname(os.path.abspath(path))
    os.makedirs(parent, exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="") as output:
        writer = csv.writer(output, delimiter="\t", lineterminator="\n")
        writer.writerow(["evidence_id", "start_sec", "end_sec"])
        for index, entry in enumerate(entries):
            start = float(entry["start"])
            end = float(entry["end"])
            if end <= start:
                raise ValueError(
                    f"invalid business speaker span at index {index}")
            writer.writerow([f"business_speaker:{index}", start, end])


def write_json(path, value):
    parent = os.path.dirname(os.path.abspath(path))
    os.makedirs(parent, exist_ok=True)
    with open(path, "w", encoding="utf-8") as output:
        json.dump(value, output, ensure_ascii=False, indent=2)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--timeline", required=True)
    parser.add_argument("--sortformer-frames")
    parser.add_argument("--titanet-turns")
    parser.add_argument("--titanet-diar-segments")
    parser.add_argument(
        "--diar-segments",
        help="optional external diar_evidence_probe segment CSV",
    )
    parser.add_argument(
        "--candidate-config",
        help="optional TOML used to produce external model evidence",
    )
    parser.add_argument("--out")
    parser.add_argument("--export-diar-spans")
    parser.add_argument("--export-business-spans")
    args = parser.parse_args()

    if args.export_diar_spans and args.export_business_spans:
        raise ValueError("select only one span export mode")

    package = load_json(args.timeline)
    timeline = timeline_from(package)
    tracks = tracks_by_kind(timeline)
    required = {"diarization", "asr", "vad", "align", "business_speaker"}
    missing = sorted(required - set(tracks))
    if missing:
        raise ValueError("timeline tracks missing: " + ",".join(missing))

    diar_entries = (
        read_diar_segments(args.diar_segments)
        if args.diar_segments else tracks["diarization"])

    if args.export_diar_spans:
        write_diar_spans(args.export_diar_spans, diar_entries)
        print(json.dumps({
            "diar_segments": len(diar_entries),
            "out": os.path.abspath(args.export_diar_spans),
        }, ensure_ascii=False))
        return

    if args.export_business_spans:
        write_business_spans(
            args.export_business_spans, tracks["business_speaker"])
        print(json.dumps({
            "business_spans": len(tracks["business_speaker"]),
            "out": os.path.abspath(args.export_business_spans),
        }, ensure_ascii=False))
        return

    required_paths = {
        "--sortformer-frames": args.sortformer_frames,
        "--titanet-turns": args.titanet_turns,
        "--titanet-diar-segments": args.titanet_diar_segments,
        "--out": args.out,
    }
    absent = [name for name, value in required_paths.items() if not value]
    if absent:
        raise ValueError("build mode requires: " + ",".join(absent))

    frames, speaker_columns, frame_period = read_frames(
        args.sortformer_frames)
    frame_times = [row["time_sec"] for row in frames]
    turn_titanet = read_titanet(args.titanet_turns)
    diar_titanet = read_titanet(args.titanet_diar_segments)
    expected_diar_ids = {
        f"diarization:{index}" for index in range(len(diar_entries))
    }
    if set(diar_titanet) != expected_diar_ids:
        missing_ids = sorted(expected_diar_ids - set(diar_titanet))
        extra_ids = sorted(set(diar_titanet) - expected_diar_ids)
        raise ValueError(
            "TitaNet diar evidence ID mismatch: "
            f"missing={missing_ids[:3]} extra={extra_ids[:3]}")
    resolved = candidate_resolved_config(timeline, args.candidate_config)
    onset = float(resolved.get("diarizer", {}).get("onset", 0.45))
    align_by_id = {
        int(entry["text_id"]): entry for entry in tracks["align"]
    }

    turns = []
    for index, entry in enumerate(tracks["business_speaker"]):
        evidence_id = f"business_speaker:{index}"
        if evidence_id not in turn_titanet:
            raise ValueError(f"TitaNet evidence missing {evidence_id}")
        start = float(entry["start"])
        end = float(entry["end"])
        if end <= start:
            raise ValueError(f"invalid business turn {evidence_id}")
        text_id = int(entry.get("text_id", -1))
        turns.append({
            "turn_id": evidence_id,
            "start_sec": start,
            "end_sec": end,
            "duration_sec": end - start,
            "text_id": text_id,
            "text": str(entry.get("text", "")),
            "baseline_output": {
                "local_speaker": int(entry.get("speaker", -1)),
                "speaker_id": entry.get("speaker_id"),
                "speaker_support": entry.get("speaker_support"),
                "speaker_uncertain": bool(
                    entry.get("speaker_uncertain", False)),
            },
            "sortformer": summarize_frames(
                frames, frame_times, frame_period, start, end, onset),
            "diar_segments": diar_segment_evidence(
                diar_entries, diar_titanet, start, end),
            "titanet": turn_titanet[evidence_id],
            "vad": interval_metrics(start, end, tracks["vad"]),
            "align": alignment_evidence(
                align_by_id, text_id, start, end),
        })

    output = {
        "schema_version": 3,
        "kind": "orator_frozen_speaker_evidence",
        "sources": {
            "timeline": {
                "path": os.path.abspath(args.timeline),
                "sha256": sha256_file(args.timeline),
            },
            "sortformer_frames": {
                "path": os.path.abspath(args.sortformer_frames),
                "sha256": sha256_file(args.sortformer_frames),
            },
            "titanet_turns": {
                "path": os.path.abspath(args.titanet_turns),
                "sha256": sha256_file(args.titanet_turns),
            },
            "titanet_diar_segments": {
                "path": os.path.abspath(args.titanet_diar_segments),
                "sha256": sha256_file(args.titanet_diar_segments),
            },
            "diar_segments": (
                {
                    "path": os.path.abspath(args.diar_segments),
                    "sha256": sha256_file(args.diar_segments),
                }
                if args.diar_segments else {
                    "source": "timeline:diarization",
                    "count": len(diar_entries),
                }
            ),
            "candidate_config": (
                {
                    "path": os.path.abspath(args.candidate_config),
                    "sha256": sha256_file(args.candidate_config),
                }
                if args.candidate_config else None
            ),
            "resolved_config_sha256": package.get("meta", {}).get(
                "resolved_config_sha256"),
        },
        "resolved_config": resolved,
        "audio_sec": float(timeline["audio_sec"]),
        "sample_rate": int(timeline["sample_rate"]),
        "sortformer": {
            "frame_period_sec": frame_period,
            "frame_count": len(frames),
            "speaker_columns": speaker_columns,
            "onset": onset,
        },
        "turn_count": len(turns),
        "turns": turns,
    }
    write_json(args.out, output)
    print(json.dumps({
        "turns": len(turns),
        "frames": len(frames),
        "titanet_ok": sum(
            turn["titanet"]["status"] == "ok" for turn in turns),
        "titanet_diar_ok": sum(
            value["status"] == "ok" for value in diar_titanet.values()),
        "out": os.path.abspath(args.out),
    }, ensure_ascii=False))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, csv.Error) as error:
        raise SystemExit(f"speaker frozen evidence: {error}")
