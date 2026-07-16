#!/usr/bin/env python3
"""Build complete local-phrase evidence without product-result scoring."""

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
    "require_padded_vad_interval",
    "require_single_top1_channel",
    "require_uniform_known_baseline_identity",
    "veto_any_different_session_voiceprint",
    "veto_any_different_robust_voiceprint",
    "veto_top_ranked_baseline_voiceprint",
    "project_complete_phrase",
}


def load_policy(path):
    voiceprint = posterior.load_policy(path)
    with open(path, "rb") as source:
        document = tomllib.load(source)
    section = document.get("complete_local_phrase", {})
    missing = sorted(REQUIRED_POLICY - set(section))
    if missing:
        raise ValueError(
            "complete-local-phrase policy missing: " + ",".join(missing))
    policy = {name: bool(section[name]) for name in REQUIRED_POLICY}
    if not all(policy.values()):
        raise ValueError("all complete-local-phrase contracts are mandatory")
    for name in ("minimum_phrase_sec", "maximum_phrase_sec"):
        if name not in section:
            raise ValueError(f"complete-local-phrase policy missing: {name}")
        policy[name] = float(section[name])
    if abs(policy["minimum_phrase_sec"] -
           float(voiceprint["minimum_sustained_run_sec"])) > EPSILON:
        raise ValueError("complete-local-phrase minimum differs from FR16J")
    speaker = document.get("speaker", {})
    if "max_embed_window_sec" not in speaker:
        raise ValueError("speaker max_embed_window_sec is missing")
    if abs(policy["maximum_phrase_sec"] -
           float(speaker["max_embed_window_sec"])) > EPSILON:
        raise ValueError("complete-local-phrase maximum differs from speaker")
    policy["voiceprint"] = voiceprint
    return policy


def containing_vad(vad, starts, start, end):
    index = bisect.bisect_right(starts, start + EPSILON) - 1
    if index < 0:
        return None
    vad_start, vad_end = vad[index]
    if vad_start <= start + EPSILON and end <= vad_end + EPSILON:
        return index, vad_start, vad_end
    return None


def enumerate_pieces(metadata, frames, vad, policy):
    vad_starts = [item[0] for item in vad]
    pieces = []
    for phrase in metadata.get("phrases", []):
        start = float(phrase["start"])
        end = float(phrase["end"])
        duration = end - start
        if (duration + EPSILON < policy["minimum_phrase_sec"] or
                duration > policy["maximum_phrase_sec"] + EPSILON):
            continue
        container = containing_vad(vad, vad_starts, start, end)
        if container is None:
            continue
        contained = [item for item in frames
                     if start - EPSILON <= item["time"] < end - EPSILON]
        channels = {int(item["local"]) for item in contained}
        if not contained or len(channels) != 1:
            continue
        local = next(iter(channels))
        vad_index, vad_start, vad_end = container
        pieces.append({
            "evidence_id": str(phrase["evidence_id"]),
            "text_id": int(phrase["text_id"]),
            "source_start": int(phrase["source_start"]),
            "source_end": int(phrase["source_end"]),
            "start": start,
            "end": end,
            "duration_sec": duration,
            "text": str(phrase["text"]),
            "local_speaker": local,
            "frame_start": int(contained[0]["frame"]),
            "frame_end": int(contained[-1]["frame"]) + 1,
            "frame_count": len(contained),
            "minimum_probability": min(
                item["probability"] for item in contained),
            "mean_probability": sum(
                item["probability"] for item in contained) / len(contained),
            "maximum_probability": max(
                item["probability"] for item in contained),
            "vad_index": vad_index,
            "vad_start": vad_start,
            "vad_end": vad_end,
        })
    return pieces


def select_view(evidence, active_ids, voiceprint):
    selected, reason, ranked = phrase_tools.select_identity(
        evidence, active_ids, voiceprint)
    return selected, reason, [
        {"speaker_id": identity, "score": score}
        for identity, score in ranked
    ]


def decide_piece(piece, current, robust, fragments, active_ids,
                 local_to_id, policy, reason_prefix):
    current_id, current_reason, current_ranked = select_view(
        current, active_ids, policy["voiceprint"])
    robust_id, robust_reason, robust_ranked = select_view(
        robust, active_ids, policy["voiceprint"])
    mapped_id = local_to_id.get(int(piece["local_speaker"]))
    baseline_ids = [item["entry"].get("speaker_id") for item in fragments]
    known_ids = sorted({str(value) for value in baseline_ids
                        if value is not None})
    uniform_known = (
        bool(fragments) and all(value is not None for value in baseline_ids) and
        len(known_ids) == 1)

    accepted = False
    reason = reason_prefix + "_local_mapping_missing"
    if mapped_id not in active_ids:
        reason = reason_prefix + "_local_mapping_missing"
    elif not uniform_known:
        reason = reason_prefix + "_baseline_not_uniform_known"
    elif known_ids[0] == mapped_id:
        reason = reason_prefix + "_known_baseline_conflict_missing"
    elif current_id is not None and current_id != mapped_id:
        reason = reason_prefix + "_session_voiceprint_veto"
    elif robust_id is not None and robust_id != mapped_id:
        reason = reason_prefix + "_robust_voiceprint_veto"
    elif (current_ranked and
          current_ranked[0]["speaker_id"] == known_ids[0]):
        reason = reason_prefix + "_session_top_ranked_baseline_veto"
    elif (robust_ranked and
          robust_ranked[0]["speaker_id"] == known_ids[0]):
        reason = reason_prefix + "_robust_top_ranked_baseline_veto"
    else:
        accepted = True
        reason = reason_prefix + "_sortformer_consensus"
    return {
        **piece,
        "mapped_speaker_id": mapped_id,
        "baseline_speaker_ids": known_ids,
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
    pieces = enumerate_pieces(metadata, frames, vad, policy)
    posterior.write_spans(args.out, pieces)
    result = {
        "schema_version": 1,
        "kind": "orator_complete_local_phrase_spans",
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
                "timeline": args.timeline,
                "config": args.config,
            }.items()
        },
    }
    with open(args.metadata, "w", encoding="utf-8") as output:
        json.dump(result, output, ensure_ascii=False, indent=2)
    print(json.dumps({
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
        expected_metadata_kind="orator_complete_local_phrase_spans",
        candidate_kind="v21_complete_local_phrase_consensus",
        reason_prefix="complete_local_phrase",
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
        raise SystemExit(f"speaker complete-local-phrase candidate: {error}")
