#!/usr/bin/env python3
"""Build a reference-free phrase view bounded by raw Sortformer posteriors."""

import argparse
import bisect
import csv
import hashlib
import json
import os
import tomllib
from collections import defaultdict

import speaker_punctuation_phrase_candidate as phrase_tools


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


def load_policy(path):
    with open(path, "rb") as source:
        document = tomllib.load(source)
    bounded = document.get("posterior_bounded_phrase", {})
    fusion = document.get("speaker_fusion", {})
    bounded_required = {
        "frame_activity_threshold", "minimum_sustained_run_sec",
        "minimum_piece_duration_sec", "regular_piece_min_sec",
        "preserve_regular_direct_anchors",
        "allow_regular_consensus_over_short_anchor",
    }
    fusion_required = {
        "short_max_sec", "short_min_score", "short_min_margin",
        "regular_min_score", "regular_min_margin",
    }
    missing = sorted(
        (bounded_required - set(bounded)) | (fusion_required - set(fusion)))
    if missing:
        raise ValueError("posterior phrase policy missing: " + ",".join(missing))
    policy = {
        **{name: float(bounded[name]) for name in (
            "frame_activity_threshold", "minimum_sustained_run_sec",
            "minimum_piece_duration_sec", "regular_piece_min_sec")},
        **{name: bool(bounded[name]) for name in (
            "preserve_regular_direct_anchors",
            "allow_regular_consensus_over_short_anchor")},
        **{name: float(fusion[name]) for name in fusion_required},
    }
    if not 0.0 < policy["frame_activity_threshold"] < 1.0:
        raise ValueError("frame_activity_threshold must be between zero and one")
    if policy["minimum_sustained_run_sec"] <= 0.0:
        raise ValueError("minimum_sustained_run_sec must be positive")
    if policy["minimum_piece_duration_sec"] <= 0.0:
        raise ValueError("minimum_piece_duration_sec must be positive")
    if policy["regular_piece_min_sec"] < policy["minimum_piece_duration_sec"]:
        raise ValueError("regular_piece_min_sec is below the piece minimum")
    if not policy["preserve_regular_direct_anchors"]:
        raise ValueError("regular direct anchor preservation is mandatory")
    return policy


def read_frames(path):
    frames = []
    with open(path, encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source)
        required = {"frame", "time_sec", "top1", "top1_prob"}
        if not required.issubset(reader.fieldnames or []):
            raise ValueError("raw frame CSV is missing required columns")
        for row in reader:
            frames.append({
                "frame": int(row["frame"]),
                "time": float(row["time_sec"]),
                "local": int(row["top1"]),
                "probability": float(row["top1_prob"]),
            })
    if len(frames) < 2:
        raise ValueError("raw frame CSV requires at least two frames")
    for left, right in zip(frames, frames[1:]):
        if right["frame"] != left["frame"] + 1 or right["time"] <= left["time"]:
            raise ValueError("raw frames are not contiguous and monotonic")
    frame_sec = sum(
        right["time"] - left["time"]
        for left, right in zip(frames, frames[1:])) / (len(frames) - 1)
    return frames, frame_sec


def active_runs(frames, frame_sec, start, end, policy):
    times = [item["time"] for item in frames]
    first = bisect.bisect_left(times, start - 0.5 * frame_sec)
    limit = bisect.bisect_left(times, end + 0.5 * frame_sec)
    runs = []
    current = []

    def finish():
        if not current:
            return
        run_start = max(start, current[0]["time"] - 0.5 * frame_sec)
        run_end = min(end, current[-1]["time"] + 0.5 * frame_sec)
        if run_end - run_start + EPSILON >= policy["minimum_sustained_run_sec"]:
            runs.append({
                "start": run_start,
                "end": run_end,
                "local_speaker": current[0]["local"],
                "frame_start": current[0]["frame"],
                "frame_end": current[-1]["frame"] + 1,
                "frame_count": len(current),
                "mean_probability": sum(
                    item["probability"] for item in current) / len(current),
            })

    for item in frames[first:limit]:
        active = item["probability"] >= policy["frame_activity_threshold"]
        if (not active or
                (current and item["local"] != current[-1]["local"])):
            finish()
            current = []
        if active:
            current.append(item)
    finish()
    return runs


def run_for_time(runs, time_sec):
    for index, run in enumerate(runs):
        if run["start"] - EPSILON <= time_sec < run["end"] + EPSILON:
            return index
    return None


def phrase_pieces(phrase, source, units, runs, policy):
    times = phrase_tools.aligned_character_times(source, units)
    groups = []
    current_run = None
    current_indices = []

    def finish():
        if current_run is None or not current_indices:
            return
        run = runs[current_run]
        aligned = [times[index] for index in current_indices]
        start = max(run["start"], min(item["start"] for item in aligned))
        end = min(run["end"], max(item["end"] for item in aligned))
        if end - start + EPSILON < policy["minimum_piece_duration_sec"]:
            return
        source_end = current_indices[-1] + 1
        while (source_end < phrase["source_end"] and
               times[source_end] is None):
            source_end += 1
        groups.append({
            "source_start": current_indices[0],
            "source_end": source_end,
            "start": start,
            "end": end,
            "local_speaker": run["local_speaker"],
            "frame_start": run["frame_start"],
            "frame_end": run["frame_end"],
            "frame_count": run["frame_count"],
            "mean_probability": run["mean_probability"],
        })

    for index in range(phrase["source_start"], phrase["source_end"]):
        aligned = times[index]
        if aligned is None or aligned["end"] <= aligned["start"]:
            continue
        run_index = run_for_time(
            runs, 0.5 * (aligned["start"] + aligned["end"]))
        if run_index is None:
            finish()
            current_run = None
            current_indices = []
            continue
        if current_run is not None and run_index != current_run:
            finish()
            current_indices = []
        current_run = run_index
        current_indices.append(index)
    finish()
    return groups


def enumerate_pieces(metadata, frames, frame_sec, policy):
    pieces = []
    for phrase in metadata["phrases"]:
        text_id = int(phrase["text_id"])
        source = metadata["asr"][str(text_id)]["text"]
        units = metadata["align"][str(text_id)]
        runs = active_runs(
            frames, frame_sec, float(phrase["start"]), float(phrase["end"]),
            policy)
        for value in phrase_pieces(phrase, source, units, runs, policy):
            pieces.append({
                "evidence_id": f"posterior_phrase:{len(pieces)}",
                "phrase_evidence_id": phrase["evidence_id"],
                "text_id": text_id,
                "text": source[value["source_start"]:value["source_end"]],
                **value,
            })
    return pieces


def write_spans(path, pieces):
    with open(path, "w", encoding="ascii", newline="") as output:
        writer = csv.writer(output, delimiter="\t", lineterminator="\n")
        writer.writerow(["evidence_id", "start_sec", "end_sec"])
        for piece in pieces:
            writer.writerow([piece["evidence_id"], piece["start"], piece["end"]])


def is_regular_anchor(fragment):
    return (fragment["anchor"] and
            "regular_direct_voiceprint" in
            str(fragment["decision"].get("reason", "")))


def overlapping_fragments(piece, fragments):
    return [
        fragment for fragment in fragments
        if fragment["source_start"] < piece["source_end"] and
        fragment["source_end"] > piece["source_start"]
    ]


def accept_piece(piece, selected, mapped_id, fragments, policy):
    if selected is None:
        return False, "piece_voiceprint_ineligible"
    if selected != mapped_id:
        return False, "piece_posterior_voiceprint_conflict"
    regular = [item for item in fragments if is_regular_anchor(item)]
    if any(item["decision"].get("speaker_id") != selected for item in regular):
        return False, "piece_regular_direct_anchor_conflict"
    short = [item for item in fragments if item["anchor"] and item not in regular]
    short_conflict = any(
        item["decision"].get("speaker_id") != selected for item in short)
    regular_piece = (
        float(piece["end"]) - float(piece["start"]) + EPSILON >=
        policy["regular_piece_min_sec"])
    if (short_conflict and
            not (regular_piece and
                 policy["allow_regular_consensus_over_short_anchor"])):
        return False, "piece_short_direct_anchor_conflict"
    return True, "piece_posterior_voiceprint_consensus"


def overlay_ranges(piece, selected, fragments, policy):
    selected_positions = []
    for index in range(piece["source_start"], piece["source_end"]):
        owner = next((
            item for item in fragments
            if item["source_start"] <= index < item["source_end"]), None)
        if owner is not None and is_regular_anchor(owner):
            continue
        if (owner is not None and owner["anchor"] and
                owner["decision"].get("speaker_id") == selected):
            continue
        selected_positions.append(index)
    ranges = []
    for index in selected_positions:
        if ranges and ranges[-1][1] == index:
            ranges[-1] = (ranges[-1][0], index + 1)
        else:
            ranges.append((index, index + 1))
    return ranges


def evaluate_pieces(direct, metadata, evidence, manifest, policy):
    fragments_by_text = phrase_tools.source_fragments(direct)
    active_ids = [str(item) for item in direct["active_speaker_ids"]]
    local_to_id = {
        int(local): str(speaker_id)
        for local, speaker_id in manifest["mapping"].items()
    }
    id_to_local = {value: key for key, value in local_to_id.items()}
    overlays_by_text = defaultdict(list)
    decisions = []

    for piece in metadata["pieces"]:
        text_id = int(piece["text_id"])
        local = int(piece["local_speaker"])
        if local not in local_to_id:
            raise ValueError(f"no identity mapping for local {local}")
        voiceprint = evidence.get(piece["evidence_id"], {})
        selected, voiceprint_reason, ranked = phrase_tools.select_identity(
            voiceprint, active_ids, policy)
        overlaps = overlapping_fragments(piece, fragments_by_text[text_id])
        accepted, reason = accept_piece(
            piece, selected, local_to_id[local], overlaps, policy)
        ranges = []
        if accepted:
            ranges = overlay_ranges(piece, selected, overlaps, policy)
            for source_start, source_end in ranges:
                overlays_by_text[text_id].append({
                    "source_start": source_start,
                    "source_end": source_end,
                    "speaker_id": selected,
                    "reason": reason,
                })
        decisions.append({
            **piece,
            "mapped_speaker_id": local_to_id[local],
            "speaker_id": selected,
            "accepted": accepted,
            "reason": reason if accepted else voiceprint_reason + ":" + reason,
            "overlay_ranges": [list(value) for value in ranges],
            "ranked": [
                {"speaker_id": speaker_id, "score": score}
                for speaker_id, score in ranked
            ],
        })

    return {
        "active_ids": active_ids,
        "local_to_id": local_to_id,
        "id_to_local": id_to_local,
        "fragments_by_text": fragments_by_text,
        "overlays_by_text": overlays_by_text,
        "decisions": decisions,
    }


def project_track(metadata, fragments_by_text, overlays_by_text, id_to_local):
    track = []
    for text_id in sorted(fragments_by_text):
        source = metadata["asr"][str(text_id)]["text"]
        fragments = fragments_by_text[text_id]
        if "".join(str(item["entry"].get("text", ""))
                   for item in fragments) != source:
            raise ValueError(f"direct source text mismatch for {text_id}")
        track.extend(phrase_tools.reproject_text(
            text_id, source, metadata["align"][str(text_id)], fragments,
            overlays_by_text[text_id], id_to_local))
    for text_id in sorted(fragments_by_text):
        expected = metadata["asr"][str(text_id)]["text"]
        actual = "".join(
            item["text"] for item in track if int(item["text_id"]) == text_id)
        if actual != expected:
            raise ValueError(f"reprojected source text mismatch for {text_id}")
    return track


def build_candidate(direct, metadata, evidence, manifest, policy):
    evaluated = evaluate_pieces(
        direct, metadata, evidence, manifest, policy)
    decisions = evaluated["decisions"]
    fragments_by_text = evaluated["fragments_by_text"]
    track = project_track(
        metadata, fragments_by_text, evaluated["overlays_by_text"],
        evaluated["id_to_local"])

    return {
        "schema_version": 1,
        "kind": "orator_frozen_speaker_candidate",
        "candidate_kind": "v21_posterior_bounded_phrase_consensus",
        "audio_sec": float(direct["audio_sec"]),
        "sample_rate": int(direct["sample_rate"]),
        "active_speaker_ids": evaluated["active_ids"],
        "source_turn_count": len(direct.get("track", [])),
        "turn_count": len(track),
        "piece_count": len(metadata["pieces"]),
        "accepted_piece_count": sum(item["accepted"] for item in decisions),
        "policy": policy,
        "decisions": decisions,
        "track": track,
    }


def command_spans(args, policy):
    metadata = load_json(args.phrases)
    frames, frame_sec = read_frames(args.frames)
    pieces = enumerate_pieces(metadata, frames, frame_sec, policy)
    write_spans(args.out, pieces)
    result = {
        "schema_version": 1,
        "kind": "orator_posterior_bounded_phrase_spans",
        "frame_period_sec": frame_sec,
        "policy": policy,
        "asr": metadata["asr"],
        "align": metadata["align"],
        "pieces": pieces,
        "piece_count": len(pieces),
        "sources": {
            "phrases": {"path": os.path.abspath(args.phrases),
                        "sha256": sha256_file(args.phrases)},
            "frames": {"path": os.path.abspath(args.frames),
                       "sha256": sha256_file(args.frames)},
            "config": {"path": os.path.abspath(args.config),
                       "sha256": sha256_file(args.config)},
        },
    }
    with open(args.metadata, "w", encoding="utf-8") as output:
        json.dump(result, output, ensure_ascii=False, indent=2)
    print(json.dumps({
        "pieces": len(pieces), "spans": os.path.abspath(args.out),
        "metadata": os.path.abspath(args.metadata),
    }, ensure_ascii=False))


def command_build(args, policy):
    direct = load_json(args.direct)
    metadata = load_json(args.metadata)
    evidence = phrase_tools.read_titanet(args.titanet)
    manifest = load_json(args.manifest)
    result = build_candidate(direct, metadata, evidence, manifest, policy)
    result["sources"] = {
        name: {"path": os.path.abspath(path), "sha256": sha256_file(path)}
        for name, path in {
            "direct": args.direct, "metadata": args.metadata,
            "titanet": args.titanet, "manifest": args.manifest,
            "config": args.config,
        }.items()
    }
    with open(args.out, "w", encoding="utf-8") as output:
        json.dump(result, output, ensure_ascii=False, indent=2)
    print(json.dumps({
        "pieces": result["piece_count"],
        "accepted_pieces": result["accepted_piece_count"],
        "turns": result["turn_count"], "out": os.path.abspath(args.out),
    }, ensure_ascii=False))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    spans = commands.add_parser("spans")
    spans.add_argument("--phrases", required=True)
    spans.add_argument("--frames", required=True)
    spans.add_argument("--config", required=True)
    spans.add_argument("--out", required=True)
    spans.add_argument("--metadata", required=True)
    build = commands.add_parser("build")
    build.add_argument("--direct", required=True)
    build.add_argument("--metadata", required=True)
    build.add_argument("--titanet", required=True)
    build.add_argument("--manifest", required=True)
    build.add_argument("--config", required=True)
    build.add_argument("--out", required=True)
    args = parser.parse_args()
    policy = load_policy(args.config)
    if args.command == "spans":
        command_spans(args, policy)
    else:
        command_build(args, policy)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, csv.Error) as error:
        raise SystemExit(f"speaker posterior bounded phrase: {error}")
