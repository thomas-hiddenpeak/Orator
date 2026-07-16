#!/usr/bin/env python3
"""Compose reference-free primary/activity/voiceprint aligned islands."""

import argparse
import csv
import hashlib
import json
import os
import tomllib
from collections import defaultdict

import speaker_punctuation_phrase_candidate as phrase_tools
import speaker_reviewed_overlay_candidate as overlay_tools


EPSILON = 1e-6
DECISION_REASON = "primary_aligned_island_three_track_consensus"


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
    primary = document.get("speaker_primary_top1", {})
    fusion = document.get("speaker_fusion", {})
    island = document.get("primary_aligned_island", {})
    primary_required = {
        "enabled", "require_vad_support", "minimum_probability",
        "minimum_run_sec",
    }
    fusion_required = {
        "short_max_sec", "short_min_score", "short_min_margin",
        "regular_min_score", "regular_min_margin",
    }
    island_required = {
        "enabled", "require_vad_bounded_primary_run",
        "require_same_identity_activity_support",
        "require_robust_gallery_agreement",
        "require_complete_alignment_units",
        "require_uniform_known_baseline_conflict",
        "reject_conflicting_overlays", "minimum_activity_support_sec",
        "protected_decision_reasons",
    }
    missing = sorted(
        (primary_required - set(primary)) |
        (fusion_required - set(fusion)) |
        (island_required - set(island)))
    if missing:
        raise ValueError("primary aligned island policy missing: " +
                         ",".join(missing))

    mandatory = [
        primary["enabled"], primary["require_vad_support"],
        island["enabled"], island["require_vad_bounded_primary_run"],
        island["require_same_identity_activity_support"],
        island["require_robust_gallery_agreement"],
        island["require_complete_alignment_units"],
        island["require_uniform_known_baseline_conflict"],
        island["reject_conflicting_overlays"],
    ]
    if any(value is not True for value in mandatory):
        raise ValueError("primary aligned island safeguards are mandatory")

    minimum_run_sec = float(primary["minimum_run_sec"])
    minimum_activity_support_sec = float(
        island["minimum_activity_support_sec"])
    if minimum_run_sec <= 0.0:
        raise ValueError("primary minimum run must be positive")
    if abs(minimum_activity_support_sec - minimum_run_sec) > EPSILON:
        raise ValueError("activity support must inherit primary run floor")
    if abs(float(primary["minimum_probability"]) - 0.5) > EPSILON:
        raise ValueError("primary probability must retain native threshold")

    protected = island["protected_decision_reasons"]
    if not isinstance(protected, list) or not protected:
        raise ValueError("protected decision reasons must be non-empty")
    protected = [str(value) for value in protected]
    if any(not value for value in protected):
        raise ValueError("protected decision reasons must be non-empty")
    if len(set(protected)) != len(protected):
        raise ValueError("protected decision reasons contain duplicates")

    return {
        "minimum_probability": float(primary["minimum_probability"]),
        "minimum_run_sec": minimum_run_sec,
        "minimum_activity_support_sec": minimum_activity_support_sec,
        "protected_decision_reasons": protected,
        **{name: float(fusion[name]) for name in fusion_required},
    }


def read_mapping(document):
    raw = document.get("mapping")
    if not isinstance(raw, dict) or not raw:
        raise ValueError("mapping document has no local-slot mapping")
    mapping = {int(local): str(speaker_id)
               for local, speaker_id in raw.items()}
    if len(mapping) != len(set(mapping.values())):
        raise ValueError("mapping identities are not one-to-one")
    return mapping


def read_diarization(path, primary=False):
    output = []
    with open(path, encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source, delimiter="\t")
        required = {
            "start_sec", "end_sec", "local_speaker", "confidence",
            "speaker_id",
        }
        if not required.issubset(reader.fieldnames or []):
            raise ValueError("diarization TSV has an invalid schema")
        for index, row in enumerate(reader):
            start = float(row["start_sec"])
            end = float(row["end_sec"])
            if end <= start + EPSILON:
                raise ValueError("diarization span has non-positive duration")
            output.append({
                "evidence_id": f"primary_top1:{index}" if primary else None,
                "start": start,
                "end": end,
                "local": int(row["local_speaker"]),
                "confidence": float(row["confidence"]),
                "speaker_id": str(row["speaker_id"]),
            })
    if not output:
        raise ValueError("diarization TSV is empty")
    if primary:
        previous_end = -1.0
        for item in output:
            if item["start"] < previous_end - EPSILON:
                raise ValueError("primary diarization is not non-overlapping")
            previous_end = item["end"]
    return output


def read_voiceprint(path):
    output = {}
    with open(path, encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source, delimiter="\t")
        fields = reader.fieldnames or []
        required = {
            "evidence_id", "start_sec", "end_sec", "duration_sec", "status",
        }
        if not required.issubset(fields):
            raise ValueError("voiceprint TSV has an invalid schema")
        score_fields = [name for name in fields if name.startswith("score_")]
        for row in reader:
            evidence_id = row["evidence_id"]
            if evidence_id in output:
                raise ValueError("duplicate voiceprint evidence ID")
            output[evidence_id] = {
                "start": float(row["start_sec"]),
                "end": float(row["end_sec"]),
                "duration_sec": float(row["duration_sec"]),
                "status": str(row["status"]),
                "scores": {
                    name[6:]: float(row[name]) for name in score_fields
                    if row.get(name) not in (None, "")
                },
            }
    return output


def merged_coverage(intervals):
    if not intervals:
        return 0.0
    intervals = sorted(intervals)
    total = 0.0
    start, end = intervals[0]
    for next_start, next_end in intervals[1:]:
        if next_start <= end + EPSILON:
            end = max(end, next_end)
            continue
        total += end - start
        start, end = next_start, next_end
    return total + end - start


def activity_support(primary, activity):
    intersections = []
    for item in activity:
        if (item["local"] != primary["local"] or
                item["speaker_id"] != primary["speaker_id"]):
            continue
        start = max(primary["start"], item["start"])
        end = min(primary["end"], item["end"])
        if end > start + EPSILON:
            intersections.append((start, end))
    return merged_coverage(intersections)


def baseline_character_labels(fragments, source):
    labels = []
    for fragment in fragments:
        entry = fragment["entry"]
        label = {
            "speaker_id": entry.get("speaker_id"),
            "reason": str(entry.get("decision_reason", "baseline")),
        }
        labels.extend(dict(label) for _ in str(entry.get("text", "")))
    if len(labels) != len(source):
        raise ValueError("candidate source and fragment labels differ")
    return labels


def aligned_ranges(source, units, span_start, span_end):
    times = phrase_tools.aligned_character_times(source, units)
    selected = [
        value is not None and
        float(value["start"]) >= span_start - EPSILON and
        float(value["end"]) <= span_end + EPSILON
        for value in times
    ]
    ranges = []
    cursor = 0
    while cursor < len(selected):
        if not selected[cursor]:
            cursor += 1
            continue
        end = cursor + 1
        while end < len(selected) and selected[end]:
            end += 1
        has_positive_time = any(
            float(times[index]["end"]) > float(times[index]["start"]) + EPSILON
            for index in range(cursor, end))
        if has_positive_time:
            positive = [
                times[index] for index in range(cursor, end)
                if (float(times[index]["end"]) >
                    float(times[index]["start"]) + EPSILON)
            ]
            ranges.append((
                cursor, end,
                min(float(value["start"]) for value in positive),
                max(float(value["end"]) for value in positive),
            ))
        cursor = end
    return ranges


def proposal_conflicts(proposals):
    conflicts = set()
    grouped = defaultdict(list)
    for index, proposal in enumerate(proposals):
        grouped[proposal["text_id"]].append((index, proposal))
    for values in grouped.values():
        values.sort(key=lambda value: (
            value[1]["source_start"], value[1]["source_end"]))
        for left_index, (source_index, source) in enumerate(values):
            for target_index, target in values[left_index + 1:]:
                if target["source_start"] >= source["source_end"]:
                    break
                if target["source_end"] > source["source_start"]:
                    conflicts.update((source_index, target_index))
    return conflicts


def query_spans(proposals):
    grouped = defaultdict(list)
    for proposal in proposals:
        grouped[proposal["evidence_id"]].append(proposal)
    output = []
    for evidence_id, values in grouped.items():
        output.append({
            "evidence_id": "primary_aligned_island_query:" +
                           evidence_id.rsplit(":", 1)[-1],
            "primary_evidence_id": evidence_id,
            "start": min(value["alignment_start"] for value in values),
            "end": max(value["alignment_end"] for value in values),
        })
    return output


def write_query_spans(path, spans):
    with open(path, "w", encoding="ascii", newline="") as output:
        writer = csv.writer(output, delimiter="\t", lineterminator="\n")
        writer.writerow(["evidence_id", "start_sec", "end_sec"])
        for span in spans:
            writer.writerow([span["evidence_id"], span["start"], span["end"]])


def build_candidate(baseline, metadata, primary, activity, voiceprint,
                    mapping_document, policy):
    if baseline.get("kind") != "orator_frozen_speaker_candidate":
        raise ValueError("baseline is not a frozen speaker candidate")
    if metadata.get("kind") != "orator_punctuation_phrase_spans":
        raise ValueError("metadata is not a forced-alignment source")
    mapping = read_mapping(mapping_document)
    id_to_local = {speaker_id: local for local, speaker_id in mapping.items()}
    active_ids = sorted(id_to_local)
    if sorted(baseline.get("active_speaker_ids", [])) != active_ids:
        raise ValueError("baseline identities differ from mapping")

    fragments = overlay_tools.candidate_fragments(baseline)
    overlay_tools.validate_candidate_sources(fragments, metadata)
    labels = {
        text_id: baseline_character_labels(
            values, str(metadata["asr"][str(text_id)]["text"]))
        for text_id, values in fragments.items()
    }
    protected = set(policy["protected_decision_reasons"])
    proposals = []
    decisions = []

    for run in primary:
        run_decision = {
            "evidence_id": run["evidence_id"],
            "start": run["start"],
            "end": run["end"],
            "speaker_id": run["speaker_id"],
            "activity_support_sec": 0.0,
            "voiceprint_speaker_id": None,
            "voiceprint_ranked": [],
            "accepted_ranges": [],
            "reason": "",
        }
        if mapping.get(run["local"]) != run["speaker_id"]:
            raise ValueError("primary local/global identity mismatch")
        support = activity_support(run, activity)
        run_decision["activity_support_sec"] = support
        if support + EPSILON < policy["minimum_activity_support_sec"]:
            run_decision["reason"] = "same_identity_activity_support_below_floor"
            decisions.append(run_decision)
            continue

        evidence = voiceprint.get(run["evidence_id"])
        if evidence is None:
            raise ValueError("missing primary voiceprint evidence")
        if (abs(evidence["start"] - run["start"]) > EPSILON or
                abs(evidence["end"] - run["end"]) > EPSILON):
            raise ValueError("primary voiceprint span differs from primary run")
        selected, voiceprint_reason, ranked = phrase_tools.select_identity(
            evidence, active_ids, policy)
        run_decision["voiceprint_speaker_id"] = selected
        run_decision["voiceprint_ranked"] = [
            {"speaker_id": speaker_id, "score": score}
            for speaker_id, score in ranked
        ]
        if selected is None:
            run_decision["reason"] = "robust_gallery_abstained:" + voiceprint_reason
            decisions.append(run_decision)
            continue
        if selected != run["speaker_id"]:
            run_decision["reason"] = "robust_gallery_identity_disagreement"
            decisions.append(run_decision)
            continue

        run_proposals = []
        for text_id in sorted(fragments):
            source = str(metadata["asr"][str(text_id)]["text"])
            units = metadata["align"][str(text_id)]
            for source_start, source_end, alignment_start, alignment_end in aligned_ranges(
                    source, units, run["start"], run["end"]):
                range_labels = labels[text_id][source_start:source_end]
                baseline_ids = {value["speaker_id"] for value in range_labels}
                if len(baseline_ids) != 1 or None in baseline_ids:
                    continue
                baseline_id = next(iter(baseline_ids))
                if baseline_id == run["speaker_id"]:
                    continue
                if any(value["reason"] in protected for value in range_labels):
                    continue
                proposal = {
                    "text_id": text_id,
                    "source_start": source_start,
                    "source_end": source_end,
                    "speaker_id": run["speaker_id"],
                    "baseline_speaker_id": baseline_id,
                    "reason": DECISION_REASON,
                    "evidence_id": run["evidence_id"],
                    "alignment_start": alignment_start,
                    "alignment_end": alignment_end,
                }
                proposals.append(proposal)
                run_proposals.append(proposal)
        if run_proposals:
            run_decision["reason"] = DECISION_REASON
            run_decision["accepted_ranges"] = [
                {key: value for key, value in proposal.items()
                 if key not in ("speaker_id", "reason")}
                for proposal in run_proposals
            ]
        else:
            run_decision["reason"] = "no_uniform_conflicting_aligned_range"
        decisions.append(run_decision)

    conflicts = proposal_conflicts(proposals)
    accepted = [proposal for index, proposal in enumerate(proposals)
                if index not in conflicts]
    accepted_keys = {
        (item["evidence_id"], item["text_id"], item["source_start"],
         item["source_end"])
        for item in accepted
    }
    for decision in decisions:
        decision["accepted_ranges"] = [
            item for item in decision["accepted_ranges"]
            if (decision["evidence_id"], item["text_id"],
                item["source_start"], item["source_end"]) in accepted_keys
        ]
        if decision["reason"] == DECISION_REASON and not decision["accepted_ranges"]:
            decision["reason"] = "conflicting_aligned_proposal"

    overlays_by_text = defaultdict(list)
    for item in accepted:
        overlays_by_text[item["text_id"]].append(item)
    overlay_tools.validate_overlays(overlays_by_text)

    track = []
    for text_id in sorted(fragments):
        source = str(metadata["asr"][str(text_id)]["text"])
        track.extend(overlay_tools.reproject_preserving_base(
            text_id, source, metadata["align"][str(text_id)],
            fragments[text_id], overlays_by_text[text_id], id_to_local))

    return {
        "schema_version": 1,
        "kind": "orator_frozen_speaker_candidate",
        "candidate_kind": "v21_primary_aligned_island",
        "audio_sec": float(baseline["audio_sec"]),
        "sample_rate": int(baseline["sample_rate"]),
        "active_speaker_ids": active_ids,
        "source_turn_count": len(baseline.get("track", [])),
        "turn_count": len(track),
        "island_count": len(accepted),
        "query_spans": query_spans(accepted),
        "policy": policy,
        "decisions": decisions,
        "islands": accepted,
        "track": track,
    }


def command_build(args):
    baseline = load_json(args.baseline)
    metadata = load_json(args.alignment)
    primary = read_diarization(args.primary, primary=True)
    activity = read_diarization(args.activity)
    voiceprint = read_voiceprint(args.robust_titanet)
    mapping = load_json(args.mapping)
    policy = load_policy(args.config)
    result = build_candidate(
        baseline, metadata, primary, activity, voiceprint, mapping, policy)
    if args.island_spans:
        write_query_spans(args.island_spans, result["query_spans"])
    result["sources"] = {
        name: {"path": os.path.abspath(path), "sha256": sha256_file(path)}
        for name, path in {
            "baseline": args.baseline,
            "alignment": args.alignment,
            "primary": args.primary,
            "activity": args.activity,
            "robust_titanet": args.robust_titanet,
            "mapping": args.mapping,
            "config": args.config,
        }.items()
    }
    if args.island_spans:
        result["sources"]["island_spans"] = {
            "path": os.path.abspath(args.island_spans),
            "sha256": sha256_file(args.island_spans),
        }
    with open(args.out, "w", encoding="utf-8") as output:
        json.dump(result, output, ensure_ascii=False, indent=2)
    print(json.dumps({
        "turns": result["turn_count"],
        "islands": result["island_count"],
        "out": os.path.abspath(args.out),
    }, ensure_ascii=False))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", required=True)
    parser.add_argument("--alignment", required=True)
    parser.add_argument("--primary", required=True)
    parser.add_argument("--activity", required=True)
    parser.add_argument("--robust-titanet", required=True)
    parser.add_argument("--mapping", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--island-spans")
    parser.add_argument("--out", required=True)
    command_build(parser.parse_args())


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, csv.Error,
            json.JSONDecodeError) as error:
        raise SystemExit(f"speaker primary aligned island candidate: {error}")
