#!/usr/bin/env python3
"""Build a reference-free speaker view over maximal multi-slot envelopes."""

import argparse
import csv
import json
import os
import tomllib
from collections import defaultdict

import speaker_bracketed_local_churn_candidate as churn
import speaker_complete_edge_contribution_candidate as complete_edge
import speaker_posterior_bounded_phrase_candidate as posterior
import speaker_punctuation_phrase_candidate as phrase_tools
import speaker_relative_top1_phrase_candidate as relative


EPSILON = 1e-9
REQUIRED_CONTRACTS = {
    "enabled",
    "require_same_outer_local_channel",
    "select_farthest_fitting_closure",
    "maximize_available_outer_audio",
    "require_combined_outer_duration_over_foreign_duration",
    "require_dual_query_voiceprint_agreement",
    "require_query_identity_equal_outer_mapping",
    "veto_dominant_foreign_dual_phrase_top_rank",
    "project_complete_punctuation_clauses_per_source",
    "require_complete_alignment_in_query",
    "require_projection_clauses_in_one_vad_segment",
    "reject_conflicting_overlays",
}


def load_policy(path):
    with open(path, "rb") as source:
        document = tomllib.load(source)
    section = document.get("maximal_multislot_envelope", {})
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
    required = REQUIRED_CONTRACTS | numeric | {
        "minimum_distinct_foreign_channels",
    }
    missing = sorted((required - set(section)) | (voiceprint - set(fusion)))
    if missing:
        raise ValueError(
            "maximal multi-slot envelope policy missing: " +
            ",".join(missing))
    contracts = {name: bool(section[name]) for name in REQUIRED_CONTRACTS}
    if not all(contracts.values()):
        raise ValueError("all maximal multi-slot contracts are mandatory")
    punctuation = str(section.get("punctuation", ""))
    if not punctuation:
        raise ValueError("maximal multi-slot punctuation is missing")
    policy = {
        **contracts,
        **{name: float(section[name]) for name in numeric},
        "minimum_distinct_foreign_channels": int(
            section["minimum_distinct_foreign_channels"]),
        "punctuation": punctuation,
        "voiceprint": {
            name: float(fusion[name]) for name in voiceprint
        },
    }
    if not 0.0 < policy["frame_activity_threshold"] < 1.0:
        raise ValueError("frame activity threshold must be between zero and one")
    if policy["minimum_outer_run_sec"] <= 0.0:
        raise ValueError("minimum outer run duration must be positive")
    if policy["maximum_query_sec"] < 2.0 * policy[
            "minimum_outer_run_sec"]:
        raise ValueError("maximum query cannot contain both outer supports")
    if policy["minimum_distinct_foreign_channels"] < 2:
        raise ValueError("multi-slot envelope requires at least two foreign channels")
    return policy


def run_duration(run):
    return float(run["end"]) - float(run["start"])


def query_bounds(left, right, policy):
    minimum = policy["minimum_outer_run_sec"]
    maximum = policy["maximum_query_sec"]
    required_start = float(left["end"]) - minimum
    required_end = float(right["start"]) + minimum
    required_duration = required_end - required_start
    if required_duration > maximum + EPSILON:
        return None

    remaining = maximum - required_duration
    left_available = max(0.0, required_start - float(left["start"]))
    right_available = max(0.0, float(right["end"]) - required_end)
    half = 0.5 * remaining
    left_extra = min(left_available, half)
    right_extra = min(right_available, half)
    unused = max(0.0, remaining - left_extra - right_extra)
    if left_extra + EPSILON < half:
        addition = min(unused, right_available - right_extra)
        right_extra += addition
        unused -= addition
    if right_extra + EPSILON < half:
        addition = min(unused, left_available - left_extra)
        left_extra += addition
    return required_start - left_extra, required_end + right_extra


def foreign_duration_summary(inner_runs, outer_local):
    totals = defaultdict(float)
    for run in inner_runs:
        local = int(run["local_speaker"])
        if local != outer_local:
            totals[local] += run_duration(run)
    if not totals:
        return {}, None
    ranked = sorted(totals.items(), key=lambda item: (-item[1], item[0]))
    if (len(ranked) > 1 and
            abs(ranked[0][1] - ranked[1][1]) <= EPSILON):
        return dict(sorted(totals.items())), None
    return dict(sorted(totals.items())), ranked[0][0]


def maximal_envelopes(runs, policy):
    envelopes = []
    minimum = policy["minimum_outer_run_sec"]
    maximum = policy["maximum_query_sec"]
    required_foreign = policy["minimum_distinct_foreign_channels"]
    for left_index, left in enumerate(runs):
        outer_local = int(left["local_speaker"])
        selected = None
        for right_index in range(left_index + 1, len(runs)):
            right = runs[right_index]
            if (float(right["start"]) - float(left["end"]) +
                    2.0 * minimum > maximum + EPSILON):
                break
            if int(right["local_speaker"]) != outer_local:
                continue
            inner_runs = runs[left_index + 1:right_index]
            totals, dominant = foreign_duration_summary(
                inner_runs, outer_local)
            if len(totals) < required_foreign or dominant is None:
                continue
            foreign_duration = sum(totals.values())
            outer_duration = run_duration(left) + run_duration(right)
            if outer_duration <= foreign_duration + EPSILON:
                continue
            bounds = query_bounds(left, right, policy)
            if bounds is None:
                continue
            selected = {
                "left_run_index": left_index,
                "right_run_index": right_index,
                "left_run": left,
                "right_run": right,
                "inner_runs": inner_runs,
                "outer_local_speaker": outer_local,
                "foreign_duration_by_local": {
                    str(local): duration
                    for local, duration in totals.items()
                },
                "foreign_duration_sec": foreign_duration,
                "outer_duration_sec": outer_duration,
                "dominant_foreign_local_speaker": dominant,
                "interior_start": float(left["end"]),
                "interior_end": float(right["start"]),
                "query_start": bounds[0],
                "query_end": bounds[1],
            }
        if selected is not None:
            envelopes.append(selected)
    return envelopes


def clause_ownership(clause, fragments):
    overlaps = posterior.overlapping_fragments(clause, fragments)
    identities = sorted({
        str(item["entry"]["speaker_id"])
        for item in overlaps if item["entry"].get("speaker_id") is not None
    })
    return identities, any(
        item["entry"].get("speaker_id") is None for item in overlaps)


def projection_clauses(metadata, envelope, fragments_by_text, outer_identity,
                       policy):
    query = {"start": envelope["query_start"],
             "end": envelope["query_end"]}
    selected = []
    for text_id in sorted(int(value) for value in metadata["asr"]):
        asr = metadata["asr"][str(text_id)]
        if (float(asr["end"]) <= envelope["interior_start"] + EPSILON or
                float(asr["start"]) >=
                envelope["interior_end"] - EPSILON):
            continue
        source = str(asr["text"])
        times = phrase_tools.aligned_character_times(
            source, metadata["align"][str(text_id)])
        complete = []
        for source_start, source_end in phrase_tools.phrase_ranges(
                source, policy["punctuation"]):
            clause = complete_edge.complete_clause(
                source, times, source_start, source_end, query,
                policy["punctuation"])
            if clause is None:
                continue
            if (float(clause["projection_end"]) <=
                    envelope["interior_start"] + EPSILON or
                    float(clause["projection_start"]) >=
                    envelope["interior_end"] - EPSILON):
                continue
            complete.append(clause)
        for group_index, group in enumerate(
                complete_edge.group_adjacent(complete)):
            for clause_index, clause in enumerate(group):
                identities, has_unknown = clause_ownership(
                    clause, fragments_by_text[text_id])
                if not has_unknown and identities == [outer_identity]:
                    continue
                selected.append({
                    **clause,
                    "text_id": text_id,
                    "text": source[
                        int(clause["source_start"]):
                        int(clause["source_end"])],
                    "source_group": group_index,
                    "source_group_clause": clause_index,
                    "baseline_identity_ids": identities,
                    "baseline_has_unknown_identity": has_unknown,
                })
    return selected


def intersecting_phrase_ids(metadata, envelope):
    return [
        str(phrase["evidence_id"])
        for phrase in metadata.get("phrases", [])
        if (float(phrase["end"]) > envelope["interior_start"] + EPSILON and
            float(phrase["start"]) < envelope["interior_end"] - EPSILON)
    ]


def containing_vad_segment(clauses, vad):
    for index, (start, end) in enumerate(vad):
        if all(
                float(clause["projection_start"]) >= start - EPSILON and
                float(clause["projection_end"]) <= end + EPSILON
                for clause in clauses):
            return index, start, end
    return None


def enumerate_pieces(metadata, frames, frame_sec, vad, baseline, mapping,
                     policy):
    if metadata.get("kind") != "orator_punctuation_phrase_spans":
        raise ValueError("alignment input is not punctuation phrase metadata")
    fragments = relative.source_fragments(baseline.get("track", []))
    relative.validate_source_text(fragments, metadata, "baseline")
    runs = churn.stable_runs(
        frames, frame_sec, float(baseline["audio_sec"]), policy)
    pieces = []
    for envelope in maximal_envelopes(runs, policy):
        outer_local = int(envelope["outer_local_speaker"])
        dominant_local = int(envelope["dominant_foreign_local_speaker"])
        if outer_local not in mapping or dominant_local not in mapping:
            raise ValueError("stable local speaker is missing from mapping")
        clauses = projection_clauses(
            metadata, envelope, fragments, mapping[outer_local], policy)
        if not clauses:
            continue
        vad_segment = containing_vad_segment(clauses, vad)
        if vad_segment is None:
            continue
        pieces.append({
            "evidence_id": f"maximal_multislot_envelope:{len(pieces)}",
            **envelope,
            "mapped_outer_identity": mapping[outer_local],
            "mapped_dominant_foreign_identity": mapping[dominant_local],
            "projection_vad_index": vad_segment[0],
            "projection_vad_start": vad_segment[1],
            "projection_vad_end": vad_segment[2],
            "projection_clauses": clauses,
            "intersecting_phrase_evidence_ids": intersecting_phrase_ids(
                metadata, envelope),
        })
    return pieces, runs


def phrase_top_rank(evidence, active_ids):
    if evidence.get("status") != "ok":
        return None, []
    scores = evidence.get("scores", {})
    ranked = sorted(
        ((identity, float(scores[identity]))
         for identity in active_ids if identity in scores),
        key=lambda item: (-item[1], item[0]))
    if len(ranked) < 2:
        return None, ranked
    return ranked[0][0], ranked


def dominant_phrase_veto(piece, session_phrase, robust_phrase, active_ids):
    dominant = str(piece["mapped_dominant_foreign_identity"])
    audits = []
    veto = False
    for evidence_id in piece["intersecting_phrase_evidence_ids"]:
        if evidence_id not in session_phrase or evidence_id not in robust_phrase:
            raise ValueError(f"missing phrase evidence {evidence_id}")
        session_top, session_ranked = phrase_top_rank(
            session_phrase[evidence_id], active_ids)
        robust_top, robust_ranked = phrase_top_rank(
            robust_phrase[evidence_id], active_ids)
        matched = session_top == robust_top == dominant
        veto = veto or matched
        audits.append({
            "phrase_evidence_id": evidence_id,
            "session_top_ranked_identity": session_top,
            "session_ranked": session_ranked,
            "robust_top_ranked_identity": robust_top,
            "robust_ranked": robust_ranked,
            "dominant_foreign_identity": dominant,
            "veto": matched,
        })
    return veto, audits


def decision_intervals_overlap(left, right):
    return any(
        int(a["text_id"]) == int(b["text_id"]) and
        int(a["source_start"]) < int(b["source_end"]) and
        int(a["source_end"]) > int(b["source_start"])
        for a in left["projection_clauses"]
        for b in right["projection_clauses"])


def build_candidate(baseline, metadata, session_query, robust_query,
                    session_phrase, robust_phrase, manifest, policy):
    if metadata.get("kind") != "orator_maximal_multislot_envelope_spans":
        raise ValueError("metadata is not maximal multi-slot envelope evidence")
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
        expected = str(piece["mapped_outer_identity"])
        veto, phrase_audit = dominant_phrase_veto(
            piece, session_phrase, robust_phrase, active_ids)
        accepted = False
        reason = "maximal_multislot_envelope_session_abstention"
        if session_id is None:
            reason = "maximal_multislot_envelope_session_abstention"
        elif robust_id is None:
            reason = "maximal_multislot_envelope_robust_abstention"
        elif session_id != robust_id:
            reason = "maximal_multislot_envelope_registry_disagreement"
        elif session_id != expected:
            reason = "maximal_multislot_envelope_outer_mapping_disagreement"
        elif veto:
            reason = "maximal_multislot_envelope_dominant_foreign_phrase_veto"
        elif not piece["projection_clauses"]:
            reason = "maximal_multislot_envelope_no_projection"
        else:
            accepted = True
            reason = "maximal_multislot_envelope_dual_voiceprint_consensus"
        decisions.append({
            **piece,
            "session_registry_speaker_id": session_id,
            "session_registry_reason": session_reason,
            "session_registry_ranked": session_ranked,
            "robust_gallery_speaker_id": robust_id,
            "robust_gallery_reason": robust_reason,
            "robust_gallery_ranked": robust_ranked,
            "dominant_foreign_phrase_veto": veto,
            "phrase_veto_audit": phrase_audit,
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
                    decision_intervals_overlap(left, right) and
                    left["selected_speaker_id"] !=
                    right["selected_speaker_id"]):
                conflicts.update((left_index, right_index))
    for index in conflicts:
        decisions[index]["accepted"] = False
        decisions[index]["selected_speaker_id"] = None
        decisions[index]["reason"] = (
            "maximal_multislot_envelope_conflicting_overlay")

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
        "candidate_kind": "v21_maximal_multislot_envelope_contribution",
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
            writer.writerow([
                piece["evidence_id"], piece["query_start"], piece["query_end"]])


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
        "kind": "orator_maximal_multislot_envelope_spans",
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
        raise SystemExit(f"speaker maximal multi-slot envelope: {error}")
