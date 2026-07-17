#!/usr/bin/env python3
"""Build expanded-local-run phrase evidence without result scoring."""

import argparse
import csv
import json
import os
import tomllib

import speaker_complete_local_phrase_candidate as complete
import speaker_posterior_bounded_phrase_candidate as posterior
import speaker_punctuation_phrase_candidate as phrase_tools
import speaker_relative_top1_phrase_candidate as relative
import speaker_vad_utterance_candidate as vad_tools


EPSILON = 1e-9
REQUIRED_POLICY = {
    "enabled",
    "require_maximal_contiguous_top1_run",
    "require_query_strictly_expands_phrase",
    "require_query_within_padded_vad",
    "require_query_within_existing_embedding_window",
    "require_uniform_known_baseline_identity",
    "require_session_registry_agreement",
    "require_robust_gallery_agreement",
    "require_raw_local_identity_agreement",
    "project_complete_phrase",
}


def load_policy(path):
    policy = complete.load_policy(path)
    with open(path, "rb") as source:
        document = tomllib.load(source)
    section = document.get("expanded_local_run_phrase", {})
    missing = sorted(REQUIRED_POLICY - set(section))
    if missing:
        raise ValueError(
            "expanded-local-run policy missing: " + ",".join(missing))
    contracts = {name: bool(section[name]) for name in REQUIRED_POLICY}
    if not all(contracts.values()):
        raise ValueError("all expanded-local-run contracts are mandatory")
    for name in ("minimum_query_sec", "maximum_query_sec"):
        if name not in section:
            raise ValueError(f"expanded-local-run policy missing: {name}")
        contracts[name] = float(section[name])
    if abs(contracts["minimum_query_sec"] -
           policy["voiceprint"]["minimum_sustained_run_sec"]) > EPSILON:
        raise ValueError("expanded-local-run minimum differs from FR16J")
    speaker = document.get("speaker", {})
    if abs(contracts["maximum_query_sec"] -
           float(speaker.get("max_embed_window_sec", -1.0))) > EPSILON:
        raise ValueError("expanded-local-run maximum differs from speaker")
    policy.update(contracts)
    return policy


def maximal_query(piece, frames, frame_sec):
    positions = {int(item["frame"]): index for index, item in enumerate(frames)}
    first = positions.get(int(piece["frame_start"]))
    last = positions.get(int(piece["frame_end"]) - 1)
    if first is None or last is None or first > last:
        raise ValueError(f"phrase frame range is missing: {piece['evidence_id']}")
    local = int(piece["local_speaker"])
    vad_start = float(piece["vad_start"])
    vad_end = float(piece["vad_end"])

    while first > 0:
        candidate = frames[first - 1]
        if (int(candidate["local"]) != local or
                float(candidate["time"]) < vad_start - EPSILON):
            break
        first -= 1
    while last + 1 < len(frames):
        candidate = frames[last + 1]
        if (int(candidate["local"]) != local or
                float(candidate["time"]) >= vad_end + EPSILON):
            break
        last += 1

    query_start = max(vad_start, float(frames[first]["time"]) - frame_sec / 2.0)
    query_end = min(vad_end, float(frames[last]["time"]) + frame_sec / 2.0)
    return {
        "query_start": query_start,
        "query_end": query_end,
        "query_duration_sec": query_end - query_start,
        "query_frame_start": int(frames[first]["frame"]),
        "query_frame_end": int(frames[last]["frame"]) + 1,
        "query_frame_count": last - first + 1,
        "query_minimum_probability": min(
            float(item["probability"]) for item in frames[first:last + 1]),
        "query_mean_probability": sum(
            float(item["probability"]) for item in frames[first:last + 1]) /
            (last - first + 1),
        "query_maximum_probability": max(
            float(item["probability"]) for item in frames[first:last + 1]),
    }


def uniform_known_conflict(piece, fragments, mapped_id):
    overlaps = posterior.overlapping_fragments(piece, fragments)
    identities = [item["entry"].get("speaker_id") for item in overlaps]
    known = {str(value) for value in identities if value is not None}
    if (not overlaps or any(value is None for value in identities) or
            len(known) != 1):
        return None
    identity = next(iter(known))
    return identity if identity != mapped_id else None


def enumerate_pieces(metadata, frames, frame_sec, vad, baseline, manifest,
                     policy):
    fragments = relative.source_fragments(baseline.get("track", []))
    relative.validate_source_text(fragments, metadata, "baseline")
    local_to_id = {
        int(local): str(identity)
        for local, identity in manifest.get("mapping", {}).items()
    }
    pieces = []
    for phrase in complete.enumerate_pieces(metadata, frames, vad, policy):
        mapped_id = local_to_id.get(int(phrase["local_speaker"]))
        if mapped_id is None:
            continue
        baseline_id = uniform_known_conflict(
            phrase, fragments[int(phrase["text_id"])], mapped_id)
        if baseline_id is None:
            continue
        query = maximal_query(phrase, frames, frame_sec)
        query_start = query["query_start"]
        query_end = query["query_end"]
        duration = query["query_duration_sec"]
        strictly_expands = (
            query_start < float(phrase["start"]) - EPSILON or
            query_end > float(phrase["end"]) + EPSILON)
        if not strictly_expands:
            continue
        if (query_start > float(phrase["start"]) + EPSILON or
                query_end < float(phrase["end"]) - EPSILON):
            continue
        if (duration + EPSILON < policy["minimum_query_sec"] or
                duration > policy["maximum_query_sec"] + EPSILON):
            continue
        pieces.append({
            **phrase,
            "evidence_id": f"expanded_local_run_phrase:{len(pieces)}",
            "phrase_evidence_id": str(phrase["evidence_id"]),
            "phrase_start": float(phrase["start"]),
            "phrase_end": float(phrase["end"]),
            "phrase_duration_sec": float(phrase["duration_sec"]),
            "phrase_frame_start": int(phrase["frame_start"]),
            "phrase_frame_end": int(phrase["frame_end"]),
            "phrase_frame_count": int(phrase["frame_count"]),
            "start": query_start,
            "end": query_end,
            "duration_sec": duration,
            "mapped_speaker_id": mapped_id,
            "baseline_speaker_id": baseline_id,
            "query_strictly_expands_phrase": strictly_expands,
            **query,
        })
    return pieces


def decide_piece(piece, current, robust, fragments, active_ids, local_to_id,
                 policy, reason_prefix="expanded_local_run_phrase"):
    current_id, current_reason, current_ranked = complete.select_view(
        current, active_ids, policy["voiceprint"])
    robust_id, robust_reason, robust_ranked = complete.select_view(
        robust, active_ids, policy["voiceprint"])
    mapped_id = local_to_id.get(int(piece["local_speaker"]))
    identities = [item["entry"].get("speaker_id") for item in fragments]
    known = sorted({str(value) for value in identities if value is not None})
    uniform_known = (
        bool(fragments) and all(value is not None for value in identities) and
        len(known) == 1)

    accepted = False
    reason = reason_prefix + "_session_registry_abstention"
    if not uniform_known:
        reason = reason_prefix + "_baseline_not_uniform_known"
    elif known[0] == mapped_id:
        reason = reason_prefix + "_known_baseline_conflict_missing"
    elif current_id is None:
        reason = reason_prefix + "_session_registry_abstention"
    elif robust_id is None:
        reason = reason_prefix + "_robust_gallery_abstention"
    elif current_id != robust_id:
        reason = reason_prefix + "_registry_disagreement"
    elif current_id != mapped_id:
        reason = reason_prefix + "_raw_local_identity_disagreement"
    elif mapped_id not in active_ids:
        reason = reason_prefix + "_local_mapping_missing"
    else:
        accepted = True
        reason = reason_prefix + "_dual_voiceprint_local_consensus"
    return {
        **piece,
        "mapped_speaker_id": mapped_id,
        "baseline_speaker_ids": known,
        "uniform_known_baseline": uniform_known,
        "session_registry_speaker_id": current_id,
        "session_registry_reason": current_reason,
        "session_registry_ranked": current_ranked,
        "robust_gallery_speaker_id": robust_id,
        "robust_gallery_reason": robust_reason,
        "robust_gallery_ranked": robust_ranked,
        "selected_speaker_id": mapped_id if accepted else None,
        "accepted": accepted,
        "reason": reason,
    }


def command_spans(args, policy):
    metadata = posterior.load_json(args.alignment)
    if metadata.get("kind") != "orator_punctuation_phrase_spans":
        raise ValueError("alignment is not punctuation-phrase metadata")
    frames, frame_sec = posterior.read_frames(args.frames)
    vad = relative.read_vad_timeline(args.timeline)
    baseline = posterior.load_json(args.baseline)
    manifest = posterior.load_json(args.manifest)
    pieces = enumerate_pieces(
        metadata, frames, frame_sec, vad, baseline, manifest, policy)
    posterior.write_spans(args.out, pieces)
    result = {
        "schema_version": 1,
        "kind": "orator_expanded_local_run_phrase_spans",
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
                "alignment": args.alignment, "frames": args.frames,
                "timeline": args.timeline, "baseline": args.baseline,
                "manifest": args.manifest, "config": args.config,
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
        expected_metadata_kind="orator_expanded_local_run_phrase_spans",
        candidate_kind="v21_expanded_local_run_phrase_consensus",
        reason_prefix="expanded_local_run_phrase",
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
    spans.add_argument("--timeline", required=True)
    spans.add_argument("--baseline", required=True)
    spans.add_argument("--manifest", required=True)
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
        raise SystemExit(f"speaker expanded-local-run candidate: {error}")
