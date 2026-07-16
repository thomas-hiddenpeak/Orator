#!/usr/bin/env python3
"""Preserve authoritative raw-diar business ranges over enhanced fusion."""

import argparse
import json
import os
import tomllib
from collections import defaultdict

import speaker_posterior_bounded_phrase_candidate as posterior
import speaker_punctuation_phrase_candidate as phrase_tools


def load_policy(path):
    with open(path, "rb") as source:
        document = tomllib.load(source)
    section = document.get("raw_authoritative", {})
    required = {"require_known_identity", "allowed_reasons"}
    missing = sorted(required - set(section))
    if missing:
        raise ValueError("raw-authoritative policy missing: " + ",".join(missing))
    require_known = bool(section["require_known_identity"])
    reasons = [str(value) for value in section["allowed_reasons"]]
    if not require_known:
        raise ValueError("known identity is mandatory for raw authority")
    if not reasons or len(reasons) != len(set(reasons)):
        raise ValueError("allowed raw-authority reasons must be unique and nonempty")
    return {
        "require_known_identity": require_known,
        "allowed_reasons": reasons,
    }


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
    if set(fragments) != {int(value) for value in metadata["asr"]}:
        raise ValueError(f"{label} text ID set differs from metadata")
    for text_id, values in fragments.items():
        actual = "".join(str(value["entry"].get("text", ""))
                         for value in values)
        expected = metadata["asr"][str(text_id)]["text"]
        if actual != expected:
            raise ValueError(f"{label} source text mismatch for {text_id}")


def authoritative_overlays(raw_fragments, policy, active_ids):
    allowed = set(policy["allowed_reasons"])
    overlays = defaultdict(list)
    audit = []
    for text_id in sorted(raw_fragments):
        for fragment in raw_fragments[text_id]:
            entry = fragment["entry"]
            speaker_id = entry.get("speaker_id")
            reason = str(entry.get("decision_reason", ""))
            if reason not in allowed:
                accepted = False
                audit_reason = "raw_reason_not_authoritative"
            elif speaker_id is None:
                accepted = False
                audit_reason = "raw_identity_missing"
            elif speaker_id not in active_ids:
                raise ValueError(f"raw identity {speaker_id} is not active")
            else:
                accepted = True
                audit_reason = "raw_authoritative_range"
            record = {
                "text_id": text_id,
                "source_start": fragment["source_start"],
                "source_end": fragment["source_end"],
                "speaker_id": speaker_id,
                "raw_reason": reason,
                "accepted": accepted,
                "reason": audit_reason,
            }
            audit.append(record)
            if accepted:
                overlays[text_id].append({
                    "text_id": text_id,
                    "source_start": fragment["source_start"],
                    "source_end": fragment["source_end"],
                    "speaker_id": speaker_id,
                    "reason": "raw_authoritative_sole_diar_support",
                })
    return dict(overlays), audit


def build_candidate(raw, enhanced, metadata, manifest, policy):
    if raw.get("candidate_kind") != "production_business_speaker_replay":
        raise ValueError("raw input is not a production business replay")
    if enhanced.get("kind") != "orator_frozen_speaker_candidate":
        raise ValueError("enhanced input is not a frozen speaker candidate")
    active_ids = [str(value) for value in enhanced["active_speaker_ids"]]
    mapping = {
        int(local): str(speaker_id)
        for local, speaker_id in manifest["mapping"].items()
    }
    if sorted(mapping.values()) != sorted(active_ids):
        raise ValueError("manifest identity set differs from enhanced candidate")
    id_to_local = {speaker_id: local for local, speaker_id in mapping.items()}
    raw_fragments = source_fragments(raw.get("track", []))
    enhanced_fragments = source_fragments(enhanced.get("track", []))
    validate_source_text(raw_fragments, metadata, "raw")
    validate_source_text(enhanced_fragments, metadata, "enhanced")
    overlays, audit = authoritative_overlays(
        raw_fragments, policy, set(active_ids))

    track = []
    for text_id in sorted(enhanced_fragments):
        source = metadata["asr"][str(text_id)]["text"]
        track.extend(phrase_tools.reproject_text(
            text_id, source, metadata["align"][str(text_id)],
            enhanced_fragments[text_id], overlays.get(text_id, []),
            id_to_local))
    validate_source_text(source_fragments(track), metadata, "projected")
    return {
        "schema_version": 1,
        "kind": "orator_frozen_speaker_candidate",
        "candidate_kind": "v21_raw_authoritative_fusion",
        "audio_sec": float(enhanced["audio_sec"]),
        "sample_rate": int(enhanced["sample_rate"]),
        "active_speaker_ids": active_ids,
        "source_turn_count": len(enhanced.get("track", [])),
        "turn_count": len(track),
        "authoritative_range_count": sum(
            item["accepted"] for item in audit),
        "policy": policy,
        "authority_decisions": audit,
        "track": track,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--raw", required=True)
    parser.add_argument("--enhanced", required=True)
    parser.add_argument("--posterior-metadata", required=True)
    parser.add_argument("--manifest", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()
    result = build_candidate(
        posterior.load_json(args.raw),
        posterior.load_json(args.enhanced),
        posterior.load_json(args.posterior_metadata),
        posterior.load_json(args.manifest),
        load_policy(args.config))
    result["sources"] = {
        name: {
            "path": os.path.abspath(path),
            "sha256": posterior.sha256_file(path),
        }
        for name, path in {
            "raw": args.raw,
            "enhanced": args.enhanced,
            "posterior_metadata": args.posterior_metadata,
            "manifest": args.manifest,
            "config": args.config,
        }.items()
    }
    with open(args.out, "w", encoding="utf-8") as output:
        json.dump(result, output, ensure_ascii=False, indent=2)
    print(json.dumps({
        "authoritative_ranges": result["authoritative_range_count"],
        "turns": result["turn_count"],
        "out": os.path.abspath(args.out),
    }, ensure_ascii=False))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError) as error:
        raise SystemExit(f"speaker raw-authoritative candidate: {error}")
