#!/usr/bin/env python3
"""Build complete phrases supported by sustained secondary raw channels."""

import argparse
import bisect
import csv
import json
import os
import tomllib

import speaker_posterior_bounded_phrase_candidate as posterior
import speaker_punctuation_phrase_candidate as phrase_tools
import speaker_relative_top1_phrase_candidate as relative
import speaker_vad_utterance_candidate as vad_tools


EPSILON = 1e-9
REQUIRED_POLICY = {
    "enabled",
    "require_complete_punctuation_phrase",
    "require_uniform_known_baseline_identity",
    "require_session_and_robust_same_top_rank",
    "require_existing_duration_margin",
    "ignore_score_gate_only_with_raw_channel_support",
    "require_voiceprint_conflict_with_baseline",
    "require_non_top1_selected_channel",
    "require_selected_channel_sustained_activity",
    "project_complete_phrase",
}


def load_policy(path):
    phrase = phrase_tools.load_policy(path)
    bounded = posterior.load_policy(path)
    with open(path, "rb") as source:
        document = tomllib.load(source)
    section = document.get("secondary_channel_phrase", {})
    missing = sorted(REQUIRED_POLICY - set(section))
    if missing:
        raise ValueError(
            "secondary-channel policy missing: " + ",".join(missing))
    contracts = {name: bool(section[name]) for name in REQUIRED_POLICY}
    if not all(contracts.values()):
        raise ValueError("all secondary-channel contracts are mandatory")
    for name in ("frame_activity_threshold",
                 "minimum_selected_channel_sec"):
        if name not in section:
            raise ValueError(f"secondary-channel policy missing: {name}")
        contracts[name] = float(section[name])
    if abs(contracts["frame_activity_threshold"] -
           bounded["frame_activity_threshold"]) > EPSILON:
        raise ValueError("secondary-channel activity differs from FR16J")
    if abs(contracts["minimum_selected_channel_sec"] -
           bounded["minimum_sustained_run_sec"]) > EPSILON:
        raise ValueError("secondary-channel duration differs from FR16J")
    contracts["minimum_duration_sec"] = phrase["minimum_duration_sec"]
    contracts["maximum_duration_sec"] = phrase["maximum_duration_sec"]
    contracts["voiceprint"] = {
        name: phrase[name] for name in (
            "short_max_sec", "short_min_score", "short_min_margin",
            "regular_min_score", "regular_min_margin")
    }
    return contracts


def read_frames(path):
    frames = []
    with open(path, encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source)
        fields = set(reader.fieldnames or [])
        channels = sorted(
            int(name[3:]) for name in fields
            if name.startswith("spk") and name[3:].isdigit())
        required = {"frame", "time_sec", "top1"}
        if not required.issubset(fields) or not channels:
            raise ValueError("raw frame CSV is missing channel fields")
        for row in reader:
            frames.append({
                "frame": int(row["frame"]),
                "time": float(row["time_sec"]),
                "top1": int(row["top1"]),
                "probabilities": {
                    channel: float(row[f"spk{channel}"])
                    for channel in channels
                },
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


def phrase_frames(frames, frame_sec, start, end):
    times = [item["time"] for item in frames]
    first = bisect.bisect_left(times, start - 0.5 * frame_sec)
    limit = bisect.bisect_left(times, end + 0.5 * frame_sec)
    return [item for item in frames[first:limit]
            if item["time"] + 0.5 * frame_sec > start + EPSILON and
            item["time"] - 0.5 * frame_sec < end - EPSILON]


def channel_support(values, frame_sec, start, end, threshold):
    channels = sorted(values[0]["probabilities"]) if values else []
    support = {}
    non_top1 = {}
    for channel in channels:
        maximum = 0.0
        current_start = None
        current_end = None
        secondary_count = 0
        for item in values:
            frame_start = max(start, item["time"] - 0.5 * frame_sec)
            frame_end = min(end, item["time"] + 0.5 * frame_sec)
            active = (frame_end > frame_start + EPSILON and
                      item["probabilities"][channel] + EPSILON >= threshold)
            if active:
                if current_start is None:
                    current_start = frame_start
                current_end = frame_end
                if item["top1"] != channel:
                    secondary_count += 1
            elif current_start is not None:
                maximum = max(maximum, current_end - current_start)
                current_start = None
                current_end = None
        if current_start is not None:
            maximum = max(maximum, current_end - current_start)
        support[str(channel)] = maximum
        non_top1[str(channel)] = secondary_count
    return support, non_top1


def uniform_known_identity(piece, fragments):
    overlaps = posterior.overlapping_fragments(piece, fragments)
    identities = [item["entry"].get("speaker_id") for item in overlaps]
    known = {str(value) for value in identities if value is not None}
    if (not overlaps or any(value is None for value in identities) or
            len(known) != 1):
        return None
    return next(iter(known))


def enumerate_pieces(metadata, frames, frame_sec, baseline, policy):
    if metadata.get("kind") != "orator_punctuation_phrase_spans":
        raise ValueError("alignment is not punctuation-phrase metadata")
    fragments = relative.source_fragments(baseline.get("track", []))
    relative.validate_source_text(fragments, metadata, "baseline")
    pieces = []
    for phrase in metadata.get("phrases", []):
        duration = float(phrase["end"]) - float(phrase["start"])
        if (duration + EPSILON < policy["minimum_duration_sec"] or
                duration > policy["maximum_duration_sec"] + EPSILON):
            continue
        values = phrase_frames(
            frames, frame_sec, float(phrase["start"]), float(phrase["end"]))
        if not values:
            continue
        support, non_top1 = channel_support(
            values, frame_sec, float(phrase["start"]), float(phrase["end"]),
            policy["frame_activity_threshold"])
        piece = {
            **phrase,
            "duration_sec": duration,
            "query_frame_count": len(values),
            "raw_channel_sustained_sec": support,
            "raw_channel_non_top1_frame_count": non_top1,
        }
        baseline_id = uniform_known_identity(
            piece, fragments[int(piece["text_id"])])
        if baseline_id is None:
            continue
        piece["baseline_speaker_id"] = baseline_id
        pieces.append(piece)
    return pieces


def select_margin_rank(evidence, active_ids, policy):
    if evidence.get("status") != "ok":
        return None, "embedding_unavailable", []
    ranked = sorted(
        ((identity, float(evidence["scores"][identity]))
         for identity in active_ids if identity in evidence.get("scores", {})),
        key=lambda item: (-item[1], item[0]))
    if len(ranked) < 2:
        return None, "active_scores_incomplete", ranked
    duration = float(evidence["duration_sec"])
    if duration < policy["short_max_sec"]:
        margin = policy["short_min_margin"]
        kind = "short"
    else:
        margin = policy["regular_min_margin"]
        kind = "regular"
    if ranked[0][1] - ranked[1][1] + EPSILON < margin:
        return None, f"{kind}_margin_below_gate", ranked
    return ranked[0][0], f"{kind}_margin_rank", ranked


def decide_piece(piece, current, robust, fragments, active_ids, local_to_id,
                 policy, reason_prefix="secondary_channel_phrase"):
    current_id, current_reason, current_ranked = select_margin_rank(
        current, active_ids, policy["voiceprint"])
    robust_id, robust_reason, robust_ranked = select_margin_rank(
        robust, active_ids, policy["voiceprint"])
    identities = [item["entry"].get("speaker_id") for item in fragments]
    known = sorted({str(value) for value in identities if value is not None})
    uniform = bool(fragments) and all(value is not None for value in identities)
    uniform = uniform and len(known) == 1
    id_to_local = {identity: local for local, identity in local_to_id.items()}

    accepted = False
    reason = reason_prefix + "_baseline_not_uniform_known"
    if not uniform:
        reason = reason_prefix + "_baseline_not_uniform_known"
    elif current_id is None:
        reason = reason_prefix + "_session_margin_abstention"
    elif robust_id is None:
        reason = reason_prefix + "_robust_margin_abstention"
    elif current_id != robust_id:
        reason = reason_prefix + "_registry_rank_disagreement"
    elif current_id == known[0]:
        reason = reason_prefix + "_baseline_conflict_missing"
    elif current_id not in id_to_local:
        reason = reason_prefix + "_local_mapping_missing"
    else:
        local = str(id_to_local[current_id])
        support = float(piece["raw_channel_sustained_sec"].get(local, 0.0))
        secondary = int(piece[
            "raw_channel_non_top1_frame_count"].get(local, 0))
        if support + EPSILON < policy["minimum_selected_channel_sec"]:
            reason = reason_prefix + "_sustained_channel_missing"
        elif secondary < 1:
            reason = reason_prefix + "_secondary_channel_missing"
        else:
            accepted = True
            reason = reason_prefix + "_three_view_consensus"
    return {
        **piece,
        "baseline_speaker_ids": known,
        "uniform_known_baseline": uniform,
        "session_registry_speaker_id": current_id,
        "session_registry_reason": current_reason,
        "session_registry_ranked": current_ranked,
        "robust_gallery_speaker_id": robust_id,
        "robust_gallery_reason": robust_reason,
        "robust_gallery_ranked": robust_ranked,
        "selected_speaker_id": current_id if accepted else None,
        "accepted": accepted,
        "reason": reason,
    }


def command_spans(args, policy):
    metadata = posterior.load_json(args.alignment)
    frames, frame_sec = read_frames(args.frames)
    baseline = posterior.load_json(args.baseline)
    pieces = enumerate_pieces(metadata, frames, frame_sec, baseline, policy)
    posterior.write_spans(args.out, pieces)
    result = {
        "schema_version": 1,
        "kind": "orator_secondary_channel_phrase_spans",
        "frame_period_sec": frame_sec,
        "policy": {name: value for name, value in policy.items()
                   if name != "voiceprint"},
        "voiceprint_policy": policy["voiceprint"],
        "asr": metadata["asr"],
        "align": metadata["align"],
        "pieces": pieces,
        "piece_count": len(pieces),
        "sources": {
            name: {"path": os.path.abspath(path),
                   "sha256": posterior.sha256_file(path)}
            for name, path in {
                "alignment": args.alignment,
                "frames": args.frames,
                "baseline": args.baseline,
                "config": args.config,
            }.items()
        },
    }
    with open(args.metadata, "w", encoding="utf-8") as output:
        json.dump(result, output, ensure_ascii=False, indent=2)
    print(json.dumps({
        "pieces": len(pieces), "spans": os.path.abspath(args.out),
        "metadata": os.path.abspath(args.metadata)}, ensure_ascii=False))


def command_build(args, policy):
    result = vad_tools.build_candidate(
        posterior.load_json(args.baseline), posterior.load_json(args.metadata),
        phrase_tools.read_titanet(args.session_titanet),
        phrase_tools.read_titanet(args.robust_titanet),
        posterior.load_json(args.manifest), policy,
        expected_metadata_kind="orator_secondary_channel_phrase_spans",
        candidate_kind="v21_secondary_channel_phrase_consensus",
        reason_prefix="secondary_channel_phrase",
        decision_function=decide_piece)
    result["sources"] = {
        name: {"path": os.path.abspath(path),
               "sha256": posterior.sha256_file(path)}
        for name, path in {
            "baseline": args.baseline, "metadata": args.metadata,
            "session_titanet": args.session_titanet,
            "robust_titanet": args.robust_titanet,
            "manifest": args.manifest, "config": args.config,
        }.items()
    }
    with open(args.out, "w", encoding="utf-8") as output:
        json.dump(result, output, ensure_ascii=False, indent=2)
    print(json.dumps({
        "pieces": result["piece_count"],
        "accepted_pieces": result["accepted_piece_count"],
        "turns": result["turn_count"], "out": os.path.abspath(args.out)},
        ensure_ascii=False))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    spans = commands.add_parser("spans")
    spans.add_argument("--alignment", required=True)
    spans.add_argument("--frames", required=True)
    spans.add_argument("--baseline", required=True)
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
        raise SystemExit(f"speaker secondary-channel candidate: {error}")
