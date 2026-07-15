#!/usr/bin/env python3
"""Reconstruct reference-free speaker decisions from a frozen timeline.

The tool reads only terminal diarization, ASR, alignment, and business tracks.
It never reads a reference transcript, changes a selected speaker, or assigns
correctness. Timelines that already contain runtime decisions must match the
reconstruction exactly.
"""

import argparse
from collections import Counter, defaultdict
import hashlib
import json
import os


EPSILON = 1e-9
LEGACY_CANDIDATE_CONFIDENCE_ERROR = 0.000501
LEGACY_MARGIN_CONFIDENCE_ERROR = 0.001002
TIMELINE_QUANTUM_SEC = 0.001


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_json(path):
    with open(path, encoding="utf-8") as source:
        return json.load(source)


def timeline_from(package):
    timeline = package.get("timeline", package)
    if not isinstance(timeline, dict):
        raise ValueError("timeline package is not an object")
    return timeline


def tracks_by_kind(timeline):
    return {
        track["kind"]: track.get("entries", [])
        for track in timeline.get("tracks", [])
        if isinstance(track, dict) and isinstance(track.get("kind"), str)
    }


def overlap(start, end, other_start, other_end):
    return max(0.0, min(end, other_end) - max(start, other_start))


def merge_intervals(intervals):
    merged = []
    for start, end in sorted(intervals):
        if not merged or start > merged[-1][1] + EPSILON:
            merged.append([start, end])
        else:
            merged[-1][1] = max(merged[-1][1], end)
    return merged


def rounded(value, digits):
    return float(f"{float(value):.{digits}f}")


def normalized_speaker_id(value):
    return value if isinstance(value, str) and value else None


def infer_projection_sources(business, asr, align):
    business_count = Counter(int(entry.get("text_id", -1))
                             for entry in business)
    asr_by_id = {int(entry.get("text_id", -1)): entry for entry in asr}
    align_ids = {
        int(entry.get("text_id", -1))
        for entry in align if entry.get("units")
    }
    sources = []
    for entry in business:
        runtime = entry.get("speaker_decision")
        if isinstance(runtime, dict) and isinstance(
                runtime.get("text_projection_source"), str):
            sources.append(runtime["text_projection_source"])
            continue
        text_id = int(entry.get("text_id", -1))
        raw = asr_by_id.get(text_id)
        exact_span = (
            raw is not None and
            abs(float(raw.get("start", 0.0)) -
                float(entry.get("start", 0.0))) <= EPSILON and
            abs(float(raw.get("end", 0.0)) -
                float(entry.get("end", 0.0))) <= EPSILON)
        if business_count[text_id] == 1 and exact_span:
            sources.append("asr_exact")
        elif text_id in align_ids:
            sources.append("forced_alignment")
        else:
            sources.append("asr_proportional")
    return sources


def compute_decision(entry, diar_entries, projection_source,
                     gap_fill_enabled=True):
    start = float(entry["start"])
    end = float(entry["end"])
    duration = max(0.0, end - start)
    selected_key = (
        int(entry.get("speaker", -1)),
        normalized_speaker_id(entry.get("speaker_id")),
    )
    accumulated = defaultdict(lambda: {
        "intervals": [],
        "confidence_weighted_sum": 0.0,
        "confidence_weight": 0.0,
    })
    for segment in diar_entries:
        amount = overlap(
            start, end, float(segment["start"]), float(segment["end"]))
        if amount <= 0.0:
            continue
        key = (
            int(segment.get("speaker", -1)),
            normalized_speaker_id(segment.get("speaker_id")),
        )
        value = accumulated[key]
        value["intervals"].append([
            max(start, float(segment["start"])),
            min(end, float(segment["end"])),
        ])
        value["confidence_weighted_sum"] += (
            float(segment.get("confidence", 0.0)) * amount)
        value["confidence_weight"] += amount

    candidates = []
    raw_metrics = {}
    for key, value in accumulated.items():
        intervals = merge_intervals(value["intervals"])
        covered = sum(item_end - item_start
                      for item_start, item_end in intervals)
        confidence = (
            value["confidence_weighted_sum"] / value["confidence_weight"]
            if value["confidence_weight"] > EPSILON else 0.0)
        raw_metrics[key] = (covered, confidence)
        candidate = {
            "speaker": key[0],
            "overlap_sec": rounded(covered, 3),
            "coverage_ratio": rounded(
                min(1.0, covered / duration) if duration > EPSILON else 0.0,
                6),
            "confidence": rounded(confidence, 6),
            "island_count": len(intervals),
            "selected": key == selected_key,
        }
        if key[1] is not None:
            candidate["speaker_id"] = key[1]
        candidates.append(candidate)

    candidates.sort(key=lambda candidate: (
        not candidate["selected"],
        -raw_metrics[(candidate["speaker"], normalized_speaker_id(
            candidate.get("speaker_id")))][0],
        -raw_metrics[(candidate["speaker"], normalized_speaker_id(
            candidate.get("speaker_id")))][1],
        candidate["speaker"],
        candidate.get("speaker_id", ""),
    ))
    selected = next(
        (candidate for candidate in candidates if candidate["selected"]),
        None)
    rejected = [candidate for candidate in candidates
                if not candidate["selected"]]
    best_rejected = rejected[0] if rejected else None

    reason = "no_diar_support"
    overlap_margin = 0.0
    confidence_margin = 0.0
    if selected is not None and best_rejected is not None:
        reason = "competing_diar_interval_policy"
        selected_raw = raw_metrics[selected_key]
        rejected_key = (
            best_rejected["speaker"],
            normalized_speaker_id(best_rejected.get("speaker_id")),
        )
        rejected_raw = raw_metrics[rejected_key]
        overlap_margin = selected_raw[0] - rejected_raw[0]
        confidence_margin = selected_raw[1] - rejected_raw[1]
    elif selected is not None:
        if (gap_fill_enabled and selected["island_count"] > 1 and
                selected["coverage_ratio"] < 1.0 - EPSILON):
            reason = "same_speaker_gap_fill"
        else:
            reason = "sole_diar_support"

    return {
        "speaker_source": "sortformer_diarization",
        "text_projection_source": projection_source,
        "reason": reason,
        "overlap_margin_sec": rounded(overlap_margin, 3),
        "confidence_margin": rounded(confidence_margin, 6),
        "candidates": candidates,
    }


def normalize_runtime_decision(decision):
    normalized = {
        "speaker_source": str(decision.get("speaker_source", "")),
        "text_projection_source": str(
            decision.get("text_projection_source", "")),
        "reason": str(decision.get("reason", "")),
        "overlap_margin_sec": rounded(
            decision.get("overlap_margin_sec", 0.0), 3),
        "confidence_margin": rounded(
            decision.get("confidence_margin", 0.0), 6),
        "candidates": [],
    }
    for source in decision.get("candidates", []):
        candidate = {
            "speaker": int(source.get("speaker", -1)),
            "overlap_sec": rounded(source.get("overlap_sec", 0.0), 3),
            "coverage_ratio": rounded(
                source.get("coverage_ratio", 0.0), 6),
            "confidence": rounded(source.get("confidence", 0.0), 6),
            "island_count": int(source.get("island_count", 0)),
            "selected": bool(source.get("selected", False)),
        }
        speaker_id = normalized_speaker_id(source.get("speaker_id"))
        if speaker_id is not None:
            candidate["speaker_id"] = speaker_id
        normalized["candidates"].append(candidate)
    return normalized


def decision_structure(decision):
    return {
        "speaker_source": decision["speaker_source"],
        "text_projection_source": decision["text_projection_source"],
        "reason": decision["reason"],
        "candidates": [
            {key: value for key, value in candidate.items()
             if key not in {"overlap_sec", "coverage_ratio", "confidence"}}
            for candidate in decision["candidates"]
        ],
    }


def overlap_error_bound(candidate):
    return TIMELINE_QUANTUM_SEC * (
        int(candidate["island_count"]) + 1)


def coverage_error_bound(candidate, duration):
    overlap_error = overlap_error_bound(candidate)
    denominator = max(duration - TIMELINE_QUANTUM_SEC, TIMELINE_QUANTUM_SEC)
    return (
        overlap_error +
        float(candidate["coverage_ratio"]) * TIMELINE_QUANTUM_SEC
    ) / denominator + 0.000001


def compare_runtime_decision(runtime, replay, duration):
    if decision_structure(runtime) != decision_structure(replay):
        return False, False
    if len(runtime["candidates"]) != len(replay["candidates"]):
        return False, False
    candidate_deltas = [
        abs(left["confidence"] - right["confidence"])
        for left, right in zip(runtime["candidates"], replay["candidates"])
    ]
    overlap_deltas = [
        abs(left["overlap_sec"] - right["overlap_sec"])
        for left, right in zip(runtime["candidates"], replay["candidates"])
    ]
    coverage_deltas = [
        abs(left["coverage_ratio"] - right["coverage_ratio"])
        for left, right in zip(runtime["candidates"], replay["candidates"])
    ]
    margin_delta = abs(
        runtime["confidence_margin"] - replay["confidence_margin"])
    overlap_margin_delta = abs(
        runtime["overlap_margin_sec"] - replay["overlap_margin_sec"])
    exact = (
        all(delta <= EPSILON for delta in candidate_deltas) and
        all(delta <= EPSILON for delta in overlap_deltas) and
        all(delta <= EPSILON for delta in coverage_deltas) and
        margin_delta <= EPSILON and overlap_margin_delta <= EPSILON)
    overlap_margin_bound = sum(
        overlap_error_bound(candidate)
        for candidate in replay["candidates"][:2])
    bounded = (
        all(delta <= LEGACY_CANDIDATE_CONFIDENCE_ERROR
            for delta in candidate_deltas) and
        all(delta <= overlap_error_bound(candidate)
            for delta, candidate in zip(overlap_deltas,
                                        replay["candidates"])) and
        all(delta <= coverage_error_bound(candidate, duration)
            for delta, candidate in zip(coverage_deltas,
                                        replay["candidates"])) and
        margin_delta <= LEGACY_MARGIN_CONFIDENCE_ERROR and
        overlap_margin_delta <= overlap_margin_bound)
    return bounded, exact


def raw_overlap_summary(diar_entries):
    intersections = []
    pair_count = 0
    pair_overlap_sec = 0.0
    ordered = sorted(diar_entries, key=lambda entry: (
        float(entry["start"]), float(entry["end"]),
        int(entry.get("speaker", -1))))
    for index, left in enumerate(ordered):
        left_end = float(left["end"])
        for right in ordered[index + 1:]:
            right_start = float(right["start"])
            if right_start >= left_end - EPSILON:
                break
            if int(left.get("speaker", -1)) == int(
                    right.get("speaker", -1)):
                continue
            amount = overlap(
                float(left["start"]), left_end,
                right_start, float(right["end"]))
            if amount <= EPSILON:
                continue
            pair_count += 1
            pair_overlap_sec += amount
            intersections.append([
                max(float(left["start"]), right_start),
                min(left_end, float(right["end"])),
            ])
    union = merge_intervals(intersections)
    return {
        "cross_speaker_pair_count": pair_count,
        "pair_overlap_sec": rounded(pair_overlap_sec, 3),
        "contested_union_sec": rounded(sum(
            end - start for start, end in union), 3),
    }


def build_evidence(package, source_path):
    timeline = timeline_from(package)
    tracks = tracks_by_kind(timeline)
    required = {"diarization", "asr", "align", "business_speaker"}
    missing = sorted(required - set(tracks))
    if missing:
        raise ValueError("timeline tracks missing: " + ",".join(missing))
    diar = tracks["diarization"]
    business = tracks["business_speaker"]
    projection_sources = infer_projection_sources(
        business, tracks["asr"], tracks["align"])
    gap_fill_enabled = bool(
        timeline.get("resolved_config", {}).get("timeline", {}).get(
            "gap_fill_enabled", True))

    rows = []
    runtime_count = 0
    exact_runtime_count = 0
    for index, (entry, projection_source) in enumerate(
            zip(business, projection_sources)):
        decision = compute_decision(
            entry, diar, projection_source, gap_fill_enabled)
        runtime = entry.get("speaker_decision")
        if runtime is not None:
            runtime_count += 1
            normalized = normalize_runtime_decision(runtime)
            bounded, exact = compare_runtime_decision(
                normalized, decision,
                float(entry["end"]) - float(entry["start"]))
            if not bounded:
                raise ValueError(
                    f"runtime decision mismatch at business entry {index}: "
                    f"runtime={normalized} replay={decision}")
            exact_runtime_count += int(exact)
        row = {
            "business_index": index,
            "start": float(entry["start"]),
            "end": float(entry["end"]),
            "text_id": int(entry.get("text_id", -1)),
            "speaker": int(entry.get("speaker", -1)),
            "speaker_support": entry.get("speaker_support"),
            "speaker_uncertain": bool(entry.get("speaker_uncertain", False)),
            "text": str(entry.get("text", "")),
            "speaker_decision": decision,
            "confidence_order_ambiguous": any(
                abs(left["overlap_sec"] - right["overlap_sec"]) <= 0.001 and
                abs(left["confidence"] - right["confidence"]) <=
                LEGACY_MARGIN_CONFIDENCE_ERROR
                for candidate_index, left in enumerate(decision["candidates"])
                for right in decision["candidates"][candidate_index + 1:]),
        }
        speaker_id = normalized_speaker_id(entry.get("speaker_id"))
        if speaker_id is not None:
            row["speaker_id"] = speaker_id
        rows.append(row)

    reason_counts = Counter(
        row["speaker_decision"]["reason"] for row in rows)
    competing = [row for row in rows if row["speaker_decision"]["reason"] ==
                 "competing_diar_interval_policy"]
    result = {
        "schema_version": 1,
        "kind": "orator_speaker_decision_evidence",
        "source": {
            "path": os.path.abspath(source_path),
            "sha256": sha256_file(source_path),
        },
        "audio_sec": float(timeline.get("audio_sec", 0.0)),
        "business_entry_count": len(rows),
        "runtime_decision_count": runtime_count,
        "runtime_replay_match": runtime_count == len(rows),
        "runtime_replay_mode": (
            "legacy_reconstructed" if runtime_count == 0 else
            "exact" if exact_runtime_count == runtime_count else
            "terminal_quantization_envelope"),
        "confidence_reconstruction": {
            "legacy_diar_quantum": 0.001,
            "candidate_error_bound":
                LEGACY_CANDIDATE_CONFIDENCE_ERROR,
            "margin_error_bound": LEGACY_MARGIN_CONFIDENCE_ERROR,
            "ambiguous_order_count": sum(
                row["confidence_order_ambiguous"] for row in rows),
        },
        "time_reconstruction": {
            "timeline_quantum_sec": TIMELINE_QUANTUM_SEC,
            "overlap_error_formula":
                "timeline_quantum_sec * (island_count + 1)",
            "coverage_error_formula":
                "(overlap_error + coverage * timeline_quantum_sec) / "
                "max(duration - timeline_quantum_sec, timeline_quantum_sec)",
        },
        "raw_diarization": {
            "entry_count": len(diar),
            **raw_overlap_summary(diar),
        },
        "summary": {
            "reason_counts": dict(sorted(reason_counts.items())),
            "competing_entry_count": len(competing),
            "competing_duration_sec": rounded(sum(
                row["end"] - row["start"] for row in competing), 3),
            "strong_competing_count": sum(
                row["speaker_support"] == "strong" for row in competing),
            "zero_overlap_margin_count": sum(
                abs(row["speaker_decision"]["overlap_margin_sec"]) <= EPSILON
                for row in competing),
            "selected_lower_than_best_rejected_count": sum(
                row["speaker_decision"]["confidence_margin"] < -EPSILON
                for row in competing),
            "selected_lower_than_any_rejected_count": sum(
                next(candidate["confidence"] for candidate in
                     row["speaker_decision"]["candidates"]
                     if candidate["selected"]) <
                max(candidate["confidence"] for candidate in
                    row["speaker_decision"]["candidates"]
                    if not candidate["selected"])
                for row in competing),
        },
        "entries": rows,
    }
    return result


def write_json(path, value):
    parent = os.path.dirname(os.path.abspath(path))
    os.makedirs(parent, exist_ok=True)
    with open(path, "w", encoding="utf-8") as output:
        json.dump(value, output, ensure_ascii=False, indent=2)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--timeline", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()
    package = load_json(args.timeline)
    result = build_evidence(package, args.timeline)
    write_json(args.out, result)
    print(json.dumps({
        "business_entries": result["business_entry_count"],
        "runtime_decisions": result["runtime_decision_count"],
        "runtime_replay_match": result["runtime_replay_match"],
        "runtime_replay_mode": result["runtime_replay_mode"],
        **result["summary"],
        "out": os.path.abspath(args.out),
    }, ensure_ascii=False))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, TypeError) as error:
        raise SystemExit(f"speaker decision evidence: {error}")
