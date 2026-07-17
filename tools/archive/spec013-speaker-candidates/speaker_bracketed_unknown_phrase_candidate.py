#!/usr/bin/env python3
"""Build bracketed unknown-phrase evidence without product-result scoring."""

import argparse
from collections import defaultdict
import csv
import json
import os
import tomllib

import speaker_complete_local_phrase_candidate as complete
import speaker_posterior_bounded_phrase_candidate as posterior
import speaker_punctuation_phrase_candidate as phrase_tools
import speaker_relative_top1_phrase_candidate as relative


REQUIRED_POLICY = {
    "enabled",
    "require_unknown_only_phrase",
    "require_immediately_adjacent_known_fragments",
    "require_same_adjacent_identity",
    "require_raw_local_identity_agreement",
    "veto_any_different_session_voiceprint",
    "veto_any_different_robust_voiceprint",
    "project_complete_phrase",
}


def load_policy(path):
    policy = complete.load_policy(path)
    with open(path, "rb") as source:
        document = tomllib.load(source)
    section = document.get("bracketed_unknown_phrase", {})
    missing = sorted(REQUIRED_POLICY - set(section))
    if missing:
        raise ValueError(
            "bracketed-unknown policy missing: " + ",".join(missing))
    contracts = {name: bool(section[name]) for name in REQUIRED_POLICY}
    if not all(contracts.values()):
        raise ValueError("all bracketed-unknown contracts are mandatory")
    policy.update(contracts)
    return policy


def adjacent_fragments(piece, fragments):
    start = int(piece["source_start"])
    end = int(piece["source_end"])
    left = [item for item in fragments if item["source_end"] <= start]
    right = [item for item in fragments if item["source_start"] >= end]
    if not left or not right:
        return None, None
    left_item = max(left, key=lambda item: item["source_end"])
    right_item = min(right, key=lambda item: item["source_start"])
    if (left_item["source_end"] != start or
            right_item["source_start"] != end):
        return None, None
    return left_item, right_item


def decide_piece(piece, current, robust, overlaps, left, right, active_ids,
                 local_to_id, policy):
    current_id, current_reason, current_ranked = complete.select_view(
        current, active_ids, policy["voiceprint"])
    robust_id, robust_reason, robust_ranked = complete.select_view(
        robust, active_ids, policy["voiceprint"])
    mapped_id = local_to_id.get(int(piece["local_speaker"]))
    unknown_only = bool(overlaps) and all(
        item["entry"].get("speaker_id") is None for item in overlaps)
    left_id = None if left is None else left["entry"].get("speaker_id")
    right_id = None if right is None else right["entry"].get("speaker_id")
    bracket_id = left_id if left_id is not None and left_id == right_id else None

    accepted = False
    reason = "bracketed_unknown_not_unknown_only"
    if not unknown_only:
        reason = "bracketed_unknown_not_unknown_only"
    elif left is None or right is None:
        reason = "bracketed_unknown_adjacent_fragment_missing"
    elif bracket_id is None:
        reason = "bracketed_unknown_adjacent_identity_disagreement"
    elif mapped_id != bracket_id or mapped_id not in active_ids:
        reason = "bracketed_unknown_raw_local_identity_disagreement"
    elif current_id is not None and current_id != bracket_id:
        reason = "bracketed_unknown_session_voiceprint_veto"
    elif robust_id is not None and robust_id != bracket_id:
        reason = "bracketed_unknown_robust_voiceprint_veto"
    else:
        accepted = True
        reason = "bracketed_unknown_temporal_local_consensus"
    return {
        **piece,
        "mapped_speaker_id": mapped_id,
        "unknown_only_baseline": unknown_only,
        "left_speaker_id": left_id,
        "right_speaker_id": right_id,
        "bracket_speaker_id": bracket_id,
        "session_registry_speaker_id": current_id,
        "session_registry_reason": current_reason,
        "session_registry_ranked": current_ranked,
        "robust_gallery_speaker_id": robust_id,
        "robust_gallery_reason": robust_reason,
        "robust_gallery_ranked": robust_ranked,
        "selected_speaker_id": bracket_id if accepted else None,
        "accepted": accepted,
        "reason": reason,
    }


def build_candidate(baseline, metadata, current_evidence, robust_evidence,
                    manifest, policy, *,
                    expected_metadata_kind="orator_bracketed_unknown_phrase_spans",
                    candidate_kind="v21_bracketed_unknown_phrase_consensus",
                    decision_function=decide_piece,
                    context_function=adjacent_fragments):
    if baseline.get("kind") != "orator_frozen_speaker_candidate":
        raise ValueError("baseline is not a frozen speaker candidate")
    if metadata.get("kind") != expected_metadata_kind:
        raise ValueError("metadata is not bracketed-unknown evidence")
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
        source_fragments = fragments[text_id]
        overlaps = posterior.overlapping_fragments(piece, source_fragments)
        left, right = context_function(piece, source_fragments)
        decision = decision_function(
            piece, current_evidence[evidence_id], robust_evidence[evidence_id],
            overlaps, left, right, active_ids, local_to_id, policy)
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
    alignment = posterior.load_json(args.alignment)
    if alignment.get("kind") != "orator_punctuation_phrase_spans":
        raise ValueError("alignment is not punctuation-phrase metadata")
    frames, frame_sec = posterior.read_frames(args.frames)
    vad = relative.read_vad_timeline(args.timeline)
    pieces = complete.enumerate_pieces(alignment, frames, vad, policy)
    posterior.write_spans(args.out, pieces)
    result = {
        "schema_version": 1,
        "kind": "orator_bracketed_unknown_phrase_spans",
        "frame_period_sec": frame_sec,
        "policy": {name: value for name, value in policy.items()
                   if name != "voiceprint"},
        "voiceprint_policy": policy["voiceprint"],
        "asr": alignment["asr"],
        "align": alignment["align"],
        "pieces": pieces,
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
        "pieces": len(pieces), "spans": os.path.abspath(args.out),
        "metadata": os.path.abspath(args.metadata)}, ensure_ascii=False))


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
        "turns": result["turn_count"], "out": os.path.abspath(args.out)},
        ensure_ascii=False))


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
        raise SystemExit(f"speaker bracketed-unknown candidate: {error}")
