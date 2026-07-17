#!/usr/bin/env python3
"""Compose single-unit secondary-channel edge closures without judgment."""

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
import speaker_secondary_channel_phrase_candidate as secondary


EPSILON = 1e-9
DECISION_REASON = "secondary_channel_single_unit_edge_closure"


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
    phrase = phrase_tools.load_policy(path)
    bounded = posterior.load_policy(path)
    with open(path, "rb") as source:
        document = tomllib.load(source)
    section = document.get("secondary_channel_edge_closure", {})
    safeguards = {
        "enabled", "require_one_containing_vad",
        "require_two_contiguous_known_identity_runs",
        "require_selected_direct_anchor_prefix",
        "require_single_positive_duration_suffix_unit",
        "require_dual_phrase_margin_rank_identity",
        "require_selected_channel_sustained_activity",
        "require_suffix_competing_top1_with_selected_channel_active",
        "project_complete_phrase", "reject_conflicting_overlays",
        "reject_overlapping_proposals",
    }
    required = safeguards | {
        "frame_activity_threshold", "minimum_selected_channel_sec",
        "positive_suffix_unit_count", "allowed_anchor_reasons",
        "allowed_competing_reasons", "protected_decision_reasons",
    }
    missing = sorted(required - set(section))
    if missing:
        raise ValueError("secondary edge policy missing: " +
                         ",".join(missing))
    if any(section[name] is not True for name in safeguards):
        raise ValueError("secondary edge safeguards are mandatory")
    threshold = float(section["frame_activity_threshold"])
    minimum = float(section["minimum_selected_channel_sec"])
    if abs(threshold - bounded["frame_activity_threshold"]) > EPSILON:
        raise ValueError("secondary edge activity threshold differs")
    if abs(minimum - bounded["minimum_sustained_run_sec"]) > EPSILON:
        raise ValueError("secondary edge sustained duration differs")
    if int(section["positive_suffix_unit_count"]) != 1:
        raise ValueError("secondary edge requires one timed suffix unit")
    lists = {
        name: [str(value) for value in section[name]]
        for name in ("allowed_anchor_reasons", "allowed_competing_reasons",
                     "protected_decision_reasons")
    }
    if any(not values or len(set(values)) != len(values)
           for values in lists.values()):
        raise ValueError("secondary edge provenance lists are invalid")
    if ((set(lists["allowed_anchor_reasons"]) |
         set(lists["allowed_competing_reasons"])) &
            set(lists["protected_decision_reasons"])):
        raise ValueError("allowed and protected provenance overlap")
    return {
        "frame_activity_threshold": threshold,
        "minimum_selected_channel_sec": minimum,
        "positive_suffix_unit_count": 1,
        **lists,
        "voiceprint": {name: phrase[name] for name in (
            "short_max_sec", "short_min_score", "short_min_margin",
            "regular_min_score", "regular_min_margin")},
    }


def read_vad(path):
    output = []
    with open(path, encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source, delimiter="\t")
        required = {"evidence_id", "start_sec", "end_sec"}
        if not required.issubset(reader.fieldnames or []):
            raise ValueError("VAD evidence columns are incomplete")
        for row in reader:
            output.append({
                "evidence_id": str(row["evidence_id"]),
                "start": float(row["start_sec"]),
                "end": float(row["end_sec"]),
            })
    if not output:
        raise ValueError("VAD evidence is empty")
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


def identity_runs(values, source_start):
    runs = []
    for offset, value in enumerate(values):
        identity = value["speaker_id"]
        index = source_start + offset
        if not runs or runs[-1]["speaker_id"] != identity:
            runs.append({
                "speaker_id": identity, "source_start": index,
                "source_end": index + 1, "reasons": {value["reason"]},
            })
        else:
            runs[-1]["source_end"] = index + 1
            runs[-1]["reasons"].add(value["reason"])
    return runs


def aligned_units(source, units):
    output = []
    cursor = 0
    for unit in units:
        text = str(unit["text"])
        start = source.find(text, cursor)
        if start < 0:
            raise ValueError("alignment unit is not a subsequence of ASR text")
        end = start + len(text)
        output.append({
            "source_start": start, "source_end": end,
            "start": float(unit["start"]), "end": float(unit["end"]),
            "text": text,
        })
        cursor = end
    return output


def suffix_positive_units(units, source_start, source_end):
    output = []
    for unit in units:
        if (unit["source_end"] <= source_start or
                unit["source_start"] >= source_end):
            continue
        if (unit["source_start"] < source_start or
                unit["source_end"] > source_end):
            raise ValueError("alignment unit crosses identity boundary")
        if unit["end"] > unit["start"] + EPSILON:
            output.append(unit)
    return output


def margin_view(evidence, active_ids, policy):
    identity, reason, ranked = secondary.select_margin_rank(
        evidence, active_ids, policy["voiceprint"])
    return {
        "identity": identity, "reason": reason,
        "ranked": [{"speaker_id": speaker_id, "score": score}
                   for speaker_id, score in ranked],
    }


def overlaps(left, right):
    return (left["text_id"] == right["text_id"] and
            left["source_start"] < right["source_end"] and
            right["source_start"] < left["source_end"])


def build_candidate(baseline, metadata, vad, frames, frame_period,
                    session_evidence, robust_evidence, mapping_document,
                    policy):
    if metadata.get("kind") != "orator_punctuation_phrase_spans":
        raise ValueError("metadata is not punctuation phrase evidence")
    mapping = overlay_tools.read_mapping(mapping_document)
    id_to_local = {identity: local for local, identity in mapping.items()}
    active_ids = sorted(id_to_local)
    if sorted(baseline.get("active_speaker_ids", [])) != active_ids:
        raise ValueError("baseline identities differ from mapping")
    fragments = overlay_tools.candidate_fragments(baseline)
    overlay_tools.validate_candidate_sources(fragments, metadata)
    labels = {
        text_id: labels_for_source(
            values, str(metadata["asr"][str(text_id)]["text"]))
        for text_id, values in fragments.items()
    }
    units = {
        int(text_id): aligned_units(
            str(metadata["asr"][text_id]["text"]), values)
        for text_id, values in metadata["align"].items()
    }
    allowed_anchor = set(policy["allowed_anchor_reasons"])
    allowed_competing = set(policy["allowed_competing_reasons"])
    protected = set(policy["protected_decision_reasons"])
    proposals = defaultdict(list)
    decisions = []
    for phrase in metadata.get("phrases", []):
        phrase_start = float(phrase["start"])
        phrase_end = float(phrase["end"])
        containing = [item for item in vad if
                      phrase_start >= item["start"] - EPSILON and
                      phrase_end <= item["end"] + EPSILON]
        if len(containing) != 1:
            continue
        text_id = int(phrase["text_id"])
        source_start = int(phrase["source_start"])
        source_end = int(phrase["source_end"])
        range_labels = labels[text_id][source_start:source_end]
        runs = identity_runs(range_labels, source_start)
        if (len(runs) != 2 or any(run["speaker_id"] is None for run in runs) or
                runs[0]["speaker_id"] == runs[1]["speaker_id"]):
            continue
        anchor, competing = runs
        phrase_id = str(phrase["evidence_id"])
        if (phrase_id not in session_evidence or
                phrase_id not in robust_evidence):
            raise ValueError("missing secondary edge phrase evidence")
        views = {
            "session": margin_view(
                session_evidence[phrase_id], active_ids, policy),
            "robust": margin_view(
                robust_evidence[phrase_id], active_ids, policy),
        }
        decision = {
            "vad_evidence_id": containing[0]["evidence_id"],
            "phrase_evidence_id": phrase_id,
            "text_id": text_id, "source_start": source_start,
            "source_end": source_end, "start": phrase_start,
            "end": phrase_end,
            "anchor_speaker_id": anchor["speaker_id"],
            "competing_speaker_id": competing["speaker_id"],
            "anchor_source_start": anchor["source_start"],
            "anchor_source_end": anchor["source_end"],
            "competing_source_start": competing["source_start"],
            "competing_source_end": competing["source_end"],
            "anchor_reasons": sorted(anchor["reasons"]),
            "competing_reasons": sorted(competing["reasons"]),
            "views": views, "selected_speaker_id": None,
            "accepted": False, "reason": "anchor_provenance_not_allowed",
        }
        if not anchor["reasons"].issubset(allowed_anchor):
            decisions.append(decision)
            continue
        if not competing["reasons"].issubset(allowed_competing):
            decision["reason"] = "competing_provenance_not_allowed"
            decisions.append(decision)
            continue
        if any(value["reason"] in protected for value in range_labels):
            decision["reason"] = "protected_overlay_conflict"
            decisions.append(decision)
            continue
        selected = views["session"]["identity"]
        if selected is None or views["robust"]["identity"] is None:
            decision["reason"] = "dual_phrase_margin_rank_abstention"
            decisions.append(decision)
            continue
        if selected != views["robust"]["identity"]:
            decision["reason"] = "dual_phrase_margin_rank_disagreement"
            decisions.append(decision)
            continue
        if selected != anchor["speaker_id"]:
            decision["reason"] = "selected_identity_not_anchor"
            decisions.append(decision)
            continue
        timed = suffix_positive_units(
            units[text_id], competing["source_start"],
            competing["source_end"])
        decision["positive_suffix_units"] = timed
        if len(timed) != policy["positive_suffix_unit_count"]:
            decision["reason"] = "single_positive_suffix_unit_missing"
            decisions.append(decision)
            continue
        selected_local = id_to_local.get(selected)
        competing_local = id_to_local.get(competing["speaker_id"])
        if selected_local is None or competing_local is None:
            decision["reason"] = "raw_channel_mapping_missing"
            decisions.append(decision)
            continue
        phrase_frames = secondary.phrase_frames(
            frames, frame_period, phrase_start, phrase_end)
        support, non_top1 = secondary.channel_support(
            phrase_frames, frame_period, phrase_start, phrase_end,
            policy["frame_activity_threshold"])
        decision["selected_channel_sustained_sec"] = support.get(
            str(selected_local), 0.0)
        decision["selected_channel_non_top1_frame_count"] = non_top1.get(
            str(selected_local), 0)
        if (decision["selected_channel_sustained_sec"] + EPSILON <
                policy["minimum_selected_channel_sec"]):
            decision["reason"] = "selected_channel_sustained_activity_missing"
            decisions.append(decision)
            continue
        unit = timed[0]
        tail_frames = secondary.phrase_frames(
            frames, frame_period, unit["start"], unit["end"])
        decision["suffix_frame_count"] = len(tail_frames)
        if (not tail_frames or any(
                frame["top1"] != competing_local or
                frame["probabilities"].get(selected_local, 0.0) + EPSILON <
                policy["frame_activity_threshold"]
                for frame in tail_frames)):
            decision["reason"] = "suffix_overlap_channel_contract_missing"
            decisions.append(decision)
            continue
        decision["selected_speaker_id"] = selected
        decision["accepted"] = True
        decision["reason"] = DECISION_REASON
        proposals[text_id].append(decision)
        decisions.append(decision)

    for values in proposals.values():
        conflicted = set()
        ordered = sorted(values, key=lambda item: (
            item["source_start"], item["source_end"],
            item["phrase_evidence_id"]))
        for index, left in enumerate(ordered):
            for right in ordered[index + 1:]:
                if right["source_start"] >= left["source_end"]:
                    break
                if overlaps(left, right):
                    conflicted.add(left["phrase_evidence_id"])
                    conflicted.add(right["phrase_evidence_id"])
        for item in values:
            if item["phrase_evidence_id"] in conflicted:
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
        "schema_version": 1,
        "kind": "orator_frozen_speaker_candidate",
        "candidate_kind": "v21_secondary_channel_single_unit_edge_closure",
        "audio_sec": float(baseline["audio_sec"]),
        "sample_rate": int(baseline["sample_rate"]),
        "active_speaker_ids": active_ids,
        "source_turn_count": len(baseline.get("track", [])),
        "turn_count": len(track),
        "accepted_closure_count": sum(item["accepted"]
                                      for item in decisions),
        "policy": policy, "closure_decisions": decisions, "track": track,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", required=True)
    parser.add_argument("--metadata", required=True)
    parser.add_argument("--vad", required=True)
    parser.add_argument("--frames", required=True)
    parser.add_argument("--session-evidence", required=True)
    parser.add_argument("--robust-evidence", required=True)
    parser.add_argument("--mapping", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()
    frames, frame_period = secondary.read_frames(args.frames)
    result = build_candidate(
        load_json(args.baseline), load_json(args.metadata), read_vad(args.vad),
        frames, frame_period,
        phrase_tools.read_titanet(args.session_evidence),
        phrase_tools.read_titanet(args.robust_evidence),
        load_json(args.mapping), load_policy(args.config))
    sources = {
        "baseline": args.baseline, "metadata": args.metadata,
        "vad": args.vad, "frames": args.frames,
        "session_evidence": args.session_evidence,
        "robust_evidence": args.robust_evidence,
        "mapping": args.mapping, "config": args.config,
    }
    result["sources"] = {
        name: {"path": os.path.abspath(path), "sha256": sha256_file(path)}
        for name, path in sources.items()
    }
    with open(args.out, "w", encoding="utf-8") as output:
        json.dump(result, output, ensure_ascii=False, indent=2)
    print(json.dumps({
        "closures": result["accepted_closure_count"],
        "turns": result["turn_count"], "out": os.path.abspath(args.out),
    }, ensure_ascii=False))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, csv.Error,
            json.JSONDecodeError) as error:
        raise SystemExit(f"speaker secondary edge closure: {error}")
