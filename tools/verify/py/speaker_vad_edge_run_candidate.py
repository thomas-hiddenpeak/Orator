#!/usr/bin/env python3
"""Build padded-VAD edge-run evidence without result scoring."""

import argparse
import csv
import json
import os
import tomllib

import speaker_local_channel_island_candidate as island_tools
import speaker_posterior_bounded_phrase_candidate as posterior
import speaker_punctuation_phrase_candidate as phrase_tools
import speaker_relative_top1_phrase_candidate as relative
import speaker_vad_utterance_candidate as vad_tools


EPSILON = 1e-9
REQUIRED_POLICY = {
    "enabled",
    "require_padded_vad_interval",
    "require_multiple_top1_runs",
    "require_edge_run",
    "require_adjacent_sustained_run",
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
    section = document.get("vad_edge_run", {})
    missing = sorted(REQUIRED_POLICY - set(section))
    if missing:
        raise ValueError("VAD edge-run policy missing: " + ",".join(missing))
    policy = {name: bool(section[name]) for name in REQUIRED_POLICY}
    if not all(policy.values()):
        raise ValueError("all VAD edge-run safety contracts are mandatory")
    if "require_below_activity_threshold" not in section:
        raise ValueError(
            "VAD edge-run policy missing: require_below_activity_threshold")
    policy["require_below_activity_threshold"] = bool(
        section["require_below_activity_threshold"])
    for name in ("minimum_run_sec", "maximum_run_sec"):
        if name not in section:
            raise ValueError(f"VAD edge-run policy missing: {name}")
        policy[name] = float(section[name])
    if abs(policy["minimum_run_sec"] -
           float(voiceprint["minimum_sustained_run_sec"])) > EPSILON:
        raise ValueError("VAD edge-run minimum differs from FR16J")
    speaker = document.get("speaker", {})
    if "max_embed_window_sec" not in speaker:
        raise ValueError("speaker max_embed_window_sec is missing")
    if abs(policy["maximum_run_sec"] -
           float(speaker["max_embed_window_sec"])) > EPSILON:
        raise ValueError("VAD edge-run maximum differs from embedding window")
    if policy["maximum_run_sec"] <= policy["minimum_run_sec"]:
        raise ValueError("VAD edge-run duration bounds are invalid")
    policy["frame_activity_threshold"] = float(
        voiceprint["frame_activity_threshold"])
    policy["voiceprint"] = voiceprint
    return policy


def edge_runs(frames, frame_sec, vad, policy):
    output = []
    minimum = policy["minimum_run_sec"]
    maximum = policy["maximum_run_sec"]
    for vad_index, interval in enumerate(vad):
        runs = island_tools.raw_runs_for_vad(frames, frame_sec, interval)
        if len(runs) < 2:
            continue
        edge_indices = [0, len(runs) - 1]
        for index in edge_indices:
            run = runs[index]
            adjacent = runs[1] if index == 0 else runs[-2]
            duration = run["end"] - run["start"]
            adjacent_duration = adjacent["end"] - adjacent["start"]
            if duration + EPSILON < minimum or duration > maximum + EPSILON:
                continue
            if adjacent_duration + EPSILON < minimum:
                continue
            if (policy["require_below_activity_threshold"] and
                    run["maximum_probability"] + EPSILON >=
                    policy["frame_activity_threshold"]):
                continue
            output.append({
                **run,
                "vad_index": vad_index,
                "edge": "start" if index == 0 else "end",
                "run_index": index,
                "run_count": len(runs),
                "adjacent_start": adjacent["start"],
                "adjacent_end": adjacent["end"],
                "adjacent_local_speaker": adjacent["local_speaker"],
                "adjacent_frame_count": adjacent["frame_count"],
                "adjacent_mean_probability": adjacent["mean_probability"],
                "adjacent_minimum_probability": adjacent["minimum_probability"],
                "adjacent_maximum_probability": adjacent["maximum_probability"],
            })
    return output


def enumerate_pieces(metadata, frames, frame_sec, vad, policy):
    runs = edge_runs(frames, frame_sec, vad, policy)
    pieces = []
    for run_index, run in enumerate(runs):
        run_id = f"vad_edge_run:{run_index}"
        for group in island_tools.projected_groups(metadata, run):
            pieces.append({
                "evidence_id": f"vad_edge_run_piece:{len(pieces)}",
                "edge_run_id": run_id,
                **run,
                **group,
            })
    return pieces, runs


def command_spans(args, policy):
    metadata = posterior.load_json(args.alignment)
    frames, frame_sec = posterior.read_frames(args.frames)
    vad = relative.read_vad_timeline(args.timeline)
    pieces, runs = enumerate_pieces(
        metadata, frames, frame_sec, vad, policy)
    posterior.write_spans(args.out, pieces)
    result = {
        "schema_version": 1,
        "kind": "orator_vad_edge_run_spans",
        "frame_period_sec": frame_sec,
        "policy": {name: value for name, value in policy.items()
                   if name != "voiceprint"},
        "voiceprint_policy": policy["voiceprint"],
        "asr": metadata["asr"],
        "align": metadata["align"],
        "edge_runs": runs,
        "pieces": pieces,
        "edge_run_count": len(runs),
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
        "edge_runs": len(runs),
        "pieces": len(pieces),
        "spans": os.path.abspath(args.out),
        "metadata": os.path.abspath(args.metadata),
    }, ensure_ascii=False))


def command_build(args, policy):
    guarded = policy["require_below_activity_threshold"]
    result = vad_tools.build_candidate(
        posterior.load_json(args.baseline), posterior.load_json(args.metadata),
        phrase_tools.read_titanet(args.session_titanet),
        phrase_tools.read_titanet(args.robust_titanet),
        posterior.load_json(args.manifest), policy,
        expected_metadata_kind="orator_vad_edge_run_spans",
        candidate_kind=("v21_vad_edge_run_low_activity_consensus" if guarded
                        else "v21_vad_edge_run_consensus"),
        reason_prefix=("vad_edge_run_low_activity" if guarded
                       else "vad_edge_run"))
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
        raise SystemExit(f"speaker VAD edge-run candidate: {error}")
