#!/usr/bin/env python3
"""Compose phrase-led outer-abstention challenges without result judgment."""

import argparse
import csv
import hashlib
import json
import os
import tomllib
from collections import defaultdict

import speaker_posterior_bounded_phrase_candidate as posterior
import speaker_primary_aligned_island_candidate as primary_tools
import speaker_punctuation_phrase_candidate as phrase_tools
import speaker_reviewed_overlay_candidate as overlay_tools


DECISION_REASON = "phrase_led_outer_abstention_challenge"


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_json(path):
    with open(path, encoding="utf-8") as source:
        return json.load(source)


def load_policy(path):
    with open(path, "rb") as source:
        document = tomllib.load(source)
    fusion = document.get("speaker_fusion", {})
    section = document.get("phrase_led_outer_abstention", {})
    fusion_names = (
        "short_max_sec", "short_min_score", "short_min_margin",
        "regular_min_score", "regular_min_margin",
    )
    safeguards = {
        "enabled", "require_one_containing_vad",
        "require_one_covering_primary_run",
        "require_uniform_mapped_baseline_identity",
        "require_allowed_unconfirmed_baseline_provenance",
        "require_outer_dual_abstention",
        "require_outer_top_rank_mapped_identity",
        "require_phrase_same_top_rank",
        "require_exactly_one_eligible_phrase_view",
        "require_other_phrase_view_margin_only_abstention",
        "reject_conflicting_overlays", "reject_overlapping_proposals",
    }
    required = safeguards | {
        "boundary_tolerance_frames", "allowed_baseline_reasons",
        "protected_decision_reasons",
    }
    missing = sorted((set(fusion_names) - set(fusion)) |
                     (required - set(section)))
    if missing:
        raise ValueError("phrase-led abstention policy missing: " +
                         ",".join(missing))
    if any(section[name] is not True for name in safeguards):
        raise ValueError("phrase-led abstention safeguards are mandatory")
    if int(section["boundary_tolerance_frames"]) != 1:
        raise ValueError("boundary tolerance must be one frozen frame")
    allowed = [str(value) for value in section["allowed_baseline_reasons"]]
    protected = [str(value)
                 for value in section["protected_decision_reasons"]]
    if (not allowed or not protected or
            len(set(allowed)) != len(allowed) or
            len(set(protected)) != len(protected) or
            set(allowed) & set(protected)):
        raise ValueError("baseline provenance lists are invalid")
    return {
        **{name: float(fusion[name]) for name in fusion_names},
        "boundary_tolerance_frames": 1,
        "allowed_baseline_reasons": allowed,
        "protected_decision_reasons": protected,
    }


def read_vad(path):
    output = []
    with open(path, encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source, delimiter="\t")
        required = {"evidence_id", "start_sec", "end_sec"}
        if not required.issubset(reader.fieldnames or []):
            raise ValueError("VAD evidence columns are incomplete")
        for row in reader:
            output.append({
                "evidence_id": str(row["evidence_id"]),
                "start": float(row["start_sec"]),
                "end": float(row["end_sec"]),
            })
    if not output:
        raise ValueError("VAD evidence is empty")
    return output


def labels_for_source(fragments, source):
    labels = []
    for fragment in fragments:
        entry = fragment["entry"]
        labels.extend({
            "speaker_id": entry.get("speaker_id"),
            "reason": str(entry.get("decision_reason", "baseline")),
        } for _ in str(entry.get("text", "")))
    if len(labels) != len(source):
        raise ValueError("baseline source and labels differ")
    return labels


def evidence_view(evidence, active_ids, policy):
    identity, reason, ranked = phrase_tools.select_identity(
        evidence, active_ids, policy)
    return {
        "identity": identity,
        "reason": reason,
        "ranked": [
            {"speaker_id": speaker_id, "score": score}
            for speaker_id, score in ranked
        ],
    }


def top_identity(view):
    if not view["ranked"]:
        return None
    return view["ranked"][0]["speaker_id"]


def overlaps(left, right):
    return (left["text_id"] == right["text_id"] and
            left["source_start"] < right["source_end"] and
            right["source_start"] < left["source_end"])


def build_candidate(baseline, metadata, vad, primary, frame_period,
                    vad_session, vad_robust, phrase_session, phrase_robust,
                    mapping_document, policy):
    if metadata.get("kind") != "orator_punctuation_phrase_spans":
        raise ValueError("metadata is not punctuation phrase evidence")
    if frame_period <= 0.0:
        raise ValueError("native frame period must be positive")
    mapping = overlay_tools.read_mapping(mapping_document)
    id_to_local = {identity: local for local, identity in mapping.items()}
    active_ids = sorted(id_to_local)
    if sorted(baseline.get("active_speaker_ids", [])) != active_ids:
        raise ValueError("baseline identities differ from mapping")
    if any(mapping.get(item["local"]) != item["speaker_id"]
           for item in primary):
        raise ValueError("primary local/global identity differs from mapping")
    fragments = overlay_tools.candidate_fragments(baseline)
    overlay_tools.validate_candidate_sources(fragments, metadata)
    labels = {
        text_id: labels_for_source(
            values, str(metadata["asr"][str(text_id)]["text"]))
        for text_id, values in fragments.items()
    }
    tolerance = policy["boundary_tolerance_frames"] * frame_period
    allowed = set(policy["allowed_baseline_reasons"])
    protected = set(policy["protected_decision_reasons"])
    decisions = []
    proposals = defaultdict(list)
    for phrase in metadata.get("phrases", []):
        containing = [item for item in vad if
                      float(phrase["start"]) >= item["start"] - tolerance and
                      float(phrase["end"]) <= item["end"] + tolerance]
        covering = [item for item in primary if
                    float(phrase["start"]) >= item["start"] - tolerance and
                    float(phrase["end"]) <= item["end"] + tolerance]
        if len(containing) != 1 or len(covering) != 1:
            continue
        vad_item = containing[0]
        primary_item = covering[0]
        vad_id = vad_item["evidence_id"]
        phrase_id = str(phrase["evidence_id"])
        evidence = {
            "vad_session": vad_session.get(vad_id),
            "vad_robust": vad_robust.get(vad_id),
            "phrase_session": phrase_session.get(phrase_id),
            "phrase_robust": phrase_robust.get(phrase_id),
        }
        if any(value is None for value in evidence.values()):
            raise ValueError("missing phrase-led voiceprint evidence")
        views = {name: evidence_view(value, active_ids, policy)
                 for name, value in evidence.items()}
        mapped_id = primary_item["speaker_id"]
        text_id = int(phrase["text_id"])
        source_start = int(phrase["source_start"])
        source_end = int(phrase["source_end"])
        range_labels = labels[text_id][source_start:source_end]
        baseline_ids = {value["speaker_id"] for value in range_labels}
        baseline_reasons = {value["reason"] for value in range_labels}
        decision = {
            "vad_evidence_id": vad_id,
            "phrase_evidence_id": phrase_id,
            "primary_start": primary_item["start"],
            "primary_end": primary_item["end"],
            "mapped_speaker_id": mapped_id,
            "text_id": text_id,
            "source_start": source_start,
            "source_end": source_end,
            "start": float(phrase["start"]),
            "end": float(phrase["end"]),
            "views": views,
            "baseline_ids": sorted(identity for identity in baseline_ids
                                   if identity is not None),
            "baseline_reasons": sorted(baseline_reasons),
            "selected_speaker_id": None,
            "accepted": False,
            "reason": "baseline_identity_not_uniform_mapped",
        }
        if baseline_ids != {mapped_id}:
            decisions.append(decision)
            continue
        if not baseline_reasons or not baseline_reasons.issubset(allowed):
            decision["reason"] = "baseline_provenance_not_allowed"
            decisions.append(decision)
            continue
        if baseline_reasons & protected:
            decision["reason"] = "protected_overlay_conflict"
            decisions.append(decision)
            continue
        outer = [views["vad_session"], views["vad_robust"]]
        if any(view["identity"] is not None for view in outer):
            decision["reason"] = "outer_vad_eligible_veto"
            decisions.append(decision)
            continue
        if any(top_identity(view) != mapped_id for view in outer):
            decision["reason"] = "outer_vad_top_rank_disagreement"
            decisions.append(decision)
            continue
        phrase_views = [views["phrase_session"], views["phrase_robust"]]
        phrase_tops = [top_identity(view) for view in phrase_views]
        if None in phrase_tops or len(set(phrase_tops)) != 1:
            decision["reason"] = "phrase_top_rank_disagreement"
            decisions.append(decision)
            continue
        challenge_id = phrase_tops[0]
        if challenge_id == mapped_id:
            decision["reason"] = "phrase_identity_conflict_missing"
            decisions.append(decision)
            continue
        eligible = [view for view in phrase_views
                    if view["identity"] == challenge_id]
        abstained = [view for view in phrase_views
                     if view["identity"] is None]
        if len(eligible) != 1 or len(abstained) != 1:
            decision["reason"] = "phrase_single_eligible_view_missing"
            decisions.append(decision)
            continue
        if not abstained[0]["reason"].endswith("_margin_below_gate"):
            decision["reason"] = "phrase_margin_only_abstention_missing"
            decisions.append(decision)
            continue
        decision["selected_speaker_id"] = challenge_id
        decision["accepted"] = True
        decision["reason"] = DECISION_REASON
        proposals[text_id].append(decision)
        decisions.append(decision)

    for values in proposals.values():
        conflicted = set()
        ordered = sorted(values, key=lambda item: (
            item["source_start"], item["source_end"],
            item["phrase_evidence_id"]))
        for index, left in enumerate(ordered):
            for right in ordered[index + 1:]:
                if right["source_start"] >= left["source_end"]:
                    break
                if overlaps(left, right):
                    conflicted.add(left["phrase_evidence_id"])
                    conflicted.add(right["phrase_evidence_id"])
        for item in values:
            if item["phrase_evidence_id"] in conflicted:
                item["accepted"] = False
                item["selected_speaker_id"] = None
                item["reason"] = "overlapping_proposal_abstention"

    overlays = defaultdict(list)
    for values in proposals.values():
        for item in values:
            if item["accepted"]:
                overlays[item["text_id"]].append({
                    "text_id": item["text_id"],
                    "source_start": item["source_start"],
                    "source_end": item["source_end"],
                    "speaker_id": item["selected_speaker_id"],
                    "reason": DECISION_REASON,
                })
    overlay_tools.validate_overlays(overlays)
    track = []
    for text_id in sorted(fragments):
        source = str(metadata["asr"][str(text_id)]["text"])
        track.extend(overlay_tools.reproject_preserving_base(
            text_id, source, metadata["align"][str(text_id)],
            fragments[text_id], overlays.get(text_id, []), id_to_local))
    return {
        "schema_version": 1,
        "kind": "orator_frozen_speaker_candidate",
        "candidate_kind": "v21_phrase_led_outer_abstention_challenge",
        "audio_sec": float(baseline["audio_sec"]),
        "sample_rate": int(baseline["sample_rate"]),
        "active_speaker_ids": active_ids,
        "source_turn_count": len(baseline.get("track", [])),
        "turn_count": len(track),
        "accepted_challenge_count": sum(item["accepted"]
                                        for item in decisions),
        "policy": policy,
        "challenge_decisions": decisions,
        "track": track,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", required=True)
    parser.add_argument("--metadata", required=True)
    parser.add_argument("--vad", required=True)
    parser.add_argument("--primary", required=True)
    parser.add_argument("--frames", required=True)
    parser.add_argument("--vad-session", required=True)
    parser.add_argument("--vad-robust", required=True)
    parser.add_argument("--phrase-session", required=True)
    parser.add_argument("--phrase-robust", required=True)
    parser.add_argument("--mapping", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()
    _, frame_period = posterior.read_frames(args.frames)
    result = build_candidate(
        load_json(args.baseline), load_json(args.metadata), read_vad(args.vad),
        primary_tools.read_diarization(args.primary, primary=True), frame_period,
        phrase_tools.read_titanet(args.vad_session),
        phrase_tools.read_titanet(args.vad_robust),
        phrase_tools.read_titanet(args.phrase_session),
        phrase_tools.read_titanet(args.phrase_robust), load_json(args.mapping),
        load_policy(args.config))
    sources = {
        "baseline": args.baseline, "metadata": args.metadata,
        "vad": args.vad, "primary": args.primary, "frames": args.frames,
        "vad_session": args.vad_session, "vad_robust": args.vad_robust,
        "phrase_session": args.phrase_session,
        "phrase_robust": args.phrase_robust, "mapping": args.mapping,
        "config": args.config,
    }
    result["sources"] = {
        name: {"path": os.path.abspath(path), "sha256": sha256_file(path)}
        for name, path in sources.items()
    }
    with open(args.out, "w", encoding="utf-8") as output:
        json.dump(result, output, ensure_ascii=False, indent=2)
    print(json.dumps({
        "challenges": result["accepted_challenge_count"],
        "turns": result["turn_count"], "out": os.path.abspath(args.out),
    }, ensure_ascii=False))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, csv.Error,
            json.JSONDecodeError) as error:
        raise SystemExit(f"speaker phrase-led abstention candidate: {error}")
