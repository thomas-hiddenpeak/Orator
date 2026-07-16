#!/usr/bin/env python3
"""Export a VAD-contained native top-1 speaker track for C++ replay."""

import argparse
import bisect
import csv
import hashlib
import json
import os
import tomllib


EPSILON = 1e-9
TIME_EPSILON = 1e-6
INHERITED_MINIMUM_PROBABILITY = 0.5
INHERITED_MINIMUM_RUN_SEC = 0.4


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_policy(path):
    with open(path, "rb") as source:
        document = tomllib.load(source)
    section = document.get("speaker_primary_top1", {})
    required = {
        "enabled", "require_vad_support", "minimum_probability",
        "minimum_run_sec",
    }
    missing = sorted(required - set(section))
    if missing:
        raise ValueError("primary top-1 policy missing: " + ",".join(missing))
    policy = {
        "enabled": section["enabled"],
        "require_vad_support": section["require_vad_support"],
        "minimum_probability": float(section["minimum_probability"]),
        "minimum_run_sec": float(section["minimum_run_sec"]),
    }
    if policy["enabled"] is not True:
        raise ValueError("primary top-1 evidence must be enabled")
    if policy["require_vad_support"] is not True:
        raise ValueError("primary top-1 evidence requires VAD support")
    if abs(policy["minimum_probability"] -
           INHERITED_MINIMUM_PROBABILITY) > EPSILON:
        raise ValueError("minimum_probability must preserve inherited 0.5")
    if abs(policy["minimum_run_sec"] -
           INHERITED_MINIMUM_RUN_SEC) > EPSILON:
        raise ValueError("minimum_run_sec must preserve inherited 0.4 s")
    return policy


def read_frames(path):
    frames = []
    with open(path, encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source)
        required = {"frame", "time_sec", "session", "top1", "top1_prob"}
        if not required.issubset(reader.fieldnames or []):
            raise ValueError("native frame CSV is missing required columns")
        for row in reader:
            frames.append({
                "frame": int(row["frame"]),
                "time": float(row["time_sec"]),
                "session": int(row["session"]),
                "local": int(row["top1"]),
                "probability": float(row["top1_prob"]),
            })
    if len(frames) < 2:
        raise ValueError("native frame CSV requires at least two frames")
    intervals = []
    for left, right in zip(frames, frames[1:]):
        if right["frame"] != left["frame"] + 1:
            raise ValueError("native frame indices are not contiguous")
        delta = right["time"] - left["time"]
        if delta <= 0.0:
            raise ValueError("native frame times are not monotonic")
        intervals.append(delta)
    frame_sec = sum(intervals) / len(intervals)
    tolerance = max(EPSILON, frame_sec * 1e-4)
    if any(abs(value - frame_sec) > tolerance for value in intervals):
        raise ValueError("native frame period is not stable")
    return frames, frame_sec


def read_vad(path):
    spans = []
    with open(path, encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source, delimiter="\t")
        required = {"start_sec", "end_sec"}
        if not required.issubset(reader.fieldnames or []):
            raise ValueError("VAD TSV is missing start/end columns")
        for row in reader:
            start = float(row["start_sec"])
            end = float(row["end_sec"])
            if start < 0.0 or end <= start:
                raise ValueError("VAD span has invalid bounds")
            spans.append((start, end))
    if not spans:
        raise ValueError("VAD TSV contains no speech spans")
    for left, right in zip(spans, spans[1:]):
        if right[0] < left[1] - TIME_EPSILON:
            raise ValueError("VAD spans overlap or are not monotonic")
    return spans


def read_mapping(path):
    with open(path, encoding="utf-8") as source:
        document = json.load(source)
    raw = document.get("mapping")
    if not isinstance(raw, dict) or not raw:
        raise ValueError("mapping document has no local-slot mapping")
    mapping = {int(local): str(speaker_id)
               for local, speaker_id in raw.items()}
    if len(set(mapping.values())) != len(mapping):
        raise ValueError("mapping assigns one identity to multiple slots")
    if any(local < 0 or not speaker_id
           for local, speaker_id in mapping.items()):
        raise ValueError("mapping contains an invalid slot or identity")
    return mapping


def primary_runs(frames, frame_sec, vad, mapping, policy):
    times = [item["time"] for item in frames]
    runs = []
    for vad_index, (vad_start, vad_end) in enumerate(vad):
        # Native frame timestamps are interval starts, matching
        # FramesToSegments() in the production postprocessor.
        first = bisect.bisect_left(times, vad_start - TIME_EPSILON)
        limit = bisect.bisect_left(times, vad_end - TIME_EPSILON)
        current = []

        def finish():
            if not current:
                return
            start = max(vad_start, current[0]["time"])
            end = min(vad_end, current[-1]["time"] + frame_sec)
            if end - start + TIME_EPSILON < policy["minimum_run_sec"]:
                return
            local = current[0]["local"]
            if local not in mapping:
                raise ValueError(f"no identity mapping for local {local}")
            runs.append({
                "start": start,
                "end": end,
                "vad_index": vad_index,
                "session": current[0]["session"],
                "local_speaker": local,
                "speaker_id": mapping[local],
                "frame_start": current[0]["frame"],
                "frame_end": current[-1]["frame"] + 1,
                "frame_count": len(current),
                "mean_probability": sum(
                    item["probability"] for item in current) / len(current),
            })

        for item in frames[first:limit]:
            inside_vad = (item["time"] + TIME_EPSILON >= vad_start and
                          item["time"] < vad_end - TIME_EPSILON)
            eligible = (inside_vad and item["probability"] + EPSILON >=
                        policy["minimum_probability"])
            continues = (current and eligible and
                         item["frame"] == current[-1]["frame"] + 1 and
                         item["session"] == current[-1]["session"] and
                         item["local"] == current[-1]["local"])
            if current and not continues:
                finish()
                current = []
            if eligible:
                current.append(item)
        finish()

    for left, right in zip(runs, runs[1:]):
        if right["start"] < left["end"] - TIME_EPSILON:
            raise ValueError("primary runs overlap")
    return runs


def write_runs(path, runs):
    with open(path, "w", encoding="utf-8", newline="") as output:
        writer = csv.writer(output, delimiter="\t", lineterminator="\n")
        writer.writerow([
            "start_sec", "end_sec", "local_speaker", "confidence",
            "speaker_id",
        ])
        for run in runs:
            writer.writerow([
                f'{run["start"]:.6f}', f'{run["end"]:.6f}',
                run["local_speaker"], f'{run["mean_probability"]:.6f}',
                run["speaker_id"],
            ])


def write_embedding_spans(path, runs):
    with open(path, "w", encoding="utf-8", newline="") as output:
        writer = csv.writer(output, delimiter="\t", lineterminator="\n")
        writer.writerow(["evidence_id", "start_sec", "end_sec"])
        for index, run in enumerate(runs):
            writer.writerow([
                f"primary_top1:{index}", f'{run["start"]:.6f}',
                f'{run["end"]:.6f}',
            ])


def export_inputs(frames_path, vad_path, mapping_path, config_path,
                  output_path, manifest_path, embedding_spans_path=None):
    policy = load_policy(config_path)
    frames, frame_sec = read_frames(frames_path)
    vad = read_vad(vad_path)
    mapping = read_mapping(mapping_path)
    runs = primary_runs(frames, frame_sec, vad, mapping, policy)
    parent = os.path.dirname(os.path.abspath(output_path))
    if parent:
        os.makedirs(parent, exist_ok=True)
    write_runs(output_path, runs)
    if embedding_spans_path is not None:
        write_embedding_spans(embedding_spans_path, runs)
    manifest = {
        "schema_version": 1,
        "kind": "orator_primary_top1_replay_inputs",
        "frame_period_sec": frame_sec,
        "policy": policy,
        "mapping": {str(local): speaker_id
                    for local, speaker_id in sorted(mapping.items())},
        "counts": {
            "frames": len(frames),
            "vad_spans": len(vad),
            "primary_runs": len(runs),
        },
        "sources": {
            "frames": {"path": os.path.abspath(frames_path),
                       "sha256": sha256_file(frames_path)},
            "vad": {"path": os.path.abspath(vad_path),
                    "sha256": sha256_file(vad_path)},
            "mapping": {"path": os.path.abspath(mapping_path),
                        "sha256": sha256_file(mapping_path)},
            "config": {"path": os.path.abspath(config_path),
                       "sha256": sha256_file(config_path)},
        },
        "output": {"path": os.path.abspath(output_path),
                   "sha256": sha256_file(output_path)},
    }
    if embedding_spans_path is not None:
        manifest["embedding_spans"] = {
            "path": os.path.abspath(embedding_spans_path),
            "sha256": sha256_file(embedding_spans_path),
        }
    with open(manifest_path, "w", encoding="utf-8") as output:
        json.dump(manifest, output, ensure_ascii=False, indent=2)
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--frames", required=True)
    parser.add_argument("--vad", required=True)
    parser.add_argument("--mapping", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--manifest", required=True)
    parser.add_argument("--embedding-spans")
    args = parser.parse_args()
    manifest = export_inputs(
        args.frames, args.vad, args.mapping, args.config, args.out,
        args.manifest, args.embedding_spans)
    print(json.dumps({
        "counts": manifest["counts"],
        "frame_period_sec": manifest["frame_period_sec"],
        "out": manifest["output"]["path"],
        "manifest": os.path.abspath(args.manifest),
    }, ensure_ascii=False))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, csv.Error) as error:
        raise SystemExit(f"speaker primary top-1 replay inputs: {error}")
