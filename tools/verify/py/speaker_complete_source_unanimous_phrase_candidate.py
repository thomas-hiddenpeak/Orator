#!/usr/bin/env python3
"""Build phrase-exact speaker evidence from unanimous complete-source context."""

import argparse
import csv
import json
import os
import tomllib
from collections import defaultdict

import speaker_bracketed_local_churn_candidate as churn
import speaker_complete_source_voiceprint_candidate as complete_source
import speaker_maximal_multislot_envelope_candidate as envelope_tools
import speaker_posterior_bounded_phrase_candidate as posterior
import speaker_punctuation_phrase_candidate as phrase_tools
import speaker_relative_top1_phrase_candidate as relative


EPSILON = 1e-9
REQUIRED_CONTRACTS = {
    "enabled",
    "require_complete_source_alignment",
    "require_at_least_one_indexed_phrase",
    "require_each_phrase_in_one_vad_segment",
    "require_dual_source_voiceprint_agreement",
    "require_selected_native_channel_stable_support",
    "require_all_phrase_dual_top_rank_source_identity",
    "require_selected_identity_at_phrase_boundaries",
    "project_only_complete_indexed_phrases",
    "require_actual_baseline_conflict",
    "reject_conflicting_overlays",
}


def load_policy(path):
    with open(path, "rb") as source:
        document = tomllib.load(source)
    section = document.get("complete_source_unanimous_phrase", {})
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
    missing = sorted(
        ((REQUIRED_CONTRACTS | numeric) - set(section)) |
        (voiceprint - set(fusion)))
    if missing:
        raise ValueError(
            "complete-source unanimous-phrase policy missing: " +
            ",".join(missing))
    contracts = {name: bool(section[name]) for name in REQUIRED_CONTRACTS}
    if not all(contracts.values()):
        raise ValueError("all complete-source unanimous contracts are mandatory")
    punctuation = str(section.get("punctuation", ""))
    if not punctuation:
        raise ValueError("complete-source unanimous punctuation is missing")
    policy = {
        **contracts,
        **{name: float(section[name]) for name in numeric},
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
        raise ValueError("maximum query is shorter than stable support")
    return policy


def containing_vad_index(start, end, vad):
    for index, (vad_start, vad_end) in enumerate(vad):
        if start >= vad_start - EPSILON and end <= vad_end + EPSILON:
            return index
    return None


def stable_source_locals(runs, start, end, minimum_duration):
    locals_ = set()
    records = []
    for run in runs:
        overlap_start = max(start, float(run["start"]))
        overlap_end = min(end, float(run["end"]))
        if overlap_end - overlap_start + EPSILON < minimum_duration:
            continue
        local = int(run["local_speaker"])
        locals_.add(local)
        records.append({
            "start": overlap_start,
            "end": overlap_end,
            "local_speaker": local,
            "source_run": run,
        })
    return sorted(locals_), records


def phrase_baseline_ownership(phrase, fragments):
    overlaps = posterior.overlapping_fragments(phrase, fragments)
    identities = sorted({
        str(item["entry"]["speaker_id"])
        for item in overlaps if item["entry"].get("speaker_id") is not None
    })
    has_unknown = any(
        item["entry"].get("speaker_id") is None for item in overlaps)
    first_identity = overlaps[0]["entry"].get("speaker_id") if overlaps else None
    last_identity = overlaps[-1]["entry"].get("speaker_id") if overlaps else None
    return identities, has_unknown, first_identity, last_identity


def enumerate_pieces(metadata, frames, frame_sec, vad, baseline, policy):
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
    phrases_by_text = defaultdict(list)
    for phrase in metadata.get("phrases", []):
        phrases_by_text[int(phrase["text_id"])].append(phrase)

    pieces = []
    for text_id in sorted(int(value) for value in metadata["asr"]):
        source_record = metadata["asr"][str(text_id)]
        start = float(source_record["start"])
        end = float(source_record["end"])
        if end - start > policy["maximum_query_sec"] + EPSILON:
            continue
        source = str(source_record["text"])
        if not complete_source.complete_alignment_in_query(
                source, metadata["align"][str(text_id)], start, end):
            continue
        indexed = phrases_by_text.get(text_id, [])
        if not indexed:
            continue
        phrase_records = []
        phrases_complete = True
        for phrase in indexed:
            if not churn.phrase_alignment_complete(
                    metadata, phrase, start, end):
                phrases_complete = False
                break
            vad_index = containing_vad_index(
                float(phrase["start"]), float(phrase["end"]), vad)
            if vad_index is None:
                phrases_complete = False
                break
            identities, has_unknown, first_identity, last_identity = (
                phrase_baseline_ownership(
                phrase, fragments[text_id])
            )
            phrase_records.append({
                **phrase,
                "vad_index": vad_index,
                "baseline_identity_ids": identities,
                "baseline_has_unknown_identity": has_unknown,
                "baseline_first_identity": first_identity,
                "baseline_last_identity": last_identity,
            })
        if not phrases_complete:
            continue
        stable_locals, stable_records = stable_source_locals(
            runs, start, end, policy["minimum_stable_run_sec"])
        if not stable_locals:
            continue
        pieces.append({
            "evidence_id": f"complete_source_unanimous_phrase:{len(pieces)}",
            "text_id": text_id,
            "start": start,
            "end": end,
            "duration_sec": end - start,
            "text": source,
            "stable_local_speakers": stable_locals,
            "stable_support": stable_records,
            "indexed_phrases": phrase_records,
        })
    return pieces, runs


def phrase_unanimity(piece, session_phrase, robust_phrase, active_ids,
                     selected_identity):
    audits = []
    unanimous = True
    for phrase in piece["indexed_phrases"]:
        evidence_id = str(phrase["evidence_id"])
        if evidence_id not in session_phrase or evidence_id not in robust_phrase:
            raise ValueError(f"missing phrase evidence {evidence_id}")
        session_top, session_ranked = envelope_tools.phrase_top_rank(
            session_phrase[evidence_id], active_ids)
        robust_top, robust_ranked = envelope_tools.phrase_top_rank(
            robust_phrase[evidence_id], active_ids)
        accepted = session_top == robust_top == selected_identity
        unanimous = unanimous and accepted
        audits.append({
            "phrase_evidence_id": evidence_id,
            "session_top_ranked_identity": session_top,
            "session_ranked": session_ranked,
            "robust_top_ranked_identity": robust_top,
            "robust_ranked": robust_ranked,
            "selected_identity": selected_identity,
            "accepted": accepted,
        })
    return unanimous, audits


def intervals_overlap(left, right):
    if int(left["text_id"]) != int(right["text_id"]):
        return False
    return any(
        int(a["source_start"]) < int(b["source_end"]) and
        int(a["source_end"]) > int(b["source_start"])
        for a in left["selected_projection_phrases"]
        for b in right["selected_projection_phrases"])


def build_candidate(baseline, metadata, session_source, robust_source,
                    session_phrase, robust_phrase, manifest, policy):
    if metadata.get("kind") != "orator_complete_source_unanimous_phrase_spans":
        raise ValueError("metadata is not complete-source unanimous evidence")
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
        if evidence_id not in session_source or evidence_id not in robust_source:
            raise ValueError(f"missing source evidence {evidence_id}")
        session_id, session_reason, session_ranked = phrase_tools.select_identity(
            session_source[evidence_id], active_ids, policy["voiceprint"])
        robust_id, robust_reason, robust_ranked = phrase_tools.select_identity(
            robust_source[evidence_id], active_ids, policy["voiceprint"])
        selected = session_id if session_id == robust_id else None
        selected_local = id_to_local.get(selected) if selected is not None else None
        native_support = selected_local in piece["stable_local_speakers"]
        unanimous, phrase_audit = phrase_unanimity(
            piece, session_phrase, robust_phrase, active_ids, selected)
        projected = []
        if selected is not None:
            for phrase in piece["indexed_phrases"]:
                baseline_ids = [str(value) for value in
                                phrase["baseline_identity_ids"]]
                bounded = (
                    phrase["baseline_first_identity"] == selected and
                    phrase["baseline_last_identity"] == selected)
                if (bounded and
                        (phrase["baseline_has_unknown_identity"] or
                         baseline_ids != [selected])):
                    projected.append(phrase)
        accepted = False
        reason = "complete_source_unanimous_session_abstention"
        if session_id is None:
            reason = "complete_source_unanimous_session_abstention"
        elif robust_id is None:
            reason = "complete_source_unanimous_robust_abstention"
        elif session_id != robust_id:
            reason = "complete_source_unanimous_registry_disagreement"
        elif not native_support:
            reason = "complete_source_unanimous_native_support_missing"
        elif not unanimous:
            reason = "complete_source_unanimous_phrase_disagreement"
        elif not projected:
            reason = "complete_source_unanimous_no_phrase_conflict"
        else:
            accepted = True
            reason = "complete_source_unanimous_dual_voiceprint_consensus"
        decisions.append({
            **piece,
            "session_registry_speaker_id": session_id,
            "session_registry_reason": session_reason,
            "session_registry_ranked": session_ranked,
            "robust_gallery_speaker_id": robust_id,
            "robust_gallery_reason": robust_reason,
            "robust_gallery_ranked": robust_ranked,
            "selected_local_speaker": selected_local,
            "selected_native_support": native_support,
            "phrase_unanimity": unanimous,
            "phrase_audit": phrase_audit,
            "selected_projection_phrases": projected,
            "selected_speaker_id": selected if accepted else None,
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
        decisions[index]["reason"] = "complete_source_unanimous_conflicting_overlay"

    overlays = defaultdict(list)
    for decision in decisions:
        if not decision["accepted"]:
            continue
        for phrase in decision["selected_projection_phrases"]:
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
        "candidate_kind": "v21_complete_source_unanimous_phrase",
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
    frames, frame_sec = churn.read_frames(args.frames)
    vad = relative.read_vad_timeline(args.vad)
    pieces, runs = enumerate_pieces(
        alignment, frames, frame_sec, vad, baseline, policy)
    write_spans(args.out, pieces)
    result = {
        "schema_version": 1,
        "kind": "orator_complete_source_unanimous_phrase_spans",
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
        phrase_tools.read_titanet(args.session_source_titanet),
        phrase_tools.read_titanet(args.robust_source_titanet),
        phrase_tools.read_titanet(args.session_phrase_titanet),
        phrase_tools.read_titanet(args.robust_phrase_titanet),
        posterior.load_json(args.manifest), policy)
    result["sources"] = {
        name: {"path": os.path.abspath(path),
               "sha256": posterior.sha256_file(path)}
        for name, path in {
            "baseline": args.baseline,
            "metadata": args.metadata,
            "session_source_titanet": args.session_source_titanet,
            "robust_source_titanet": args.robust_source_titanet,
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
    spans.add_argument("--config", required=True)
    spans.add_argument("--out", required=True)
    spans.add_argument("--metadata", required=True)
    build = commands.add_parser("build")
    build.add_argument("--baseline", required=True)
    build.add_argument("--metadata", required=True)
    build.add_argument("--session-source-titanet", required=True)
    build.add_argument("--robust-source-titanet", required=True)
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
        raise SystemExit(f"speaker complete-source unanimous phrase: {error}")
