#!/usr/bin/env python3
"""Build a reference-free complete-phrase view over bounded local-slot churn."""

import argparse
import csv
import json
import os
import tomllib
from collections import defaultdict

import speaker_complete_source_voiceprint_candidate as complete_source
import speaker_complete_edge_contribution_candidate as complete_edge
import speaker_posterior_bounded_phrase_candidate as posterior
import speaker_punctuation_phrase_candidate as phrase_tools
import speaker_relative_top1_phrase_candidate as relative


EPSILON = 1e-9
REQUIRED_CONTRACTS = {
    "enabled",
    "require_same_outer_local_channel",
    "require_different_inner_channel",
    "require_dual_query_voiceprint_agreement",
    "require_query_identity_equal_outer_mapping",
    "project_complete_punctuation_phrases",
    "require_complete_alignment_in_query",
    "require_uniform_known_phrase_conflict",
    "require_dual_phrase_top_rank_outer_identity",
    "veto_single_inner_channel_with_dual_phrase_support",
    "allow_relative_low_margin_complete_clause_group",
    "veto_dual_agreed_different_phrase_top_rank",
    "require_single_active_channel_in_inner_runs",
    "reject_conflicting_overlays",
}


def load_policy(path):
    with open(path, "rb") as source:
        document = tomllib.load(source)
    section = document.get("bracketed_local_churn", {})
    fusion = document.get("speaker_fusion", {})
    numeric = {
        "frame_activity_threshold",
        "minimum_outer_run_sec",
        "maximum_query_sec",
    }
    voiceprint = {
        "short_max_sec",
        "short_min_score",
        "short_min_margin",
        "regular_min_score",
        "regular_min_margin",
    }
    missing = sorted(
        (REQUIRED_CONTRACTS | numeric) - set(section) |
        (voiceprint - set(fusion)))
    if missing:
        raise ValueError(
            "bracketed local churn policy missing: " + ",".join(missing))
    contracts = {name: bool(section[name]) for name in REQUIRED_CONTRACTS}
    if not all(contracts.values()):
        raise ValueError("all bracketed local churn contracts are mandatory")
    policy = {
        **contracts,
        **{name: float(section[name]) for name in numeric},
        "voiceprint": {name: float(fusion[name]) for name in voiceprint},
    }
    punctuation = str(section.get("punctuation", ""))
    if not punctuation:
        raise ValueError("bracketed local churn punctuation is missing")
    policy["punctuation"] = punctuation
    if not 0.0 < policy["frame_activity_threshold"] < 1.0:
        raise ValueError("frame activity threshold must be between zero and one")
    if policy["minimum_outer_run_sec"] <= 0.0:
        raise ValueError("minimum outer run duration must be positive")
    if policy["maximum_query_sec"] < 2.0 * policy["minimum_outer_run_sec"]:
        raise ValueError("maximum query cannot contain both outer supports")
    return policy


def read_frames(path):
    frames = []
    with open(path, encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source)
        required = {
            "frame", "time_sec", "top1", "top1_prob", "margin",
            "active_count",
        }
        if not required.issubset(reader.fieldnames or []):
            raise ValueError("raw frame CSV is missing churn evidence columns")
        for row in reader:
            frames.append({
                "frame": int(row["frame"]),
                "time": float(row["time_sec"]),
                "local": int(row["top1"]),
                "probability": float(row["top1_prob"]),
                "margin": float(row["margin"]),
                "active_count": int(row["active_count"]),
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


def stable_runs(frames, frame_sec, audio_sec, policy):
    runs = []
    current = []

    def finish():
        if not current:
            return
        start = max(0.0, current[0]["time"] - 0.5 * frame_sec)
        end = min(audio_sec, current[-1]["time"] + 0.5 * frame_sec)
        if end - start + EPSILON < policy["minimum_outer_run_sec"]:
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
            "mean_margin": sum(
                item.get("margin", 0.0) for item in current) / len(current),
            "minimum_margin": min(
                item.get("margin", 0.0) for item in current),
            "maximum_active_count": max(
                item.get("active_count", 1) for item in current),
        })

    threshold = policy["frame_activity_threshold"]
    for frame in frames:
        if (frame["probability"] + EPSILON < threshold or
                (current and frame["local"] != current[-1]["local"])):
            finish()
            current = []
        if frame["probability"] + EPSILON >= threshold:
            current.append(frame)
    finish()
    return runs


def bracket_pairs(runs, policy):
    pairs = []
    minimum = policy["minimum_outer_run_sec"]
    maximum = policy["maximum_query_sec"]
    for left_index, left in enumerate(runs):
        inner = []
        for right_index in range(left_index + 1, len(runs)):
            right = runs[right_index]
            if right["start"] - left["end"] + 2.0 * minimum > maximum:
                break
            if right["local_speaker"] == left["local_speaker"]:
                if not inner:
                    break
                query_start = float(left["start"])
                query_end = min(
                    float(right["end"]), float(right["start"]) + minimum)
                if query_end - query_start > maximum:
                    query_start = query_end - maximum
                if query_start > float(left["end"]) - minimum + EPSILON:
                    break
                pairs.append({
                    "left_run_index": left_index,
                    "right_run_index": right_index,
                    "left_run": left,
                    "right_run": right,
                    "inner_runs": list(inner),
                    "inner_local_speakers": sorted({
                        int(item["local_speaker"]) for item in inner
                        if item["local_speaker"] != left["local_speaker"]
                    }),
                    "interior_start": float(left["end"]),
                    "interior_end": float(right["start"]),
                    "query_start": query_start,
                    "query_end": query_end,
                    "inner_mean_margins_below_outer": all(
                        float(item["mean_margin"]) + EPSILON < min(
                            float(left["mean_margin"]),
                            float(right["mean_margin"]))
                        for item in inner),
                    "inner_runs_single_active_channel": all(
                        int(item["maximum_active_count"]) == 1
                        for item in inner),
                })
                break
            inner.append(right)
    return pairs


def phrase_alignment_complete(metadata, phrase, query_start, query_end):
    text_id = int(phrase["text_id"])
    source = str(metadata["asr"][str(text_id)]["text"])
    units = metadata["align"][str(text_id)]
    times = phrase_tools.aligned_character_times(source, units)
    for index in range(int(phrase["source_start"]),
                       int(phrase["source_end"])):
        if source[index] in complete_source.SEPARATORS:
            continue
        interval = times[index]
        if interval is None:
            return False
        if (float(interval["start"]) < query_start - EPSILON or
                float(interval["end"]) > query_end + EPSILON):
            return False
    return True


def overlapping_identity_ids(fragments, source_start, source_end):
    return sorted({
        str(fragment["entry"]["speaker_id"])
        for fragment in fragments
        if fragment["source_start"] < source_end and
        fragment["source_end"] > source_start and
        fragment["entry"].get("speaker_id") is not None
    })


def projection_phrases(metadata, pair, fragments_by_text, mapped_identity):
    by_text = defaultdict(list)
    for phrase in metadata.get("phrases", []):
        if (float(phrase["end"]) <= pair["interior_start"] + EPSILON or
                float(phrase["start"]) >= pair["interior_end"] - EPSILON):
            continue
        text_id = int(phrase["text_id"])
        asr = metadata["asr"][str(text_id)]
        if (pair["query_start"] < float(asr["start"]) - EPSILON or
                pair["query_end"] > float(asr["end"]) + EPSILON):
            continue
        if not phrase_alignment_complete(
                metadata, phrase, pair["query_start"], pair["query_end"]):
            continue
        source_start = int(phrase["source_start"])
        source_end = int(phrase["source_end"])
        identities = overlapping_identity_ids(
            fragments_by_text[text_id], source_start, source_end)
        if identities == [mapped_identity]:
            continue
        by_text[text_id].append({
            "phrase_evidence_id": str(phrase["evidence_id"]),
            "source_start": source_start,
            "source_end": source_end,
            "start": float(phrase["start"]),
            "end": float(phrase["end"]),
            "text": str(phrase["text"]),
            "baseline_identity_ids": identities,
        })
    return by_text


def projection_clauses(metadata, pair, text_id, fragments, policy):
    source = str(metadata["asr"][str(text_id)]["text"])
    times = phrase_tools.aligned_character_times(
        source, metadata["align"][str(text_id)])
    query = {"start": pair["query_start"], "end": pair["query_end"]}
    clauses = []
    for source_start, source_end in phrase_tools.phrase_ranges(
            source, policy["punctuation"]):
        clause = complete_edge.complete_clause(
            source, times, source_start, source_end, query,
            policy["punctuation"])
        if clause is None:
            continue
        if (float(clause["projection_end"]) <=
                pair["interior_start"] + EPSILON or
                float(clause["projection_start"]) >=
                pair["interior_end"] - EPSILON):
            continue
        clauses.append({
            **clause,
            "text": source[source_start:source_end],
            "baseline_identity_ids": overlapping_identity_ids(
                fragments, source_start, source_end),
        })
    return clauses


def enumerate_pieces(metadata, frames, frame_sec, baseline, mapping, policy):
    if metadata.get("kind") != "orator_punctuation_phrase_spans":
        raise ValueError("alignment input is not punctuation phrase metadata")
    fragments = relative.source_fragments(baseline.get("track", []))
    relative.validate_source_text(fragments, metadata, "baseline")
    audio_sec = float(baseline["audio_sec"])
    runs = stable_runs(frames, frame_sec, audio_sec, policy)
    pieces = []
    for pair in bracket_pairs(runs, policy):
        outer_local = int(pair["left_run"]["local_speaker"])
        if outer_local not in mapping:
            raise ValueError(f"missing mapping for local speaker {outer_local}")
        mapped_identity = mapping[outer_local]
        projected = projection_phrases(
            metadata, pair, fragments, mapped_identity)
        for text_id, phrases in sorted(projected.items()):
            if not phrases:
                continue
            pieces.append({
                "evidence_id": f"bracketed_local_churn:{len(pieces)}",
                "text_id": text_id,
                "start": pair["query_start"],
                "end": pair["query_end"],
                "outer_local_speaker": outer_local,
                "mapped_outer_identity": mapped_identity,
                "inner_local_speakers": pair["inner_local_speakers"],
                "inner_runs": pair["inner_runs"],
                "inner_mean_margins_below_outer": pair[
                    "inner_mean_margins_below_outer"],
                "inner_runs_single_active_channel": pair[
                    "inner_runs_single_active_channel"],
                "left_run": pair["left_run"],
                "right_run": pair["right_run"],
                "interior_start": pair["interior_start"],
                "interior_end": pair["interior_end"],
                "projection_phrases": phrases,
                "projection_clauses": projection_clauses(
                    metadata, pair, text_id, fragments[text_id], policy),
            })
    return pieces, runs


def select_regular(evidence, active_ids, policy):
    if evidence.get("status") != "ok":
        return None, "embedding_unavailable", []
    ranked = sorted(
        ((identity, float(evidence["scores"][identity]))
         for identity in active_ids if identity in evidence.get("scores", {})),
        key=lambda item: (-item[1], item[0]))
    if len(ranked) < 2:
        return None, "active_scores_incomplete", ranked
    if ranked[0][1] + EPSILON < policy["regular_min_score"]:
        return None, "regular_score_below_gate", ranked
    if (ranked[0][1] - ranked[1][1] + EPSILON <
            policy["regular_min_margin"]):
        return None, "regular_margin_below_gate", ranked
    return ranked[0][0], "regular_voiceprint", ranked


def has_single_channel_phrase_veto(piece, session_phrase, robust_phrase,
                                   active_ids, mapping, voiceprint_policy):
    inner = piece["inner_local_speakers"]
    if len(inner) != 1:
        return False, []
    inner_identity = mapping.get(int(inner[0]))
    audits = []
    for phrase in piece["projection_phrases"]:
        evidence_id = phrase["phrase_evidence_id"]
        current_id, current_reason, current_ranked = phrase_tools.select_identity(
            session_phrase.get(evidence_id, {}), active_ids,
            voiceprint_policy)
        robust_id, robust_reason, robust_ranked = phrase_tools.select_identity(
            robust_phrase.get(evidence_id, {}), active_ids,
            voiceprint_policy)
        veto = current_id == robust_id == inner_identity
        audits.append({
            "phrase_evidence_id": evidence_id,
            "inner_mapped_identity": inner_identity,
            "session_identity": current_id,
            "session_reason": current_reason,
            "session_ranked": current_ranked,
            "robust_identity": robust_id,
            "robust_reason": robust_reason,
            "robust_ranked": robust_ranked,
            "veto": veto,
        })
        if veto:
            return True, audits
    return False, audits


def guarded_projection_phrases(piece, session_phrase, robust_phrase,
                               active_ids, outer_identity,
                               voiceprint_policy):
    selected = []
    audits = []
    for phrase in piece["projection_phrases"]:
        evidence_id = phrase["phrase_evidence_id"]
        if evidence_id not in session_phrase or evidence_id not in robust_phrase:
            raise ValueError(f"missing phrase evidence {evidence_id}")
        _, current_reason, current_ranked = phrase_tools.select_identity(
            session_phrase[evidence_id], active_ids, voiceprint_policy)
        _, robust_reason, robust_ranked = phrase_tools.select_identity(
            robust_phrase[evidence_id], active_ids, voiceprint_policy)
        baseline_ids = [
            str(value) for value in phrase["baseline_identity_ids"]]
        uniform_conflict = (
            len(baseline_ids) == 1 and baseline_ids[0] != outer_identity)
        current_top = current_ranked[0][0] if current_ranked else None
        robust_top = robust_ranked[0][0] if robust_ranked else None
        dual_outer_top = current_top == robust_top == outer_identity
        accepted = uniform_conflict and dual_outer_top
        audits.append({
            "phrase_evidence_id": evidence_id,
            "baseline_identity_ids": baseline_ids,
            "uniform_known_conflict": uniform_conflict,
            "session_reason": current_reason,
            "session_top_ranked_identity": current_top,
            "robust_reason": robust_reason,
            "robust_top_ranked_identity": robust_top,
            "dual_outer_top_rank": dual_outer_top,
            "accepted": accepted,
        })
        if accepted:
            selected.append(phrase)
    return selected, audits


def has_dual_agreed_different_phrase_top(phrase_guard_audit, outer_identity):
    return any(
        item["session_top_ranked_identity"] is not None and
        item["session_top_ranked_identity"] ==
        item["robust_top_ranked_identity"] and
        item["session_top_ranked_identity"] != outer_identity
        for item in phrase_guard_audit)


def intervals_overlap(left, right):
    if int(left["text_id"]) != int(right["text_id"]):
        return False
    return any(
        int(a["source_start"]) < int(b["source_end"]) and
        int(a["source_end"]) > int(b["source_start"])
        for a in left.get(
            "selected_projection_ranges", left["projection_phrases"])
        for b in right.get(
            "selected_projection_ranges", right["projection_phrases"]))


def build_candidate(baseline, metadata, session_query, robust_query,
                    session_phrase, robust_phrase, manifest, policy):
    if metadata.get("kind") != "orator_bracketed_local_churn_spans":
        raise ValueError("metadata is not bracketed local churn evidence")
    fragments = relative.source_fragments(baseline.get("track", []))
    relative.validate_source_text(fragments, metadata, "baseline")
    active_ids = [str(value) for value in baseline["active_speaker_ids"]]
    mapping = {
        int(local): str(identity)
        for local, identity in manifest["mapping"].items()
    }
    if sorted(mapping.values()) != sorted(active_ids):
        raise ValueError("manifest identity set differs from baseline")
    id_to_local = {identity: local for local, identity in mapping.items()}
    decisions = []
    for piece in metadata["pieces"]:
        evidence_id = str(piece["evidence_id"])
        if evidence_id not in session_query or evidence_id not in robust_query:
            raise ValueError(f"missing query evidence {evidence_id}")
        current_id, current_reason, current_ranked = select_regular(
            session_query[evidence_id], active_ids, policy["voiceprint"])
        robust_id, robust_reason, robust_ranked = select_regular(
            robust_query[evidence_id], active_ids, policy["voiceprint"])
        expected = str(piece["mapped_outer_identity"])
        selected_phrases, phrase_guard_audit = guarded_projection_phrases(
            piece, session_phrase, robust_phrase, active_ids, expected,
            policy["voiceprint"])
        agreed_different_top = has_dual_agreed_different_phrase_top(
            phrase_guard_audit, expected)
        expand_complete_clauses = (
            bool(piece.get("projection_clauses")) and
            bool(piece.get("inner_mean_margins_below_outer")) and
            bool(piece.get("inner_runs_single_active_channel")) and
            not agreed_different_top)
        selected_ranges = (
            piece["projection_clauses"]
            if expand_complete_clauses else selected_phrases)
        veto, phrase_audit = has_single_channel_phrase_veto(
            piece, session_phrase, robust_phrase, active_ids, mapping,
            policy["voiceprint"])
        accepted = False
        reason = "bracketed_local_churn_session_abstention"
        if current_id is None:
            reason = "bracketed_local_churn_session_abstention"
        elif robust_id is None:
            reason = "bracketed_local_churn_robust_abstention"
        elif current_id != robust_id:
            reason = "bracketed_local_churn_registry_disagreement"
        elif current_id != expected:
            reason = "bracketed_local_churn_outer_mapping_disagreement"
        elif not selected_ranges:
            reason = "bracketed_local_churn_phrase_guard_abstention"
        elif veto:
            reason = "bracketed_local_churn_single_channel_phrase_veto"
        else:
            accepted = True
            reason = (
                "bracketed_local_churn_relative_low_margin_consensus"
                if expand_complete_clauses else
                "bracketed_local_churn_dual_voiceprint_consensus")
        decisions.append({
            **piece,
            "session_registry_speaker_id": current_id,
            "session_registry_reason": current_reason,
            "session_registry_ranked": current_ranked,
            "robust_gallery_speaker_id": robust_id,
            "robust_gallery_reason": robust_reason,
            "robust_gallery_ranked": robust_ranked,
            "phrase_veto_audit": phrase_audit,
            "phrase_guard_audit": phrase_guard_audit,
            "selected_projection_phrases": selected_phrases,
            "dual_agreed_different_phrase_top_veto": agreed_different_top,
            "complete_clause_group_selected": expand_complete_clauses,
            "selected_projection_ranges": selected_ranges,
            "selected_speaker_id": current_id if accepted else None,
            "accepted": accepted,
            "reason": reason,
        })

    conflicts = set()
    for left_index, left in enumerate(decisions):
        if not left["accepted"]:
            continue
        for right_index in range(left_index + 1, len(decisions)):
            right = decisions[right_index]
            if (right["accepted"] and intervals_overlap(left, right) and
                    left["selected_speaker_id"] !=
                    right["selected_speaker_id"]):
                conflicts.update((left_index, right_index))
    for index in conflicts:
        decisions[index]["accepted"] = False
        decisions[index]["selected_speaker_id"] = None
        decisions[index]["reason"] = (
            "bracketed_local_churn_conflicting_overlay")

    overlays = defaultdict(list)
    for decision in decisions:
        if not decision["accepted"]:
            continue
        for phrase in decision["selected_projection_ranges"]:
            overlays[int(decision["text_id"])].append({
                "source_start": int(phrase["source_start"]),
                "source_end": int(phrase["source_end"]),
                "speaker_id": decision["selected_speaker_id"],
                "reason": decision["reason"],
            })
    track = posterior.project_track(metadata, fragments, overlays, id_to_local)
    relative.validate_source_text(
        relative.source_fragments(track), metadata, "projected")
    return {
        "schema_version": 1,
        "kind": "orator_frozen_speaker_candidate",
        "candidate_kind": "v21_bracketed_local_churn_contribution",
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


def write_spans(path, pieces):
    with open(path, "w", encoding="ascii", newline="") as output:
        writer = csv.writer(output, delimiter="\t", lineterminator="\n")
        writer.writerow(["evidence_id", "start_sec", "end_sec"])
        for piece in pieces:
            writer.writerow([piece["evidence_id"], piece["start"], piece["end"]])


def command_spans(args, policy):
    alignment = posterior.load_json(args.alignment)
    baseline = posterior.load_json(args.baseline)
    manifest = posterior.load_json(args.manifest)
    mapping = {
        int(local): str(identity)
        for local, identity in manifest["mapping"].items()
    }
    frames, frame_sec = read_frames(args.frames)
    pieces, runs = enumerate_pieces(
        alignment, frames, frame_sec, baseline, mapping, policy)
    write_spans(args.out, pieces)
    result = {
        "schema_version": 1,
        "kind": "orator_bracketed_local_churn_spans",
        "frame_period_sec": frame_sec,
        "policy": {name: value for name, value in policy.items()
                   if name != "voiceprint"},
        "voiceprint_policy": policy["voiceprint"],
        "asr": alignment["asr"],
        "align": alignment["align"],
        "stable_run_count": len(runs),
        "pieces": pieces,
        "piece_count": len(pieces),
        "sources": {
            name: {"path": os.path.abspath(path),
                   "sha256": posterior.sha256_file(path)}
            for name, path in {
                "alignment": args.alignment,
                "frames": args.frames,
                "baseline": args.baseline,
                "manifest": args.manifest,
                "config": args.config,
            }.items()
        },
    }
    with open(args.metadata, "w", encoding="utf-8") as output:
        json.dump(result, output, ensure_ascii=False, indent=2)
    print(json.dumps({
        "stable_runs": len(runs),
        "pieces": len(pieces),
        "spans": os.path.abspath(args.out),
        "metadata": os.path.abspath(args.metadata),
    }, ensure_ascii=False))


def command_build(args, policy):
    result = build_candidate(
        posterior.load_json(args.baseline),
        posterior.load_json(args.metadata),
        phrase_tools.read_titanet(args.session_query_titanet),
        phrase_tools.read_titanet(args.robust_query_titanet),
        phrase_tools.read_titanet(args.session_phrase_titanet),
        phrase_tools.read_titanet(args.robust_phrase_titanet),
        posterior.load_json(args.manifest), policy)
    result["sources"] = {
        name: {"path": os.path.abspath(path),
               "sha256": posterior.sha256_file(path)}
        for name, path in {
            "baseline": args.baseline,
            "metadata": args.metadata,
            "session_query_titanet": args.session_query_titanet,
            "robust_query_titanet": args.robust_query_titanet,
            "session_phrase_titanet": args.session_phrase_titanet,
            "robust_phrase_titanet": args.robust_phrase_titanet,
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
    spans.add_argument("--baseline", required=True)
    spans.add_argument("--manifest", required=True)
    spans.add_argument("--config", required=True)
    spans.add_argument("--out", required=True)
    spans.add_argument("--metadata", required=True)
    build = commands.add_parser("build")
    build.add_argument("--baseline", required=True)
    build.add_argument("--metadata", required=True)
    build.add_argument("--session-query-titanet", required=True)
    build.add_argument("--robust-query-titanet", required=True)
    build.add_argument("--session-phrase-titanet", required=True)
    build.add_argument("--robust-phrase-titanet", required=True)
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
        raise SystemExit(f"speaker bracketed local churn: {error}")
