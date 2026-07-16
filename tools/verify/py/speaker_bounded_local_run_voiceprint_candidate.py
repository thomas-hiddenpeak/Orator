#!/usr/bin/env python3
"""Build bounded local-run voiceprint evidence without result scoring."""

import argparse
import csv
import json
import os
import tomllib

import speaker_complete_local_phrase_candidate as complete
import speaker_expanded_local_run_phrase_candidate as expanded
import speaker_posterior_bounded_phrase_candidate as posterior
import speaker_punctuation_phrase_candidate as phrase_tools
import speaker_relative_top1_phrase_candidate as relative
import speaker_vad_utterance_candidate as vad_tools


EPSILON = 1e-9
REQUIRED_POLICY = {
    "enabled",
    "require_complete_punctuation_phrase",
    "require_same_top1_context",
    "require_uniform_known_baseline_identity",
    "require_dual_voiceprint_agreement",
    "require_voiceprint_conflict_with_baseline",
    "require_voiceprint_conflict_with_raw_local",
    "require_selected_identity_active_raw_channel",
    "allow_unanimous_top2_selected_channel",
    "deterministic_max_window_query",
    "project_complete_phrase",
}


def load_policy(path):
    policy = complete.load_policy(path)
    with open(path, "rb") as source:
        document = tomllib.load(source)
    section = document.get("bounded_local_run_voiceprint", {})
    missing = sorted(REQUIRED_POLICY - set(section))
    if missing:
        raise ValueError(
            "bounded-local-run policy missing: " + ",".join(missing))
    contracts = {name: bool(section[name]) for name in REQUIRED_POLICY}
    if not all(contracts.values()):
        raise ValueError("all bounded-local-run contracts are mandatory")
    for name in ("minimum_query_sec", "maximum_query_sec",
                 "minimum_selected_channel_sec"):
        if name not in section:
            raise ValueError(f"bounded-local-run policy missing: {name}")
        contracts[name] = float(section[name])
    if abs(contracts["minimum_query_sec"] -
           policy["voiceprint"]["minimum_sustained_run_sec"]) > EPSILON:
        raise ValueError("bounded-local-run minimum differs from FR16J")
    if abs(contracts["minimum_selected_channel_sec"] -
           policy["voiceprint"]["minimum_sustained_run_sec"]) > EPSILON:
        raise ValueError("selected-channel minimum differs from FR16J")
    speaker = document.get("speaker", {})
    if abs(contracts["maximum_query_sec"] -
           float(speaker.get("max_embed_window_sec", -1.0))) > EPSILON:
        raise ValueError("bounded-local-run maximum differs from speaker")
    policy.update(contracts)
    return policy


def read_diar(path):
    segments = []
    with open(path, encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source)
        required = {"start_sec", "end_sec", "local_speaker"}
        if not required.issubset(reader.fieldnames or []):
            raise ValueError("native diar table is missing required columns")
        for row in reader:
            start = float(row["start_sec"])
            end = float(row["end_sec"])
            if end <= start:
                raise ValueError("native diar interval is not positive")
            segments.append({
                "start": start, "end": end,
                "local_speaker": int(row["local_speaker"]),
            })
    return segments


def read_top2(path):
    values = {}
    with open(path, encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source)
        required = {"frame", "top2"}
        if not required.issubset(reader.fieldnames or []):
            raise ValueError("native frame table is missing top2")
        for row in reader:
            frame = int(row["frame"])
            if frame in values:
                raise ValueError(f"duplicate native frame {frame}")
            values[frame] = int(row["top2"])
    return values


def raw_channel_support(query_start, query_end, diar):
    intervals = {}
    for segment in diar:
        start = max(query_start, float(segment["start"]))
        end = min(query_end, float(segment["end"]))
        if end <= start + EPSILON:
            continue
        intervals.setdefault(int(segment["local_speaker"]), []).append(
            (start, end))
    support = {}
    for local, values in intervals.items():
        merged = []
        for start, end in sorted(values):
            if merged and start <= merged[-1][1] + EPSILON:
                merged[-1] = (merged[-1][0], max(merged[-1][1], end))
            else:
                merged.append((start, end))
        support[str(local)] = sum(end - start for start, end in merged)
    return support


def bounded_query(phrase, run, maximum):
    phrase_start = float(phrase["start"])
    phrase_end = float(phrase["end"])
    run_start = float(run["query_start"])
    run_end = float(run["query_end"])
    if (run_start > phrase_start + EPSILON or
            run_end < phrase_end - EPSILON):
        raise ValueError(f"local run does not contain phrase: {phrase['evidence_id']}")
    run_duration = run_end - run_start
    if run_duration <= maximum + EPSILON:
        return run_start, run_end
    earliest = max(run_start, phrase_end - maximum)
    latest = min(phrase_start, run_end - maximum)
    if earliest > latest + EPSILON:
        raise ValueError(f"bounded query cannot contain phrase: {phrase['evidence_id']}")
    preferred = (phrase_start + phrase_end - maximum) / 2.0
    start = min(max(preferred, earliest), latest)
    return start, start + maximum


def uniform_known_identity(piece, fragments):
    overlaps = posterior.overlapping_fragments(piece, fragments)
    identities = [item["entry"].get("speaker_id") for item in overlaps]
    known = {str(value) for value in identities if value is not None}
    if (not overlaps or any(value is None for value in identities) or
            len(known) != 1):
        return None
    return next(iter(known))


def enumerate_pieces(metadata, frames, frame_sec, top2, vad, diar, baseline,
                     manifest, policy):
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
        baseline_id = uniform_known_identity(
            phrase, fragments[int(phrase["text_id"])])
        if baseline_id is None:
            continue
        run = expanded.maximal_query(phrase, frames, frame_sec)
        if (float(run["query_start"]) > float(phrase["start"]) + EPSILON or
                float(run["query_end"]) < float(phrase["end"]) - EPSILON):
            continue
        query_start, query_end = bounded_query(
            phrase, run, policy["maximum_query_sec"])
        duration = query_end - query_start
        if duration + EPSILON < policy["minimum_query_sec"]:
            continue
        if (query_start >= float(phrase["start"]) - EPSILON and
                query_end <= float(phrase["end"]) + EPSILON):
            continue
        query_frames = [
            item for item in frames
            if query_start - EPSILON <= float(item["time"]) <
            query_end - EPSILON
        ]
        if not query_frames:
            continue
        top2_counts = {}
        for item in query_frames:
            frame = int(item["frame"])
            if frame not in top2:
                raise ValueError(f"top2 is missing frame {frame}")
            local = str(top2[frame])
            top2_counts[local] = top2_counts.get(local, 0) + 1
        pieces.append({
            **phrase,
            "evidence_id": f"bounded_local_run_voiceprint:{len(pieces)}",
            "phrase_evidence_id": str(phrase["evidence_id"]),
            "phrase_start": float(phrase["start"]),
            "phrase_end": float(phrase["end"]),
            "phrase_duration_sec": float(phrase["duration_sec"]),
            "phrase_frame_start": int(phrase["frame_start"]),
            "phrase_frame_end": int(phrase["frame_end"]),
            "start": query_start,
            "end": query_end,
            "duration_sec": duration,
            "full_run_start": float(run["query_start"]),
            "full_run_end": float(run["query_end"]),
            "full_run_duration_sec": float(run["query_duration_sec"]),
            "query_was_bounded": (
                float(run["query_duration_sec"]) >
                policy["maximum_query_sec"] + EPSILON),
            "query_frame_start": int(query_frames[0]["frame"]),
            "query_frame_end": int(query_frames[-1]["frame"]) + 1,
            "query_frame_count": len(query_frames),
            "raw_channel_support_sec": raw_channel_support(
                query_start, query_end, diar),
            "raw_top2_frame_count": top2_counts,
            "mapped_speaker_id": mapped_id,
            "baseline_speaker_id": baseline_id,
        })
    return pieces


def decide_piece(piece, current, robust, fragments, active_ids, local_to_id,
                 policy, reason_prefix="bounded_local_run_voiceprint"):
    current_id, current_reason, current_ranked = complete.select_view(
        current, active_ids, policy["voiceprint"])
    robust_id, robust_reason, robust_ranked = complete.select_view(
        robust, active_ids, policy["voiceprint"])
    mapped_id = local_to_id.get(int(piece["local_speaker"]))
    id_to_local = {identity: local for local, identity in local_to_id.items()}
    identities = [item["entry"].get("speaker_id") for item in fragments]
    known = sorted({str(value) for value in identities if value is not None})
    uniform_known = (
        bool(fragments) and all(value is not None for value in identities) and
        len(known) == 1)

    accepted = False
    reason = reason_prefix + "_baseline_not_uniform_known"
    if not uniform_known:
        reason = reason_prefix + "_baseline_not_uniform_known"
    elif current_id is None:
        reason = reason_prefix + "_session_registry_abstention"
    elif robust_id is None:
        reason = reason_prefix + "_robust_gallery_abstention"
    elif current_id != robust_id:
        reason = reason_prefix + "_registry_disagreement"
    elif current_id == known[0]:
        reason = reason_prefix + "_baseline_identity_conflict_missing"
    elif current_id == mapped_id:
        reason = reason_prefix + "_raw_local_identity_conflict_missing"
    elif mapped_id not in active_ids:
        reason = reason_prefix + "_local_mapping_missing"
    else:
        selected_local = str(id_to_local.get(current_id))
        sustained = (
            float(piece.get("raw_channel_support_sec", {}).get(
                selected_local, 0.0)) + EPSILON >=
            policy["minimum_selected_channel_sec"])
        unanimous_top2 = (
            int(piece.get("raw_top2_frame_count", {}).get(selected_local, 0)) ==
            int(piece.get("query_frame_count", -1)))
        if not sustained and not unanimous_top2:
            reason = reason_prefix + "_selected_raw_channel_missing"
        else:
            accepted = True
            reason = reason_prefix + "_dual_voiceprint_override"
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
        "selected_speaker_id": current_id if accepted else None,
        "accepted": accepted,
        "reason": reason,
    }


def command_spans(args, policy):
    metadata = posterior.load_json(args.alignment)
    if metadata.get("kind") != "orator_punctuation_phrase_spans":
        raise ValueError("alignment is not punctuation-phrase metadata")
    frames, frame_sec = posterior.read_frames(args.frames)
    top2 = read_top2(args.frames)
    vad = relative.read_vad_timeline(args.timeline)
    diar = read_diar(args.diar)
    baseline = posterior.load_json(args.baseline)
    manifest = posterior.load_json(args.manifest)
    pieces = enumerate_pieces(
        metadata, frames, frame_sec, top2, vad, diar, baseline, manifest,
        policy)
    posterior.write_spans(args.out, pieces)
    result = {
        "schema_version": 1,
        "kind": "orator_bounded_local_run_voiceprint_spans",
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
                "timeline": args.timeline, "diar": args.diar,
                "baseline": args.baseline,
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
        expected_metadata_kind="orator_bounded_local_run_voiceprint_spans",
        candidate_kind="v21_bounded_local_run_voiceprint_override",
        reason_prefix="bounded_local_run_voiceprint",
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
    spans.add_argument("--diar", required=True)
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
        raise SystemExit(f"speaker bounded-local-run candidate: {error}")
