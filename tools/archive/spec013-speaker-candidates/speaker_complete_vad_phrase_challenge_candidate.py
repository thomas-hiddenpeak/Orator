#!/usr/bin/env python3
"""Compose four-view complete-VAD-phrase speaker challenges without scoring."""

import argparse
import hashlib
import json
import os
import tomllib
from collections import defaultdict

import speaker_punctuation_phrase_candidate as phrase_tools
import speaker_reviewed_overlay_candidate as overlay_tools


EPSILON = 1e-9
DECISION_REASON = "complete_vad_phrase_four_view_voiceprint_challenge"


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
    section = document.get("complete_vad_phrase_challenge", {})
    fusion_names = (
        "short_max_sec", "short_min_score", "short_min_margin",
        "regular_min_score", "regular_min_margin",
    )
    safeguards = {
        "enabled", "require_unique_enclosing_phrase",
        "require_vad_dual_voiceprint_identity",
        "require_vad_regular_score_floor_for_raw_challenge",
        "require_phrase_dual_voiceprint_identity",
        "require_four_view_identity_agreement",
        "require_raw_local_identity_conflict",
        "require_uniform_known_baseline_identity",
        "require_baseline_identity_conflict", "reject_conflicting_overlays",
    }
    required = safeguards | {"boundary_tolerance_frames",
                             "protected_decision_reasons"}
    missing = sorted((set(fusion_names) - set(fusion)) |
                     (required - set(section)))
    if missing:
        raise ValueError("complete VAD phrase policy missing: " +
                         ",".join(missing))
    if any(section[name] is not True for name in safeguards):
        raise ValueError("complete VAD phrase safeguards are mandatory")
    if int(section["boundary_tolerance_frames"]) != 1:
        raise ValueError("boundary tolerance must be one frozen frame")
    protected = [str(value)
                 for value in section["protected_decision_reasons"]]
    if not protected or any(not value for value in protected):
        raise ValueError("protected decision reasons are invalid")
    if len(set(protected)) != len(protected):
        raise ValueError("protected decision reasons contain duplicates")
    return {
        **{name: float(fusion[name]) for name in fusion_names},
        "boundary_tolerance_frames": 1,
        "protected_decision_reasons": protected,
    }


def read_mapping(document):
    return overlay_tools.read_mapping(document)


def baseline_labels(fragments, source):
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


def select_view(evidence, active_ids, policy):
    identity, reason, ranked = phrase_tools.select_identity(
        evidence, active_ids, policy)
    return identity, reason, [
        {"speaker_id": speaker_id, "score": score}
        for speaker_id, score in ranked
    ]


def enclosing_phrases(piece, phrases, tolerance):
    output = []
    for phrase in phrases:
        if (int(phrase["source_start"]) <= int(piece["source_start"]) and
                int(phrase["source_end"]) >= int(piece["source_end"]) and
                float(phrase["start"]) >= float(piece["start"]) - tolerance and
                float(phrase["end"]) <= float(piece["end"]) + tolerance):
            output.append(phrase)
    return output


def collect_overlays(baseline, vad_metadata, phrase_metadata, vad_session,
                     vad_robust, phrase_session, phrase_robust, mapping,
                     policy):
    frame_period = float(vad_metadata.get("frame_period_sec", 0.0))
    if frame_period <= 0.0:
        raise ValueError("VAD metadata has no frame period")
    tolerance = policy["boundary_tolerance_frames"] * frame_period
    active_ids = sorted(mapping.values())
    fragments = overlay_tools.candidate_fragments(baseline)
    overlay_tools.validate_candidate_sources(fragments, phrase_metadata)
    labels = {
        text_id: baseline_labels(
            values, str(phrase_metadata["asr"][str(text_id)]["text"]))
        for text_id, values in fragments.items()
    }
    phrases_by_text = defaultdict(list)
    for phrase in phrase_metadata.get("phrases", []):
        phrases_by_text[int(phrase["text_id"])].append(phrase)
    protected = set(policy["protected_decision_reasons"])
    overlays = defaultdict(list)
    decisions = []
    for piece in vad_metadata.get("pieces", []):
        text_id = int(piece["text_id"])
        matches = enclosing_phrases(
            piece, phrases_by_text.get(text_id, []), tolerance)
        if len(matches) != 1:
            continue
        phrase = matches[0]
        vad_id = str(piece["evidence_id"])
        phrase_id = str(phrase["evidence_id"])
        if any(vad_id not in view for view in (vad_session, vad_robust)):
            raise ValueError("missing VAD voiceprint evidence")
        if any(phrase_id not in view
               for view in (phrase_session, phrase_robust)):
            raise ValueError("missing phrase voiceprint evidence")
        selected = []
        view_audit = {}
        for name, evidence in (
                ("vad_session", vad_session[vad_id]),
                ("vad_robust", vad_robust[vad_id]),
                ("phrase_session", phrase_session[phrase_id]),
                ("phrase_robust", phrase_robust[phrase_id])):
            identity, reason, ranked = select_view(
                evidence, active_ids, policy)
            selected.append(identity)
            view_audit[name] = {"identity": identity, "reason": reason,
                                "ranked": ranked}
        decision = {
            "vad_evidence_id": vad_id,
            "phrase_evidence_id": phrase_id,
            "text_id": text_id,
            "source_start": int(phrase["source_start"]),
            "source_end": int(phrase["source_end"]),
            "start": float(phrase["start"]),
            "end": float(phrase["end"]),
            "vad_start": float(piece["start"]),
            "vad_end": float(piece["end"]),
            "boundary_tolerance_sec": tolerance,
            "mapped_speaker_id": mapping.get(int(piece["local_speaker"])),
            "views": view_audit,
            "selected_speaker_id": None,
            "accepted": False,
            "reason": "four_view_voiceprint_abstention",
        }
        if any(identity is None for identity in selected):
            decisions.append(decision)
            continue
        if len(set(selected)) != 1:
            decision["reason"] = "four_view_identity_disagreement"
            decisions.append(decision)
            continue
        vad_score_floor = float(policy["regular_min_score"])
        if any(view_audit[name]["ranked"][0]["score"] + EPSILON <
               vad_score_floor for name in ("vad_session", "vad_robust")):
            decision["reason"] = "vad_challenge_score_below_regular_floor"
            decisions.append(decision)
            continue
        identity = selected[0]
        if decision["mapped_speaker_id"] == identity:
            decision["reason"] = "raw_local_identity_agreement"
            decisions.append(decision)
            continue
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
        overlays[text_id].append({
            "text_id": text_id, "source_start": start, "source_end": end,
            "speaker_id": identity, "reason": DECISION_REASON,
        })
        decisions.append(decision)
    overlay_tools.validate_overlays(overlays)
    return fragments, dict(overlays), decisions


def build_candidate(baseline, vad_metadata, phrase_metadata, vad_session,
                    vad_robust, phrase_session, phrase_robust,
                    mapping_document, policy):
    if vad_metadata.get("kind") != "orator_vad_utterance_spans":
        raise ValueError("metadata is not short VAD utterance evidence")
    if phrase_metadata.get("kind") != "orator_punctuation_phrase_spans":
        raise ValueError("metadata is not punctuation phrase evidence")
    mapping = read_mapping(mapping_document)
    active_ids = sorted(mapping.values())
    if sorted(baseline.get("active_speaker_ids", [])) != active_ids:
        raise ValueError("baseline identities differ from mapping")
    fragments, overlays, decisions = collect_overlays(
        baseline, vad_metadata, phrase_metadata, vad_session, vad_robust,
        phrase_session, phrase_robust, mapping, policy)
    id_to_local = {identity: local for local, identity in mapping.items()}
    track = []
    for text_id in sorted(fragments):
        source = str(phrase_metadata["asr"][str(text_id)]["text"])
        track.extend(overlay_tools.reproject_preserving_base(
            text_id, source, phrase_metadata["align"][str(text_id)],
            fragments[text_id], overlays.get(text_id, []), id_to_local))
    return {
        "schema_version": 1,
        "kind": "orator_frozen_speaker_candidate",
        "candidate_kind": "v21_complete_vad_phrase_voiceprint_challenge",
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
    parser.add_argument("--vad-metadata", required=True)
    parser.add_argument("--phrase-metadata", required=True)
    parser.add_argument("--vad-session", required=True)
    parser.add_argument("--vad-robust", required=True)
    parser.add_argument("--phrase-session", required=True)
    parser.add_argument("--phrase-robust", required=True)
    parser.add_argument("--mapping", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()
    paths = vars(args)
    result = build_candidate(
        load_json(args.baseline), load_json(args.vad_metadata),
        load_json(args.phrase_metadata),
        phrase_tools.read_titanet(args.vad_session),
        phrase_tools.read_titanet(args.vad_robust),
        phrase_tools.read_titanet(args.phrase_session),
        phrase_tools.read_titanet(args.phrase_robust),
        load_json(args.mapping), load_policy(args.config))
    result["sources"] = {
        name: {"path": os.path.abspath(path), "sha256": sha256_file(path)}
        for name, path in paths.items() if name != "out"
    }
    with open(args.out, "w", encoding="utf-8") as output:
        json.dump(result, output, ensure_ascii=False, indent=2)
    print(json.dumps({
        "turns": result["turn_count"],
        "challenges": result["accepted_challenge_count"],
        "out": os.path.abspath(args.out),
    }, ensure_ascii=False))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as error:
        raise SystemExit(f"speaker complete VAD phrase challenge: {error}")
