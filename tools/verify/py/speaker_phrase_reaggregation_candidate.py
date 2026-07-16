#!/usr/bin/env python3
"""Bridge low-evidence fragments between aligned voiceprint anchors.

The generator is reference-free. It reads a frozen direct-voiceprint candidate,
forced-alignment units, an active-identity mapping, and a frozen TOML policy.
It never reads reference text, assigns correctness, or evaluates a candidate.
"""

import argparse
import csv
import hashlib
import json
import os
import tomllib
from collections import defaultdict


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
    section = document.get("speaker_phrase_fusion", {})
    required = {
        "max_alignment_gap_sec",
        "max_bridge_sec",
        "require_two_sided_agreement",
    }
    missing = sorted(required - set(section))
    if missing:
        raise ValueError("speaker_phrase_fusion missing: " + ",".join(missing))
    policy = {
        "max_alignment_gap_sec": float(section["max_alignment_gap_sec"]),
        "max_bridge_sec": float(section["max_bridge_sec"]),
        "require_two_sided_agreement": bool(
            section["require_two_sided_agreement"]),
    }
    if policy["max_alignment_gap_sec"] < 0.0:
        raise ValueError("max_alignment_gap_sec must be non-negative")
    if policy["max_bridge_sec"] <= 0.0:
        raise ValueError("max_bridge_sec must be positive")
    if not policy["require_two_sided_agreement"]:
        raise ValueError("two-sided agreement is mandatory")
    return policy


def read_alignment_units(path):
    grouped = defaultdict(list)
    with open(path, encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source, delimiter="\t")
        required = {"text_id", "unit_start_sec", "unit_end_sec"}
        if not required.issubset(reader.fieldnames or []):
            raise ValueError("alignment TSV is missing required columns")
        for row in reader:
            text_id = int(row["text_id"])
            start = float(row["unit_start_sec"])
            end = float(row["unit_end_sec"])
            if start < 0.0 or end + EPSILON < start:
                raise ValueError("invalid alignment unit")
            grouped[text_id].append((start, end))
    return grouped


def build_alignment_runs(units_by_text, max_gap_sec):
    runs = {}
    for text_id, units in units_by_text.items():
        ordered = sorted(units)
        current = []
        text_runs = []
        for unit in ordered:
            if (current and
                    unit[0] - current[-1][1] > max_gap_sec + EPSILON):
                text_runs.append((current[0][0], current[-1][1]))
                current = []
            current.append(unit)
        if current:
            text_runs.append((current[0][0], current[-1][1]))
        runs[text_id] = text_runs
    return runs


def interval_overlap(start, end, other_start, other_end):
    return max(0.0, min(end, other_end) - max(start, other_start))


def alignment_run_index(entry, runs_by_text):
    text_id = int(entry.get("text_id", -1))
    values = runs_by_text.get(text_id, [])
    start = float(entry["start"])
    end = float(entry["end"])
    midpoint = 0.5 * (start + end)
    best_index = None
    best_overlap = 0.0
    for index, (run_start, run_end) in enumerate(values):
        overlap = interval_overlap(start, end, run_start, run_end)
        if overlap > best_overlap + EPSILON:
            best_overlap = overlap
            best_index = index
        elif (best_index is None and
              run_start - EPSILON <= midpoint <= run_end + EPSILON):
            best_index = index
    return best_index


def is_direct_anchor(decision):
    reason = str(decision.get("reason", ""))
    return (decision.get("speaker_id") is not None and
            "direct_voiceprint" in reason and
            "below_gate" not in reason)


def consecutive_groups(track, runs_by_text):
    groups = []
    current_key = None
    for index, entry in enumerate(track):
        key = (int(entry.get("text_id", -1)),
               alignment_run_index(entry, runs_by_text))
        if key[1] is None:
            key = (key[0], f"unaligned:{index}")
        if key != current_key:
            groups.append([])
            current_key = key
        groups[-1].append(index)
    return groups


def bridge_has_legal_gaps(indices, track, max_gap_sec):
    for left, right in zip(indices, indices[1:]):
        gap = float(track[right]["start"]) - float(track[left]["end"])
        if gap > max_gap_sec + EPSILON:
            return False
    return True


def find_bridges(track, decisions, runs_by_text, policy):
    bridges = []
    for group in consecutive_groups(track, runs_by_text):
        anchors = [index for index in group if is_direct_anchor(decisions[index])]
        for left, right in zip(anchors, anchors[1:]):
            if right <= left + 1:
                continue
            left_id = decisions[left]["speaker_id"]
            right_id = decisions[right]["speaker_id"]
            if left_id != right_id:
                continue
            middle = list(range(left + 1, right))
            bridge_start = float(track[middle[0]]["start"])
            bridge_end = float(track[middle[-1]]["end"])
            if bridge_end - bridge_start > policy["max_bridge_sec"] + EPSILON:
                continue
            if not bridge_has_legal_gaps(
                    [left, *middle, right], track,
                    policy["max_alignment_gap_sec"]):
                continue
            bridges.append({
                "indices": middle,
                "speaker_id": left_id,
                "left_anchor": left,
                "right_anchor": right,
                "start": bridge_start,
                "end": bridge_end,
            })
    return bridges


def build_candidate(direct, alignment_units, manifest, policy):
    if direct.get("kind") != "orator_frozen_speaker_candidate":
        raise ValueError("input is not a frozen speaker candidate")
    track = [dict(item) for item in direct.get("track", [])]
    decisions = [dict(item) for item in direct.get("decisions", [])]
    if len(track) != len(decisions):
        raise ValueError("direct candidate track and decision counts differ")
    mapping = {int(local): speaker_id
               for local, speaker_id in manifest["mapping"].items()}
    id_to_local = {speaker_id: local for local, speaker_id in mapping.items()}
    active_ids = sorted(set(mapping.values()))
    if sorted(direct.get("active_speaker_ids", [])) != active_ids:
        raise ValueError("active identities differ from mapping")

    runs = build_alignment_runs(
        alignment_units, policy["max_alignment_gap_sec"])
    bridges = find_bridges(track, decisions, runs, policy)
    changed_indices = set()
    bridge_audit = []
    for bridge_index, bridge in enumerate(bridges):
        selected = bridge["speaker_id"]
        for index in bridge["indices"]:
            previous_id = track[index].get("speaker_id")
            changed = previous_id != selected
            track[index].update({
                "speaker": id_to_local[selected],
                "speaker_id": selected,
                "speaker_uncertain": False,
                "decision_reason": "aligned_phrase_two_sided_voiceprint_bridge",
            })
            decisions[index].update({
                "baseline_speaker_id": previous_id,
                "speaker_id": selected,
                "changed": changed,
                "reason": "aligned_phrase_two_sided_voiceprint_bridge",
                "phrase_bridge": bridge_index,
                "anchor_turn_ids": [
                    track[bridge["left_anchor"]].get("turn_id"),
                    track[bridge["right_anchor"]].get("turn_id"),
                ],
            })
            if changed:
                changed_indices.add(index)
        bridge_audit.append({
            "bridge_id": bridge_index,
            "start": bridge["start"],
            "end": bridge["end"],
            "speaker_id": selected,
            "entry_indices": bridge["indices"],
            "left_anchor_index": bridge["left_anchor"],
            "right_anchor_index": bridge["right_anchor"],
        })

    return {
        "schema_version": 1,
        "kind": "orator_frozen_speaker_candidate",
        "candidate_kind": "v21_aligned_phrase_voiceprint_bridge",
        "audio_sec": float(direct["audio_sec"]),
        "sample_rate": int(direct["sample_rate"]),
        "active_speaker_ids": active_ids,
        "source_turn_count": len(track),
        "turn_count": len(track),
        "bridge_count": len(bridges),
        "changed_entry_count": len(changed_indices),
        "policy": policy,
        "bridges": bridge_audit,
        "decisions": decisions,
        "track": track,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--direct", required=True)
    parser.add_argument("--align", required=True)
    parser.add_argument("--manifest", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()

    direct = load_json(args.direct)
    manifest = load_json(args.manifest)
    units = read_alignment_units(args.align)
    policy = load_policy(args.config)
    result = build_candidate(direct, units, manifest, policy)
    result["sources"] = {
        "direct": {"path": os.path.abspath(args.direct),
                   "sha256": sha256_file(args.direct)},
        "align": {"path": os.path.abspath(args.align),
                  "sha256": sha256_file(args.align)},
        "manifest": {"path": os.path.abspath(args.manifest),
                     "sha256": sha256_file(args.manifest)},
        "config": {"path": os.path.abspath(args.config),
                   "sha256": sha256_file(args.config)},
    }
    with open(args.out, "w", encoding="utf-8") as output:
        json.dump(result, output, ensure_ascii=False, indent=2)
    print(json.dumps({
        "turns": result["turn_count"],
        "bridges": result["bridge_count"],
        "changed_entries": result["changed_entry_count"],
        "out": os.path.abspath(args.out),
    }, ensure_ascii=False))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, csv.Error) as error:
        raise SystemExit(f"speaker phrase reaggregation candidate: {error}")
