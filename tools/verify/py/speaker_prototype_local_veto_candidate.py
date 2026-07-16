#!/usr/bin/env python3
"""Build a bounded initial-prototype candidate with current-audio vetoes."""

import argparse
import bisect
import csv
import json
import os
import tomllib
from collections import defaultdict

import speaker_posterior_bounded_phrase_candidate as posterior
import speaker_punctuation_phrase_candidate as phrase_tools


REQUIRED_POLICY = {
    "require_initial_direct_anchor",
    "require_terminal_direct_disagreement",
    "require_raw_local_identity_agreement",
    "veto_on_eligible_piece_conflict",
    "veto_on_eligible_phrase_conflict",
    "allow_cross_prototype_margin_veto",
    "require_cross_prototype_top_pair",
    "allow_clean_gallery_current_voiceprint_consensus",
    "allow_clean_gallery_non_top_channel_consensus",
    "allow_gallery_multiscale_channel_consensus",
    "allow_gallery_single_scale_channel_consensus",
    "allow_current_multiscale_channel_consensus",
    "allow_current_single_scale_channel_consensus",
    "allow_sustained_raw_local_consensus",
    "allow_dual_registry_multiscale_consensus",
    "allow_dominant_raw_micro_consensus",
}

MANDATORY_TRUE_POLICY = {
    "require_initial_direct_anchor",
    "require_terminal_direct_disagreement",
    "require_raw_local_identity_agreement",
    "veto_on_eligible_piece_conflict",
    "veto_on_eligible_phrase_conflict",
    "require_cross_prototype_top_pair",
}

REQUIRED_FLOAT_POLICY = {
    "active_channel_threshold",
    "minimum_active_channel_run_sec",
    "minimum_raw_local_run_sec",
}


def load_policy(path):
    with open(path, "rb") as source:
        document = tomllib.load(source)
    section = document.get("prototype_local_veto", {})
    missing = sorted((REQUIRED_POLICY | REQUIRED_FLOAT_POLICY) - set(section))
    if missing:
        raise ValueError(
            "prototype-local-veto policy missing: " + ",".join(missing))
    policy = {name: bool(section[name]) for name in REQUIRED_POLICY}
    if not all(policy[name] for name in MANDATORY_TRUE_POLICY):
        raise ValueError("all prototype-local-veto safety contracts are mandatory")
    policy.update({name: float(section[name])
                   for name in REQUIRED_FLOAT_POLICY})
    if not 0.0 < policy["active_channel_threshold"] < 1.0:
        raise ValueError("active-channel threshold must be between zero and one")
    if policy["minimum_active_channel_run_sec"] <= 0.0:
        raise ValueError("minimum active-channel run must be positive")
    if policy["minimum_raw_local_run_sec"] <= 0.0:
        raise ValueError("minimum raw-local run must be positive")
    return policy


def read_frame_channels(path):
    frames = []
    with open(path, encoding="ascii", newline="") as source:
        reader = csv.DictReader(source)
        columns = reader.fieldnames or []
        speaker_columns = sorted(
            (name for name in columns
             if name.startswith("spk") and name[3:].isdigit()),
            key=lambda name: int(name[3:]))
        if not speaker_columns:
            raise ValueError("frame table has no speaker channels")
        for expected_frame, row in enumerate(reader):
            frame = int(row["frame"])
            if frame != expected_frame:
                raise ValueError("frame table is not contiguous")
            frames.append({
                "frame": frame,
                "time": float(row["time_sec"]),
                "top1": int(row["top1"]),
                "active_count": int(row["active_count"]),
                "channels": [float(row[name]) for name in speaker_columns],
            })
    if len(frames) < 2:
        raise ValueError("frame table must contain at least two frames")
    frame_sec = frames[1]["time"] - frames[0]["time"]
    if frame_sec <= 0.0:
        raise ValueError("frame times are not increasing")
    for index, frame in enumerate(frames):
        expected_time = frames[0]["time"] + index * frame_sec
        if abs(frame["time"] - expected_time) > 1e-4:
            raise ValueError("frame table does not use a stable time base")
    return frames, frame_sec


def select_gallery_identity(evidence, evidence_id, active_ids, policy):
    value = evidence.get(evidence_id)
    if value is None:
        raise ValueError(f"missing gallery evidence {evidence_id}")
    selected, reason, ranked = phrase_tools.select_identity(
        value, active_ids, policy)
    return {
        "speaker_id": selected,
        "reason": reason,
        "ranked": [
            {"speaker_id": speaker_id, "score": score}
            for speaker_id, score in ranked
        ],
    }


def active_channel_support(evidence, speaker_id, id_to_local, frames,
                           frame_sec, policy):
    if speaker_id is None:
        return {
            "speaker_id": None,
            "local_speaker": None,
            "longest_run_sec": 0.0,
            "qualified": False,
        }
    local = id_to_local.get(speaker_id)
    if local is None:
        raise ValueError(f"identity {speaker_id} has no local channel")
    times = [frame["time"] for frame in frames]
    begin = bisect.bisect_left(times, float(evidence["start"]) - 1e-9)
    end = bisect.bisect_left(times, float(evidence["end"]) - 1e-9)
    threshold = policy["active_channel_threshold"]
    longest = 0
    current = 0
    for frame in frames[begin:end]:
        channels = frame["channels"]
        if local >= len(channels):
            raise ValueError(f"frame table is missing local channel {local}")
        if channels[local] >= threshold:
            current += 1
            longest = max(longest, current)
        else:
            current = 0
    run_sec = longest * frame_sec
    return {
        "speaker_id": speaker_id,
        "local_speaker": local,
        "longest_run_sec": run_sec,
        "qualified": (
            run_sec + 1e-9 >= policy["minimum_active_channel_run_sec"]),
    }


def validate_candidate(value, expected_kind):
    if value.get("kind") != "orator_frozen_speaker_candidate":
        raise ValueError("input is not a frozen speaker candidate")
    expected = ({expected_kind} if isinstance(expected_kind, str)
                else set(expected_kind))
    if value.get("candidate_kind") not in expected:
        raise ValueError("unexpected frozen candidate kind")


def validate_direct_parity(initial, terminal):
    initial_track = initial.get("track", [])
    terminal_track = terminal.get("track", [])
    if len(initial_track) != len(terminal_track):
        raise ValueError("initial and terminal direct track lengths differ")
    immutable = ("turn_id", "start", "end", "text_id", "text")
    for index, (left, right) in enumerate(zip(initial_track, terminal_track)):
        if any(left.get(name) != right.get(name) for name in immutable):
            raise ValueError(f"direct track metadata mismatch at {index}")
    if initial.get("active_speaker_ids") != terminal.get("active_speaker_ids"):
        raise ValueError("direct active identity sets differ")


def baseline_fragments(track):
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


def direct_fragment_pairs(initial, terminal):
    left = phrase_tools.source_fragments(initial)
    right = phrase_tools.source_fragments(terminal)
    if set(left) != set(right):
        raise ValueError("direct text ID sets differ")
    output = defaultdict(list)
    for text_id in sorted(left):
        if len(left[text_id]) != len(right[text_id]):
            raise ValueError(f"direct fragment count differs for {text_id}")
        for initial_fragment, terminal_fragment in zip(
                left[text_id], right[text_id]):
            bounds = ("source_start", "source_end")
            if any(initial_fragment[name] != terminal_fragment[name]
                   for name in bounds):
                raise ValueError(f"direct source bounds differ for {text_id}")
            output[text_id].append((initial_fragment, terminal_fragment))
    return dict(output)


def challenge_decision(initial_fragment, terminal_fragment, local_evidence,
                       phrase_decision, policy, gallery_piece_id=None,
                       gallery_phrase_id=None, non_top_channel_active=False):
    initial_id = initial_fragment["decision"].get("speaker_id")
    terminal_id = terminal_fragment["decision"].get("speaker_id")
    mapped_id = local_evidence.get("mapped_speaker_id")
    piece_id = local_evidence.get("speaker_id")
    phrase_id = phrase_decision.get("speaker_id")

    if (policy["require_initial_direct_anchor"] and
            not initial_fragment["anchor"]):
        return False, "initial_direct_ineligible"
    if initial_id is None:
        return False, "initial_identity_missing"
    if (policy["require_terminal_direct_disagreement"] and
            initial_id == terminal_id):
        return False, "terminal_direct_already_agrees"
    if (policy["veto_on_eligible_piece_conflict"] and
            piece_id is not None and piece_id != initial_id):
        return False, "eligible_piece_identity_veto"
    if (policy["veto_on_eligible_phrase_conflict"] and
            phrase_id is not None and phrase_id != initial_id):
        return False, "eligible_phrase_identity_veto"
    if (policy["require_raw_local_identity_agreement"] and
            mapped_id != initial_id):
        ranked = initial_fragment["decision"].get("ranked", [])
        clean_gallery = any(
            item.get("prototype_evidence_id") for item in ranked)
        current_ids = [value for value in (piece_id, phrase_id)
                       if value is not None]
        if (policy["allow_clean_gallery_current_voiceprint_consensus"] and
                clean_gallery and current_ids and
                all(value == initial_id for value in current_ids)):
            return True, "clean_gallery_current_voiceprint_consensus"
        if (policy["allow_clean_gallery_non_top_channel_consensus"] and
                clean_gallery and gallery_piece_id == initial_id and
                gallery_phrase_id == initial_id and non_top_channel_active):
            return True, "clean_gallery_non_top_channel_consensus"
        return False, "raw_local_identity_disagreement"
    return True, "initial_prototype_local_consensus"


def margin_veto_decision(multiprototype_fragment, terminal_fragment,
                         local_evidence, phrase_decision, policy):
    source_id = terminal_fragment["decision"].get("baseline_speaker_id")
    terminal_id = terminal_fragment["decision"].get("speaker_id")
    reduced = multiprototype_fragment["decision"]
    reduced_id = reduced.get("speaker_id")
    mapped_id = local_evidence.get("mapped_speaker_id")
    piece_id = local_evidence.get("speaker_id")
    phrase_id = phrase_decision.get("speaker_id")

    if not policy["allow_cross_prototype_margin_veto"]:
        return False, "cross_prototype_margin_veto_disabled"
    if not terminal_fragment["anchor"] or not terminal_fragment[
            "decision"].get("changed"):
        return False, "terminal_direct_is_not_override"
    if multiprototype_fragment["anchor"]:
        return False, "cross_prototype_decision_still_eligible"
    if "margin_below_gate" not in str(reduced.get("reason", "")):
        return False, "cross_prototype_failure_is_not_margin"
    if source_id is None or reduced_id != source_id or source_id == terminal_id:
        return False, "cross_prototype_did_not_preserve_source"
    ranked = reduced.get("ranked", [])
    if policy["require_cross_prototype_top_pair"]:
        if (len(ranked) < 2 or
                ranked[0].get("speaker_id") != terminal_id or
                ranked[1].get("speaker_id") != source_id):
            return False, "cross_prototype_top_pair_mismatch"
    if (policy["require_raw_local_identity_agreement"] and
            mapped_id != source_id):
        return False, "raw_local_identity_disagreement"
    if (policy["veto_on_eligible_piece_conflict"] and
            piece_id is not None and piece_id != source_id):
        return False, "eligible_piece_identity_veto"
    if (policy["veto_on_eligible_phrase_conflict"] and
            phrase_id is not None and phrase_id != source_id):
        return False, "eligible_phrase_identity_veto"
    return True, "cross_prototype_margin_veto"


def gallery_channel_decision(local_evidence, phrase_decision, gallery_piece,
                             gallery_phrase, channel_support,
                             baseline_disagrees, policy):
    gallery_piece_id = ((gallery_piece or {}).get("speaker_id"))
    gallery_phrase_id = ((gallery_phrase or {}).get("speaker_id"))
    gallery_ids = [speaker_id for speaker_id in (
        gallery_piece_id, gallery_phrase_id) if speaker_id is not None]
    if not gallery_ids:
        return False, None, "gallery_identity_missing"
    if len(gallery_ids) == 2 and gallery_ids[0] != gallery_ids[1]:
        return False, None, "gallery_scale_identity_disagreement"
    gallery_id = gallery_ids[0]
    if len(gallery_ids) == 2:
        if not policy["allow_gallery_multiscale_channel_consensus"]:
            return False, None, "gallery_multiscale_channel_disabled"
        accepted_reason = "gallery_multiscale_channel_consensus"
    else:
        if not policy["allow_gallery_single_scale_channel_consensus"]:
            return False, None, "gallery_single_scale_channel_disabled"
        accepted_reason = "gallery_single_scale_channel_consensus"
    if local_evidence.get("mapped_speaker_id") != gallery_id:
        return False, None, "raw_local_identity_disagreement"
    piece_id = local_evidence.get("speaker_id")
    if (policy["veto_on_eligible_piece_conflict"] and
            piece_id is not None and piece_id != gallery_id):
        return False, None, "eligible_piece_identity_veto"
    phrase_id = phrase_decision.get("speaker_id")
    if (policy["veto_on_eligible_phrase_conflict"] and
            phrase_id is not None and phrase_id != gallery_id):
        return False, None, "eligible_phrase_identity_veto"
    if (channel_support.get("speaker_id") != gallery_id or
            not channel_support.get("qualified", False)):
        return False, None, "gallery_identity_channel_inactive"
    if not baseline_disagrees:
        return False, None, "baseline_identity_already_agrees"
    return True, gallery_id, accepted_reason


def current_channel_decision(local_evidence, phrase_decision, gallery_piece,
                             gallery_phrase, channel_support,
                             baseline_disagrees, policy):
    piece_id = local_evidence.get("speaker_id")
    phrase_id = phrase_decision.get("speaker_id")
    current_ids = [speaker_id for speaker_id in (piece_id, phrase_id)
                   if speaker_id is not None]
    if not current_ids:
        return False, None, "current_identity_missing"
    if len(current_ids) == 2 and current_ids[0] != current_ids[1]:
        return False, None, "current_scale_identity_disagreement"
    current_id = current_ids[0]
    if len(current_ids) == 2:
        if not policy["allow_current_multiscale_channel_consensus"]:
            return False, None, "current_multiscale_channel_disabled"
        accepted_reason = "current_multiscale_channel_consensus"
    else:
        if not policy["allow_current_single_scale_channel_consensus"]:
            return False, None, "current_single_scale_channel_disabled"
        accepted_reason = "current_single_scale_channel_consensus"
    if local_evidence.get("mapped_speaker_id") != current_id:
        return False, None, "raw_local_identity_disagreement"
    gallery_ids = [
        value for value in (
            (gallery_piece or {}).get("speaker_id"),
            (gallery_phrase or {}).get("speaker_id"))
        if value is not None
    ]
    if any(value != current_id for value in gallery_ids):
        return False, None, "clean_gallery_identity_veto"
    if len(current_ids) == 1 and not gallery_ids:
        return False, None, "clean_gallery_identity_missing"
    if (channel_support.get("speaker_id") != current_id or
            not channel_support.get("qualified", False)):
        return False, None, "current_identity_channel_inactive"
    if not baseline_disagrees:
        return False, None, "baseline_identity_already_agrees"
    return True, current_id, accepted_reason


def raw_local_decision(local_evidence, phrase_decision, gallery_piece,
                       gallery_phrase, channel_support,
                       baseline_disagrees, policy):
    if not policy["allow_sustained_raw_local_consensus"]:
        return False, None, "sustained_raw_local_disabled"
    mapped_id = local_evidence.get("mapped_speaker_id")
    if mapped_id is None:
        return False, None, "raw_local_identity_missing"
    voiceprint_ids = [
        value for value in (
            local_evidence.get("speaker_id"),
            phrase_decision.get("speaker_id"),
            (gallery_piece or {}).get("speaker_id"),
            (gallery_phrase or {}).get("speaker_id"))
        if value is not None
    ]
    if any(value != mapped_id for value in voiceprint_ids):
        return False, None, "eligible_voiceprint_identity_veto"
    if (channel_support.get("speaker_id") != mapped_id or
            channel_support.get("longest_run_sec", 0.0) + 1e-9 <
            policy["minimum_raw_local_run_sec"]):
        return False, None, "raw_local_channel_run_too_short"
    if not baseline_disagrees:
        return False, None, "baseline_identity_already_agrees"
    return True, mapped_id, "sustained_raw_local_consensus"


def dual_registry_decision(local_evidence, phrase_decision, gallery_piece,
                           gallery_phrase, baseline_disagrees, policy):
    if not policy["allow_dual_registry_multiscale_consensus"]:
        return False, None, "dual_registry_multiscale_disabled"
    identities = [
        local_evidence.get("speaker_id"),
        phrase_decision.get("speaker_id"),
        (gallery_piece or {}).get("speaker_id"),
        (gallery_phrase or {}).get("speaker_id"),
    ]
    if any(value is None for value in identities):
        return False, None, "dual_registry_voiceprint_abstention"
    selected_id = identities[0]
    if any(value != selected_id for value in identities[1:]):
        return False, None, "dual_registry_voiceprint_disagreement"
    if not baseline_disagrees:
        return False, None, "baseline_identity_already_agrees"
    return True, selected_id, "dual_registry_multiscale_consensus"


def dominant_micro_decision(evidence, frames, id_to_local,
                            baseline_disagrees, policy):
    if not policy["allow_dominant_raw_micro_consensus"]:
        return False, None, "dominant_raw_micro_disabled"
    mapped_id = evidence.get("mapped_speaker_id")
    local = int(evidence["local_speaker"])
    if mapped_id is None or id_to_local.get(mapped_id) != local:
        return False, None, "micro_local_identity_mapping_mismatch"
    frame_start = int(evidence["frame_start"])
    frame_end = int(evidence["frame_end"])
    frame_count = int(evidence["frame_count"])
    if (frame_start < 0 or frame_end > len(frames) or
            frame_end <= frame_start or
            frame_end - frame_start != frame_count):
        return False, None, "micro_frame_contract_mismatch"
    selected_frames = frames[frame_start:frame_end]
    threshold = policy["active_channel_threshold"]
    if any(frame["frame"] != frame_start + index
           for index, frame in enumerate(selected_frames)):
        return False, None, "micro_frame_contract_mismatch"
    if any(frame["active_count"] != 1 or frame["top1"] != local or
           local >= len(frame["channels"]) or
           frame["channels"][local] < threshold
           for frame in selected_frames):
        return False, None, "micro_native_frame_not_dominant"
    if not baseline_disagrees:
        return False, None, "baseline_identity_already_agrees"
    return True, mapped_id, "dominant_raw_micro_consensus"


def source_intersection(left, right):
    start = max(int(left["source_start"]), int(right["source_start"]))
    end = min(int(left["source_end"]), int(right["source_end"]))
    return (start, end) if end > start else None


def validate_overlays(overlays):
    by_text = defaultdict(list)
    for overlay in overlays:
        by_text[int(overlay["text_id"])].append(overlay)
    for text_id, values in by_text.items():
        ordered = sorted(values, key=lambda item: (
            item["source_start"], item["source_end"], item["speaker_id"]))
        active = []
        for item in ordered:
            active = [other for other in active
                      if other["source_end"] > item["source_start"]]
            if any(other["speaker_id"] != item["speaker_id"]
                   for other in active):
                raise ValueError(
                    f"conflicting accepted overlays for text {text_id}")
            active.append(item)


def overlay_has_identity_conflict(overlays, candidate):
    for existing in overlays:
        if int(existing["text_id"]) != int(candidate["text_id"]):
            continue
        if (int(existing["source_end"]) <= int(candidate["source_start"]) or
                int(candidate["source_end"]) <=
                int(existing["source_start"])):
            continue
        if existing["speaker_id"] != candidate["speaker_id"]:
            return True
    return False


def baseline_range_disagrees(fragments, source_start, source_end, speaker_id):
    for fragment in fragments:
        overlap_start = max(int(source_start), int(fragment["source_start"]))
        overlap_end = min(int(source_end), int(fragment["source_end"]))
        if (overlap_end > overlap_start and
                fragment["entry"].get("speaker_id") != speaker_id):
            return True
    return False


def build_candidate(baseline, initial, multiprototype, terminal, metadata,
                    manifest, gallery_piece_evidence, gallery_phrase_evidence,
                    frames, frame_sec, policy):
    validate_candidate(baseline, {
        "v21_multiresolution_phrase_consensus",
        "v21_prototype_local_veto",
    })
    validate_candidate(initial, {
        "v21_replay_interval_direct_voiceprint",
        "v21_clean_gallery_direct_voiceprint",
    })
    validate_candidate(
        multiprototype, "v21_replay_interval_direct_voiceprint")
    validate_candidate(terminal, "v21_replay_interval_direct_voiceprint")
    validate_direct_parity(initial, terminal)
    validate_direct_parity(multiprototype, terminal)

    active_ids = [str(value) for value in baseline["active_speaker_ids"]]
    if (active_ids != [str(value) for value in initial["active_speaker_ids"]] or
            active_ids != [str(value) for value in terminal["active_speaker_ids"]]):
        raise ValueError("candidate active identity sets differ")
    mapping = {
        int(local): str(speaker_id)
        for local, speaker_id in manifest["mapping"].items()
    }
    if sorted(mapping.values()) != sorted(active_ids):
        raise ValueError("manifest identity gallery is incomplete")
    id_to_local = {speaker_id: local for local, speaker_id in mapping.items()}

    source_pairs = direct_fragment_pairs(initial, terminal)
    multiprototype_pairs = direct_fragment_pairs(multiprototype, terminal)
    fragments = baseline_fragments(baseline.get("track", []))
    phrase_decisions = {
        item["evidence_id"]: item
        for item in baseline.get("phrase_decisions", [])
    }
    voiceprint_policy = baseline.get("voiceprint_policy", baseline["policy"])
    if abs(policy["minimum_raw_local_run_sec"] -
           voiceprint_policy["minimum_sustained_run_sec"]) > 1e-6:
        raise ValueError("raw-local run floor differs from inherited policy")
    gallery_phrase_decisions = {
        evidence_id: select_gallery_identity(
            gallery_phrase_evidence, evidence_id, active_ids,
            voiceprint_policy)
        for evidence_id in phrase_decisions
    }
    local_evidence = [
        *baseline.get("posterior_decisions", []),
        *baseline.get("micro_decisions", []),
    ]
    overlays = []
    decisions = []
    gallery_channel_decisions = []
    current_channel_decisions = []
    raw_local_decisions = []
    dual_registry_decisions = []
    dominant_micro_decisions = []
    for evidence in local_evidence:
        text_id = int(evidence["text_id"])
        phrase_id = evidence["phrase_evidence_id"]
        if phrase_id not in phrase_decisions:
            raise ValueError(f"missing phrase decision {phrase_id}")
        gallery_piece = None
        if str(evidence["evidence_id"]).startswith("posterior_phrase:"):
            gallery_piece = select_gallery_identity(
                gallery_piece_evidence, evidence["evidence_id"], active_ids,
                voiceprint_policy)
        gallery_phrase = gallery_phrase_decisions[phrase_id]
        initial_values = source_pairs.get(text_id, [])
        multiprototype_values = multiprototype_pairs.get(text_id, [])
        if len(initial_values) != len(multiprototype_values):
            raise ValueError(f"prototype fragment count differs for {text_id}")
        for pair_index, (initial_fragment, terminal_fragment) in enumerate(
                initial_values):
            intersection = source_intersection(evidence, initial_fragment)
            if intersection is None:
                continue
            initial_id = initial_fragment["decision"].get("speaker_id")
            channel_support = active_channel_support(
                evidence, initial_id, id_to_local, frames, frame_sec, policy)
            accepted, reason = challenge_decision(
                initial_fragment, terminal_fragment, evidence,
                phrase_decisions[phrase_id], policy,
                (gallery_piece or {}).get("speaker_id"),
                gallery_phrase.get("speaker_id"),
                channel_support["qualified"])
            selected_id = initial_fragment["decision"].get("speaker_id")
            if not accepted:
                multiprototype_fragment, reduced_terminal = (
                    multiprototype_values[pair_index])
                if (reduced_terminal["source_start"] !=
                        terminal_fragment["source_start"] or
                        reduced_terminal["source_end"] !=
                        terminal_fragment["source_end"]):
                    raise ValueError("prototype terminal fragment bounds differ")
                accepted, reason = margin_veto_decision(
                    multiprototype_fragment, terminal_fragment, evidence,
                    phrase_decisions[phrase_id], policy)
                if accepted:
                    selected_id = terminal_fragment["decision"].get(
                        "baseline_speaker_id")
            source_start, source_end = intersection
            audit = {
                "initial_evidence_id": initial_fragment["decision"].get(
                    "evidence_id"),
                "local_evidence_id": evidence["evidence_id"],
                "phrase_evidence_id": phrase_id,
                "text_id": text_id,
                "source_start": source_start,
                "source_end": source_end,
                "initial_speaker_id": initial_fragment["decision"].get(
                    "speaker_id"),
                "multiprototype_speaker_id": multiprototype_values[
                    pair_index][0]["decision"].get("speaker_id"),
                "terminal_speaker_id": terminal_fragment["decision"].get(
                    "speaker_id"),
                "source_speaker_id": terminal_fragment["decision"].get(
                    "baseline_speaker_id"),
                "mapped_speaker_id": evidence.get("mapped_speaker_id"),
                "piece_speaker_id": evidence.get("speaker_id"),
                "phrase_speaker_id": phrase_decisions[phrase_id].get(
                    "speaker_id"),
                "gallery_piece_speaker_id": (gallery_piece or {}).get(
                    "speaker_id"),
                "gallery_phrase_speaker_id": gallery_phrase.get(
                    "speaker_id"),
                "active_channel_speaker_id": channel_support["speaker_id"],
                "active_channel_local_speaker": channel_support[
                    "local_speaker"],
                "active_channel_longest_run_sec": channel_support[
                    "longest_run_sec"],
                "active_channel_qualified": channel_support["qualified"],
                "accepted": accepted,
                "reason": reason,
            }
            decisions.append(audit)
            if accepted:
                overlays.append({
                    "text_id": text_id,
                    "source_start": source_start,
                    "source_end": source_end,
                    "speaker_id": selected_id,
                    "reason": reason,
                })
        if gallery_piece is not None:
            gallery_id = gallery_piece.get("speaker_id")
            gallery_channel = active_channel_support(
                evidence, gallery_id, id_to_local, frames, frame_sec, policy)
            baseline_disagrees = baseline_range_disagrees(
                fragments.get(text_id, []), evidence["source_start"],
                evidence["source_end"], gallery_id)
            gallery_accepted, gallery_selected_id, gallery_reason = (
                gallery_channel_decision(
                    evidence, phrase_decisions[phrase_id], gallery_piece,
                    gallery_phrase, gallery_channel, baseline_disagrees,
                    policy))
            gallery_overlay = {
                "text_id": text_id,
                "source_start": int(evidence["source_start"]),
                "source_end": int(evidence["source_end"]),
                "speaker_id": gallery_selected_id,
                "reason": gallery_reason,
            }
            if (gallery_accepted and
                    overlay_has_identity_conflict(overlays, gallery_overlay)):
                gallery_accepted = False
                gallery_reason = "accepted_overlay_identity_conflict"
            gallery_channel_decisions.append({
                "evidence_id": evidence["evidence_id"],
                "phrase_evidence_id": phrase_id,
                "text_id": text_id,
                "source_start": int(evidence["source_start"]),
                "source_end": int(evidence["source_end"]),
                "mapped_speaker_id": evidence.get("mapped_speaker_id"),
                "piece_speaker_id": evidence.get("speaker_id"),
                "phrase_speaker_id": phrase_decisions[phrase_id].get(
                    "speaker_id"),
                "gallery_piece_speaker_id": gallery_piece.get("speaker_id"),
                "gallery_phrase_speaker_id": gallery_phrase.get("speaker_id"),
                "active_channel_local_speaker": gallery_channel.get(
                    "local_speaker"),
                "active_channel_longest_run_sec": gallery_channel.get(
                    "longest_run_sec"),
                "active_channel_qualified": gallery_channel.get("qualified"),
                "baseline_identity_disagrees": baseline_disagrees,
                "selected_speaker_id": gallery_selected_id,
                "accepted": gallery_accepted,
                "reason": gallery_reason,
            })
            if gallery_accepted:
                gallery_overlay["speaker_id"] = gallery_selected_id
                overlays.append(gallery_overlay)
            current_id = (evidence.get("speaker_id") or
                          phrase_decisions[phrase_id].get("speaker_id"))
            current_channel = active_channel_support(
                evidence, current_id, id_to_local, frames, frame_sec, policy)
            current_baseline_disagrees = baseline_range_disagrees(
                fragments.get(text_id, []), evidence["source_start"],
                evidence["source_end"], current_id)
            current_accepted, current_selected_id, current_reason = (
                current_channel_decision(
                    evidence, phrase_decisions[phrase_id], gallery_piece,
                    gallery_phrase, current_channel,
                    current_baseline_disagrees, policy))
            current_overlay = {
                "text_id": text_id,
                "source_start": int(evidence["source_start"]),
                "source_end": int(evidence["source_end"]),
                "speaker_id": current_selected_id,
                "reason": current_reason,
            }
            if (current_accepted and
                    overlay_has_identity_conflict(overlays, current_overlay)):
                current_accepted = False
                current_reason = "accepted_overlay_identity_conflict"
            current_channel_decisions.append({
                "evidence_id": evidence["evidence_id"],
                "phrase_evidence_id": phrase_id,
                "text_id": text_id,
                "source_start": int(evidence["source_start"]),
                "source_end": int(evidence["source_end"]),
                "mapped_speaker_id": evidence.get("mapped_speaker_id"),
                "piece_speaker_id": evidence.get("speaker_id"),
                "phrase_speaker_id": phrase_decisions[phrase_id].get(
                    "speaker_id"),
                "gallery_piece_speaker_id": gallery_piece.get("speaker_id"),
                "gallery_phrase_speaker_id": gallery_phrase.get("speaker_id"),
                "active_channel_local_speaker": current_channel.get(
                    "local_speaker"),
                "active_channel_longest_run_sec": current_channel.get(
                    "longest_run_sec"),
                "active_channel_qualified": current_channel.get("qualified"),
                "baseline_identity_disagrees": current_baseline_disagrees,
                "selected_speaker_id": current_selected_id,
                "accepted": current_accepted,
                "reason": current_reason,
            })
            if current_accepted:
                current_overlay["speaker_id"] = current_selected_id
                overlays.append(current_overlay)
            mapped_id = evidence.get("mapped_speaker_id")
            mapped_channel = active_channel_support(
                evidence, mapped_id, id_to_local, frames, frame_sec, policy)
            mapped_baseline_disagrees = baseline_range_disagrees(
                fragments.get(text_id, []), evidence["source_start"],
                evidence["source_end"], mapped_id)
            raw_accepted, raw_selected_id, raw_reason = raw_local_decision(
                evidence, phrase_decisions[phrase_id], gallery_piece,
                gallery_phrase, mapped_channel, mapped_baseline_disagrees,
                policy)
            raw_overlay = {
                "text_id": text_id,
                "source_start": int(evidence["source_start"]),
                "source_end": int(evidence["source_end"]),
                "speaker_id": raw_selected_id,
                "reason": raw_reason,
            }
            if (raw_accepted and
                    overlay_has_identity_conflict(overlays, raw_overlay)):
                raw_accepted = False
                raw_reason = "accepted_overlay_identity_conflict"
            raw_local_decisions.append({
                "evidence_id": evidence["evidence_id"],
                "phrase_evidence_id": phrase_id,
                "text_id": text_id,
                "source_start": int(evidence["source_start"]),
                "source_end": int(evidence["source_end"]),
                "mapped_speaker_id": mapped_id,
                "piece_speaker_id": evidence.get("speaker_id"),
                "phrase_speaker_id": phrase_decisions[phrase_id].get(
                    "speaker_id"),
                "gallery_piece_speaker_id": gallery_piece.get("speaker_id"),
                "gallery_phrase_speaker_id": gallery_phrase.get("speaker_id"),
                "active_channel_local_speaker": mapped_channel.get(
                    "local_speaker"),
                "active_channel_longest_run_sec": mapped_channel.get(
                    "longest_run_sec"),
                "baseline_identity_disagrees": mapped_baseline_disagrees,
                "selected_speaker_id": raw_selected_id,
                "accepted": raw_accepted,
                "reason": raw_reason,
            })
            if raw_accepted:
                raw_overlay["speaker_id"] = raw_selected_id
                overlays.append(raw_overlay)
            dual_id = evidence.get("speaker_id")
            dual_baseline_disagrees = baseline_range_disagrees(
                fragments.get(text_id, []), evidence["source_start"],
                evidence["source_end"], dual_id)
            dual_accepted, dual_selected_id, dual_reason = (
                dual_registry_decision(
                    evidence, phrase_decisions[phrase_id], gallery_piece,
                    gallery_phrase, dual_baseline_disagrees, policy))
            dual_overlay = {
                "text_id": text_id,
                "source_start": int(evidence["source_start"]),
                "source_end": int(evidence["source_end"]),
                "speaker_id": dual_selected_id,
                "reason": dual_reason,
            }
            if (dual_accepted and
                    overlay_has_identity_conflict(overlays, dual_overlay)):
                dual_accepted = False
                dual_reason = "accepted_overlay_identity_conflict"
            dual_registry_decisions.append({
                "evidence_id": evidence["evidence_id"],
                "phrase_evidence_id": phrase_id,
                "text_id": text_id,
                "source_start": int(evidence["source_start"]),
                "source_end": int(evidence["source_end"]),
                "mapped_speaker_id": evidence.get("mapped_speaker_id"),
                "piece_speaker_id": evidence.get("speaker_id"),
                "phrase_speaker_id": phrase_decisions[phrase_id].get(
                    "speaker_id"),
                "gallery_piece_speaker_id": gallery_piece.get("speaker_id"),
                "gallery_phrase_speaker_id": gallery_phrase.get("speaker_id"),
                "baseline_identity_disagrees": dual_baseline_disagrees,
                "selected_speaker_id": dual_selected_id,
                "accepted": dual_accepted,
                "reason": dual_reason,
            })
            if dual_accepted:
                dual_overlay["speaker_id"] = dual_selected_id
                overlays.append(dual_overlay)
        if str(evidence["evidence_id"]).startswith("micro_phrase:"):
            micro_id = evidence.get("mapped_speaker_id")
            micro_baseline_disagrees = baseline_range_disagrees(
                fragments.get(text_id, []), evidence["source_start"],
                evidence["source_end"], micro_id)
            micro_accepted, micro_selected_id, micro_reason = (
                dominant_micro_decision(
                    evidence, frames, id_to_local,
                    micro_baseline_disagrees, policy))
            micro_overlay = {
                "text_id": text_id,
                "source_start": int(evidence["source_start"]),
                "source_end": int(evidence["source_end"]),
                "speaker_id": micro_selected_id,
                "reason": micro_reason,
            }
            if (micro_accepted and
                    overlay_has_identity_conflict(overlays, micro_overlay)):
                micro_accepted = False
                micro_reason = "accepted_overlay_identity_conflict"
            dominant_micro_decisions.append({
                "evidence_id": evidence["evidence_id"],
                "phrase_evidence_id": phrase_id,
                "text_id": text_id,
                "source_start": int(evidence["source_start"]),
                "source_end": int(evidence["source_end"]),
                "mapped_speaker_id": micro_id,
                "local_speaker": int(evidence["local_speaker"]),
                "frame_start": int(evidence["frame_start"]),
                "frame_end": int(evidence["frame_end"]),
                "frame_count": int(evidence["frame_count"]),
                "baseline_identity_disagrees": micro_baseline_disagrees,
                "selected_speaker_id": micro_selected_id,
                "accepted": micro_accepted,
                "reason": micro_reason,
            })
            if micro_accepted:
                micro_overlay["speaker_id"] = micro_selected_id
                overlays.append(micro_overlay)
    validate_overlays(overlays)

    overlays_by_text = defaultdict(list)
    for overlay in overlays:
        overlays_by_text[overlay["text_id"]].append(overlay)
    track = []
    for text_id in sorted(fragments):
        source = metadata["asr"][str(text_id)]["text"]
        actual = "".join(str(item["entry"].get("text", ""))
                         for item in fragments[text_id])
        if actual != source:
            raise ValueError(f"baseline source text mismatch for {text_id}")
        track.extend(phrase_tools.reproject_text(
            text_id, source, metadata["align"][str(text_id)],
            fragments[text_id], overlays_by_text[text_id], id_to_local))
    for text_id in sorted(fragments):
        expected = metadata["asr"][str(text_id)]["text"]
        actual = "".join(item["text"] for item in track
                         if int(item["text_id"]) == text_id)
        if actual != expected:
            raise ValueError(f"projected source text mismatch for {text_id}")

    return {
        "schema_version": 1,
        "kind": "orator_frozen_speaker_candidate",
        "candidate_kind": "v21_prototype_local_veto",
        "audio_sec": float(baseline["audio_sec"]),
        "sample_rate": int(baseline["sample_rate"]),
        "active_speaker_ids": active_ids,
        "source_turn_count": len(baseline.get("track", [])),
        "turn_count": len(track),
        "challenge_count": len(decisions),
        "accepted_challenge_count": sum(
            item["accepted"] for item in decisions),
        "gallery_channel_challenge_count": len(gallery_channel_decisions),
        "accepted_gallery_channel_count": sum(
            item["accepted"] for item in gallery_channel_decisions),
        "current_channel_challenge_count": len(current_channel_decisions),
        "accepted_current_channel_count": sum(
            item["accepted"] for item in current_channel_decisions),
        "raw_local_challenge_count": len(raw_local_decisions),
        "accepted_raw_local_count": sum(
            item["accepted"] for item in raw_local_decisions),
        "dual_registry_challenge_count": len(dual_registry_decisions),
        "accepted_dual_registry_count": sum(
            item["accepted"] for item in dual_registry_decisions),
        "dominant_micro_challenge_count": len(dominant_micro_decisions),
        "accepted_dominant_micro_count": sum(
            item["accepted"] for item in dominant_micro_decisions),
        "policy": policy,
        "voiceprint_policy": voiceprint_policy,
        "posterior_decisions": baseline.get("posterior_decisions", []),
        "phrase_decisions": baseline.get("phrase_decisions", []),
        "micro_decisions": baseline.get("micro_decisions", []),
        "challenge_decisions": decisions,
        "gallery_channel_decisions": gallery_channel_decisions,
        "current_channel_decisions": current_channel_decisions,
        "raw_local_decisions": raw_local_decisions,
        "dual_registry_decisions": dual_registry_decisions,
        "dominant_micro_decisions": dominant_micro_decisions,
        "track": track,
    }


def verify_source_link(baseline, source_names, path):
    names = ([source_names] if isinstance(source_names, str)
             else list(source_names))
    expected = [
        baseline.get("sources", {}).get(name, {}).get("sha256")
        for name in names
    ]
    if posterior.sha256_file(path) not in expected:
        raise ValueError(
            f"baseline {'/'.join(names)} provenance mismatch")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", required=True)
    parser.add_argument("--initial-direct", required=True)
    parser.add_argument("--multiprototype-direct", required=True)
    parser.add_argument("--terminal-direct", required=True)
    parser.add_argument("--posterior-metadata", required=True)
    parser.add_argument("--manifest", required=True)
    parser.add_argument("--gallery-piece-titanet", required=True)
    parser.add_argument("--gallery-phrase-titanet", required=True)
    parser.add_argument("--frames", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()

    baseline = posterior.load_json(args.baseline)
    verify_source_link(
        baseline, ("direct", "terminal_direct"), args.terminal_direct)
    verify_source_link(baseline, "posterior_metadata", args.posterior_metadata)
    frames, frame_sec = read_frame_channels(args.frames)
    result = build_candidate(
        baseline,
        posterior.load_json(args.initial_direct),
        posterior.load_json(args.multiprototype_direct),
        posterior.load_json(args.terminal_direct),
        posterior.load_json(args.posterior_metadata),
        posterior.load_json(args.manifest),
        phrase_tools.read_titanet(args.gallery_piece_titanet),
        phrase_tools.read_titanet(args.gallery_phrase_titanet),
        frames,
        frame_sec,
        load_policy(args.config))
    result["sources"] = {
        name: {
            "path": os.path.abspath(path),
            "sha256": posterior.sha256_file(path),
        }
        for name, path in {
            "baseline": args.baseline,
            "initial_direct": args.initial_direct,
            "multiprototype_direct": args.multiprototype_direct,
            "terminal_direct": args.terminal_direct,
            "posterior_metadata": args.posterior_metadata,
            "manifest": args.manifest,
            "gallery_piece_titanet": args.gallery_piece_titanet,
            "gallery_phrase_titanet": args.gallery_phrase_titanet,
            "frames": args.frames,
            "config": args.config,
        }.items()
    }
    with open(args.out, "w", encoding="utf-8") as output:
        json.dump(result, output, ensure_ascii=False, indent=2)
    print(json.dumps({
        "challenges": result["challenge_count"],
        "accepted_challenges": result["accepted_challenge_count"],
        "turns": result["turn_count"],
        "out": os.path.abspath(args.out),
    }, ensure_ascii=False))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError) as error:
        raise SystemExit(f"speaker prototype local veto: {error}")
