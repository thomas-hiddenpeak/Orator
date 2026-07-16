#!/usr/bin/env python3
"""Build a reference-free speaker view over complete VAD contributions."""

import argparse
import csv
import json
import os
import tomllib
from collections import defaultdict

import speaker_bracketed_local_churn_candidate as churn
import speaker_maximal_multislot_envelope_candidate as envelope_tools
import speaker_posterior_bounded_phrase_candidate as posterior
import speaker_punctuation_phrase_candidate as phrase_tools
import speaker_relative_top1_phrase_candidate as relative


EPSILON = 1e-9
REQUIRED_CONTRACTS = {
    "enabled",
    "require_single_frozen_vad_segment",
    "require_strict_native_duration_majority",
    "require_dual_query_voiceprint_agreement",
    "require_query_identity_equal_dominant_mapping",
    "veto_dual_phrase_top_rank_for_different_identity",
    "require_dual_clause_voiceprint_agreement",
    "require_clause_identity_equal_vad_identity",
    "require_selected_identity_already_in_clause",
    "require_all_conflicting_fragments_dual_identity",
    "project_complete_punctuation_clauses_per_source",
    "require_complete_alignment_in_vad",
    "require_actual_baseline_conflict",
    "reject_conflicting_overlays",
}


def load_policy(path):
    with open(path, "rb") as source:
        document = tomllib.load(source)
    section = document.get("vad_complete_contribution", {})
    fusion = document.get("speaker_fusion", {})
    numeric = {
        "frame_activity_threshold",
        "minimum_stable_run_sec",
        "maximum_query_sec",
    }
    voiceprint = {
        "short_max_sec",
        "short_min_score",
        "short_min_margin",
        "regular_min_score",
        "regular_min_margin",
    }
    required = REQUIRED_CONTRACTS | numeric | {
        "minimum_distinct_local_channels",
    }
    missing = sorted((required - set(section)) | (voiceprint - set(fusion)))
    if missing:
        raise ValueError(
            "VAD-complete contribution policy missing: " +
            ",".join(missing))
    contracts = {name: bool(section[name]) for name in REQUIRED_CONTRACTS}
    if not all(contracts.values()):
        raise ValueError("all VAD-complete contribution contracts are mandatory")
    punctuation = str(section.get("punctuation", ""))
    if not punctuation:
        raise ValueError("VAD-complete contribution punctuation is missing")
    policy = {
        **contracts,
        **{name: float(section[name]) for name in numeric},
        "minimum_distinct_local_channels": int(
            section["minimum_distinct_local_channels"]),
        "punctuation": punctuation,
        "voiceprint": {
            name: float(fusion[name]) for name in voiceprint
        },
    }
    if not 0.0 < policy["frame_activity_threshold"] < 1.0:
        raise ValueError("frame activity threshold must be between zero and one")
    if policy["minimum_stable_run_sec"] <= 0.0:
        raise ValueError("minimum stable run duration must be positive")
    if policy["maximum_query_sec"] < policy["minimum_stable_run_sec"]:
        raise ValueError("maximum query is shorter than one stable run")
    if policy["minimum_distinct_local_channels"] < 2:
        raise ValueError("VAD-complete contribution requires two local channels")
    return policy


def clipped_stable_runs(runs, start, end, minimum_duration):
    clipped = []
    for run in runs:
        clipped_start = max(start, float(run["start"]))
        clipped_end = min(end, float(run["end"]))
        if clipped_end - clipped_start + EPSILON < minimum_duration:
            continue
        clipped.append({
            **run,
            "start": clipped_start,
            "end": clipped_end,
            "clipped_to_vad": (
                clipped_start > float(run["start"]) + EPSILON or
                clipped_end < float(run["end"]) - EPSILON),
        })
    return clipped


def strict_duration_majority(runs, minimum_channels):
    totals = defaultdict(float)
    for run in runs:
        totals[int(run["local_speaker"])] += (
            float(run["end"]) - float(run["start"]))
    if len(totals) < minimum_channels:
        return dict(sorted(totals.items())), None
    ranked = sorted(totals.items(), key=lambda item: (-item[1], item[0]))
    if ranked[0][1] <= sum(value for _, value in ranked[1:]) + EPSILON:
        return dict(sorted(totals.items())), None
    return dict(sorted(totals.items())), ranked[0][0]


def contained_phrase_ids(metadata, start, end):
    return [
        str(phrase["evidence_id"])
        for phrase in metadata.get("phrases", [])
        if (float(phrase["start"]) >= start - EPSILON and
            float(phrase["end"]) <= end + EPSILON)
    ]


def positive_duration_clauses(clauses):
    return [
        clause for clause in clauses
        if float(clause["projection_end"]) >
        float(clause["projection_start"]) + EPSILON
    ]


def conflicting_fragment_queries(metadata, clause, fragments,
                                 selected_identity, punctuation):
    text_id = int(clause["text_id"])
    source = str(metadata["asr"][str(text_id)]["text"])
    times = phrase_tools.aligned_character_times(
        source, metadata["align"][str(text_id)])
    separators = set(punctuation) | set(" \t\n\r")
    queries = []
    for fragment in fragments:
        source_start = max(
            int(clause["source_start"]), int(fragment["source_start"]))
        source_end = min(
            int(clause["source_end"]), int(fragment["source_end"]))
        if source_end <= source_start:
            continue
        baseline_id = fragment["entry"].get("speaker_id")
        if baseline_id == selected_identity:
            continue
        aligned = []
        for index in range(source_start, source_end):
            if source[index] in separators:
                continue
            interval = times[index]
            if (interval is None or
                    float(interval["end"]) <=
                    float(interval["start"]) + EPSILON):
                return None
            aligned.append(interval)
        if not aligned:
            return None
        queries.append({
            "source_start": source_start,
            "source_end": source_end,
            "text": source[source_start:source_end],
            "baseline_speaker_id": baseline_id,
            "projection_start": min(float(item["start"]) for item in aligned),
            "projection_end": max(float(item["end"]) for item in aligned),
        })
    return queries


def enumerate_pieces(metadata, frames, frame_sec, vad, baseline, mapping,
                     policy):
    if metadata.get("kind") != "orator_punctuation_phrase_spans":
        raise ValueError("alignment input is not punctuation phrase metadata")
    fragments = relative.source_fragments(baseline.get("track", []))
    relative.validate_source_text(fragments, metadata, "baseline")
    stable_policy = {
        **policy,
        "minimum_outer_run_sec": policy["minimum_stable_run_sec"],
    }
    runs = churn.stable_runs(
        frames, frame_sec, float(baseline["audio_sec"]), stable_policy)
    pieces = []
    clause_count = 0
    fragment_count = 0
    for vad_index, (start, end) in enumerate(vad):
        duration = end - start
        if duration > policy["maximum_query_sec"] + EPSILON:
            continue
        inside = clipped_stable_runs(
            runs, start, end, policy["minimum_stable_run_sec"])
        totals, dominant = strict_duration_majority(
            inside, policy["minimum_distinct_local_channels"])
        if dominant is None:
            continue
        if dominant not in mapping:
            raise ValueError(f"missing mapping for local speaker {dominant}")
        contribution = {
            "query_start": start,
            "query_end": end,
            "interior_start": start,
            "interior_end": end,
        }
        clauses = envelope_tools.projection_clauses(
            metadata, contribution, fragments, mapping[dominant], policy)
        clauses = positive_duration_clauses(clauses)
        guarded_clauses = []
        for clause in clauses:
            if mapping[dominant] not in clause["baseline_identity_ids"]:
                continue
            conflicting = conflicting_fragment_queries(
                metadata, clause, fragments[int(clause["text_id"])],
                mapping[dominant], policy["punctuation"])
            if not conflicting:
                continue
            for fragment in conflicting:
                fragment["fragment_evidence_id"] = (
                    f"vad_complete_fragment:{fragment_count}")
                fragment_count += 1
            clause["conflicting_fragments"] = conflicting
            guarded_clauses.append(clause)
        clauses = guarded_clauses
        if not clauses:
            continue
        for clause in clauses:
            clause["clause_evidence_id"] = (
                f"vad_complete_clause:{clause_count}")
            clause_count += 1
        pieces.append({
            "evidence_id": f"vad_complete_contribution:{len(pieces)}",
            "vad_index": vad_index,
            "start": start,
            "end": end,
            "duration_sec": duration,
            "dominant_local_speaker": dominant,
            "mapped_dominant_identity": mapping[dominant],
            "stable_duration_by_local": {
                str(local): value for local, value in totals.items()
            },
            "stable_runs": inside,
            "projection_clauses": clauses,
            "contained_phrase_evidence_ids": contained_phrase_ids(
                metadata, start, end),
        })
    return pieces, runs


def different_phrase_veto(piece, session_phrase, robust_phrase, active_ids):
    expected = str(piece["mapped_dominant_identity"])
    audits = []
    veto = False
    for evidence_id in piece["contained_phrase_evidence_ids"]:
        if evidence_id not in session_phrase or evidence_id not in robust_phrase:
            raise ValueError(f"missing phrase evidence {evidence_id}")
        session_top, session_ranked = envelope_tools.phrase_top_rank(
            session_phrase[evidence_id], active_ids)
        robust_top, robust_ranked = envelope_tools.phrase_top_rank(
            robust_phrase[evidence_id], active_ids)
        matched = (
            session_top is not None and session_top == robust_top and
            session_top != expected)
        veto = veto or matched
        audits.append({
            "phrase_evidence_id": evidence_id,
            "session_top_ranked_identity": session_top,
            "session_ranked": session_ranked,
            "robust_top_ranked_identity": robust_top,
            "robust_ranked": robust_ranked,
            "expected_identity": expected,
            "veto": matched,
        })
    return veto, audits


def selected_clauses(piece, session_clause, robust_clause, active_ids, policy):
    expected = str(piece["mapped_dominant_identity"])
    selected = []
    audits = []
    for clause in piece["projection_clauses"]:
        evidence_id = str(clause["clause_evidence_id"])
        if evidence_id not in session_clause or evidence_id not in robust_clause:
            raise ValueError(f"missing clause evidence {evidence_id}")
        session_id, session_reason, session_ranked = phrase_tools.select_identity(
            session_clause[evidence_id], active_ids, policy["voiceprint"])
        robust_id, robust_reason, robust_ranked = phrase_tools.select_identity(
            robust_clause[evidence_id], active_ids, policy["voiceprint"])
        fragment_audit = []
        fragments_accepted = True
        for fragment in clause["conflicting_fragments"]:
            fragment_id = str(fragment["fragment_evidence_id"])
            if (fragment_id not in session_clause or
                    fragment_id not in robust_clause):
                raise ValueError(f"missing fragment evidence {fragment_id}")
            fragment_session = phrase_tools.select_identity(
                session_clause[fragment_id], active_ids,
                policy["voiceprint"])
            fragment_robust = phrase_tools.select_identity(
                robust_clause[fragment_id], active_ids,
                policy["voiceprint"])
            fragment_accepted = (
                fragment_session[0] == fragment_robust[0] == expected)
            fragments_accepted = fragments_accepted and fragment_accepted
            fragment_audit.append({
                "fragment_evidence_id": fragment_id,
                "baseline_speaker_id": fragment["baseline_speaker_id"],
                "session_identity": fragment_session[0],
                "session_reason": fragment_session[1],
                "session_ranked": fragment_session[2],
                "robust_identity": fragment_robust[0],
                "robust_reason": fragment_robust[1],
                "robust_ranked": fragment_robust[2],
                "expected_identity": expected,
                "accepted": fragment_accepted,
            })
        accepted = (
            session_id == robust_id == expected and fragments_accepted)
        audits.append({
            "clause_evidence_id": evidence_id,
            "session_identity": session_id,
            "session_reason": session_reason,
            "session_ranked": session_ranked,
            "robust_identity": robust_id,
            "robust_reason": robust_reason,
            "robust_ranked": robust_ranked,
            "expected_identity": expected,
            "fragment_audit": fragment_audit,
            "accepted": accepted,
        })
        if accepted:
            selected.append(clause)
    return selected, audits


def build_candidate(baseline, metadata, session_query, robust_query,
                    session_phrase, robust_phrase, manifest, policy):
    if metadata.get("kind") != "orator_vad_complete_contribution_spans":
        raise ValueError("metadata is not VAD-complete contribution evidence")
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
        session_id, session_reason, session_ranked = churn.select_regular(
            session_query[evidence_id], active_ids, policy["voiceprint"])
        robust_id, robust_reason, robust_ranked = churn.select_regular(
            robust_query[evidence_id], active_ids, policy["voiceprint"])
        expected = str(piece["mapped_dominant_identity"])
        veto, phrase_audit = different_phrase_veto(
            piece, session_phrase, robust_phrase, active_ids)
        clauses, clause_audit = selected_clauses(
            piece, session_query, robust_query, active_ids, policy)
        accepted = False
        reason = "vad_complete_contribution_session_abstention"
        if session_id is None:
            reason = "vad_complete_contribution_session_abstention"
        elif robust_id is None:
            reason = "vad_complete_contribution_robust_abstention"
        elif session_id != robust_id:
            reason = "vad_complete_contribution_registry_disagreement"
        elif session_id != expected:
            reason = "vad_complete_contribution_dominant_mapping_disagreement"
        elif veto:
            reason = "vad_complete_contribution_different_phrase_veto"
        elif not clauses:
            reason = "vad_complete_contribution_clause_voiceprint_abstention"
        else:
            accepted = True
            reason = "vad_complete_contribution_dual_voiceprint_consensus"
        decisions.append({
            **piece,
            "session_registry_speaker_id": session_id,
            "session_registry_reason": session_reason,
            "session_registry_ranked": session_ranked,
            "robust_gallery_speaker_id": robust_id,
            "robust_gallery_reason": robust_reason,
            "robust_gallery_ranked": robust_ranked,
            "different_phrase_veto": veto,
            "phrase_veto_audit": phrase_audit,
            "enumerated_projection_clauses": piece["projection_clauses"],
            "projection_clauses": clauses,
            "clause_voiceprint_audit": clause_audit,
            "selected_speaker_id": session_id if accepted else None,
            "accepted": accepted,
            "reason": reason,
        })

    conflicts = set()
    for left_index, left in enumerate(decisions):
        if not left["accepted"]:
            continue
        for right_index in range(left_index + 1, len(decisions)):
            right = decisions[right_index]
            if (right["accepted"] and
                    envelope_tools.decision_intervals_overlap(left, right) and
                    left["selected_speaker_id"] !=
                    right["selected_speaker_id"]):
                conflicts.update((left_index, right_index))
    for index in conflicts:
        decisions[index]["accepted"] = False
        decisions[index]["selected_speaker_id"] = None
        decisions[index]["reason"] = "vad_complete_contribution_conflicting_overlay"

    overlays = defaultdict(list)
    for decision in decisions:
        if not decision["accepted"]:
            continue
        for clause in decision["projection_clauses"]:
            overlays[int(clause["text_id"])].append({
                "source_start": int(clause["source_start"]),
                "source_end": int(clause["source_end"]),
                "speaker_id": decision["selected_speaker_id"],
                "reason": decision["reason"],
            })
    track = posterior.project_track(metadata, fragments, overlays, id_to_local)
    relative.validate_source_text(
        relative.source_fragments(track), metadata, "projected")
    return {
        "schema_version": 1,
        "kind": "orator_frozen_speaker_candidate",
        "candidate_kind": "v21_vad_complete_contribution",
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
            for clause in piece["projection_clauses"]:
                writer.writerow([
                    clause["clause_evidence_id"],
                    clause["projection_start"], clause["projection_end"]])
                for fragment in clause["conflicting_fragments"]:
                    writer.writerow([
                        fragment["fragment_evidence_id"],
                        fragment["projection_start"],
                        fragment["projection_end"]])


def command_spans(args, policy):
    alignment = posterior.load_json(args.alignment)
    baseline = posterior.load_json(args.baseline)
    manifest = posterior.load_json(args.manifest)
    mapping = {
        int(local): str(identity)
        for local, identity in manifest["mapping"].items()
    }
    frames, frame_sec = churn.read_frames(args.frames)
    vad = relative.read_vad_timeline(args.vad)
    pieces, runs = enumerate_pieces(
        alignment, frames, frame_sec, vad, baseline, mapping, policy)
    write_spans(args.out, pieces)
    result = {
        "schema_version": 1,
        "kind": "orator_vad_complete_contribution_spans",
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
                "vad": args.vad,
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
        "clauses": sum(len(item["projection_clauses"]) for item in pieces),
        "fragments": sum(
            len(clause["conflicting_fragments"])
            for item in pieces for clause in item["projection_clauses"]),
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
    spans.add_argument("--vad", required=True)
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
        raise SystemExit(f"speaker VAD-complete contribution: {error}")
