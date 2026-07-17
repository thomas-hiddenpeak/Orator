#!/usr/bin/env python3
"""Compose VAD-contextual exact phrase challenges without result scoring."""

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


EPSILON = 1e-9
DECISION_REASON = "contextual_vad_phrase_four_view_voiceprint_challenge"


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
    section = document.get("contextual_vad_phrase_challenge", {})
    fusion_names = (
        "short_max_sec", "short_min_score", "short_min_margin",
        "regular_min_score", "regular_min_margin",
    )
    safeguards = {
        "enabled", "require_one_containing_vad",
        "require_one_covering_primary_run", "require_four_view_same_top_rank",
        "require_existing_margin", "require_one_outer_regular_score",
        "require_raw_local_identity_conflict",
        "require_uniform_known_baseline_identity",
        "require_baseline_identity_conflict", "reject_conflicting_overlays",
    }
    required = safeguards | {
        "view_margin", "outer_regular_score", "boundary_tolerance_frames",
        "protected_decision_reasons",
    }
    missing = sorted((set(fusion_names) - set(fusion)) |
                     (required - set(section)))
    if missing:
        raise ValueError("contextual VAD phrase policy missing: " +
                         ",".join(missing))
    if any(section[name] is not True for name in safeguards):
        raise ValueError("contextual VAD phrase safeguards are mandatory")
    margin = float(section["view_margin"])
    if abs(margin - min(float(fusion["short_min_margin"]),
                        float(fusion["regular_min_margin"]))) > EPSILON:
        raise ValueError("view margin must inherit the fusion margin")
    outer_score = float(section["outer_regular_score"])
    if abs(outer_score - float(fusion["regular_min_score"])) > EPSILON:
        raise ValueError("outer score must inherit the regular score floor")
    if int(section["boundary_tolerance_frames"]) != 1:
        raise ValueError("boundary tolerance must be one frozen frame")
    protected = [str(value)
                 for value in section["protected_decision_reasons"]]
    if not protected or len(set(protected)) != len(protected):
        raise ValueError("protected decision reasons are invalid")
    return {
        **{name: float(fusion[name]) for name in fusion_names},
        "view_margin": margin, "outer_regular_score": outer_score,
        "boundary_tolerance_frames": 1,
        "protected_decision_reasons": protected,
    }


def read_vad(path):
    output = []
    with open(path, encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source, delimiter="\t")
        for row in reader:
            output.append({"evidence_id": row["evidence_id"],
                           "start": float(row["start_sec"]),
                           "end": float(row["end_sec"])})
    if not output:
        raise ValueError("VAD evidence is empty")
    return output


def ranked_view(evidence, active_ids, margin):
    if evidence.get("status") != "ok":
        return None, []
    ranked = sorted(
        ((identity, float(evidence["scores"][identity]))
         for identity in active_ids if identity in evidence.get("scores", {})),
        key=lambda value: (-value[1], value[0]))
    if len(ranked) < 2 or ranked[0][1] - ranked[1][1] + EPSILON < margin:
        return None, ranked
    return ranked[0][0], ranked


def labels_for_source(fragments, source):
    labels = []
    for fragment in fragments:
        entry = fragment["entry"]
        labels.extend({"speaker_id": entry.get("speaker_id"),
                       "reason": str(entry.get("decision_reason", "baseline"))}
                      for _ in str(entry.get("text", "")))
    if len(labels) != len(source):
        raise ValueError("baseline source and labels differ")
    return labels


def build_candidate(baseline, metadata, vad, primary, frame_period,
                    vad_session, vad_robust, phrase_session, phrase_robust,
                    mapping_document, policy):
    if metadata.get("kind") != "orator_punctuation_phrase_spans":
        raise ValueError("metadata is not punctuation phrase evidence")
    mapping = overlay_tools.read_mapping(mapping_document)
    id_to_local = {identity: local for local, identity in mapping.items()}
    active_ids = sorted(id_to_local)
    if sorted(baseline.get("active_speaker_ids", [])) != active_ids:
        raise ValueError("baseline identities differ from mapping")
    if frame_period <= 0.0:
        raise ValueError("native frame period must be positive")
    tolerance = policy["boundary_tolerance_frames"] * frame_period
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
    protected = set(policy["protected_decision_reasons"])
    overlays = defaultdict(list)
    decisions = []
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
        vad_id = str(vad_item["evidence_id"])
        phrase_id = str(phrase["evidence_id"])
        views = {
            "vad_session": vad_session.get(vad_id),
            "vad_robust": vad_robust.get(vad_id),
            "phrase_session": phrase_session.get(phrase_id),
            "phrase_robust": phrase_robust.get(phrase_id),
        }
        if any(value is None for value in views.values()):
            raise ValueError("missing contextual voiceprint evidence")
        identities = []
        audit = {}
        for name, evidence in views.items():
            identity, ranked = ranked_view(
                evidence, active_ids, policy["view_margin"])
            identities.append(identity)
            audit[name] = [{"speaker_id": speaker_id, "score": score}
                           for speaker_id, score in ranked]
        decision = {
            "vad_evidence_id": vad_id, "phrase_evidence_id": phrase_id,
            "primary_start": primary_item["start"],
            "primary_end": primary_item["end"],
            "mapped_speaker_id": primary_item["speaker_id"],
            "text_id": int(phrase["text_id"]),
            "source_start": int(phrase["source_start"]),
            "source_end": int(phrase["source_end"]),
            "start": float(phrase["start"]), "end": float(phrase["end"]),
            "views": audit, "selected_speaker_id": None,
            "accepted": False, "reason": "four_view_margin_abstention",
        }
        if any(identity is None for identity in identities):
            decisions.append(decision)
            continue
        if len(set(identities)) != 1:
            decision["reason"] = "four_view_identity_disagreement"
            decisions.append(decision)
            continue
        outer_scores = [audit[name][0]["score"]
                        for name in ("vad_session", "vad_robust")]
        if max(outer_scores) + EPSILON < policy["outer_regular_score"]:
            decision["reason"] = "outer_vad_regular_score_missing"
            decisions.append(decision)
            continue
        identity = identities[0]
        if primary_item["speaker_id"] == identity:
            decision["reason"] = "raw_local_identity_agreement"
            decisions.append(decision)
            continue
        text_id = int(phrase["text_id"])
        start = int(phrase["source_start"])
        end = int(phrase["source_end"])
        range_labels = labels[text_id][start:end]
        baseline_ids = {value["speaker_id"] for value in range_labels}
        if None in baseline_ids or len(baseline_ids) != 1:
            decision["reason"] = "baseline_identity_not_uniform_known"
            decisions.append(decision)
            continue
        if identity in baseline_ids:
            decision["reason"] = "baseline_identity_conflict_missing"
            decisions.append(decision)
            continue
        if any(value["reason"] in protected for value in range_labels):
            decision["reason"] = "protected_overlay_conflict"
            decisions.append(decision)
            continue
        decision["selected_speaker_id"] = identity
        decision["accepted"] = True
        decision["reason"] = DECISION_REASON
        overlays[text_id].append({"text_id": text_id, "source_start": start,
                                  "source_end": end, "speaker_id": identity,
                                  "reason": DECISION_REASON})
        decisions.append(decision)
    overlay_tools.validate_overlays(overlays)
    track = []
    for text_id in sorted(fragments):
        source = str(metadata["asr"][str(text_id)]["text"])
        track.extend(overlay_tools.reproject_preserving_base(
            text_id, source, metadata["align"][str(text_id)],
            fragments[text_id], overlays.get(text_id, []), id_to_local))
    return {
        "schema_version": 1, "kind": "orator_frozen_speaker_candidate",
        "candidate_kind": "v21_contextual_vad_phrase_voiceprint_challenge",
        "audio_sec": float(baseline["audio_sec"]),
        "sample_rate": int(baseline["sample_rate"]),
        "active_speaker_ids": active_ids,
        "source_turn_count": len(baseline.get("track", [])),
        "turn_count": len(track),
        "accepted_challenge_count": sum(item["accepted"] for item in decisions),
        "policy": policy, "challenge_decisions": decisions, "track": track,
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
    paths = vars(args)
    _, frame_period = posterior.read_frames(args.frames)
    result = build_candidate(
        load_json(args.baseline), load_json(args.metadata), read_vad(args.vad),
        primary_tools.read_diarization(args.primary, primary=True), frame_period,
        phrase_tools.read_titanet(args.vad_session),
        phrase_tools.read_titanet(args.vad_robust),
        phrase_tools.read_titanet(args.phrase_session),
        phrase_tools.read_titanet(args.phrase_robust), load_json(args.mapping),
        load_policy(args.config))
    result["sources"] = {
        name: {"path": os.path.abspath(path), "sha256": sha256_file(path)}
        for name, path in paths.items() if name != "out"
    }
    with open(args.out, "w", encoding="utf-8") as output:
        json.dump(result, output, ensure_ascii=False, indent=2)
    print(json.dumps({"turns": result["turn_count"],
                      "challenges": result["accepted_challenge_count"],
                      "out": os.path.abspath(args.out)}, ensure_ascii=False))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as error:
        raise SystemExit(f"speaker contextual VAD phrase challenge: {error}")
