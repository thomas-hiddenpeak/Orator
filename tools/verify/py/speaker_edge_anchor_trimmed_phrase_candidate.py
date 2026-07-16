#!/usr/bin/env python3
"""Compose edge-anchor-trimmed phrase challenges without result judgment."""

import argparse
import hashlib
import json
import os
import tomllib
from collections import defaultdict

import speaker_posterior_bounded_phrase_candidate as posterior
import speaker_punctuation_phrase_candidate as phrase_tools
import speaker_reviewed_overlay_candidate as overlay_tools


EPSILON = 1e-9
DECISION_REASON = "edge_anchor_trimmed_dual_voiceprint_challenge"


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
    section = document.get("edge_anchor_trimmed_phrase", {})
    fusion_names = (
        "short_max_sec", "short_min_score", "short_min_margin",
        "regular_min_score", "regular_min_margin",
    )
    safeguards = {
        "enabled", "require_dual_phrase_voiceprint_consensus",
        "require_one_competing_anchor_edge",
        "require_contiguous_anchor_overlap",
        "require_one_frame_alignment_separation",
        "require_known_conflicting_remainder",
        "reject_conflicting_overlays",
    }
    required = safeguards | {
        "boundary_separation_frames", "protected_decision_reasons",
    }
    missing = sorted((set(fusion_names) - set(fusion)) |
                     (required - set(section)))
    if missing:
        raise ValueError("edge-anchor policy missing: " + ",".join(missing))
    if any(section[name] is not True for name in safeguards):
        raise ValueError("edge-anchor safeguards are mandatory")
    if int(section["boundary_separation_frames"]) != 1:
        raise ValueError("edge-anchor separation must be one native frame")
    protected = [str(value)
                 for value in section["protected_decision_reasons"]]
    if not protected or len(set(protected)) != len(protected):
        raise ValueError("protected decision reasons are invalid")
    return {
        **{name: float(fusion[name]) for name in fusion_names},
        "boundary_separation_frames": 1,
        "protected_decision_reasons": protected,
    }


def is_direct_anchor(entry):
    reason = str(entry.get("decision_reason", ""))
    return (entry.get("speaker_id") is not None and
            "direct_voiceprint" in reason and "below_gate" not in reason)


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


def merge_ranges(ranges):
    merged = []
    for start, end in sorted(ranges):
        if not merged or start > merged[-1][1]:
            merged.append([start, end])
        else:
            merged[-1][1] = max(merged[-1][1], end)
    return [(start, end) for start, end in merged]


def aligned_bounds(times, start, end):
    values = [times[index] for index in range(start, end)
              if times[index] is not None and
              float(times[index]["end"]) > float(times[index]["start"])]
    if not values:
        return None
    return (min(float(value["start"]) for value in values),
            max(float(value["end"]) for value in values))


def edge_remainder(phrase, fragments, target_id, times, separation):
    phrase_start = int(phrase["source_start"])
    phrase_end = int(phrase["source_end"])
    anchors = []
    anchor_ids = set()
    for fragment in fragments:
        entry = fragment["entry"]
        if not is_direct_anchor(entry):
            continue
        start = max(phrase_start, int(fragment["source_start"]))
        end = min(phrase_end, int(fragment["source_end"]))
        if end <= start:
            continue
        anchors.append((start, end))
        anchor_ids.add(str(entry["speaker_id"]))
    audit = {"anchor_ranges": anchors,
             "anchor_speaker_ids": sorted(anchor_ids)}
    if not anchors:
        return None, "competing_anchor_missing", audit
    if target_id in anchor_ids or len(anchor_ids) != 1:
        return None, "competing_anchor_identity_invalid", audit
    merged = merge_ranges(anchors)
    if len(merged) != 1:
        return None, "anchor_overlap_not_contiguous", audit
    anchor_start, anchor_end = merged[0]
    if anchor_start == phrase_start and anchor_end < phrase_end:
        remainder = (anchor_end, phrase_end)
        anchor_bounds = aligned_bounds(times, anchor_start, anchor_end)
        remainder_bounds = aligned_bounds(times, anchor_end, phrase_end)
        edge = "prefix"
        separated = (anchor_bounds is not None and
                     remainder_bounds is not None and
                     remainder_bounds[0] - anchor_bounds[1] + EPSILON >=
                     separation)
    elif anchor_end == phrase_end and anchor_start > phrase_start:
        remainder = (phrase_start, anchor_start)
        anchor_bounds = aligned_bounds(times, anchor_start, anchor_end)
        remainder_bounds = aligned_bounds(times, phrase_start, anchor_start)
        edge = "suffix"
        separated = (anchor_bounds is not None and
                     remainder_bounds is not None and
                     anchor_bounds[0] - remainder_bounds[1] + EPSILON >=
                     separation)
    else:
        return None, "anchor_not_one_phrase_edge", audit
    audit.update({"edge": edge, "anchor_aligned_bounds": anchor_bounds,
                  "remainder_aligned_bounds": remainder_bounds,
                  "remainder": remainder})
    if not separated:
        return None, "one_frame_separation_missing", audit
    return remainder, "eligible_edge_remainder", audit


def ranked_audit(ranked):
    return [{"speaker_id": speaker_id, "score": score}
            for speaker_id, score in ranked]


def build_candidate(baseline, metadata, session_evidence, robust_evidence,
                    mapping_document, frame_period, policy):
    if metadata.get("kind") != "orator_punctuation_phrase_spans":
        raise ValueError("metadata is not punctuation phrase evidence")
    if frame_period <= 0.0:
        raise ValueError("native frame period must be positive")
    mapping = overlay_tools.read_mapping(mapping_document)
    id_to_local = {speaker_id: local for local, speaker_id in mapping.items()}
    active_ids = sorted(id_to_local)
    if sorted(baseline.get("active_speaker_ids", [])) != active_ids:
        raise ValueError("baseline identities differ from mapping")
    fragments = overlay_tools.candidate_fragments(baseline)
    overlay_tools.validate_candidate_sources(fragments, metadata)
    labels = {}
    times = {}
    for text_id, values in fragments.items():
        source = str(metadata["asr"][str(text_id)]["text"])
        labels[text_id] = labels_for_source(values, source)
        times[text_id] = phrase_tools.aligned_character_times(
            source, metadata["align"][str(text_id)])

    protected = set(policy["protected_decision_reasons"])
    separation = policy["boundary_separation_frames"] * frame_period
    overlays = defaultdict(list)
    decisions = []
    for phrase in metadata.get("phrases", []):
        evidence_id = str(phrase["evidence_id"])
        if evidence_id not in session_evidence or evidence_id not in robust_evidence:
            raise ValueError("missing complete-phrase voiceprint evidence")
        session_id, session_reason, session_ranked = phrase_tools.select_identity(
            session_evidence[evidence_id], active_ids, policy)
        robust_id, robust_reason, robust_ranked = phrase_tools.select_identity(
            robust_evidence[evidence_id], active_ids, policy)
        decision = {
            "evidence_id": evidence_id, "text_id": int(phrase["text_id"]),
            "phrase_source_start": int(phrase["source_start"]),
            "phrase_source_end": int(phrase["source_end"]),
            "phrase_start": float(phrase["start"]),
            "phrase_end": float(phrase["end"]),
            "session_identity": session_id, "session_reason": session_reason,
            "session_ranked": ranked_audit(session_ranked),
            "robust_identity": robust_id, "robust_reason": robust_reason,
            "robust_ranked": ranked_audit(robust_ranked),
            "selected_speaker_id": None, "accepted": False,
            "reason": "dual_phrase_voiceprint_abstention",
        }
        if session_id is None or robust_id is None:
            decisions.append(decision)
            continue
        if session_id != robust_id:
            decision["reason"] = "dual_phrase_voiceprint_disagreement"
            decisions.append(decision)
            continue
        text_id = int(phrase["text_id"])
        remainder, reason, anchor_audit = edge_remainder(
            phrase, fragments[text_id], session_id, times[text_id], separation)
        decision["anchor"] = anchor_audit
        if remainder is None:
            decision["reason"] = reason
            decisions.append(decision)
            continue
        start, end = remainder
        range_labels = labels[text_id][start:end]
        known = {value["speaker_id"] for value in range_labels
                 if value["speaker_id"] is not None}
        if not known or session_id in known:
            decision["reason"] = "known_conflicting_remainder_missing"
            decisions.append(decision)
            continue
        if any(value["reason"] in protected for value in range_labels):
            decision["reason"] = "protected_overlay_conflict"
            decisions.append(decision)
            continue
        decision.update({
            "source_start": start, "source_end": end,
            "selected_speaker_id": session_id, "accepted": True,
            "reason": DECISION_REASON,
        })
        overlays[text_id].append({
            "text_id": text_id, "source_start": start, "source_end": end,
            "speaker_id": session_id, "reason": DECISION_REASON,
        })
        decisions.append(decision)

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
        "candidate_kind": "v21_edge_anchor_trimmed_phrase_challenge",
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
    parser.add_argument("--session-evidence", required=True)
    parser.add_argument("--robust-evidence", required=True)
    parser.add_argument("--mapping", required=True)
    parser.add_argument("--frames", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()
    _, frame_period = posterior.read_frames(args.frames)
    result = build_candidate(
        load_json(args.baseline), load_json(args.metadata),
        phrase_tools.read_titanet(args.session_evidence),
        phrase_tools.read_titanet(args.robust_evidence),
        load_json(args.mapping), frame_period, load_policy(args.config))
    result["sources"] = {
        name: {"path": os.path.abspath(path), "sha256": sha256_file(path)}
        for name, path in {
            "baseline": args.baseline, "metadata": args.metadata,
            "session_evidence": args.session_evidence,
            "robust_evidence": args.robust_evidence,
            "mapping": args.mapping, "frames": args.frames,
            "config": args.config,
        }.items()
    }
    with open(args.out, "w", encoding="utf-8") as output:
        json.dump(result, output, ensure_ascii=False, indent=2)
    print(json.dumps({
        "accepted_challenges": result["accepted_challenge_count"],
        "turns": result["turn_count"], "out": os.path.abspath(args.out),
    }, ensure_ascii=False))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError) as error:
        raise SystemExit(f"speaker edge-anchor candidate: {error}")
