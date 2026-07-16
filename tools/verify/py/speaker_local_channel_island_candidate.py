#!/usr/bin/env python3
"""Build complete local-channel-island evidence without result scoring."""

import argparse
import bisect
import csv
from collections import defaultdict
import json
import os
import tomllib

import speaker_posterior_bounded_phrase_candidate as posterior
import speaker_punctuation_phrase_candidate as phrase_tools
import speaker_relative_top1_phrase_candidate as relative


EPSILON = 1e-9
REQUIRED_POLICY = {
    "enabled",
    "require_vad_continuity",
    "require_same_bracketing_channel",
    "require_session_registry_agreement",
    "require_robust_gallery_agreement",
    "require_raw_local_identity_agreement",
    "require_known_baseline_conflict",
    "project_only_wholly_contained_alignment_units",
}


def read_vad(path):
    return relative.read_vad_timeline(path)


def load_policy(path):
    voiceprint = posterior.load_policy(path)
    with open(path, "rb") as source:
        document = tomllib.load(source)
    section = document.get("local_channel_island", {})
    missing = sorted(REQUIRED_POLICY - set(section))
    if missing:
        raise ValueError("local-channel-island policy missing: " +
                         ",".join(missing))
    policy = {name: bool(section[name]) for name in REQUIRED_POLICY}
    if not all(policy.values()):
        raise ValueError("all local-channel-island safety contracts are mandatory")
    inherited = float(voiceprint["minimum_sustained_run_sec"])
    for name in ("minimum_island_run_sec", "minimum_adjacent_run_sec"):
        if name not in section:
            raise ValueError(f"local-channel-island policy missing: {name}")
        value = float(section[name])
        if abs(value - inherited) > EPSILON:
            raise ValueError(f"local-channel-island {name} differs from FR16J")
        policy[name] = value
    policy["voiceprint"] = voiceprint
    return policy


def raw_runs_for_vad(frames, frame_sec, vad_interval):
    vad_start, vad_end = vad_interval
    times = [item["time"] for item in frames]
    first = bisect.bisect_left(times, vad_start - 0.5 * frame_sec)
    limit = bisect.bisect_left(times, vad_end + 0.5 * frame_sec)
    runs = []
    current = []

    def finish():
        if not current:
            return
        start = max(vad_start, current[0]["time"] - 0.5 * frame_sec)
        end = min(vad_end, current[-1]["time"] + 0.5 * frame_sec)
        if end <= start + EPSILON:
            return
        runs.append({
            "start": start,
            "end": end,
            "local_speaker": current[0]["local"],
            "frame_start": current[0]["frame"],
            "frame_end": current[-1]["frame"] + 1,
            "frame_count": len(current),
            "mean_probability": sum(
                item["probability"] for item in current) / len(current),
            "minimum_probability": min(
                item["probability"] for item in current),
            "maximum_probability": max(
                item["probability"] for item in current),
        })

    for item in frames[first:limit]:
        if not (vad_start - EPSILON <= item["time"] < vad_end - EPSILON):
            continue
        if current and item["local"] != current[-1]["local"]:
            finish()
            current = []
        current.append(item)
    finish()
    return runs


def channel_islands(frames, frame_sec, vad, policy):
    islands = []
    for vad_index, interval in enumerate(vad):
        runs = raw_runs_for_vad(frames, frame_sec, interval)
        for index in range(1, len(runs) - 1):
            left, island, right = runs[index - 1:index + 2]
            if left["local_speaker"] != right["local_speaker"]:
                continue
            if left["local_speaker"] == island["local_speaker"]:
                continue
            if (island["end"] - island["start"] + EPSILON <
                    policy["minimum_island_run_sec"]):
                continue
            if any(run["end"] - run["start"] + EPSILON <
                   policy["minimum_adjacent_run_sec"]
                   for run in (left, right)):
                continue
            islands.append({
                **island,
                "vad_index": vad_index,
                "left_local_speaker": left["local_speaker"],
                "left_start": left["start"],
                "left_end": left["end"],
                "right_local_speaker": right["local_speaker"],
                "right_start": right["start"],
                "right_end": right["end"],
            })
    return islands


def projected_groups(metadata, island):
    output = []
    for text_id_text, asr in metadata["asr"].items():
        text_id = int(text_id_text)
        if (float(asr["end"]) <= island["start"] or
                float(asr["start"]) >= island["end"]):
            continue
        source = str(asr["text"])
        times = phrase_tools.aligned_character_times(
            source, metadata["align"][text_id_text])
        current = []

        def finish():
            if not current:
                return
            aligned = [times[index] for index in current]
            output.append({
                "text_id": text_id,
                "source_start": current[0],
                "source_end": current[-1] + 1,
                "text": source[current[0]:current[-1] + 1],
                "projection_start": min(value["start"] for value in aligned),
                "projection_end": max(value["end"] for value in aligned),
            })

        for index, value in enumerate(times):
            contained = (
                value is not None and value["end"] > value["start"] and
                value["start"] + EPSILON >= island["start"] and
                value["end"] <= island["end"] + EPSILON)
            if not contained:
                finish()
                current = []
                continue
            if current and index != current[-1] + 1:
                finish()
                current = []
            current.append(index)
        finish()
    return output


def enumerate_pieces(metadata, frames, frame_sec, vad, policy):
    pieces = []
    islands = channel_islands(frames, frame_sec, vad, policy)
    for island_index, island in enumerate(islands):
        for group in projected_groups(metadata, island):
            pieces.append({
                "evidence_id": f"local_channel_island:{len(pieces)}",
                "island_id": f"native_island:{island_index}",
                "start": island["start"],
                "end": island["end"],
                **group,
                **{name: value for name, value in island.items()
                   if name not in {"start", "end"}},
            })
    return pieces, islands


def decide_piece(piece, current, robust, fragments, active_ids,
                 local_to_id, policy):
    voiceprint = policy["voiceprint"]
    current_id, current_reason, current_ranked = phrase_tools.select_identity(
        current, active_ids, voiceprint)
    robust_id, robust_reason, robust_ranked = phrase_tools.select_identity(
        robust, active_ids, voiceprint)
    mapped_id = local_to_id.get(int(piece["local_speaker"]))
    conflicts = relative.known_conflicts(fragments, current_id)
    accepted = False
    reason = "local_island_session_registry_abstention"
    if current_id is None:
        reason = "local_island_session_registry_abstention"
    elif robust_id is None:
        reason = "local_island_robust_gallery_abstention"
    elif current_id != robust_id:
        reason = "local_island_registry_disagreement"
    elif current_id != mapped_id:
        reason = "local_island_raw_local_identity_disagreement"
    elif not conflicts:
        reason = "local_island_known_baseline_conflict_missing"
    else:
        accepted = True
        reason = "local_island_dual_voiceprint_consensus"
    return {
        **piece,
        "mapped_speaker_id": mapped_id,
        "session_registry_speaker_id": current_id,
        "session_registry_reason": current_reason,
        "session_registry_ranked": [
            {"speaker_id": identity, "score": score}
            for identity, score in current_ranked
        ],
        "robust_gallery_speaker_id": robust_id,
        "robust_gallery_reason": robust_reason,
        "robust_gallery_ranked": [
            {"speaker_id": identity, "score": score}
            for identity, score in robust_ranked
        ],
        "known_baseline_conflict_ids": conflicts,
        "selected_speaker_id": current_id if accepted else None,
        "accepted": accepted,
        "reason": reason,
    }


def build_candidate(baseline, metadata, current_evidence, robust_evidence,
                    manifest, policy):
    if baseline.get("kind") != "orator_frozen_speaker_candidate":
        raise ValueError("baseline is not a frozen speaker candidate")
    if metadata.get("kind") != "orator_local_channel_island_spans":
        raise ValueError("metadata is not local-channel-island evidence")
    fragments = relative.source_fragments(baseline.get("track", []))
    relative.validate_source_text(fragments, metadata, "baseline")
    active_ids = [str(value) for value in baseline["active_speaker_ids"]]
    local_to_id = {
        int(local): str(identity)
        for local, identity in manifest["mapping"].items()
    }
    if sorted(local_to_id.values()) != sorted(active_ids):
        raise ValueError("manifest identity set differs from baseline")
    id_to_local = {identity: local for local, identity in local_to_id.items()}
    overlays = defaultdict(list)
    decisions = []
    for piece in metadata["pieces"]:
        evidence_id = str(piece["evidence_id"])
        if evidence_id not in current_evidence:
            raise ValueError(f"missing session evidence {evidence_id}")
        if evidence_id not in robust_evidence:
            raise ValueError(f"missing robust evidence {evidence_id}")
        text_id = int(piece["text_id"])
        overlaps = posterior.overlapping_fragments(piece, fragments[text_id])
        decision = decide_piece(
            piece, current_evidence[evidence_id], robust_evidence[evidence_id],
            overlaps, active_ids, local_to_id, policy)
        decisions.append(decision)
        if decision["accepted"]:
            overlays[text_id].append({
                "source_start": int(piece["source_start"]),
                "source_end": int(piece["source_end"]),
                "speaker_id": decision["selected_speaker_id"],
                "reason": decision["reason"],
            })
    track = posterior.project_track(metadata, fragments, overlays, id_to_local)
    relative.validate_source_text(
        relative.source_fragments(track), metadata, "projected")
    return {
        "schema_version": 1,
        "kind": "orator_frozen_speaker_candidate",
        "candidate_kind": "v21_local_channel_island_consensus",
        "audio_sec": float(baseline["audio_sec"]),
        "sample_rate": int(baseline["sample_rate"]),
        "active_speaker_ids": active_ids,
        "source_turn_count": len(baseline.get("track", [])),
        "turn_count": len(track),
        "piece_count": len(decisions),
        "accepted_piece_count": sum(item["accepted"] for item in decisions),
        "policy": {name: value for name, value in policy.items()
                   if name != "voiceprint"},
        "voiceprint_policy": policy["voiceprint"],
        "piece_decisions": decisions,
        "track": track,
    }


def command_spans(args, policy):
    metadata = posterior.load_json(args.alignment)
    frames, frame_sec = posterior.read_frames(args.frames)
    vad = read_vad(args.timeline)
    pieces, islands = enumerate_pieces(
        metadata, frames, frame_sec, vad, policy)
    posterior.write_spans(args.out, pieces)
    result = {
        "schema_version": 1,
        "kind": "orator_local_channel_island_spans",
        "frame_period_sec": frame_sec,
        "policy": {name: value for name, value in policy.items()
                   if name != "voiceprint"},
        "voiceprint_policy": policy["voiceprint"],
        "asr": metadata["asr"],
        "align": metadata["align"],
        "islands": islands,
        "pieces": pieces,
        "island_count": len(islands),
        "piece_count": len(pieces),
        "sources": {
            name: {"path": os.path.abspath(path),
                   "sha256": posterior.sha256_file(path)}
            for name, path in {
                "alignment": args.alignment,
                "frames": args.frames,
                "timeline": args.timeline,
                "config": args.config,
            }.items()
        },
    }
    with open(args.metadata, "w", encoding="utf-8") as output:
        json.dump(result, output, ensure_ascii=False, indent=2)
    print(json.dumps({
        "islands": len(islands),
        "pieces": len(pieces),
        "spans": os.path.abspath(args.out),
        "metadata": os.path.abspath(args.metadata),
    }, ensure_ascii=False))


def command_build(args, policy):
    result = build_candidate(
        posterior.load_json(args.baseline), posterior.load_json(args.metadata),
        phrase_tools.read_titanet(args.session_titanet),
        phrase_tools.read_titanet(args.robust_titanet),
        posterior.load_json(args.manifest), policy)
    result["sources"] = {
        name: {"path": os.path.abspath(path),
               "sha256": posterior.sha256_file(path)}
        for name, path in {
            "baseline": args.baseline,
            "metadata": args.metadata,
            "session_titanet": args.session_titanet,
            "robust_titanet": args.robust_titanet,
            "manifest": args.manifest,
            "config": args.config,
        }.items()
    }
    with open(args.out, "w", encoding="utf-8") as output:
        json.dump(result, output, ensure_ascii=False, indent=2)
    print(json.dumps({
        "pieces": result["piece_count"],
        "accepted_pieces": result["accepted_piece_count"],
        "turns": result["turn_count"],
        "out": os.path.abspath(args.out),
    }, ensure_ascii=False))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    spans = commands.add_parser("spans")
    spans.add_argument("--alignment", required=True)
    spans.add_argument("--frames", required=True)
    spans.add_argument("--timeline", required=True)
    spans.add_argument("--config", required=True)
    spans.add_argument("--out", required=True)
    spans.add_argument("--metadata", required=True)
    build = commands.add_parser("build")
    build.add_argument("--baseline", required=True)
    build.add_argument("--metadata", required=True)
    build.add_argument("--session-titanet", required=True)
    build.add_argument("--robust-titanet", required=True)
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
        raise SystemExit(f"speaker local-channel island candidate: {error}")
