#!/usr/bin/env python3
"""Build a reference-free speaker view from multiresolution phrase evidence."""

import argparse
import json
import os
import tomllib
from collections import defaultdict

import speaker_posterior_bounded_phrase_candidate as posterior
import speaker_punctuation_phrase_candidate as phrase_tools


EPSILON = 1e-9


def load_policy(path):
    policy = posterior.load_policy(path)
    with open(path, "rb") as source:
        document = tomllib.load(source)
    section = document.get("multiresolution_phrase", {})
    required = {
        "minimum_micro_run_sec",
        "allow_micro_local_phrase_consensus",
        "allow_three_view_regular_anchor_split",
        "allow_regular_score_dual_voiceprint_override",
        "allow_isolated_unanchored_short_override",
        "reject_competing_direct_anchors_for_dual_override",
    }
    missing = sorted(required - set(section))
    if missing:
        raise ValueError(
            "multiresolution phrase policy missing: " + ",".join(missing))
    policy.update({
        "minimum_micro_run_sec": float(section["minimum_micro_run_sec"]),
        **{name: bool(section[name]) for name in required
           if name != "minimum_micro_run_sec"},
    })
    if (policy["minimum_micro_run_sec"] <= 0.0 or
            policy["minimum_micro_run_sec"] + EPSILON >=
            policy["minimum_piece_duration_sec"]):
        raise ValueError("micro-run floor must be below the embedding floor")
    enabled = [policy[name] for name in required
               if name != "minimum_micro_run_sec"]
    if not all(enabled):
        raise ValueError("all frozen multiresolution evidence paths are required")
    return policy


def micro_policy(policy):
    value = dict(policy)
    value["minimum_sustained_run_sec"] = policy["minimum_micro_run_sec"]
    value["minimum_piece_duration_sec"] = policy["minimum_micro_run_sec"]
    return value


def global_active_runs(frames, frame_sec, policy):
    start = frames[0]["time"] - 0.5 * frame_sec
    end = frames[-1]["time"] + 0.5 * frame_sec
    return posterior.active_runs(
        frames, frame_sec, start, end, micro_policy(policy))


def global_run_for_piece(piece, runs):
    midpoint = 0.5 * (float(piece["start"]) + float(piece["end"]))
    index = posterior.run_for_time(runs, midpoint)
    if index is None:
        return None
    run = runs[index]
    if int(run["local_speaker"]) != int(piece["local_speaker"]):
        return None
    return run


def enumerate_micro_pieces(phrases, frames, frame_sec, policy):
    values = []
    local_policy = micro_policy(policy)
    for phrase in phrases["phrases"]:
        text_id = int(phrase["text_id"])
        source = phrases["asr"][str(text_id)]["text"]
        units = phrases["align"][str(text_id)]
        runs = posterior.active_runs(
            frames, frame_sec, float(phrase["start"]), float(phrase["end"]),
            local_policy)
        for piece in posterior.phrase_pieces(
                phrase, source, units, runs, local_policy):
            duration = float(piece["end"]) - float(piece["start"])
            if duration + EPSILON >= policy["minimum_piece_duration_sec"]:
                continue
            values.append({
                "evidence_id": f"micro_phrase:{len(values)}",
                "phrase_evidence_id": phrase["evidence_id"],
                "text_id": text_id,
                "text": source[piece["source_start"]:piece["source_end"]],
                **piece,
            })
    return values


def build_phrase_decisions(phrases, evidence, active_ids, fragments, policy):
    output = {}
    for phrase in phrases["phrases"]:
        text_id = int(phrase["text_id"])
        selected, reason, ranked = phrase_tools.select_identity(
            evidence.get(phrase["evidence_id"], {}), active_ids, policy)
        overlaps = posterior.overlapping_fragments(
            phrase, fragments[text_id])
        anchor_ids = sorted({
            item["decision"].get("speaker_id") for item in overlaps
            if item["anchor"] and item["decision"].get("speaker_id")
        })
        output[phrase["evidence_id"]] = {
            **phrase,
            "speaker_id": selected,
            "reason": reason,
            "direct_anchor_ids": anchor_ids,
            "ranked": [
                {"speaker_id": speaker_id, "score": score}
                for speaker_id, score in ranked
            ],
        }
    return output


def add_overlay(overlays, piece, speaker_id, reason):
    overlays[int(piece["text_id"])].append({
        "source_start": int(piece["source_start"]),
        "source_end": int(piece["source_end"]),
        "speaker_id": speaker_id,
        "reason": reason,
    })


def correction_for_piece(decision, phrase, overlaps, run, policy):
    selected = decision.get("speaker_id")
    mapped = decision.get("mapped_speaker_id")
    phrase_selected = phrase.get("speaker_id")
    if selected is None or phrase_selected != selected:
        return False, "multiresolution_phrase_voiceprint_disagreement"

    anchor_ids = {
        item["decision"].get("speaker_id") for item in overlaps
        if item["anchor"] and item["decision"].get("speaker_id")
    }
    inherited_reason = str(decision.get("reason", ""))
    if (selected == mapped and
            "piece_regular_direct_anchor_conflict" in inherited_reason and
            policy["allow_three_view_regular_anchor_split"]):
        return True, "three_view_regular_anchor_split"

    if selected == mapped:
        return False, "multiresolution_inherited_consensus"

    ranked = decision.get("ranked", [])
    strong = bool(ranked) and (
        float(ranked[0]["score"]) + EPSILON >= policy["regular_min_score"])
    competing = len(anchor_ids) > 1
    if (strong and
            policy["allow_regular_score_dual_voiceprint_override"] and
            not (competing and policy[
                "reject_competing_direct_anchors_for_dual_override"])):
        return True, "regular_score_dual_voiceprint_override"

    run_duration = (
        float(run["end"]) - float(run["start"]) if run is not None else None)
    if (not anchor_ids and run_duration is not None and
            run_duration <= policy["short_max_sec"] + EPSILON and
            policy["allow_isolated_unanchored_short_override"]):
        return True, "isolated_unanchored_short_voiceprint_override"
    return False, "multiresolution_correction_rejected"


def micro_decision(piece, phrase, mapped_id, overlaps, policy):
    selected = phrase.get("speaker_id")
    if (not policy["allow_micro_local_phrase_consensus"] or
            selected is None or selected != mapped_id):
        return False, "micro_local_phrase_disagreement"
    anchor_ids = {
        item["decision"].get("speaker_id") for item in overlaps
        if item["anchor"] and item["decision"].get("speaker_id")
    }
    if anchor_ids and selected not in anchor_ids:
        return False, "micro_conflicting_sole_direct_anchor"
    return True, "micro_local_phrase_consensus"


def build_candidate(direct, posterior_metadata, piece_evidence, phrases,
                    phrase_evidence, frames, frame_sec, manifest, policy):
    if posterior_metadata["asr"] != phrases["asr"]:
        raise ValueError("posterior and phrase ASR sources differ")
    if posterior_metadata["align"] != phrases["align"]:
        raise ValueError("posterior and phrase alignment sources differ")
    if abs(frame_sec - policy["minimum_micro_run_sec"]) > 1e-6:
        raise ValueError("micro-run floor does not equal one native frame")

    evaluated = posterior.evaluate_pieces(
        direct, posterior_metadata, piece_evidence, manifest, policy)
    overlays = defaultdict(list)
    for text_id, values in evaluated["overlays_by_text"].items():
        overlays[text_id].extend(values)

    phrase_decisions = build_phrase_decisions(
        phrases, phrase_evidence, evaluated["active_ids"],
        evaluated["fragments_by_text"], policy)
    runs = global_active_runs(frames, frame_sec, policy)
    corrections = []
    for decision in evaluated["decisions"]:
        text_id = int(decision["text_id"])
        phrase = phrase_decisions[decision["phrase_evidence_id"]]
        overlaps = posterior.overlapping_fragments(
            decision, evaluated["fragments_by_text"][text_id])
        run = global_run_for_piece(decision, runs)
        accepted, reason = correction_for_piece(
            decision, phrase, overlaps, run, policy)
        if accepted:
            add_overlay(overlays, decision, decision["speaker_id"], reason)
        corrections.append({
            "evidence_id": decision["evidence_id"],
            "phrase_evidence_id": decision["phrase_evidence_id"],
            "text_id": text_id,
            "source_start": decision["source_start"],
            "source_end": decision["source_end"],
            "start": decision["start"],
            "end": decision["end"],
            "text": decision["text"],
            "mapped_speaker_id": decision["mapped_speaker_id"],
            "piece_speaker_id": decision["speaker_id"],
            "phrase_speaker_id": phrase["speaker_id"],
            "direct_anchor_ids": phrase["direct_anchor_ids"],
            "global_run_start": run["start"] if run is not None else None,
            "global_run_end": run["end"] if run is not None else None,
            "accepted": accepted,
            "reason": reason,
        })

    micro_pieces = enumerate_micro_pieces(
        phrases, frames, frame_sec, policy)
    micro_decisions = []
    for piece in micro_pieces:
        text_id = int(piece["text_id"])
        local = int(piece["local_speaker"])
        if local not in evaluated["local_to_id"]:
            raise ValueError(f"no identity mapping for micro local {local}")
        mapped_id = evaluated["local_to_id"][local]
        phrase = phrase_decisions[piece["phrase_evidence_id"]]
        overlaps = posterior.overlapping_fragments(
            piece, evaluated["fragments_by_text"][text_id])
        accepted, reason = micro_decision(
            piece, phrase, mapped_id, overlaps, policy)
        if accepted:
            add_overlay(overlays, piece, mapped_id, reason)
        micro_decisions.append({
            **piece,
            "mapped_speaker_id": mapped_id,
            "phrase_speaker_id": phrase["speaker_id"],
            "direct_anchor_ids": phrase["direct_anchor_ids"],
            "accepted": accepted,
            "reason": reason,
        })

    track = posterior.project_track(
        posterior_metadata, evaluated["fragments_by_text"], overlays,
        evaluated["id_to_local"])
    return {
        "schema_version": 1,
        "kind": "orator_frozen_speaker_candidate",
        "candidate_kind": "v21_multiresolution_phrase_consensus",
        "audio_sec": float(direct["audio_sec"]),
        "sample_rate": int(direct["sample_rate"]),
        "active_speaker_ids": evaluated["active_ids"],
        "source_turn_count": len(direct.get("track", [])),
        "turn_count": len(track),
        "posterior_piece_count": len(evaluated["decisions"]),
        "inherited_piece_count": sum(
            item["accepted"] for item in evaluated["decisions"]),
        "correction_count": sum(item["accepted"] for item in corrections),
        "micro_piece_count": len(micro_decisions),
        "accepted_micro_piece_count": sum(
            item["accepted"] for item in micro_decisions),
        "policy": policy,
        "posterior_decisions": evaluated["decisions"],
        "phrase_decisions": list(phrase_decisions.values()),
        "correction_decisions": corrections,
        "micro_decisions": micro_decisions,
        "track": track,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--direct", required=True)
    parser.add_argument("--posterior-metadata", required=True)
    parser.add_argument("--piece-titanet", required=True)
    parser.add_argument("--phrases", required=True)
    parser.add_argument("--phrase-titanet", required=True)
    parser.add_argument("--frames", required=True)
    parser.add_argument("--manifest", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()

    frames, frame_sec = posterior.read_frames(args.frames)
    result = build_candidate(
        posterior.load_json(args.direct),
        posterior.load_json(args.posterior_metadata),
        phrase_tools.read_titanet(args.piece_titanet),
        posterior.load_json(args.phrases),
        phrase_tools.read_titanet(args.phrase_titanet),
        frames, frame_sec, posterior.load_json(args.manifest),
        load_policy(args.config))
    result["sources"] = {
        name: {"path": os.path.abspath(path),
               "sha256": posterior.sha256_file(path)}
        for name, path in {
            "direct": args.direct,
            "posterior_metadata": args.posterior_metadata,
            "piece_titanet": args.piece_titanet,
            "phrases": args.phrases,
            "phrase_titanet": args.phrase_titanet,
            "frames": args.frames,
            "manifest": args.manifest,
            "config": args.config,
        }.items()
    }
    with open(args.out, "w", encoding="utf-8") as output:
        json.dump(result, output, ensure_ascii=False, indent=2)
    print(json.dumps({
        "posterior_pieces": result["posterior_piece_count"],
        "corrections": result["correction_count"],
        "micro_pieces": result["micro_piece_count"],
        "accepted_micro_pieces": result["accepted_micro_piece_count"],
        "turns": result["turn_count"],
        "out": os.path.abspath(args.out),
    }, ensure_ascii=False))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError) as error:
        raise SystemExit(f"speaker multiresolution phrase: {error}")
