#!/usr/bin/env python3
"""Build reference-free multi-scale TitaNet evidence on the session clock.

The span mode prepares native TitaNet probe inputs. The build mode combines
probe scores only for identities active in the captured runtime session. It
never reads a reference transcript and never assigns correctness.
"""

import argparse
import csv
import hashlib
import json
import os
import tomllib
from collections import Counter, defaultdict


EPSILON = 1e-9


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_json(path):
    with open(path, encoding="utf-8") as source:
        return json.load(source)


def load_toml(path):
    with open(path, "rb") as source:
        return tomllib.load(source)


def timeline_from(package):
    timeline = package.get("timeline", package)
    if not isinstance(timeline, dict):
        raise ValueError("timeline package is not an object")
    return timeline


def track_entries(timeline, kind):
    for track in timeline.get("tracks", []):
        if isinstance(track, dict) and track.get("kind") == kind:
            entries = track.get("entries", [])
            if not isinstance(entries, list):
                raise ValueError(f"{kind} track entries are not a list")
            return entries
    raise ValueError(f"timeline is missing {kind} track")


def read_policy(config, timeline):
    section = config.get("sliding_voiceprint")
    if not isinstance(section, dict):
        raise ValueError("config is missing [sliding_voiceprint]")
    windows = sorted({float(value) for value in section["window_sec"]})
    step = float(section["step_sec"])
    digits = int(section["round_digits"])
    agreement = int(section["min_scale_agreement"])
    max_gap = float(section["max_center_gap_sec"])
    min_run = float(section["min_run_sec"])
    projection = section.get("projection")
    if not isinstance(projection, dict):
        raise ValueError(
            "config is missing [sliding_voiceprint.projection]")
    unknown_fill = bool(projection["unknown_fill_enabled"])
    known_override = bool(projection["known_override_enabled"])
    override_sec = float(
        projection["known_override_min_rolling_sec"])
    override_ratio = float(
        projection["known_override_min_rolling_ratio"])
    competing_sec = float(
        projection["known_override_max_competing_rolling_sec"])
    clip_candidate = bool(
        projection["candidate_override_clip_to_rolling"])
    align_pause = float(projection["align_pause_min_sec"])
    snap_tolerance = float(projection["boundary_snap_tolerance_sec"])
    inward_tolerance = float(
        projection["boundary_snap_inward_tolerance_sec"])
    reject_sandwich = bool(
        projection["reject_selected_neighbor_sandwich"])
    neighbor_gap = float(projection["neighbor_gap_sec"])
    if len(windows) < 2 or any(value <= 0.0 for value in windows):
        raise ValueError("window_sec must contain at least two positive values")
    if step <= 0.0:
        raise ValueError("step_sec must be positive")
    if digits < 0 or digits > 9:
        raise ValueError("round_digits must be between 0 and 9")
    if agreement < 2 or agreement > len(windows):
        raise ValueError("min_scale_agreement is outside window_sec")
    if max_gap + EPSILON < step:
        raise ValueError("max_center_gap_sec must be at least step_sec")
    if min_run <= 0.0:
        raise ValueError("min_run_sec must be positive")
    if (override_sec < 0.0 or competing_sec < 0.0 or
            align_pause < 0.0 or snap_tolerance < 0.0 or
            inward_tolerance < 0.0 or neighbor_gap < 0.0):
        raise ValueError("projection durations must be non-negative")
    if override_ratio < 0.0 or override_ratio > 1.0:
        raise ValueError(
            "known_override_min_rolling_ratio must be between 0 and 1")

    resolved = timeline.get("resolved_config", {})
    speaker = resolved.get("speaker", {})
    diarizer = resolved.get("diarizer", {})
    required_speaker = {
        "local_drift_competing_candidate_threshold",
        "local_drift_competing_candidate_margin",
        "local_drift_competing_threshold",
        "local_drift_competing_margin",
    }
    missing = sorted(required_speaker - set(speaker))
    if missing:
        raise ValueError(
            "resolved speaker config is missing: " + ",".join(missing))
    if "max_speakers" not in diarizer:
        raise ValueError("resolved diarizer config is missing max_speakers")
    return {
        "window_sec": windows,
        "step_sec": step,
        "round_digits": digits,
        "min_scale_agreement": agreement,
        "max_center_gap_sec": max_gap,
        "min_run_sec": min_run,
        "max_speakers": int(diarizer["max_speakers"]),
        "candidate_score": float(
            speaker["local_drift_competing_candidate_threshold"]),
        "candidate_margin": float(
            speaker["local_drift_competing_candidate_margin"]),
        "strong_score": float(
            speaker["local_drift_competing_threshold"]),
        "strong_margin": float(
            speaker["local_drift_competing_margin"]),
        "projection": {
            "unknown_fill_enabled": unknown_fill,
            "known_override_enabled": known_override,
            "known_override_min_rolling_sec": override_sec,
            "known_override_min_rolling_ratio": override_ratio,
            "known_override_max_competing_rolling_sec": competing_sec,
            "candidate_override_clip_to_rolling": clip_candidate,
            "align_pause_min_sec": align_pause,
            "boundary_snap_tolerance_sec": snap_tolerance,
            "boundary_snap_inward_tolerance_sec": inward_tolerance,
            "reject_selected_neighbor_sandwich": reject_sandwich,
            "neighbor_gap_sec": neighbor_gap,
        },
    }


def discover_active_ids(timeline, max_speakers):
    durations = Counter()
    counts = Counter()
    for entry in track_entries(timeline, "diarization"):
        speaker_id = entry.get("speaker_id")
        start = float(entry.get("start", 0.0))
        end = float(entry.get("end", start))
        if not isinstance(speaker_id, str) or not speaker_id or end <= start:
            continue
        durations[speaker_id] += end - start
        counts[speaker_id] += 1
    ordered = sorted(
        durations, key=lambda speaker_id: (-durations[speaker_id], speaker_id))
    active = ordered[:max_speakers]
    if len(active) != max_speakers:
        raise ValueError(
            f"expected {max_speakers} active identities, found {len(active)}")
    diagnostics = {
        speaker_id: {
            "diar_segment_count": counts[speaker_id],
            "diar_duration_sec": durations[speaker_id],
        }
        for speaker_id in active
    }
    return active, diagnostics


def generate_spans(audio_sec, policy):
    spans = []
    digits = policy["round_digits"]
    step = policy["step_sec"]
    for window in policy["window_sec"]:
        index = 0
        while True:
            start = round(index * step, digits)
            end = round(start + window, digits)
            if end > audio_sec + EPSILON:
                break
            spans.append({
                "evidence_id": f"multiscale:{window:.3f}:{index}",
                "start_sec": start,
                "end_sec": end,
            })
            index += 1
    return spans


def write_spans(path, spans):
    parent = os.path.dirname(os.path.abspath(path))
    os.makedirs(parent, exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="") as output:
        writer = csv.writer(output, delimiter="\t", lineterminator="\n")
        writer.writerow(["evidence_id", "start_sec", "end_sec"])
        for span in spans:
            writer.writerow([
                span["evidence_id"],
                f"{span['start_sec']:.6f}",
                f"{span['end_sec']:.6f}",
            ])


def read_titanet(path, digits):
    grouped = defaultdict(dict)
    seen = set()
    with open(path, encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source, delimiter="\t")
        fields = set(reader.fieldnames or [])
        required = {"evidence_id", "start_sec", "end_sec", "status"}
        missing = sorted(required - fields)
        if missing:
            raise ValueError("TitaNet TSV is missing: " + ",".join(missing))
        score_columns = sorted(
            name for name in fields if name.startswith("score_"))
        if not score_columns:
            raise ValueError("TitaNet TSV has no score columns")
        for row_number, row in enumerate(reader, 2):
            evidence_id = row["evidence_id"]
            if evidence_id in seen:
                raise ValueError(f"duplicate TitaNet evidence ID {evidence_id}")
            seen.add(evidence_id)
            start = float(row["start_sec"])
            end = float(row["end_sec"])
            if end <= start:
                raise ValueError(f"invalid TitaNet interval at row {row_number}")
            window = round(end - start, digits)
            center = round(0.5 * (start + end), digits)
            if window in grouped[center]:
                raise ValueError(
                    f"duplicate TitaNet scale {window} at centre {center}")
            grouped[center][window] = {
                "evidence_id": evidence_id,
                "start_sec": start,
                "end_sec": end,
                "status": row["status"],
                "scores": {
                    name.removeprefix("score_"): float(row[name])
                    for name in score_columns if row.get(name)
                },
            }
    if not seen:
        raise ValueError("TitaNet TSV is empty")
    return grouped, len(seen)


def read_titanet_by_id(path):
    rows = {}
    with open(path, encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source, delimiter="\t")
        fields = set(reader.fieldnames or [])
        required = {"evidence_id", "start_sec", "end_sec", "status"}
        missing = sorted(required - fields)
        if missing:
            raise ValueError("TitaNet TSV is missing: " + ",".join(missing))
        score_columns = sorted(
            name for name in fields if name.startswith("score_"))
        if not score_columns:
            raise ValueError("TitaNet TSV has no score columns")
        for row_number, row in enumerate(reader, 2):
            evidence_id = row["evidence_id"]
            if evidence_id in rows:
                raise ValueError(f"duplicate TitaNet evidence ID {evidence_id}")
            start = float(row["start_sec"])
            end = float(row["end_sec"])
            if end <= start:
                raise ValueError(f"invalid TitaNet interval at row {row_number}")
            rows[evidence_id] = {
                "evidence_id": evidence_id,
                "start_sec": start,
                "end_sec": end,
                "status": row["status"],
                "scores": {
                    name.removeprefix("score_"): float(row[name])
                    for name in score_columns if row.get(name)
                },
            }
    if not rows:
        raise ValueError("TitaNet TSV is empty")
    return rows


def rank_active(scores, active_ids):
    missing = [speaker_id for speaker_id in active_ids
               if speaker_id not in scores]
    if missing:
        raise ValueError(
            "TitaNet scores are missing active identities: " +
            ",".join(missing))
    return sorted(
        ((speaker_id, float(scores[speaker_id]))
         for speaker_id in active_ids),
        key=lambda item: (-item[1], item[0]))


def scale_evaluation(row, active_ids, policy):
    if row["status"] != "ok":
        return {
            "evidence_id": row["evidence_id"],
            "status": row["status"],
            "candidate": False,
            "strong": False,
        }
    ranked = rank_active(row["scores"], active_ids)
    score = ranked[0][1]
    margin = score - ranked[1][1]
    return {
        "evidence_id": row["evidence_id"],
        "status": "ok",
        "speaker_id": ranked[0][0],
        "score": score,
        "second_id": ranked[1][0],
        "second_score": ranked[1][1],
        "margin": margin,
        "candidate": (
            score >= policy["candidate_score"] and
            margin >= policy["candidate_margin"]),
        "strong": (
            score >= policy["strong_score"] and
            margin >= policy["strong_margin"]),
    }


def consensus(evaluations, field, minimum):
    votes = Counter(
        value["speaker_id"] for value in evaluations.values()
        if value.get(field))
    if not votes:
        return None
    ordered = votes.most_common()
    if ordered[0][1] < minimum:
        return None
    if len(ordered) > 1 and ordered[0][1] == ordered[1][1]:
        return None
    return ordered[0][0]


def build_points(grouped, active_ids, policy):
    configured = set(policy["window_sec"])
    minimum = policy["min_scale_agreement"]
    selected = []
    rejected = Counter()
    audits = []
    for center in sorted(grouped):
        rows = grouped[center]
        available = {
            scale: rows[scale] for scale in configured if scale in rows
        }
        evaluations = {
            str(scale): scale_evaluation(row, active_ids, policy)
            for scale, row in sorted(available.items())
        }
        if len(evaluations) < minimum:
            rejected["insufficient_scales"] += 1
            continue
        speaker_id = consensus(evaluations, "candidate", minimum)
        if speaker_id is None:
            rejected["candidate_gate_or_agreement"] += 1
            audits.append({
                "center_sec": center,
                "status": "rejected",
                "scales": evaluations,
            })
            continue
        strong_id = consensus(evaluations, "strong", minimum)
        strength = "strong" if strong_id == speaker_id else "candidate"
        support = [
            value for value in evaluations.values()
            if value.get("candidate") and value.get("speaker_id") == speaker_id
        ]
        point = {
            "center_sec": center,
            "speaker_id": speaker_id,
            "strength": strength,
            "score_floor": min(value["score"] for value in support),
            "margin_floor": min(value["margin"] for value in support),
            "evidence_ids": sorted(
                value["evidence_id"] for value in support),
            "scales": evaluations,
        }
        selected.append(point)
        audits.append({"status": "selected", **point})
    return selected, audits, dict(rejected)


def finalize_run(points, policy, audio_sec):
    step = policy["step_sec"]
    start = max(0.0, points[0]["center_sec"] - 0.5 * step)
    end = min(audio_sec, points[-1]["center_sec"] + 0.5 * step)
    return {
        "start_sec": start,
        "end_sec": end,
        "duration_sec": end - start,
        "speaker_id": points[0]["speaker_id"],
        "point_count": len(points),
        "strong_point_count": sum(
            point["strength"] == "strong" for point in points),
        "score_floor": min(point["score_floor"] for point in points),
        "margin_floor": min(point["margin_floor"] for point in points),
        "center_start_sec": points[0]["center_sec"],
        "center_end_sec": points[-1]["center_sec"],
        "evidence_ids": sorted({
            evidence_id for point in points
            for evidence_id in point["evidence_ids"]
        }),
    }


def build_runs(points, policy, audio_sec):
    if not points:
        return [], []
    raw = []
    current = [points[0]]
    for point in points[1:]:
        previous = current[-1]
        same_identity = point["speaker_id"] == previous["speaker_id"]
        close = (
            point["center_sec"] - previous["center_sec"] <=
            policy["max_center_gap_sec"] + EPSILON)
        if same_identity and close:
            current.append(point)
            continue
        raw.append(finalize_run(current, policy, audio_sec))
        current = [point]
    raw.append(finalize_run(current, policy, audio_sec))
    accepted = [
        run for run in raw
        if run["duration_sec"] + EPSILON >= policy["min_run_sec"]
    ]
    return accepted, raw


def interval_overlap(start, end, other_start, other_end):
    return max(0.0, min(end, other_end) - max(start, other_start))


def rolling_support(start, end, runs):
    duration = Counter()
    evidence = defaultdict(list)
    for index, run in enumerate(runs):
        amount = interval_overlap(
            start, end, float(run["start_sec"]), float(run["end_sec"]))
        if amount <= EPSILON:
            continue
        speaker_id = run["speaker_id"]
        duration[speaker_id] += amount
        evidence[speaker_id].append({
            "run_index": index,
            "start_sec": run["start_sec"],
            "end_sec": run["end_sec"],
            "overlap_sec": amount,
            "point_count": run["point_count"],
            "strong_point_count": run["strong_point_count"],
            "score_floor": run["score_floor"],
            "margin_floor": run["margin_floor"],
            "evidence_ids": run["evidence_ids"],
        })
    return duration, evidence


def direct_voiceprint(row, active_ids, policy):
    if row["status"] != "ok":
        return {"status": row["status"], "candidate": False}
    ranked = rank_active(row["scores"], active_ids)
    score = ranked[0][1]
    margin = score - ranked[1][1]
    return {
        "status": "ok",
        "speaker_id": ranked[0][0],
        "score": score,
        "second_id": ranked[1][0],
        "second_score": ranked[1][1],
        "margin": margin,
        "candidate": (
            score >= policy["candidate_score"] and
            margin >= policy["candidate_margin"]),
        "strong": (
            score >= policy["strong_score"] and
            margin >= policy["strong_margin"]),
        "evidence_id": row["evidence_id"],
    }


def selected_neighbor_sandwich(selected, previous_entry, entry, next_entry,
                               max_gap):
    if previous_entry is None or next_entry is None:
        return False
    if (previous_entry.get("speaker_id") != selected or
            next_entry.get("speaker_id") != selected):
        return False
    before_gap = float(entry["start"]) - float(previous_entry["end"])
    after_gap = float(next_entry["start"]) - float(entry["end"])
    return (before_gap <= max_gap + EPSILON and
            after_gap <= max_gap + EPSILON)


def alignment_boundaries(entry, align_by_id, minimum_pause):
    start = float(entry["start"])
    end = float(entry["end"])
    boundaries = [start, end]
    group = align_by_id.get(int(entry.get("text_id", -1)))
    if group is None:
        return boundaries
    units = sorted(
        (unit for unit in group.get("units", [])
         if isinstance(unit, dict)),
        key=lambda unit: (
            float(unit.get("start", 0.0)),
            float(unit.get("end", unit.get("start", 0.0)))))
    for before, after in zip(units, units[1:]):
        gap_start = float(before.get("end", before.get("start", 0.0)))
        gap_end = float(after.get("start", gap_start))
        if (gap_end - gap_start + EPSILON < minimum_pause or
                gap_start < start - EPSILON or
                gap_end > end + EPSILON):
            continue
        boundaries.append(0.5 * (gap_start + gap_end))
    return sorted(set(boundaries))


def snap_override_intervals(entry, selected_evidence, boundaries,
                            tolerance, inward_tolerance):
    start = float(entry["start"])
    end = float(entry["end"])
    snapped = []
    for item in selected_evidence:
        desired_start = max(start, float(item["start_sec"]))
        desired_end = min(end, float(item["end_sec"]))
        before = [
            boundary for boundary in boundaries
            if boundary <= desired_start + EPSILON and
            desired_start - boundary <= tolerance + EPSILON
        ]
        after = [
            boundary for boundary in boundaries
            if boundary >= desired_end - EPSILON and
            boundary - desired_end <= tolerance + EPSILON
        ]
        if not before:
            before = [
                boundary for boundary in boundaries
                if boundary >= desired_start - EPSILON and
                boundary - desired_start <= inward_tolerance + EPSILON
            ]
        if not after:
            after = [
                boundary for boundary in boundaries
                if boundary <= desired_end + EPSILON and
                desired_end - boundary <= inward_tolerance + EPSILON
            ]
        if not before or not after:
            continue
        outward_start = [
            boundary for boundary in before
            if boundary <= desired_start + EPSILON
        ]
        outward_end = [
            boundary for boundary in after
            if boundary >= desired_end - EPSILON
        ]
        interval_start = (
            max(outward_start) if outward_start else min(before))
        interval_end = min(outward_end) if outward_end else max(after)
        if interval_end > interval_start + EPSILON:
            snapped.append([interval_start, interval_end])
    snapped.sort()
    merged = []
    for interval_start, interval_end in snapped:
        if (not merged or
                interval_start > merged[-1][1] + EPSILON):
            merged.append([interval_start, interval_end])
        else:
            merged[-1][1] = max(merged[-1][1], interval_end)
    return merged


def project_decision(entry, direct, runs, policy, align_by_id=None,
                     previous_entry=None, next_entry=None):
    start = float(entry["start"])
    end = float(entry["end"])
    duration = end - start
    baseline_id = entry.get("speaker_id")
    support, support_evidence = rolling_support(start, end, runs)
    audit = {
        "baseline_speaker_id": baseline_id,
        "direct_voiceprint": direct,
        "rolling_support_sec": dict(support),
        "rolling_evidence": dict(support_evidence),
        "boundary_policy": "preserve_captured_business_span",
        "override_intervals": [],
    }
    if not direct.get("candidate"):
        return baseline_id, "baseline_direct_voiceprint_ineligible", audit
    selected = direct["speaker_id"]
    if selected == baseline_id:
        return baseline_id, "baseline_voiceprint_agreement", audit
    projection = policy["projection"]
    if baseline_id is None:
        if projection["unknown_fill_enabled"]:
            audit["override_intervals"] = [[start, end]]
            return selected, "candidate_direct_voiceprint_fill", audit
        return baseline_id, "baseline_unknown_fill_disabled", audit
    if not projection["known_override_enabled"]:
        return baseline_id, "baseline_known_override_disabled", audit
    if (projection["reject_selected_neighbor_sandwich"] and
            selected_neighbor_sandwich(
                selected, previous_entry, entry, next_entry,
                projection["neighbor_gap_sec"])):
        return baseline_id, "baseline_selected_neighbor_sandwich", audit
    selected_support = support[selected]
    competing_support = max(
        (amount for speaker_id, amount in support.items()
         if speaker_id != selected),
        default=0.0)
    audit["selected_rolling_sec"] = selected_support
    audit["selected_rolling_ratio"] = (
        selected_support / duration if duration > 0.0 else 0.0)
    audit["competing_rolling_sec"] = competing_support
    if selected_support + EPSILON < projection[
            "known_override_min_rolling_sec"]:
        return baseline_id, "baseline_insufficient_rolling_duration", audit
    if audit["selected_rolling_ratio"] + EPSILON < projection[
            "known_override_min_rolling_ratio"]:
        return baseline_id, "baseline_insufficient_rolling_ratio", audit
    if competing_support > projection[
            "known_override_max_competing_rolling_sec"] + EPSILON:
        return baseline_id, "baseline_competing_rolling_identity", audit
    if direct.get("strong") or not projection[
            "candidate_override_clip_to_rolling"]:
        audit["override_intervals"] = [[start, end]]
        return selected, "candidate_direct_and_rolling_voiceprint", audit

    boundaries = alignment_boundaries(
        entry, align_by_id or {}, projection["align_pause_min_sec"])
    intervals = snap_override_intervals(
        entry, support_evidence[selected], boundaries,
        projection["boundary_snap_tolerance_sec"],
        projection["boundary_snap_inward_tolerance_sec"])
    audit["alignment_boundaries_sec"] = boundaries
    audit["override_intervals"] = intervals
    audit["boundary_policy"] = "rolling_run_snapped_to_alignment_pause"
    if not intervals:
        return baseline_id, "baseline_no_legal_alignment_boundary", audit
    return selected, "candidate_direct_and_rolling_voiceprint", audit


def aligned_text(entry, start, end, align_by_id):
    group = align_by_id.get(int(entry.get("text_id", -1)))
    if group is None:
        return str(entry.get("text", ""))
    values = []
    for unit in group.get("units", []):
        unit_start = float(unit.get("start", 0.0))
        unit_end = float(unit.get("end", unit_start))
        is_point = abs(unit_end - unit_start) <= EPSILON
        included = (
            start - EPSILON <= unit_start < end - EPSILON
            if is_point else
            interval_overlap(start, end, unit_start, unit_end) > EPSILON)
        if included:
            values.append(str(unit.get("text", "")))
    return "".join(values) or str(entry.get("text", ""))


def split_projected_entry(entry, selected_id, reason, audit, direct,
                          align_by_id):
    start = float(entry["start"])
    end = float(entry["end"])
    baseline_id = entry.get("speaker_id")
    intervals = audit.get("override_intervals", [])
    if selected_id == baseline_id or not intervals:
        intervals = []
    boundaries = {start, end}
    for interval_start, interval_end in intervals:
        boundaries.add(float(interval_start))
        boundaries.add(float(interval_end))
    ordered = sorted(boundaries)
    pieces = []
    for piece_start, piece_end in zip(ordered, ordered[1:]):
        if piece_end <= piece_start + EPSILON:
            continue
        midpoint = 0.5 * (piece_start + piece_end)
        overridden = any(
            interval_start - EPSILON <= midpoint <= interval_end + EPSILON
            for interval_start, interval_end in intervals)
        piece_id = selected_id if overridden else baseline_id
        pieces.append({
            "start": piece_start,
            "end": piece_end,
            "text_id": int(entry.get("text_id", -1)),
            "text": aligned_text(
                entry, piece_start, piece_end, align_by_id)
            if len(ordered) > 2 else str(entry.get("text", "")),
            "speaker": int(entry.get("speaker", -1)),
            "speaker_id": piece_id,
            "speaker_uncertain": (
                False if overridden else
                bool(entry.get("speaker_uncertain", False))),
            "confidence": (
                max(0.0, min(1.0, direct.get("score", 0.0)))
                if overridden else float(entry.get("confidence", 0.0))),
            "decision_reason": (
                reason if overridden else "baseline_outside_rolling_support"),
        })
    return pieces


def project_candidate(timeline_path, config_path, evidence_path,
                      turn_titanet_path):
    package = load_json(timeline_path)
    timeline = timeline_from(package)
    policy = read_policy(load_toml(config_path), timeline)
    active_ids, active_evidence = discover_active_ids(
        timeline, policy["max_speakers"])
    evidence = load_json(evidence_path)
    if evidence.get("kind") != "orator_sliding_voiceprint_evidence":
        raise ValueError("rolling input is not sliding voiceprint evidence")
    if evidence.get("active_speaker_ids") != active_ids:
        raise ValueError("rolling evidence active identities differ")
    turns = track_entries(timeline, "business_speaker")
    align_by_id = {
        int(entry["text_id"]): entry
        for entry in track_entries(timeline, "align")
    }
    direct_rows = read_titanet_by_id(turn_titanet_path)
    expected = {f"business_speaker:{index}" for index in range(len(turns))}
    if set(direct_rows) != expected:
        missing = sorted(expected - set(direct_rows))
        extra = sorted(set(direct_rows) - expected)
        raise ValueError(
            "business TitaNet evidence ID mismatch: "
            f"missing={missing[:3]} extra={extra[:3]}")

    decisions = []
    track = []
    reasons = Counter()
    change_count = 0
    for index, entry in enumerate(turns):
        evidence_id = f"business_speaker:{index}"
        direct = direct_voiceprint(
            direct_rows[evidence_id], active_ids, policy)
        previous_entry = turns[index - 1] if index > 0 else None
        next_entry = turns[index + 1] if index + 1 < len(turns) else None
        speaker_id, reason, audit = project_decision(
            entry, direct, evidence["runs"], policy, align_by_id,
            previous_entry, next_entry)
        baseline_id = entry.get("speaker_id")
        changed = speaker_id != baseline_id
        change_count += changed
        reasons[reason] += 1
        decision = {
            "turn_id": evidence_id,
            "start": float(entry["start"]),
            "end": float(entry["end"]),
            "baseline_speaker_id": baseline_id,
            "speaker_id": speaker_id,
            "changed": changed,
            "reason": reason,
            "audit": audit,
        }
        decisions.append(decision)
        pieces = split_projected_entry(
            entry, speaker_id, reason, audit, direct, align_by_id)
        for piece_index, piece in enumerate(pieces):
            track.append({
                "turn_id": evidence_id,
                "piece_index": piece_index,
                **piece,
            })
    return {
        "schema_version": 1,
        "kind": "orator_frozen_speaker_candidate",
        "candidate_kind": "v21_multiscale_voiceprint_projection",
        "audio_sec": float(timeline["audio_sec"]),
        "sample_rate": int(timeline["sample_rate"]),
        "sources": {
            "timeline": {
                "path": os.path.abspath(timeline_path),
                "sha256": sha256_file(timeline_path),
            },
            "config": {
                "path": os.path.abspath(config_path),
                "sha256": sha256_file(config_path),
            },
            "rolling_voiceprint": {
                "path": os.path.abspath(evidence_path),
                "sha256": sha256_file(evidence_path),
            },
            "turn_titanet": {
                "path": os.path.abspath(turn_titanet_path),
                "sha256": sha256_file(turn_titanet_path),
            },
        },
        "active_speaker_ids": active_ids,
        "active_identity_evidence": active_evidence,
        "policy": policy,
        "source_turn_count": len(turns),
        "track_entry_count": len(track),
        "change_count": change_count,
        "decision_counts": dict(reasons),
        "decisions": decisions,
        "track": track,
    }


def build_evidence(timeline_path, config_path, titanet_path):
    package = load_json(timeline_path)
    timeline = timeline_from(package)
    policy = read_policy(load_toml(config_path), timeline)
    audio_sec = float(timeline["audio_sec"])
    active_ids, active_evidence = discover_active_ids(
        timeline, policy["max_speakers"])
    grouped, input_count = read_titanet(
        titanet_path, policy["round_digits"])
    points, audits, rejected = build_points(grouped, active_ids, policy)
    runs, raw_runs = build_runs(points, policy, audio_sec)
    return {
        "schema_version": 1,
        "kind": "orator_sliding_voiceprint_evidence",
        "sources": {
            "timeline": {
                "path": os.path.abspath(timeline_path),
                "sha256": sha256_file(timeline_path),
            },
            "config": {
                "path": os.path.abspath(config_path),
                "sha256": sha256_file(config_path),
            },
            "titanet": {
                "path": os.path.abspath(titanet_path),
                "sha256": sha256_file(titanet_path),
            },
        },
        "audio_sec": audio_sec,
        "sample_rate": int(timeline["sample_rate"]),
        "active_speaker_ids": active_ids,
        "active_identity_evidence": active_evidence,
        "policy": policy,
        "input_span_count": input_count,
        "selected_point_count": len(points),
        "retained_run_count": len(runs),
        "raw_run_count": len(raw_runs),
        "rejected_point_counts": rejected,
        "points": points,
        "point_audit": audits,
        "runs": runs,
        "raw_runs": raw_runs,
    }


def write_json(path, value):
    parent = os.path.dirname(os.path.abspath(path))
    os.makedirs(parent, exist_ok=True)
    with open(path, "w", encoding="utf-8") as output:
        json.dump(value, output, ensure_ascii=False, indent=2)


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    for command in ("spans", "build", "project"):
        child = subparsers.add_parser(command)
        child.add_argument("--timeline", required=True)
        child.add_argument("--config", required=True)
        child.add_argument("--out", required=True)
        if command == "build":
            child.add_argument("--titanet", required=True)
        if command == "project":
            child.add_argument("--evidence", required=True)
            child.add_argument("--turn-titanet", required=True)
    return parser.parse_args()


def main():
    args = parse_args()
    if args.command == "spans":
        package = load_json(args.timeline)
        timeline = timeline_from(package)
        policy = read_policy(load_toml(args.config), timeline)
        spans = generate_spans(float(timeline["audio_sec"]), policy)
        write_spans(args.out, spans)
        print(json.dumps({
            "spans": len(spans),
            "out": os.path.abspath(args.out),
        }, ensure_ascii=False))
        return
    if args.command == "project":
        result = project_candidate(
            args.timeline, args.config, args.evidence, args.turn_titanet)
        write_json(args.out, result)
        print(json.dumps({
            "source_turns": result["source_turn_count"],
            "track_entries": result["track_entry_count"],
            "changes": result["change_count"],
            "decision_counts": result["decision_counts"],
            "out": os.path.abspath(args.out),
        }, ensure_ascii=False))
        return
    result = build_evidence(args.timeline, args.config, args.titanet)
    write_json(args.out, result)
    print(json.dumps({
        "input_spans": result["input_span_count"],
        "selected_points": result["selected_point_count"],
        "retained_runs": result["retained_run_count"],
        "active_speaker_ids": result["active_speaker_ids"],
        "out": os.path.abspath(args.out),
    }, ensure_ascii=False))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, csv.Error) as error:
        raise SystemExit(f"speaker sliding voiceprint: {error}")
