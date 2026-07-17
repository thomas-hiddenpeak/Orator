#!/usr/bin/env python3
"""Compose two-phrase primary-run anchor expansions without result judgment."""

import argparse
import csv
import hashlib
import json
import os
import tomllib
from collections import defaultdict

import speaker_posterior_bounded_phrase_candidate as posterior
import speaker_punctuation_phrase_candidate as phrase_tools
import speaker_reviewed_overlay_candidate as overlay_tools


EPSILON = 1e-9
DECISION_REASON = "primary_run_dual_phrase_anchor_expansion"


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
    section = document.get("primary_run_phrase_anchor", {})
    fusion_names = (
        "short_max_sec", "short_min_score", "short_min_margin",
        "regular_min_score", "regular_min_margin",
    )
    safeguards = {
        "enabled", "require_same_source_adjacent_phrases",
        "require_anchor_contains_mapped_identity",
        "require_dual_anchor_voiceprint_consensus",
        "require_known_single_competitor_combined_range",
        "project_both_complete_phrases",
        "reject_dual_eligible_competitor",
        "reject_dual_eligible_disagreement", "reject_conflicting_overlays",
        "reject_overlapping_proposals",
    }
    required = safeguards | {
        "contained_phrase_count", "boundary_tolerance_frames",
        "protected_decision_reasons",
    }
    missing = sorted((set(fusion_names) - set(fusion)) |
                     (required - set(section)))
    if missing:
        raise ValueError("primary-run anchor policy missing: " +
                         ",".join(missing))
    if any(section[name] is not True for name in safeguards):
        raise ValueError("primary-run anchor safeguards are mandatory")
    if int(section["contained_phrase_count"]) != 2:
        raise ValueError("primary-run anchor requires exactly two phrases")
    if int(section["boundary_tolerance_frames"]) != 1:
        raise ValueError("primary-run boundary tolerance must be one frame")
    protected = [str(value)
                 for value in section["protected_decision_reasons"]]
    if not protected or len(set(protected)) != len(protected):
        raise ValueError("protected decision reasons are invalid")
    return {
        **{name: float(fusion[name]) for name in fusion_names},
        "contained_phrase_count": 2, "boundary_tolerance_frames": 1,
        "protected_decision_reasons": protected,
    }


def read_primary(path):
    output = []
    with open(path, encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source, delimiter="\t")
        required = {"start_sec", "end_sec", "local_speaker", "speaker_id"}
        if not required.issubset(reader.fieldnames or []):
            raise ValueError("primary diarization columns are incomplete")
        for row in reader:
            output.append({
                "start": float(row["start_sec"]),
                "end": float(row["end_sec"]),
                "local": int(row["local_speaker"]),
                "speaker_id": str(row["speaker_id"]),
            })
    if not output:
        raise ValueError("primary diarization is empty")
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


def select(evidence, active_ids, policy):
    identity, reason, ranked = phrase_tools.select_identity(
        evidence, active_ids, policy)
    return identity, reason, [
        {"speaker_id": speaker_id, "score": score}
        for speaker_id, score in ranked
    ]


def phrase_view(phrase, session_evidence, robust_evidence, active_ids, policy):
    evidence_id = str(phrase["evidence_id"])
    if evidence_id not in session_evidence or evidence_id not in robust_evidence:
        raise ValueError("missing phrase voiceprint evidence")
    session_id, session_reason, session_ranked = select(
        session_evidence[evidence_id], active_ids, policy)
    robust_id, robust_reason, robust_ranked = select(
        robust_evidence[evidence_id], active_ids, policy)
    return {
        "session_identity": session_id, "session_reason": session_reason,
        "session_ranked": session_ranked,
        "robust_identity": robust_id, "robust_reason": robust_reason,
        "robust_ranked": robust_ranked,
    }


def source_labels(labels, phrase):
    return labels[int(phrase["source_start"]):int(phrase["source_end"])]


def challenge_allowed(view, mapped_id):
    identities = [view["session_identity"], view["robust_identity"]]
    eligible = [identity for identity in identities if identity is not None]
    if len(eligible) < 2:
        return True
    return all(identity == mapped_id for identity in eligible)


def adjacent_phrases(values):
    return (len(values) == 2 and
            int(values[0]["text_id"]) == int(values[1]["text_id"]) and
            int(values[0]["source_end"]) == int(values[1]["source_start"]))


def overlaps(left, right):
    return (int(left["source_start"]) < int(right["source_end"]) and
            int(left["source_end"]) > int(right["source_start"]))


def build_candidate(baseline, metadata, primary, frame_period,
                    session_evidence, robust_evidence, mapping_document,
                    policy):
    if metadata.get("kind") != "orator_punctuation_phrase_spans":
        raise ValueError("metadata is not punctuation phrase evidence")
    if frame_period <= 0.0:
        raise ValueError("native frame period must be positive")
    mapping = overlay_tools.read_mapping(mapping_document)
    id_to_local = {speaker_id: local for local, speaker_id in mapping.items()}
    active_ids = sorted(id_to_local)
    if sorted(baseline.get("active_speaker_ids", [])) != active_ids:
        raise ValueError("baseline identities differ from mapping")
    if any(mapping.get(item["local"]) != item["speaker_id"] for item in primary):
        raise ValueError("primary local/global mapping differs")
    fragments = overlay_tools.candidate_fragments(baseline)
    overlay_tools.validate_candidate_sources(fragments, metadata)
    labels = {
        text_id: labels_for_source(
            values, str(metadata["asr"][str(text_id)]["text"]))
        for text_id, values in fragments.items()
    }
    protected = set(policy["protected_decision_reasons"])
    tolerance = policy["boundary_tolerance_frames"] * frame_period
    decisions = []
    proposals = defaultdict(list)
    for run_index, run in enumerate(primary):
        contained = [phrase for phrase in metadata.get("phrases", [])
                     if float(phrase["start"]) >= run["start"] - tolerance and
                     float(phrase["end"]) <= run["end"] + tolerance]
        if len(contained) != policy["contained_phrase_count"]:
            continue
        contained.sort(key=lambda item: (
            int(item["text_id"]), int(item["source_start"])))
        if not adjacent_phrases(contained):
            continue
        mapped_id = run["speaker_id"]
        views = [phrase_view(item, session_evidence, robust_evidence,
                             active_ids, policy) for item in contained]
        accepted_for_run = False
        for anchor_index, challenge_index in ((0, 1), (1, 0)):
            anchor = contained[anchor_index]
            challenge = contained[challenge_index]
            anchor_view = views[anchor_index]
            challenge_view = views[challenge_index]
            text_id = int(challenge["text_id"])
            anchor_labels = source_labels(labels[text_id], anchor)
            challenge_labels = source_labels(labels[text_id], challenge)
            anchor_ids = {item["speaker_id"] for item in anchor_labels}
            challenge_ids = {item["speaker_id"] for item in challenge_labels}
            combined_labels = anchor_labels + challenge_labels
            combined_ids = anchor_ids | challenge_ids
            competing_ids = {identity for identity in combined_ids
                             if identity is not None and identity != mapped_id}
            projected_start = int(contained[0]["source_start"])
            projected_end = int(contained[1]["source_end"])
            decision = {
                "run_index": run_index, "run_start": run["start"],
                "run_end": run["end"], "mapped_speaker_id": mapped_id,
                "anchor_evidence_id": anchor["evidence_id"],
                "challenge_evidence_id": challenge["evidence_id"],
                "text_id": text_id,
                "source_start": projected_start,
                "source_end": projected_end,
                "start": float(contained[0]["start"]),
                "end": float(contained[1]["end"]),
                "challenge_source_start": int(challenge["source_start"]),
                "challenge_source_end": int(challenge["source_end"]),
                "anchor_view": anchor_view, "challenge_view": challenge_view,
                "anchor_baseline_ids": sorted(
                    identity for identity in anchor_ids if identity is not None),
                "challenge_baseline_ids": sorted(
                    identity for identity in challenge_ids
                    if identity is not None),
                "selected_speaker_id": None, "accepted": False,
                "reason": "anchor_mapped_identity_missing",
            }
            if None in anchor_ids or mapped_id not in anchor_ids:
                decisions.append(decision)
                continue
            if (anchor_view["session_identity"] != mapped_id or
                    anchor_view["robust_identity"] != mapped_id):
                decision["reason"] = "dual_anchor_voiceprint_missing"
                decisions.append(decision)
                continue
            if (None in combined_ids or mapped_id not in combined_ids or
                    len(competing_ids) != 1):
                decision["reason"] = "single_known_combined_competitor_missing"
                decisions.append(decision)
                continue
            if not challenge_allowed(challenge_view, mapped_id):
                decision["reason"] = "dual_eligible_challenge_veto"
                decisions.append(decision)
                continue
            if any(item["reason"] in protected for item in combined_labels):
                decision["reason"] = "protected_overlay_conflict"
                decisions.append(decision)
                continue
            decision["selected_speaker_id"] = mapped_id
            decision["accepted"] = True
            decision["reason"] = DECISION_REASON
            proposals[text_id].append(decision)
            decisions.append(decision)
            accepted_for_run = True
            break
        if accepted_for_run:
            continue

    for values in proposals.values():
        conflicted = set()
        ordered = sorted(values, key=lambda item: (
            item["source_start"], item["source_end"],
            item["challenge_evidence_id"]))
        for index, left in enumerate(ordered):
            for right in ordered[index + 1:]:
                if right["source_start"] >= left["source_end"]:
                    break
                if overlaps(left, right):
                    conflicted.add(left["challenge_evidence_id"])
                    conflicted.add(right["challenge_evidence_id"])
        for item in values:
            if item["challenge_evidence_id"] in conflicted:
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
        "schema_version": 1, "kind": "orator_frozen_speaker_candidate",
        "candidate_kind": "v21_primary_run_phrase_anchor_expansion",
        "audio_sec": float(baseline["audio_sec"]),
        "sample_rate": int(baseline["sample_rate"]),
        "active_speaker_ids": active_ids,
        "source_turn_count": len(baseline.get("track", [])),
        "turn_count": len(track),
        "accepted_expansion_count": sum(item["accepted"] for item in decisions),
        "policy": policy, "expansion_decisions": decisions, "track": track,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", required=True)
    parser.add_argument("--metadata", required=True)
    parser.add_argument("--primary", required=True)
    parser.add_argument("--frames", required=True)
    parser.add_argument("--session-evidence", required=True)
    parser.add_argument("--robust-evidence", required=True)
    parser.add_argument("--mapping", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()
    _, frame_period = posterior.read_frames(args.frames)
    result = build_candidate(
        load_json(args.baseline), load_json(args.metadata),
        read_primary(args.primary), frame_period,
        phrase_tools.read_titanet(args.session_evidence),
        phrase_tools.read_titanet(args.robust_evidence),
        load_json(args.mapping), load_policy(args.config))
    result["sources"] = {
        name: {"path": os.path.abspath(path), "sha256": sha256_file(path)}
        for name, path in {
            "baseline": args.baseline, "metadata": args.metadata,
            "primary": args.primary, "frames": args.frames,
            "session_evidence": args.session_evidence,
            "robust_evidence": args.robust_evidence,
            "mapping": args.mapping, "config": args.config,
        }.items()
    }
    with open(args.out, "w", encoding="utf-8") as output:
        json.dump(result, output, ensure_ascii=False, indent=2)
    print(json.dumps({
        "accepted_expansions": result["accepted_expansion_count"],
        "turns": result["turn_count"], "out": os.path.abspath(args.out),
    }, ensure_ascii=False))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, csv.Error) as error:
        raise SystemExit(f"speaker primary-run anchor candidate: {error}")
