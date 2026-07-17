#!/usr/bin/env python3
"""Build complete short-VAD-utterance evidence without result scoring."""

import argparse
import csv
from collections import defaultdict
import json
import os
import tomllib

import speaker_local_channel_island_candidate as island_tools
import speaker_posterior_bounded_phrase_candidate as posterior
import speaker_punctuation_phrase_candidate as phrase_tools
import speaker_relative_top1_phrase_candidate as relative


EPSILON = 1e-9
REQUIRED_POLICY = {
    "enabled",
    "require_padded_vad_interval",
    "require_single_top1_channel",
    "require_session_registry_agreement",
    "require_robust_gallery_agreement",
    "require_raw_local_identity_agreement",
    "require_known_baseline_conflict",
    "project_only_wholly_contained_alignment_units",
}


def load_policy(path):
    voiceprint = posterior.load_policy(path)
    with open(path, "rb") as source:
        document = tomllib.load(source)
    section = document.get("vad_utterance", {})
    missing = sorted(REQUIRED_POLICY - set(section))
    if missing:
        raise ValueError("VAD-utterance policy missing: " + ",".join(missing))
    policy = {name: bool(section[name]) for name in REQUIRED_POLICY}
    if not all(policy.values()):
        raise ValueError("all VAD-utterance safety contracts are mandatory")

    for name in ("minimum_duration_sec", "maximum_duration_sec"):
        if name not in section:
            raise ValueError(f"VAD-utterance policy missing: {name}")
        policy[name] = float(section[name])
    if abs(policy["minimum_duration_sec"] -
           float(voiceprint["minimum_sustained_run_sec"])) > EPSILON:
        raise ValueError("VAD-utterance minimum differs from FR16J")
    if abs(policy["maximum_duration_sec"] -
           float(voiceprint["short_max_sec"])) > EPSILON:
        raise ValueError("VAD-utterance maximum differs from TitaNet short class")
    if policy["maximum_duration_sec"] <= policy["minimum_duration_sec"]:
        raise ValueError("VAD-utterance duration bounds are invalid")
    policy["voiceprint"] = voiceprint
    return policy


def frames_in_interval(frames, interval):
    start, end = interval
    return [item for item in frames
            if start - EPSILON <= item["time"] < end - EPSILON]


def enumerate_pieces(metadata, frames, vad, policy):
    utterances = []
    pieces = []
    minimum = policy["minimum_duration_sec"]
    maximum = policy["maximum_duration_sec"]
    for vad_index, interval in enumerate(vad):
        start, end = interval
        duration = end - start
        if duration + EPSILON < minimum or duration >= maximum - EPSILON:
            continue
        contained_frames = frames_in_interval(frames, interval)
        channels = {int(item["local"]) for item in contained_frames}
        if not contained_frames or len(channels) != 1:
            continue
        local_speaker = next(iter(channels))
        utterance = {
            "utterance_id": f"vad_utterance:{len(utterances)}",
            "vad_index": vad_index,
            "start": start,
            "end": end,
            "duration_sec": duration,
            "local_speaker": local_speaker,
            "frame_start": int(contained_frames[0]["frame"]),
            "frame_end": int(contained_frames[-1]["frame"]) + 1,
            "frame_count": len(contained_frames),
            "mean_probability": sum(
                item["probability"] for item in contained_frames) /
                len(contained_frames),
            "minimum_probability": min(
                item["probability"] for item in contained_frames),
            "maximum_probability": max(
                item["probability"] for item in contained_frames),
        }
        utterances.append(utterance)
        for group in island_tools.projected_groups(metadata, utterance):
            pieces.append({
                "evidence_id": f"vad_utterance_piece:{len(pieces)}",
                **utterance,
                **group,
            })
    return pieces, utterances


def decide_piece(piece, current, robust, fragments, active_ids,
                 local_to_id, policy, reason_prefix="vad_utterance"):
    voiceprint = policy["voiceprint"]
    current_id, current_reason, current_ranked = phrase_tools.select_identity(
        current, active_ids, voiceprint)
    robust_id, robust_reason, robust_ranked = phrase_tools.select_identity(
        robust, active_ids, voiceprint)
    mapped_id = local_to_id.get(int(piece["local_speaker"]))
    conflicts = relative.known_conflicts(fragments, current_id)
    accepted = False
    reason = reason_prefix + "_session_registry_abstention"
    if current_id is None:
        reason = reason_prefix + "_session_registry_abstention"
    elif robust_id is None:
        reason = reason_prefix + "_robust_gallery_abstention"
    elif current_id != robust_id:
        reason = reason_prefix + "_registry_disagreement"
    elif current_id != mapped_id:
        reason = reason_prefix + "_raw_local_identity_disagreement"
    elif not conflicts:
        reason = reason_prefix + "_known_baseline_conflict_missing"
    else:
        accepted = True
        reason = reason_prefix + "_dual_voiceprint_consensus"
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
                    manifest, policy, *,
                    expected_metadata_kind="orator_vad_utterance_spans",
                    candidate_kind="v21_vad_utterance_consensus",
                    reason_prefix="vad_utterance",
                    decision_function=decide_piece):
    if baseline.get("kind") != "orator_frozen_speaker_candidate":
        raise ValueError("baseline is not a frozen speaker candidate")
    if metadata.get("kind") != expected_metadata_kind:
        raise ValueError("metadata kind differs from candidate contract")
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
        decision = decision_function(
            piece, current_evidence[evidence_id], robust_evidence[evidence_id],
            overlaps, active_ids, local_to_id, policy, reason_prefix)
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
        "candidate_kind": candidate_kind,
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
    vad = relative.read_vad_timeline(args.timeline)
    pieces, utterances = enumerate_pieces(metadata, frames, vad, policy)
    posterior.write_spans(args.out, pieces)
    result = {
        "schema_version": 1,
        "kind": "orator_vad_utterance_spans",
        "frame_period_sec": frame_sec,
        "policy": {name: value for name, value in policy.items()
                   if name != "voiceprint"},
        "voiceprint_policy": policy["voiceprint"],
        "asr": metadata["asr"],
        "align": metadata["align"],
        "utterances": utterances,
        "pieces": pieces,
        "utterance_count": len(utterances),
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
        "utterances": len(utterances),
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
        raise SystemExit(f"speaker VAD-utterance candidate: {error}")
