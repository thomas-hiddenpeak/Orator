#!/usr/bin/env python3
"""Build an auditable speaker candidate from frozen, reference-free evidence."""

import argparse
import bisect
import hashlib
import json
import math
import os
from collections import Counter, defaultdict


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_json(path):
    with open(path, encoding="utf-8") as source:
        return json.load(source)


def rank_score_map(scores, allowed_ids=None):
    ranked = [
        (speaker_id, float(score)) for speaker_id, score in scores.items()
        if allowed_ids is None or speaker_id in allowed_ids
    ]
    return sorted(ranked, key=lambda item: (-item[1], item[0]))


def ranked_scores(turn, allowed_ids=None):
    return rank_score_map(turn["titanet"].get("scores", {}), allowed_ids)


def score_gate(ranked, threshold, margin):
    if len(ranked) < 2:
        return False
    return (ranked[0][1] >= threshold and
            ranked[0][1] - ranked[1][1] >= margin)


def voiceprint_inputs(turns):
    seen = set()
    for turn in turns:
        turn_id = turn["turn_id"]
        if turn_id not in seen:
            seen.add(turn_id)
            yield turn_id, turn["titanet"], float(turn["duration_sec"])
        for segment in turn["diar_segments"]:
            evidence_id = segment["evidence_id"]
            if evidence_id in seen:
                continue
            seen.add(evidence_id)
            evidence = segment.get("titanet", {})
            duration = float(evidence.get(
                "source_duration_sec",
                segment["end_sec"] - segment["start_sec"]))
            yield evidence_id, evidence, duration


def discover_active_ids(turns, config):
    speaker = config["speaker"]
    threshold = float(speaker["local_drift_competing_threshold"])
    margin = float(speaker["local_drift_competing_margin"])
    max_speakers = int(config["diarizer"]["max_speakers"])
    duration = Counter()
    count = Counter()
    for _, evidence, evidence_duration in voiceprint_inputs(turns):
        if evidence.get("status") != "ok":
            continue
        ranked = rank_score_map(evidence.get("scores", {}))
        if not score_gate(ranked, threshold, margin):
            continue
        speaker_id = ranked[0][0]
        duration[speaker_id] += evidence_duration
        count[speaker_id] += 1
    ordered = sorted(
        duration, key=lambda speaker_id: (-duration[speaker_id], speaker_id))
    active = ordered[:max_speakers]
    if len(active) != max_speakers:
        raise ValueError(
            f"expected {max_speakers} active identities, found {len(active)}")
    return active, {
        speaker_id: {
            "strong_evidence_count": count[speaker_id],
            "strong_evidence_duration_sec": duration[speaker_id],
        }
        for speaker_id in active
    }


def dominant_local(turn, minimum_ratio):
    top1_durations = turn["sortformer"]["active_top1_duration_sec"]
    active_durations = turn["sortformer"]["active_duration_sec"]
    active_coverage = float(
        turn["sortformer"]["any_active_duration_sec"])
    if not top1_durations or active_coverage <= 0.0:
        return None, 0.0, 0.0
    local = max(
        range(len(top1_durations)), key=lambda index: top1_durations[index])
    dominance_ratio = min(
        1.0, float(top1_durations[local]) / active_coverage)
    activity_ratio = min(
        1.0, float(active_durations[local]) / active_coverage)
    if (dominance_ratio < minimum_ratio or
            activity_ratio < minimum_ratio):
        return None, dominance_ratio, activity_ratio
    return local, dominance_ratio, activity_ratio


def aggregate_segment_scores(turn, local, active_ids):
    if local is None:
        return [], []
    sums = Counter()
    total_weight = 0.0
    support_ids = []
    allowed = set(active_ids)
    for segment in turn["diar_segments"]:
        if int(segment["local_speaker"]) != local:
            continue
        evidence = segment.get("titanet", {})
        if evidence.get("status") != "ok":
            continue
        weight = float(segment["overlap_sec"])
        if weight <= 0.0:
            continue
        scores = evidence.get("scores", {})
        if not allowed.issubset(scores):
            continue
        for speaker_id in allowed:
            sums[speaker_id] += weight * float(scores[speaker_id])
        total_weight += weight
        support_ids.append(segment["evidence_id"])
    if total_weight <= 0.0:
        return [], []
    return rank_score_map(
        {speaker_id: value / total_weight
         for speaker_id, value in sums.items()}, allowed), support_ids


def source_record(name, ranked, support_ids, config):
    speaker = config["speaker"]
    strong = score_gate(
        ranked,
        float(speaker["local_drift_competing_threshold"]),
        float(speaker["local_drift_competing_margin"]))
    candidate = score_gate(
        ranked,
        float(speaker["local_drift_competing_candidate_threshold"]),
        float(speaker["local_drift_competing_candidate_margin"]))
    return {
        "name": name,
        "ranked": ranked,
        "support_ids": support_ids,
        "strong": strong,
        "candidate": candidate,
    }


def select_direct_voiceprint(sources):
    for strength in ("strong", "candidate"):
        accepted = [source for source in sources if source[strength]]
        if not accepted:
            continue
        identities = {source["ranked"][0][0] for source in accepted}
        if len(identities) != 1:
            return {
                "status": "conflict",
                "strength": strength,
                "sources": accepted,
            }
        selected = max(
            accepted,
            key=lambda source: (
                source["ranked"][0][1] - source["ranked"][1][1],
                source["ranked"][0][1],
                source["name"]))
        source_name = (
            selected["name"] if len(accepted) == 1 else "combined")
        return {
            "status": "selected",
            "strength": strength,
            "source": source_name,
            "speaker_id": selected["ranked"][0][0],
            "ranked": selected["ranked"],
            "support_ids": sorted({
                evidence_id
                for source in accepted
                for evidence_id in source["support_ids"]
            }),
        }
    return {"status": "none", "sources": sources}


def build_turn_features(turns, active_ids, config):
    minimum_ratio = float(
        config["timeline"]["speaker_support_min_coverage_ratio"])
    active_set = set(active_ids)
    features = []
    for turn in turns:
        local, local_ratio, activity_ratio = dominant_local(
            turn, minimum_ratio)
        turn_ranked = (
            ranked_scores(turn, active_set)
            if turn["titanet"]["status"] == "ok" else [])
        segment_ranked, segment_ids = aggregate_segment_scores(
            turn, local, active_ids)
        sources = [
            source_record("turn", turn_ranked, [turn["turn_id"]], config),
            source_record(
                "diar_segment", segment_ranked, segment_ids, config),
        ]
        features.append({
            "turn": turn,
            "local": local,
            "local_ratio": local_ratio,
            "activity_ratio": activity_ratio,
            "sources": sources,
            "direct": select_direct_voiceprint(sources),
        })
    return features


def build_anchors(features):
    anchors = defaultdict(list)
    for feature in features:
        direct = feature["direct"]
        if feature["local"] is None or direct["status"] != "selected":
            continue
        turn = feature["turn"]
        anchors[feature["local"]].append({
            "time_sec": 0.5 * (
                float(turn["start_sec"]) + float(turn["end_sec"])),
            "speaker_id": direct["speaker_id"],
            "score": direct["ranked"][0][1],
            "margin": direct["ranked"][0][1] - direct["ranked"][1][1],
            "strength": direct["strength"],
            "source": direct["source"],
            "turn_id": turn["turn_id"],
            "duration_sec": float(turn["duration_sec"]),
        })
    for values in anchors.values():
        values.sort(key=lambda item: item["time_sec"])
    return anchors


def stable_local_priors(anchors):
    priors = {}
    diagnostics = {}
    for local, values in anchors.items():
        counts = Counter(item["speaker_id"] for item in values)
        diagnostics[str(local)] = {
            "identity_counts": dict(counts),
            "stable_identity": (
                next(iter(counts)) if len(counts) == 1 else None),
        }
        if len(counts) == 1:
            speaker_id = next(iter(counts))
            priors[local] = speaker_id
    return priors, diagnostics


def nearest_anchor_prior(local, time_sec, anchors, max_distance, conflict_gap):
    values = anchors.get(local, [])
    if not values:
        return None
    times = [item["time_sec"] for item in values]
    index = bisect.bisect_left(times, time_sec)
    before = values[index - 1] if index > 0 else None
    after = values[index] if index < len(values) else None
    candidates = []
    if before is not None and time_sec - before["time_sec"] <= max_distance:
        candidates.append((time_sec - before["time_sec"], before))
    if after is not None and after["time_sec"] - time_sec <= max_distance:
        candidates.append((after["time_sec"] - time_sec, after))
    if not candidates:
        return None
    candidates.sort(key=lambda item: item[0])
    if (len(candidates) == 2 and
            candidates[0][1]["speaker_id"] != candidates[1][1]["speaker_id"] and
            abs(candidates[0][0] - candidates[1][0]) <= conflict_gap):
        return None
    distance, anchor = candidates[0]
    confidence = math.exp(-distance / max(max_distance, 1e-9))
    return {
        "speaker_id": anchor["speaker_id"],
        "confidence": confidence,
        "distance_sec": distance,
        "anchor_turn_id": anchor["turn_id"],
    }


def evidence_ids(turn):
    frame_start = turn["sortformer"]["frame_start_index"]
    frame_end = turn["sortformer"]["frame_end_index"]
    values = [turn["turn_id"]]
    if frame_start is not None and frame_end is not None:
        values.append(f"sortformer_frames:{frame_start}:{frame_end}")
    values.extend(item["evidence_id"] for item in turn["diar_segments"])
    values.extend(item["evidence_id"] for item in turn["vad"]["evidence"])
    values.extend(item["evidence_id"] for item in turn["align"]["units"])
    return values


def source_audit(sources):
    return [
        {
            "source": source["name"],
            "strong": source["strong"],
            "candidate": source["candidate"],
            "support_ids": source["support_ids"],
            "scores": [
                {"speaker_id": speaker_id, "score": score}
                for speaker_id, score in source["ranked"]
            ],
        }
        for source in sources if source["ranked"]
    ]


def decide(feature, anchors, stable_priors, config):
    turn = feature["turn"]
    speaker = config["speaker"]
    timeline = config["timeline"]
    local = feature["local"]
    max_distance = float(speaker["local_drift_competing_backfill_sec"])
    conflict_gap = float(
        speaker["local_drift_competing_backfill_gap_sec"])
    minimum_ratio = float(timeline["speaker_support_min_coverage_ratio"])
    active_duration = float(
        turn["sortformer"]["any_active_duration_sec"])
    overlap_duration = float(turn["sortformer"]["overlap_duration_sec"])
    overlap_ratio = (
        overlap_duration / active_duration if active_duration > 0.0 else 0.0)
    midpoint = 0.5 * (float(turn["start_sec"]) + float(turn["end_sec"]))

    base = {
        "speaker_id": None,
        "speaker_uncertain": True,
        "confidence": 0.0,
        "reason": "insufficient_evidence",
        "dominant_local_speaker": local,
        "local_dominance_ratio": feature["local_ratio"],
        "local_activity_ratio": feature["activity_ratio"],
        "overlap_ratio": overlap_ratio,
        "voiceprint_sources": source_audit(feature["sources"]),
        "evidence_ids": evidence_ids(turn),
    }

    direct = feature["direct"]
    if direct["status"] == "conflict":
        base["reason"] = f"{direct['strength']}_voiceprint_conflict"
        return base
    if direct["status"] == "selected":
        best_score = direct["ranked"][0][1]
        margin = best_score - direct["ranked"][1][1]
        base.update({
            "speaker_id": direct["speaker_id"],
            "speaker_uncertain": False,
            "confidence": max(
                0.0, min(1.0, 0.5 * best_score + 0.5 * margin)),
            "reason": (
                f"{direct['strength']}_{direct['source']}_voiceprint"),
        })
        return base

    nearest = (
        nearest_anchor_prior(
            local, midpoint, anchors, max_distance, conflict_gap)
        if local is not None else None)
    prior_id = nearest["speaker_id"] if nearest is not None else None
    prior_confidence = (
        nearest["confidence"] if nearest is not None else 0.0)
    prior_reason = "nearest_local_voiceprint"
    if prior_id is None and local in stable_priors:
        prior_id = stable_priors[local]
        prior_confidence = minimum_ratio
        prior_reason = "stable_local_voiceprint"

    if (prior_id is not None and local is not None and
            overlap_ratio < minimum_ratio and
            turn["vad"]["coverage_sec"] > 0.0):
        base.update({
            "speaker_id": prior_id,
            "speaker_uncertain": False,
            "confidence": min(
                1.0,
                (prior_confidence + feature["local_ratio"] +
                 feature["activity_ratio"]) / 3.0),
            "reason": prior_reason,
        })
    return base


def build_candidate(evidence):
    turns = evidence["turns"]
    config = evidence["resolved_config"]
    active_ids, active_diagnostics = discover_active_ids(turns, config)
    features = build_turn_features(turns, active_ids, config)
    anchors = build_anchors(features)
    stable_priors, local_diagnostics = stable_local_priors(anchors)

    decisions = []
    track = []
    for feature in features:
        turn = feature["turn"]
        decision = decide(feature, anchors, stable_priors, config)
        decisions.append({"turn_id": turn["turn_id"], **decision})
        track.append({
            "turn_id": turn["turn_id"],
            "start": turn["start_sec"],
            "end": turn["end_sec"],
            "text_id": turn["text_id"],
            "text": turn["text"],
            "speaker": (
                decision["dominant_local_speaker"]
                if decision["speaker_id"] is not None and
                decision["dominant_local_speaker"] is not None else -1),
            "speaker_id": decision["speaker_id"],
            "speaker_uncertain": decision["speaker_uncertain"],
            "confidence": decision["confidence"],
            "decision_reason": decision["reason"],
        })

    reason_counts = Counter(item["reason"] for item in decisions)
    return {
        "schema_version": 2,
        "kind": "orator_frozen_speaker_candidate",
        "audio_sec": float(evidence["audio_sec"]),
        "sample_rate": int(evidence["sample_rate"]),
        "active_speaker_ids": active_ids,
        "active_identity_evidence": active_diagnostics,
        "local_identity_evidence": local_diagnostics,
        "anchor_counts": {
            str(local): len(values) for local, values in anchors.items()
        },
        "decision_counts": dict(reason_counts),
        "turn_count": len(track),
        "decisions": decisions,
        "track": track,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--evidence", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()
    evidence = load_json(args.evidence)
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
        "decision_counts": result["decision_counts"],
        "out": os.path.abspath(args.out),
    }, ensure_ascii=False))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError) as error:
        raise SystemExit(f"speaker sequence candidate: {error}")
