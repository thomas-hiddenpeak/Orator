#!/usr/bin/env python3
"""Expand accepted relative-top-1 pieces to dual-vetoed complete phrases."""

import argparse
import hashlib
import json
import os
import tomllib
from collections import defaultdict

import speaker_punctuation_phrase_candidate as phrase_tools
import speaker_reviewed_overlay_candidate as overlay_tools


EPSILON = 1e-9
DECISION_REASON = "relative_top1_complete_phrase_expansion"


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
    section = document.get("relative_top1_phrase_expansion", {})
    fusion_required = {
        "short_max_sec", "short_min_score", "short_min_margin",
        "regular_min_score", "regular_min_margin",
    }
    required = {
        "enabled", "required_candidate_kind", "required_piece_reason",
        "require_accepted_relative_piece", "require_piece_within_phrase",
        "require_phrase_dual_top_rank_identity", "require_existing_margin",
        "phrase_margin", "require_all_known_baseline_identities",
        "require_baseline_conflict", "reject_conflicting_overlays",
        "protected_decision_reasons",
    }
    missing = sorted((fusion_required - set(fusion)) | (required - set(section)))
    if missing:
        raise ValueError("relative expansion policy missing: " + ",".join(missing))
    safeguards = [
        section["enabled"], section["require_accepted_relative_piece"],
        section["require_piece_within_phrase"],
        section["require_phrase_dual_top_rank_identity"],
        section["require_existing_margin"],
        section["require_all_known_baseline_identities"],
        section["require_baseline_conflict"],
        section["reject_conflicting_overlays"],
    ]
    if any(value is not True for value in safeguards):
        raise ValueError("relative expansion safeguards are mandatory")
    margin = float(section["phrase_margin"])
    inherited = min(float(fusion["short_min_margin"]),
                    float(fusion["regular_min_margin"]))
    if abs(margin - inherited) > EPSILON:
        raise ValueError("relative expansion margin must inherit fusion margin")
    protected = section["protected_decision_reasons"]
    if not isinstance(protected, list) or not protected:
        raise ValueError("protected decision reasons must be non-empty")
    protected = [str(value) for value in protected]
    if any(not value for value in protected) or len(set(protected)) != len(protected):
        raise ValueError("protected decision reasons are invalid")
    return {
        "required_candidate_kind": str(section["required_candidate_kind"]),
        "required_piece_reason": str(section["required_piece_reason"]),
        "phrase_margin": margin,
        "protected_decision_reasons": protected,
    }


def read_mapping(document):
    raw = document.get("mapping")
    if not isinstance(raw, dict) or not raw:
        raise ValueError("mapping document has no local-slot mapping")
    mapping = {int(local): str(identity) for local, identity in raw.items()}
    if len(mapping) != len(set(mapping.values())):
        raise ValueError("mapping identities are not one-to-one")
    return mapping


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


def ranked_active(evidence, active_ids):
    if evidence.get("status") != "ok":
        return []
    return sorted(
        ((identity, float(evidence["scores"][identity]))
         for identity in active_ids if identity in evidence.get("scores", {})),
        key=lambda value: (-value[1], value[0]))


def phrase_view_confirms(evidence, phrase, identity, active_ids, margin):
    if (abs(float(evidence.get("duration_sec", -1.0)) -
            (float(phrase["end"]) - float(phrase["start"]))) > 1e-6):
        raise ValueError("phrase evidence duration differs")
    ranked = ranked_active(evidence, active_ids)
    return (len(ranked) >= 2 and ranked[0][0] == identity and
            ranked[0][1] - ranked[1][1] + EPSILON >= margin), ranked


def collect_overlays(relative, metadata, session, robust, labels,
                     active_ids, mapping, policy):
    if relative.get("candidate_kind") != policy["required_candidate_kind"]:
        raise ValueError("unexpected relative-top-1 candidate kind")
    phrases = {str(item["evidence_id"]): item
               for item in metadata.get("phrases", [])}
    protected = set(policy["protected_decision_reasons"])
    overlays = defaultdict(list)
    decisions = []
    for piece in relative.get("piece_decisions", []):
        if piece.get("accepted") is not True:
            continue
        if str(piece.get("reason")) != policy["required_piece_reason"]:
            continue
        phrase_id = str(piece["phrase_evidence_id"])
        phrase = phrases.get(phrase_id)
        if phrase is None:
            raise ValueError("relative piece has no enclosing phrase")
        text_id = int(piece["text_id"])
        if text_id != int(phrase["text_id"]):
            raise ValueError("relative piece and phrase text ID differ")
        piece_start = int(piece["source_start"])
        piece_end = int(piece["source_end"])
        phrase_start = int(phrase["source_start"])
        phrase_end = int(phrase["source_end"])
        if piece_start < phrase_start or piece_end > phrase_end:
            raise ValueError("relative piece is not contained by phrase")
        selected = str(piece["selected_speaker_id"])
        local = int(piece["local_speaker"])
        if mapping.get(local) != selected:
            raise ValueError("relative piece local/global identity differs")
        session_value = session.get(phrase_id)
        robust_value = robust.get(phrase_id)
        if session_value is None or robust_value is None:
            raise ValueError("missing complete-phrase voiceprint evidence")
        session_ok, session_ranked = phrase_view_confirms(
            session_value, phrase, selected, active_ids,
            policy["phrase_margin"])
        robust_ok, robust_ranked = phrase_view_confirms(
            robust_value, phrase, selected, active_ids,
            policy["phrase_margin"])
        decision = {
            "piece_evidence_id": str(piece["evidence_id"]),
            "phrase_evidence_id": phrase_id,
            "text_id": text_id,
            "source_start": phrase_start,
            "source_end": phrase_end,
            "speaker_id": selected,
            "session_ranked": [
                {"speaker_id": identity, "score": score}
                for identity, score in session_ranked
            ],
            "robust_ranked": [
                {"speaker_id": identity, "score": score}
                for identity, score in robust_ranked
            ],
            "accepted": False,
            "reason": "phrase_boundary_voiceprint_veto",
        }
        if not session_ok or not robust_ok:
            decisions.append(decision)
            continue
        range_labels = labels[text_id][phrase_start:phrase_end]
        baseline_ids = {value["speaker_id"] for value in range_labels}
        if None in baseline_ids:
            decision["reason"] = "phrase_baseline_identity_unknown"
            decisions.append(decision)
            continue
        if not any(identity != selected for identity in baseline_ids):
            decision["reason"] = "phrase_baseline_conflict_missing"
            decisions.append(decision)
            continue
        if any(value["reason"] in protected for value in range_labels):
            decision["reason"] = "phrase_protected_overlay_conflict"
            decisions.append(decision)
            continue
        decision["accepted"] = True
        decision["reason"] = DECISION_REASON
        overlays[text_id].append({
            "text_id": text_id,
            "source_start": phrase_start,
            "source_end": phrase_end,
            "speaker_id": selected,
            "reason": DECISION_REASON,
        })
        decisions.append(decision)
    overlay_tools.validate_overlays(overlays)
    return dict(overlays), decisions


def build_candidate(baseline, relative, metadata, session, robust,
                    mapping_document, policy):
    if metadata.get("kind") != "orator_punctuation_phrase_spans":
        raise ValueError("metadata is not punctuation phrase evidence")
    mapping = read_mapping(mapping_document)
    id_to_local = {identity: local for local, identity in mapping.items()}
    active_ids = sorted(id_to_local)
    if sorted(baseline.get("active_speaker_ids", [])) != active_ids:
        raise ValueError("baseline identities differ from mapping")
    fragments = overlay_tools.candidate_fragments(baseline)
    overlay_tools.validate_candidate_sources(fragments, metadata)
    labels = {
        text_id: baseline_labels(
            values, str(metadata["asr"][str(text_id)]["text"]))
        for text_id, values in fragments.items()
    }
    overlays, decisions = collect_overlays(
        relative, metadata, session, robust, labels, active_ids, mapping, policy)
    track = []
    for text_id in sorted(fragments):
        source = str(metadata["asr"][str(text_id)]["text"])
        track.extend(overlay_tools.reproject_preserving_base(
            text_id, source, metadata["align"][str(text_id)],
            fragments[text_id], overlays.get(text_id, []), id_to_local))
    return {
        "schema_version": 1,
        "kind": "orator_frozen_speaker_candidate",
        "candidate_kind": "v21_relative_top1_complete_phrase_expansion",
        "audio_sec": float(baseline["audio_sec"]),
        "sample_rate": int(baseline["sample_rate"]),
        "active_speaker_ids": active_ids,
        "source_turn_count": len(baseline.get("track", [])),
        "turn_count": len(track),
        "accepted_expansion_count": sum(item["accepted"] for item in decisions),
        "policy": policy,
        "expansion_decisions": decisions,
        "track": track,
    }


def command_build(args):
    result = build_candidate(
        load_json(args.baseline), load_json(args.relative),
        load_json(args.metadata), phrase_tools.read_titanet(args.session_titanet),
        phrase_tools.read_titanet(args.robust_titanet), load_json(args.mapping),
        load_policy(args.config))
    result["sources"] = {
        name: {"path": os.path.abspath(path), "sha256": sha256_file(path)}
        for name, path in {
            "baseline": args.baseline, "relative": args.relative,
            "metadata": args.metadata, "session_titanet": args.session_titanet,
            "robust_titanet": args.robust_titanet, "mapping": args.mapping,
            "config": args.config,
        }.items()
    }
    with open(args.out, "w", encoding="utf-8") as output:
        json.dump(result, output, ensure_ascii=False, indent=2)
    print(json.dumps({
        "turns": result["turn_count"],
        "expansions": result["accepted_expansion_count"],
        "out": os.path.abspath(args.out),
    }, ensure_ascii=False))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", required=True)
    parser.add_argument("--relative", required=True)
    parser.add_argument("--metadata", required=True)
    parser.add_argument("--session-titanet", required=True)
    parser.add_argument("--robust-titanet", required=True)
    parser.add_argument("--mapping", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--out", required=True)
    command_build(parser.parse_args())


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as error:
        raise SystemExit(f"speaker relative-top1 phrase expansion: {error}")
