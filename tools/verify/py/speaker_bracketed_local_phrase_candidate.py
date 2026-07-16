#!/usr/bin/env python3
"""Build bracketed known local-phrase evidence without result scoring."""

import argparse
import csv
import json
import os
import tomllib

import speaker_bracketed_unknown_phrase_candidate as bracket_tools
import speaker_complete_local_phrase_candidate as complete
import speaker_posterior_bounded_phrase_candidate as posterior
import speaker_punctuation_phrase_candidate as phrase_tools
import speaker_relative_top1_phrase_candidate as relative


REQUIRED_POLICY = {
    "enabled",
    "require_uniform_known_phrase_identity",
    "require_immediately_adjacent_phrases",
    "require_uniform_known_adjacent_phrases",
    "require_same_adjacent_identity",
    "require_raw_local_identity_agreement",
    "require_known_phrase_conflict",
    "veto_any_different_session_voiceprint",
    "veto_any_different_robust_voiceprint",
    "project_complete_phrase",
}


def load_policy(path):
    policy = complete.load_policy(path)
    with open(path, "rb") as source:
        document = tomllib.load(source)
    section = document.get("bracketed_local_phrase", {})
    missing = sorted(REQUIRED_POLICY - set(section))
    if missing:
        raise ValueError(
            "bracketed-local policy missing: " + ",".join(missing))
    contracts = {name: bool(section[name]) for name in REQUIRED_POLICY}
    if not all(contracts.values()):
        raise ValueError("all bracketed-local contracts are mandatory")
    policy.update(contracts)
    return policy


def decide_piece(piece, current, robust, overlaps, left, right, active_ids,
                 local_to_id, policy):
    current_id, current_reason, current_ranked = complete.select_view(
        current, active_ids, policy["voiceprint"])
    robust_id, robust_reason, robust_ranked = complete.select_view(
        robust, active_ids, policy["voiceprint"])
    mapped_id = local_to_id.get(int(piece["local_speaker"]))
    phrase_ids = [item["entry"].get("speaker_id") for item in overlaps]
    known_phrase_ids = sorted({str(value) for value in phrase_ids
                               if value is not None})
    uniform_known = (
        bool(overlaps) and all(value is not None for value in phrase_ids) and
        len(known_phrase_ids) == 1)
    left_id = None if left is None else left["entry"].get("speaker_id")
    right_id = None if right is None else right["entry"].get("speaker_id")
    bracket_id = left_id if left_id is not None and left_id == right_id else None

    accepted = False
    reason = "bracketed_local_phrase_not_uniform_known"
    if not uniform_known:
        reason = "bracketed_local_phrase_not_uniform_known"
    elif left is None or right is None:
        reason = "bracketed_local_phrase_adjacent_phrase_missing"
    elif bracket_id is None:
        reason = "bracketed_local_phrase_adjacent_phrase_identity_disagreement"
    elif known_phrase_ids[0] == bracket_id:
        reason = "bracketed_local_phrase_known_conflict_missing"
    elif mapped_id != bracket_id or mapped_id not in active_ids:
        reason = "bracketed_local_phrase_raw_local_identity_disagreement"
    elif current_id is not None and current_id != bracket_id:
        reason = "bracketed_local_phrase_session_voiceprint_veto"
    elif robust_id is not None and robust_id != bracket_id:
        reason = "bracketed_local_phrase_robust_voiceprint_veto"
    else:
        accepted = True
        reason = "bracketed_local_phrase_temporal_local_consensus"
    return {
        **piece,
        "mapped_speaker_id": mapped_id,
        "phrase_speaker_ids": known_phrase_ids,
        "uniform_known_phrase": uniform_known,
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


def uniform_phrase_fragment(piece, fragments, prefix):
    start_name = prefix + "_phrase_source_start"
    end_name = prefix + "_phrase_source_end"
    if start_name not in piece or end_name not in piece:
        return None
    phrase = {
        "source_start": int(piece[start_name]),
        "source_end": int(piece[end_name]),
    }
    overlaps = posterior.overlapping_fragments(phrase, fragments)
    identities = [item["entry"].get("speaker_id") for item in overlaps]
    known = {str(value) for value in identities if value is not None}
    if (not overlaps or any(value is None for value in identities) or
            len(known) != 1):
        return None
    return {"entry": {"speaker_id": next(iter(known))}}


def phrase_neighbors(piece, fragments):
    return (uniform_phrase_fragment(piece, fragments, "left"),
            uniform_phrase_fragment(piece, fragments, "right"))


def annotate_phrase_neighbors(alignment, pieces):
    phrases = alignment.get("phrases", [])
    indices = {str(value["evidence_id"]): index
               for index, value in enumerate(phrases)}
    output = []
    for piece in pieces:
        index = indices[str(piece["evidence_id"])]
        current = phrases[index]
        value = dict(piece)
        if index > 0 and phrases[index - 1]["text_id"] == current["text_id"]:
            value["left_phrase_source_start"] = int(
                phrases[index - 1]["source_start"])
            value["left_phrase_source_end"] = int(
                phrases[index - 1]["source_end"])
        if (index + 1 < len(phrases) and
                phrases[index + 1]["text_id"] == current["text_id"]):
            value["right_phrase_source_start"] = int(
                phrases[index + 1]["source_start"])
            value["right_phrase_source_end"] = int(
                phrases[index + 1]["source_end"])
        output.append(value)
    return output


def command_spans(args, policy):
    alignment = posterior.load_json(args.alignment)
    if alignment.get("kind") != "orator_punctuation_phrase_spans":
        raise ValueError("alignment is not punctuation-phrase metadata")
    frames, frame_sec = posterior.read_frames(args.frames)
    vad = relative.read_vad_timeline(args.timeline)
    pieces = annotate_phrase_neighbors(
        alignment, complete.enumerate_pieces(alignment, frames, vad, policy))
    posterior.write_spans(args.out, pieces)
    result = {
        "schema_version": 1,
        "kind": "orator_bracketed_local_phrase_spans",
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
                "alignment": args.alignment, "frames": args.frames,
                "timeline": args.timeline, "config": args.config,
            }.items()
        },
    }
    with open(args.metadata, "w", encoding="utf-8") as output:
        json.dump(result, output, ensure_ascii=False, indent=2)
    print(json.dumps({
        "pieces": len(pieces), "spans": os.path.abspath(args.out),
        "metadata": os.path.abspath(args.metadata)}, ensure_ascii=False))


def command_build(args, policy):
    result = bracket_tools.build_candidate(
        posterior.load_json(args.baseline), posterior.load_json(args.metadata),
        phrase_tools.read_titanet(args.session_titanet),
        phrase_tools.read_titanet(args.robust_titanet),
        posterior.load_json(args.manifest), policy,
        expected_metadata_kind="orator_bracketed_local_phrase_spans",
        candidate_kind="v21_bracketed_local_phrase_consensus",
        decision_function=decide_piece,
        context_function=phrase_neighbors)
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
        raise SystemExit(f"speaker bracketed-local candidate: {error}")
