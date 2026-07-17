#!/usr/bin/env python3
"""Apply strong short-phrase TitaNet evidence to a frozen business view.

The generator is reference-free. It preserves every baseline attribution unless
all available strong TitaNet sources agree on a different active identity. It
emits an auditable candidate and never evaluates correctness.
"""

import argparse
import hashlib
import json
import os
from collections import Counter


EPSILON = 1e-9


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def rank_scores(scores, allowed_ids=None):
    allowed = set(allowed_ids) if allowed_ids is not None else None
    return sorted(
        ((speaker_id, float(score))
         for speaker_id, score in scores.items()
         if allowed is None or speaker_id in allowed),
        key=lambda item: (-item[1], item[0]))


def score_gate(ranked, threshold, margin):
    return (len(ranked) >= 2 and ranked[0][1] >= threshold and
            ranked[0][1] - ranked[1][1] >= margin)


def unique_voiceprints(turns):
    seen = set()
    for turn in turns:
        evidence_id = str(turn["turn_id"])
        if evidence_id not in seen:
            seen.add(evidence_id)
            yield turn["titanet"], float(turn["duration_sec"])
        for segment in turn["diar_segments"]:
            evidence_id = str(segment["evidence_id"])
            if evidence_id in seen:
                continue
            seen.add(evidence_id)
            yield (segment["titanet"],
                   float(segment["end_sec"]) -
                   float(segment["start_sec"]))


def discover_active_ids(turns, config):
    speaker = config["speaker"]
    threshold = float(speaker["local_drift_competing_threshold"])
    margin = float(speaker["local_drift_competing_margin"])
    max_speakers = int(config["diarizer"]["max_speakers"])
    duration = Counter()
    for evidence, evidence_duration in unique_voiceprints(turns):
        if evidence.get("status") != "ok":
            continue
        ranked = rank_scores(evidence.get("scores", {}))
        if score_gate(ranked, threshold, margin):
            duration[ranked[0][0]] += evidence_duration
    active = sorted(
        duration, key=lambda speaker_id: (-duration[speaker_id], speaker_id)
    )[:max_speakers]
    if len(active) != max_speakers:
        raise ValueError(
            f"expected {max_speakers} active identities, found {len(active)}")
    return active


def aggregate_segment_scores(turn, active_ids):
    sums = Counter()
    total_weight = 0.0
    evidence_ids = []
    allowed = set(active_ids)
    for segment in turn["diar_segments"]:
        evidence = segment["titanet"]
        if evidence.get("status") != "ok":
            continue
        scores = evidence.get("scores", {})
        if not allowed.issubset(scores):
            continue
        weight = float(segment["overlap_sec"])
        if weight <= EPSILON:
            continue
        for speaker_id in active_ids:
            sums[speaker_id] += weight * float(scores[speaker_id])
        total_weight += weight
        evidence_ids.append(str(segment["evidence_id"]))
    if total_weight <= EPSILON:
        return [], []
    return rank_scores(
        {speaker_id: sums[speaker_id] / total_weight
         for speaker_id in active_ids}, active_ids), sorted(set(evidence_ids))


def strong_source(name, ranked, evidence_ids, config):
    speaker = config["speaker"]
    strong = score_gate(
        ranked,
        float(speaker["local_drift_competing_threshold"]),
        float(speaker["local_drift_competing_margin"]))
    return {
        "name": name,
        "strong": strong,
        "ranked": ranked,
        "evidence_ids": evidence_ids,
    }


def select_strong_identity(turn, active_ids, config):
    turn_ranked = (
        rank_scores(turn["titanet"].get("scores", {}), active_ids)
        if turn["titanet"].get("status") == "ok" else [])
    segment_ranked, segment_ids = aggregate_segment_scores(turn, active_ids)
    sources = [
        strong_source("turn", turn_ranked, [str(turn["turn_id"])], config),
        strong_source("diar_segment", segment_ranked, segment_ids, config),
    ]
    accepted = [source for source in sources if source["strong"]]
    if not accepted:
        return None, "no_strong_voiceprint", sources
    identities = {source["ranked"][0][0] for source in accepted}
    if len(identities) != 1:
        return None, "strong_voiceprint_conflict", sources
    identity = next(iter(identities))
    if len(accepted) == 2:
        reason = "strong_combined_voiceprint"
    else:
        reason = f"strong_{accepted[0]['name']}_voiceprint"
    return identity, reason, sources


def decide_turn(turn, active_ids, config):
    baseline = turn["baseline_output"]
    baseline_id = baseline.get("speaker_id")
    selected, evidence_reason, sources = select_strong_identity(
        turn, active_ids, config)
    changed = selected is not None and selected != baseline_id
    if not changed:
        selected = baseline_id
        reason = (
            "baseline_voiceprint_confirmed"
            if selected is not None and evidence_reason != "no_strong_voiceprint"
            else "baseline_passthrough")
    elif baseline_id:
        reason = f"{evidence_reason}_override"
    else:
        reason = f"{evidence_reason}_fill"
    return {
        "speaker_id": selected,
        "local_speaker": int(baseline.get("local_speaker", -1)),
        "speaker_uncertain": (
            bool(baseline.get("speaker_uncertain", False))
            if not changed else False),
        "reason": reason,
        "changed": changed,
        "baseline_speaker_id": baseline_id,
        "voiceprint_sources": sources,
    }


def build_candidate(evidence):
    turns = evidence["turns"]
    config = evidence["resolved_config"]
    active_ids = discover_active_ids(turns, config)
    decisions = []
    track = []
    for turn in turns:
        decision = decide_turn(turn, active_ids, config)
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
            "decision_reason": decision["reason"],
        })
    return {
        "schema_version": 1,
        "kind": "orator_frozen_speaker_candidate",
        "candidate_kind": "v21_strong_short_phrase_voiceprint",
        "audio_sec": float(evidence["audio_sec"]),
        "sample_rate": int(evidence["sample_rate"]),
        "active_speaker_ids": active_ids,
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
        "active_speaker_ids": result["active_speaker_ids"],
        "out": os.path.abspath(args.out),
    }, ensure_ascii=False))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError) as error:
        raise SystemExit(f"speaker short phrase candidate: {error}")
