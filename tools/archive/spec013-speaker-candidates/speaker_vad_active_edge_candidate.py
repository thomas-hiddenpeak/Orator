#!/usr/bin/env python3
"""Build active padded-VAD edge-handoff evidence without result scoring."""

import argparse
import csv
import json
import os
import tomllib

import speaker_posterior_bounded_phrase_candidate as posterior
import speaker_punctuation_phrase_candidate as phrase_tools
import speaker_relative_top1_phrase_candidate as relative
import speaker_vad_edge_run_candidate as edge_tools
import speaker_vad_utterance_candidate as vad_tools


EPSILON = 1e-9
REQUIRED_POLICY = {
    "enabled",
    "require_padded_vad_interval",
    "require_multiple_top1_runs",
    "require_edge_run",
    "require_adjacent_sustained_run",
    "require_all_edge_frames_active",
    "require_all_adjacent_frames_active",
    "require_raw_local_identity",
    "veto_agreed_different_dual_voiceprint",
    "require_known_baseline_conflict",
    "project_only_wholly_contained_alignment_units",
}


def load_policy(path):
    voiceprint = posterior.load_policy(path)
    with open(path, "rb") as source:
        document = tomllib.load(source)
    section = document.get("vad_active_edge", {})
    missing = sorted(REQUIRED_POLICY - set(section))
    if missing:
        raise ValueError("VAD active-edge policy missing: " + ",".join(missing))
    policy = {name: bool(section[name]) for name in REQUIRED_POLICY}
    if not all(policy.values()):
        raise ValueError("all VAD active-edge contracts are mandatory")
    for name in ("minimum_run_sec", "maximum_run_sec",
                 "frame_activity_threshold"):
        if name not in section:
            raise ValueError(f"VAD active-edge policy missing: {name}")
        policy[name] = float(section[name])
    if abs(policy["minimum_run_sec"] -
           float(voiceprint["minimum_sustained_run_sec"])) > EPSILON:
        raise ValueError("VAD active-edge minimum differs from FR16J")
    if abs(policy["frame_activity_threshold"] -
           float(voiceprint["frame_activity_threshold"])) > EPSILON:
        raise ValueError("VAD active-edge activity differs from FR16J")
    speaker = document.get("speaker", {})
    if "max_embed_window_sec" not in speaker:
        raise ValueError("speaker max_embed_window_sec is missing")
    if abs(policy["maximum_run_sec"] -
           float(speaker["max_embed_window_sec"])) > EPSILON:
        raise ValueError("VAD active-edge maximum differs from embedding window")
    policy["voiceprint"] = voiceprint
    return policy


def active_edge_runs(frames, frame_sec, vad, policy):
    base_policy = {
        **policy,
        "require_below_activity_threshold": False,
    }
    candidates = edge_tools.edge_runs(frames, frame_sec, vad, base_policy)
    threshold = policy["frame_activity_threshold"]
    return [run for run in candidates
            if run["minimum_probability"] + EPSILON >= threshold and
            run["adjacent_minimum_probability"] + EPSILON >= threshold]


def enumerate_pieces(metadata, frames, frame_sec, vad, policy):
    runs = active_edge_runs(frames, frame_sec, vad, policy)
    pieces = []
    for run_index, run in enumerate(runs):
        run_id = f"vad_active_edge:{run_index}"
        for group in edge_tools.island_tools.projected_groups(metadata, run):
            pieces.append({
                "evidence_id": f"vad_active_edge_piece:{len(pieces)}",
                "active_edge_run_id": run_id,
                **run,
                **group,
            })
    return pieces, runs


def decide_piece(piece, current, robust, fragments, active_ids,
                 local_to_id, policy, reason_prefix):
    voiceprint = policy["voiceprint"]
    current_id, current_reason, current_ranked = phrase_tools.select_identity(
        current, active_ids, voiceprint)
    robust_id, robust_reason, robust_ranked = phrase_tools.select_identity(
        robust, active_ids, voiceprint)
    mapped_id = local_to_id.get(int(piece["local_speaker"]))
    conflicts = relative.known_conflicts(fragments, mapped_id)
    dual_conflict = (
        current_id is not None and robust_id is not None and
        current_id == robust_id and current_id != mapped_id)
    accepted = False
    reason = reason_prefix + "_local_mapping_missing"
    if mapped_id not in active_ids:
        reason = reason_prefix + "_local_mapping_missing"
    elif dual_conflict:
        reason = reason_prefix + "_agreed_different_voiceprint_veto"
    elif not conflicts:
        reason = reason_prefix + "_known_baseline_conflict_missing"
    else:
        accepted = True
        reason = reason_prefix + "_sortformer_handoff"
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
        "agreed_different_voiceprint_veto": dual_conflict,
        "known_baseline_conflict_ids": conflicts,
        "selected_speaker_id": mapped_id if accepted else None,
        "accepted": accepted,
        "reason": reason,
    }


def command_spans(args, policy):
    metadata = posterior.load_json(args.alignment)
    frames, frame_sec = posterior.read_frames(args.frames)
    vad = relative.read_vad_timeline(args.timeline)
    pieces, runs = enumerate_pieces(
        metadata, frames, frame_sec, vad, policy)
    posterior.write_spans(args.out, pieces)
    result = {
        "schema_version": 1,
        "kind": "orator_vad_active_edge_spans",
        "frame_period_sec": frame_sec,
        "policy": {name: value for name, value in policy.items()
                   if name != "voiceprint"},
        "voiceprint_policy": policy["voiceprint"],
        "asr": metadata["asr"],
        "align": metadata["align"],
        "active_edge_runs": runs,
        "pieces": pieces,
        "active_edge_run_count": len(runs),
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
        "active_edge_runs": len(runs),
        "pieces": len(pieces),
        "spans": os.path.abspath(args.out),
        "metadata": os.path.abspath(args.metadata),
    }, ensure_ascii=False))


def command_build(args, policy):
    result = vad_tools.build_candidate(
        posterior.load_json(args.baseline), posterior.load_json(args.metadata),
        phrase_tools.read_titanet(args.session_titanet),
        phrase_tools.read_titanet(args.robust_titanet),
        posterior.load_json(args.manifest), policy,
        expected_metadata_kind="orator_vad_active_edge_spans",
        candidate_kind="v21_vad_active_edge_handoff",
        reason_prefix="vad_active_edge",
        decision_function=decide_piece)
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
        raise SystemExit(f"speaker VAD active-edge candidate: {error}")
