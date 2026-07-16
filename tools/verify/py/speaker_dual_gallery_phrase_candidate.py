#!/usr/bin/env python3
"""Build exact-phrase overlays from two independently frozen TitaNet galleries."""

import argparse
import bisect
from collections import defaultdict
import json
import os
import tomllib

import speaker_posterior_bounded_phrase_candidate as posterior
import speaker_prototype_local_veto_candidate as local_tools
import speaker_punctuation_phrase_candidate as phrase_tools


REQUIRED_POLICY = {
    "enabled",
    "require_baseline_disagreement",
    "veto_competing_direct_anchor",
    "require_target_channel_top1",
    "reject_competing_sustained_top1",
    "require_known_baseline_conflict",
    "reject_short_only_direct_support",
}
VOICEPRINT_KEYS = {
    "short_max_sec",
    "short_min_score",
    "short_min_margin",
    "regular_min_score",
    "regular_min_margin",
    "frame_activity_threshold",
    "minimum_sustained_run_sec",
}


def load_policy(path):
    with open(path, "rb") as source:
        document = tomllib.load(source)
    section = document.get("dual_gallery_phrase", {})
    missing = sorted(REQUIRED_POLICY - set(section))
    if missing:
        raise ValueError("dual-gallery phrase policy missing: " + ",".join(missing))
    policy = {name: bool(section[name]) for name in REQUIRED_POLICY}
    if not all(policy.values()):
        raise ValueError("all dual-gallery phrase safety contracts are mandatory")
    return policy


def load_voiceprint_policy(path):
    policy = phrase_tools.load_policy(path)
    with open(path, "rb") as source:
        document = tomllib.load(source)
    posterior_policy = document.get("posterior_bounded_phrase", {})
    required = {"frame_activity_threshold", "minimum_sustained_run_sec"}
    missing = sorted(required - set(posterior_policy))
    if missing:
        raise ValueError(
            "dual-gallery native policy missing: " + ",".join(missing))
    policy.update({name: float(posterior_policy[name]) for name in required})
    return policy


def validate_voiceprint_policy(baseline, configured):
    inherited = baseline.get("voiceprint_policy", {})
    for name in VOICEPRINT_KEYS:
        if name not in inherited:
            raise ValueError(f"baseline voiceprint policy is missing {name}")
        if abs(float(inherited[name]) - float(configured[name])) > 1e-9:
            raise ValueError(f"voiceprint policy differs for {name}")


def source_fragments(track):
    grouped = defaultdict(list)
    cursors = defaultdict(int)
    for item in track:
        text_id = int(item["text_id"])
        text = str(item.get("text", ""))
        start = cursors[text_id]
        grouped[text_id].append({
            "source_start": start,
            "source_end": start + len(text),
            "entry": item,
        })
        cursors[text_id] += len(text)
    return dict(grouped)


def validate_source_text(fragments, metadata, label):
    expected_ids = {int(value) for value in metadata["asr"]}
    if set(fragments) != expected_ids:
        raise ValueError(f"{label} text ID set differs from metadata")
    for text_id, values in fragments.items():
        actual = "".join(str(value["entry"].get("text", ""))
                         for value in values)
        expected = str(metadata["asr"][str(text_id)]["text"])
        if actual != expected:
            raise ValueError(f"{label} source text mismatch for {text_id}")


def overlaps(fragment, source_start, source_end):
    return (fragment["source_start"] < source_end and
            fragment["source_end"] > source_start)


def is_direct_anchor(entry):
    reason = str(entry.get("decision_reason", ""))
    return (entry.get("speaker_id") is not None and
            "direct_voiceprint" in reason and
            "below_gate" not in reason)


def anchor_records(fragments, source_start, source_end):
    return [
        {
            "speaker_id": str(fragment["entry"]["speaker_id"]),
            "reason": str(fragment["entry"].get("decision_reason", "")),
        }
        for fragment in fragments
        if overlaps(fragment, source_start, source_end) and
        is_direct_anchor(fragment["entry"])
    ]


def known_baseline_conflicts(fragments, source_start, source_end, speaker_id):
    return sorted({
        str(fragment["entry"]["speaker_id"])
        for fragment in fragments
        if overlaps(fragment, source_start, source_end) and
        fragment["entry"].get("speaker_id") is not None and
        fragment["entry"].get("speaker_id") != speaker_id
    })


def range_disagrees(fragments, source_start, source_end, speaker_id):
    return any(
        overlaps(fragment, source_start, source_end) and
        fragment["entry"].get("speaker_id") != speaker_id
        for fragment in fragments
    )


def native_channel_support(start, end, speaker_id, id_to_local, frames,
                           frame_sec, voiceprint_policy):
    local = id_to_local.get(speaker_id)
    if local is None:
        raise ValueError(f"identity {speaker_id} has no native channel")
    times = [frame["time"] for frame in frames]
    begin = bisect.bisect_left(times, float(start) - 1e-9)
    finish = bisect.bisect_left(times, float(end) - 1e-9)
    selected = frames[begin:finish]
    if not selected:
        raise ValueError("phrase has no native frames")
    channel_count = len(selected[0]["channels"])
    if local >= channel_count:
        raise ValueError(f"frame table is missing native channel {local}")

    target_top1 = any(frame["top1"] == local for frame in selected)
    threshold = float(voiceprint_policy["frame_activity_threshold"])
    current = [0] * channel_count
    longest = [0] * channel_count
    for frame in selected:
        if len(frame["channels"]) != channel_count:
            raise ValueError("native frame channel count changed")
        for channel in range(channel_count):
            active_top1 = (
                channel != local and frame["top1"] == channel and
                frame["channels"][channel] >= threshold)
            current[channel] = current[channel] + 1 if active_top1 else 0
            longest[channel] = max(longest[channel], current[channel])
    competing_channel = max(
        (channel for channel in range(channel_count) if channel != local),
        key=lambda channel: (longest[channel], -channel),
        default=-1)
    competing_run_sec = (
        longest[competing_channel] * frame_sec
        if competing_channel >= 0 else 0.0)
    return {
        "target_local_speaker": local,
        "target_top1_present": target_top1,
        "competing_local_speaker": competing_channel,
        "competing_longest_active_top1_run_sec": competing_run_sec,
        "competing_sustained": (
            competing_run_sec + 1e-9 >=
            float(voiceprint_policy["minimum_sustained_run_sec"])),
    }


def decide_phrase(phrase, gallery_evidence, fragments, active_ids,
                  id_to_local, frames, frame_sec, voiceprint_policy, policy):
    evidence_id = str(phrase["evidence_id"])
    if evidence_id not in gallery_evidence:
        raise ValueError(f"missing clean-gallery phrase evidence {evidence_id}")
    gallery_id, gallery_reason, gallery_ranked = phrase_tools.select_identity(
        gallery_evidence[evidence_id], active_ids, voiceprint_policy)
    current_id = phrase.get("speaker_id")
    source_start = int(phrase["source_start"])
    source_end = int(phrase["source_end"])
    anchors = anchor_records(fragments, source_start, source_end)
    anchor_ids = sorted({record["speaker_id"] for record in anchors})
    known_conflicts = (
        known_baseline_conflicts(
            fragments, source_start, source_end, current_id)
        if current_id is not None else [])
    regular_target_anchor = any(
        record["speaker_id"] == current_id and
        "regular_direct_voiceprint" in record["reason"]
        for record in anchors)
    channel_support = None
    if current_id is not None and gallery_id == current_id:
        channel_support = native_channel_support(
            phrase["start"], phrase["end"], current_id, id_to_local, frames,
            frame_sec, voiceprint_policy)

    accepted = False
    reason = "dual_gallery_phrase_current_abstention"
    if current_id is None:
        reason = "dual_gallery_phrase_current_abstention"
    elif gallery_id is None:
        reason = "dual_gallery_phrase_clean_gallery_abstention"
    elif current_id != gallery_id:
        reason = "dual_gallery_phrase_identity_disagreement"
    elif (policy["require_target_channel_top1"] and
          not channel_support["target_top1_present"]):
        reason = "dual_gallery_phrase_target_channel_never_top1"
    elif (policy["reject_competing_sustained_top1"] and
          channel_support["competing_sustained"]):
        reason = "dual_gallery_phrase_competing_sustained_top1"
    elif (policy["veto_competing_direct_anchor"] and
          any(identity != current_id for identity in anchor_ids)):
        reason = "dual_gallery_phrase_competing_direct_anchor"
    elif (policy["require_known_baseline_conflict"] and
          not known_conflicts):
        reason = "dual_gallery_phrase_known_conflict_missing"
    elif (policy["reject_short_only_direct_support"] and anchors and
          not regular_target_anchor):
        reason = "dual_gallery_phrase_short_only_direct_support"
    elif (policy["require_baseline_disagreement"] and
          not range_disagrees(
              fragments, source_start, source_end, current_id)):
        reason = "dual_gallery_phrase_baseline_already_agrees"
    else:
        accepted = True
        reason = "dual_gallery_exact_phrase_consensus"

    return {
        "evidence_id": evidence_id,
        "text_id": int(phrase["text_id"]),
        "source_start": source_start,
        "source_end": source_end,
        "start": float(phrase["start"]),
        "end": float(phrase["end"]),
        "current_speaker_id": current_id,
        "current_reason": str(phrase.get("reason", "")),
        "clean_gallery_speaker_id": gallery_id,
        "clean_gallery_reason": gallery_reason,
        "clean_gallery_ranked": [
            {"speaker_id": speaker_id, "score": score}
            for speaker_id, score in gallery_ranked
        ],
        "direct_anchors": anchors,
        "direct_anchor_ids": anchor_ids,
        "known_baseline_conflict_ids": known_conflicts,
        "regular_target_anchor": regular_target_anchor,
        "native_channel_support": channel_support,
        "selected_speaker_id": current_id if accepted else None,
        "accepted": accepted,
        "reason": reason,
    }


def validate_phrase_contract(phrase, metadata_phrase):
    fields = ("text_id", "source_start", "source_end", "start", "end")
    for name in fields:
        left = phrase[name]
        right = metadata_phrase[name]
        if isinstance(left, (int, float)) and isinstance(right, (int, float)):
            if abs(float(left) - float(right)) > 1e-6:
                raise ValueError(
                    f"phrase metadata differs for {phrase['evidence_id']}")
        elif left != right:
            raise ValueError(
                f"phrase metadata differs for {phrase['evidence_id']}")


def build_candidate(baseline, metadata, gallery_evidence, manifest,
                    frames, frame_sec, voiceprint_policy, policy):
    if baseline.get("kind") != "orator_frozen_speaker_candidate":
        raise ValueError("baseline is not a frozen speaker candidate")
    if metadata.get("kind") != "orator_punctuation_phrase_spans":
        raise ValueError("metadata is not punctuation phrase spans")
    validate_voiceprint_policy(baseline, voiceprint_policy)

    active_ids = [str(value) for value in baseline["active_speaker_ids"]]
    mapping = {
        int(local): str(speaker_id)
        for local, speaker_id in manifest["mapping"].items()
    }
    if sorted(mapping.values()) != sorted(active_ids):
        raise ValueError("manifest identity set differs from baseline")
    id_to_local = {speaker_id: local for local, speaker_id in mapping.items()}

    fragments = source_fragments(baseline.get("track", []))
    validate_source_text(fragments, metadata, "baseline")
    metadata_phrases = {
        str(item["evidence_id"]): item for item in metadata["phrases"]
    }
    decisions = []
    overlays = defaultdict(list)
    for phrase in baseline.get("phrase_decisions", []):
        evidence_id = str(phrase["evidence_id"])
        if evidence_id not in metadata_phrases:
            raise ValueError(f"missing phrase metadata {evidence_id}")
        validate_phrase_contract(phrase, metadata_phrases[evidence_id])
        text_id = int(phrase["text_id"])
        decision = decide_phrase(
            phrase, gallery_evidence, fragments[text_id], active_ids,
            id_to_local, frames, frame_sec, voiceprint_policy, policy)
        decisions.append(decision)
        if decision["accepted"]:
            overlays[text_id].append({
                "text_id": text_id,
                "source_start": decision["source_start"],
                "source_end": decision["source_end"],
                "speaker_id": decision["selected_speaker_id"],
                "reason": decision["reason"],
            })

    track = []
    for text_id in sorted(fragments):
        source = str(metadata["asr"][str(text_id)]["text"])
        track.extend(phrase_tools.reproject_text(
            text_id, source, metadata["align"][str(text_id)],
            fragments[text_id], overlays.get(text_id, []), id_to_local))
    validate_source_text(source_fragments(track), metadata, "projected")
    return {
        "schema_version": 1,
        "kind": "orator_frozen_speaker_candidate",
        "candidate_kind": "v21_dual_gallery_exact_phrase_consensus",
        "audio_sec": float(baseline["audio_sec"]),
        "sample_rate": int(baseline["sample_rate"]),
        "active_speaker_ids": active_ids,
        "source_turn_count": len(baseline.get("track", [])),
        "turn_count": len(track),
        "accepted_phrase_count": sum(
            decision["accepted"] for decision in decisions),
        "policy": policy,
        "voiceprint_policy": {
            name: voiceprint_policy[name] for name in sorted(VOICEPRINT_KEYS)
        },
        "phrase_decisions": decisions,
        "track": track,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", required=True)
    parser.add_argument("--metadata", required=True)
    parser.add_argument("--clean-gallery-phrases", required=True)
    parser.add_argument("--manifest", required=True)
    parser.add_argument("--frames", required=True)
    parser.add_argument("--voiceprint-config", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()

    voiceprint_policy = load_voiceprint_policy(args.voiceprint_config)
    frames, frame_sec = local_tools.read_frame_channels(args.frames)
    result = build_candidate(
        posterior.load_json(args.baseline),
        posterior.load_json(args.metadata),
        phrase_tools.read_titanet(args.clean_gallery_phrases),
        posterior.load_json(args.manifest),
        frames,
        frame_sec,
        voiceprint_policy,
        load_policy(args.config))
    result["sources"] = {
        name: {"path": os.path.abspath(path),
               "sha256": posterior.sha256_file(path)}
        for name, path in {
            "baseline": args.baseline,
            "metadata": args.metadata,
            "clean_gallery_phrases": args.clean_gallery_phrases,
            "manifest": args.manifest,
            "frames": args.frames,
            "voiceprint_config": args.voiceprint_config,
            "config": args.config,
        }.items()
    }
    with open(args.out, "w", encoding="utf-8") as output:
        json.dump(result, output, ensure_ascii=False, indent=2)
    print(json.dumps({
        "accepted_phrases": result["accepted_phrase_count"],
        "turns": result["turn_count"],
        "out": os.path.abspath(args.out),
    }, ensure_ascii=False))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError) as error:
        raise SystemExit(f"speaker dual-gallery phrase candidate: {error}")
