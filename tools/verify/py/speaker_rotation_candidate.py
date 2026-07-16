#!/usr/bin/env python3
"""Build a session-qualified speaker candidate from frozen acoustic evidence.

The tool never reads reference annotations. It maps each rotated Sortformer
session's local slots to active registry identities with TitaNet scores and a
within-session one-to-one constraint, then projects those mappings onto the
frozen business intervals. It emits decisions and evidence, not evaluation.
"""

import argparse
import hashlib
import itertools
import json
import os
from collections import Counter, defaultdict


EPSILON = 1e-9


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def rank_scores(scores, allowed_ids):
    allowed = set(allowed_ids)
    return sorted(
        ((speaker_id, float(score))
         for speaker_id, score in scores.items()
         if speaker_id in allowed),
        key=lambda item: (-item[1], item[0]))


def score_gate(ranked, threshold, margin):
    return (len(ranked) >= 2 and ranked[0][1] >= threshold and
            ranked[0][1] - ranked[1][1] >= margin)


def unique_voiceprints(turns):
    seen = set()
    for turn in turns:
        turn_id = str(turn["turn_id"])
        if turn_id not in seen:
            seen.add(turn_id)
            yield turn_id, turn["titanet"], float(turn["duration_sec"])
        for segment in turn["diar_segments"]:
            evidence_id = str(segment["evidence_id"])
            if evidence_id in seen:
                continue
            seen.add(evidence_id)
            yield (evidence_id, segment["titanet"],
                   float(segment["end_sec"]) -
                   float(segment["start_sec"]))


def discover_active_ids(turns, config):
    speaker = config["speaker"]
    threshold = float(speaker["local_drift_competing_threshold"])
    margin = float(speaker["local_drift_competing_margin"])
    max_speakers = int(config["diarizer"]["max_speakers"])
    duration = Counter()
    count = Counter()
    for _, evidence, evidence_duration in unique_voiceprints(turns):
        if evidence.get("status") != "ok":
            continue
        ranked = rank_scores(evidence.get("scores", {}),
                             evidence.get("scores", {}).keys())
        if not score_gate(ranked, threshold, margin):
            continue
        duration[ranked[0][0]] += evidence_duration
        count[ranked[0][0]] += 1
    active = sorted(
        duration, key=lambda speaker_id: (-duration[speaker_id], speaker_id)
    )[:max_speakers]
    if len(active) != max_speakers:
        raise ValueError(
            f"expected {max_speakers} active identities, found {len(active)}")
    diagnostics = {
        speaker_id: {
            "strong_evidence_count": count[speaker_id],
            "strong_evidence_duration_sec": duration[speaker_id],
        }
        for speaker_id in active
    }
    return active, diagnostics


def local_support(turn):
    support = Counter()
    for segment in turn["diar_segments"]:
        support[int(segment["local_speaker"])] += float(
            segment["overlap_sec"])
    return support


def add_scores(record, evidence_id, evidence, weight, active_ids):
    if evidence.get("status") != "ok" or weight <= EPSILON:
        return
    scores = evidence.get("scores", {})
    if not set(active_ids).issubset(scores):
        return
    record["weight"] += weight
    record["evidence_ids"].append(evidence_id)
    for speaker_id in active_ids:
        record["score_sums"][speaker_id] += (
            weight * float(scores[speaker_id]))


def collect_slot_evidence(turns, active_ids, config):
    max_window = float(config["speaker"]["max_embed_window_sec"])
    minimum_ratio = float(
        config["timeline"]["speaker_support_min_coverage_ratio"])
    records = defaultdict(lambda: {
        "weight": 0.0,
        "score_sums": Counter(),
        "evidence_ids": [],
    })
    active_locals = defaultdict(set)
    seen_segments = set()

    for turn in turns:
        for segment in turn["diar_segments"]:
            local = int(segment["local_speaker"])
            session = int(segment.get(
                "source_session",
                local // int(config["diarizer"]["max_speakers"])))
            active_locals[session].add(local)
            evidence_id = str(segment["evidence_id"])
            if evidence_id in seen_segments:
                continue
            seen_segments.add(evidence_id)
            duration = (float(segment["end_sec"]) -
                        float(segment["start_sec"]))
            confidence = max(0.0, float(segment.get("confidence", 0.0)))
            add_scores(records[local], evidence_id, segment["titanet"],
                       min(duration, max_window) * confidence, active_ids)

        support = local_support(turn)
        if not support:
            continue
        local, amount = max(support.items(), key=lambda item: (item[1], -item[0]))
        total = sum(support.values())
        dominance = amount / total if total > EPSILON else 0.0
        if dominance + EPSILON < minimum_ratio:
            continue
        duration = min(float(turn["duration_sec"]), max_window)
        add_scores(records[local], str(turn["turn_id"]), turn["titanet"],
                   duration * dominance, active_ids)
    return records, active_locals


def averaged_scores(record, active_ids):
    weight = float(record["weight"])
    if weight <= EPSILON:
        return {speaker_id: 0.0 for speaker_id in active_ids}
    return {
        speaker_id: float(record["score_sums"][speaker_id]) / weight
        for speaker_id in active_ids
    }


def assign_session_slots(turns, active_ids, config):
    records, active_locals = collect_slot_evidence(
        turns, active_ids, config)
    assignments = {}
    sessions = {}
    for session, local_set in sorted(active_locals.items()):
        locals_in_session = sorted(local_set)
        if len(locals_in_session) > len(active_ids):
            raise ValueError(
                f"session {session} has too many active local slots")
        local_scores = {
            local: averaged_scores(records[local], active_ids)
            for local in locals_in_session
        }
        best_objective = None
        best_permutation = None
        for permutation in itertools.permutations(
                sorted(active_ids), len(locals_in_session)):
            objective = sum(
                local_scores[local][speaker_id]
                for local, speaker_id in zip(locals_in_session, permutation))
            key = (objective, tuple(reversed(permutation)))
            if best_objective is None or key > best_objective:
                best_objective = key
                best_permutation = permutation
        session_slots = []
        for local, speaker_id in zip(locals_in_session, best_permutation or ()):
            scores = local_scores[local]
            alternatives = [
                score for identity, score in scores.items()
                if identity != speaker_id
            ]
            assigned_score = scores[speaker_id]
            margin = assigned_score - max(alternatives, default=0.0)
            evidence_ids = sorted(set(records[local]["evidence_ids"]))
            reason = (
                "acoustic_one_to_one_assignment" if evidence_ids
                else "cannot_link_completion")
            assignment = {
                "session": session,
                "local_speaker": local,
                "speaker_id": speaker_id,
                "score": assigned_score,
                "margin": margin,
                "evidence_weight": records[local]["weight"],
                "evidence_ids": evidence_ids,
                "reason": reason,
                "scores": scores,
            }
            assignments[local] = assignment
            session_slots.append(assignment)
        sessions[str(session)] = {
            "objective": best_objective[0] if best_objective else 0.0,
            "slots": session_slots,
        }
    return assignments, sessions


def direct_strong_identity(turn, active_ids, config):
    evidence = turn["titanet"]
    if evidence.get("status") != "ok":
        return None
    ranked = rank_scores(evidence.get("scores", {}), active_ids)
    speaker = config["speaker"]
    if not score_gate(
            ranked,
            float(speaker["local_drift_competing_threshold"]),
            float(speaker["local_drift_competing_margin"])):
        return None
    return ranked[0][0]


def decide_turn(turn, assignments, active_ids, config):
    by_identity = Counter()
    by_local = Counter()
    for segment in turn["diar_segments"]:
        local = int(segment["local_speaker"])
        assignment = assignments.get(local)
        if assignment is None:
            continue
        amount = float(segment["overlap_sec"])
        by_identity[assignment["speaker_id"]] += amount
        by_local[local] += amount

    duration = float(turn["duration_sec"])
    minimum_ratio = float(
        config["timeline"]["speaker_support_min_coverage_ratio"])
    if by_identity:
        speaker_id, amount = max(
            by_identity.items(), key=lambda item: (item[1], item[0]))
        local = max(
            (item for item in by_local.items()
             if assignments[item[0]]["speaker_id"] == speaker_id),
            key=lambda item: (item[1], -item[0]))[0]
        coverage = min(1.0, amount / duration) if duration > EPSILON else 0.0
        return {
            "speaker_id": speaker_id,
            "local_speaker": local,
            "speaker_uncertain": coverage + EPSILON < minimum_ratio,
            "confidence": coverage,
            "coverage_ratio": coverage,
            "reason": "rotated_sortformer_titanet_mapping",
            "candidate_overlap_sec": dict(by_identity),
            "slot_assignment": assignments[local],
        }

    direct = direct_strong_identity(turn, active_ids, config)
    if direct is not None and float(turn["vad"]["coverage_sec"]) > EPSILON:
        return {
            "speaker_id": direct,
            "local_speaker": -1,
            "speaker_uncertain": False,
            "confidence": 1.0,
            "coverage_ratio": 0.0,
            "reason": "strong_turn_voiceprint_without_diar",
            "candidate_overlap_sec": {},
            "slot_assignment": None,
        }
    return {
        "speaker_id": None,
        "local_speaker": -1,
        "speaker_uncertain": True,
        "confidence": 0.0,
        "coverage_ratio": 0.0,
        "reason": "insufficient_acoustic_evidence",
        "candidate_overlap_sec": {},
        "slot_assignment": None,
    }


def build_candidate(evidence):
    turns = evidence["turns"]
    config = evidence["resolved_config"]
    active_ids, active_diagnostics = discover_active_ids(turns, config)
    assignments, sessions = assign_session_slots(turns, active_ids, config)
    decisions = []
    track = []
    for turn in turns:
        decision = decide_turn(
            turn, assignments, active_ids, config)
        decisions.append({"turn_id": turn["turn_id"], **decision})
        track.append({
            "turn_id": turn["turn_id"],
            "start": turn["start_sec"],
            "end": turn["end_sec"],
            "text_id": turn["text_id"],
            "text": turn["text"],
            "speaker": decision["local_speaker"],
            "speaker_id": decision["speaker_id"],
            "speaker_uncertain": decision["speaker_uncertain"],
            "confidence": decision["confidence"],
            "decision_reason": decision["reason"],
        })
    return {
        "schema_version": 1,
        "kind": "orator_frozen_speaker_candidate",
        "candidate_kind": "v21_90s_rotation_titanet_assignment",
        "audio_sec": float(evidence["audio_sec"]),
        "sample_rate": int(evidence["sample_rate"]),
        "active_speaker_ids": active_ids,
        "active_identity_evidence": active_diagnostics,
        "session_count": len(sessions),
        "sessions": sessions,
        "turn_count": len(track),
        "decisions": decisions,
        "track": track,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--evidence", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()
    with open(args.evidence, encoding="utf-8") as source:
        evidence = json.load(source)
    if evidence.get("kind") != "orator_frozen_speaker_evidence":
        raise ValueError("input is not a frozen speaker evidence package")
    result = build_candidate(evidence)
    result["source"] = {
        "path": os.path.abspath(args.evidence),
        "sha256": sha256_file(args.evidence),
    }
    with open(args.out, "w", encoding="utf-8") as output:
        json.dump(result, output, ensure_ascii=False, indent=2)
    print(json.dumps({
        "turns": result["turn_count"],
        "sessions": result["session_count"],
        "active_speaker_ids": result["active_speaker_ids"],
        "out": os.path.abspath(args.out),
    }, ensure_ascii=False))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError) as error:
        raise SystemExit(f"speaker rotation candidate: {error}")
