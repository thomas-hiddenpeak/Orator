#!/usr/bin/env python3
"""Compose exact complete-phrase cross-prototype challenges."""

import argparse
import hashlib
import json
import os
import tomllib
from collections import defaultdict

import speaker_reviewed_overlay_candidate as overlay_tools


DECISION_REASON = "complete_phrase_cross_prototype_margin_veto"


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
    section = document.get("complete_phrase_cross_prototype", {})
    required = {
        "enabled", "required_candidate_kind", "required_decision_reason",
        "require_accepted_challenge", "require_exact_punctuation_phrase",
        "require_identity_parity", "require_uniform_known_baseline_conflict",
        "reject_conflicting_overlays", "protected_decision_reasons",
    }
    missing = sorted(required - set(section))
    if missing:
        raise ValueError("complete-phrase policy missing: " + ",".join(missing))
    safeguards = [
        section["enabled"], section["require_accepted_challenge"],
        section["require_exact_punctuation_phrase"],
        section["require_identity_parity"],
        section["require_uniform_known_baseline_conflict"],
        section["reject_conflicting_overlays"],
    ]
    if any(value is not True for value in safeguards):
        raise ValueError("complete-phrase safeguards are mandatory")
    candidate_kind = str(section["required_candidate_kind"])
    decision_reason = str(section["required_decision_reason"])
    if not candidate_kind or not decision_reason:
        raise ValueError("complete-phrase provenance must be non-empty")
    protected = section["protected_decision_reasons"]
    if not isinstance(protected, list) or not protected:
        raise ValueError("protected decision reasons must be non-empty")
    protected = [str(value) for value in protected]
    if any(not value for value in protected) or len(set(protected)) != len(protected):
        raise ValueError("protected decision reasons are invalid")
    return {
        "required_candidate_kind": candidate_kind,
        "required_decision_reason": decision_reason,
        "protected_decision_reasons": protected,
    }


def read_mapping(document):
    raw = document.get("mapping")
    if not isinstance(raw, dict) or not raw:
        raise ValueError("mapping document has no local-slot mapping")
    mapping = {int(local): str(identity) for local, identity in raw.items()}
    if len(mapping) != len(set(mapping.values())):
        raise ValueError("mapping identities are not one-to-one")
    return mapping


def phrase_keys(metadata):
    if metadata.get("kind") != "orator_punctuation_phrase_spans":
        raise ValueError("metadata is not punctuation phrase evidence")
    keys = set()
    for phrase in metadata.get("phrases", []):
        key = (int(phrase["text_id"]), int(phrase["source_start"]),
               int(phrase["source_end"]))
        if key in keys:
            raise ValueError("duplicate punctuation phrase range")
        keys.add(key)
    return keys


def baseline_labels(fragments, source):
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


def collect_overlays(challenger, phrases, labels, protected, policy):
    if challenger.get("candidate_kind") != policy["required_candidate_kind"]:
        raise ValueError("unexpected cross-prototype candidate kind")
    overlays = defaultdict(list)
    decisions = []
    for challenge in challenger.get("challenge_decisions", []):
        if challenge.get("accepted") is not True:
            continue
        if str(challenge.get("reason")) != policy["required_decision_reason"]:
            continue
        text_id = int(challenge["text_id"])
        start = int(challenge["source_start"])
        end = int(challenge["source_end"])
        if (text_id, start, end) not in phrases:
            continue
        identities = {
            str(challenge.get("initial_speaker_id", "")),
            str(challenge.get("source_speaker_id", "")),
            str(challenge.get("mapped_speaker_id", "")),
        }
        if len(identities) != 1 or "" in identities:
            raise ValueError("cross-prototype challenge identity parity failed")
        selected = next(iter(identities))
        if str(challenge.get("terminal_speaker_id", "")) == selected:
            raise ValueError("cross-prototype terminal identity does not differ")
        range_labels = labels[text_id][start:end]
        baseline_ids = {value["speaker_id"] for value in range_labels}
        if len(baseline_ids) != 1 or None in baseline_ids:
            continue
        baseline_id = next(iter(baseline_ids))
        if baseline_id == selected:
            continue
        if any(value["reason"] in protected for value in range_labels):
            continue
        item = {
            "text_id": text_id,
            "source_start": start,
            "source_end": end,
            "speaker_id": selected,
            "baseline_speaker_id": baseline_id,
            "reason": DECISION_REASON,
            "initial_evidence_id": str(challenge["initial_evidence_id"]),
            "local_evidence_id": str(challenge["local_evidence_id"]),
            "phrase_evidence_id": str(challenge["phrase_evidence_id"]),
        }
        overlays[text_id].append(item)
        decisions.append(dict(item))
    overlay_tools.validate_overlays(overlays)
    return dict(overlays), decisions


def build_candidate(baseline, challenger, metadata, mapping_document, policy):
    mapping = read_mapping(mapping_document)
    id_to_local = {identity: local for local, identity in mapping.items()}
    active_ids = sorted(id_to_local)
    if sorted(baseline.get("active_speaker_ids", [])) != active_ids:
        raise ValueError("baseline identities differ from mapping")
    fragments = overlay_tools.candidate_fragments(baseline)
    overlay_tools.validate_candidate_sources(fragments, metadata)
    labels = {
        text_id: baseline_labels(
            values, str(metadata["asr"][str(text_id)]["text"]))
        for text_id, values in fragments.items()
    }
    overlays, decisions = collect_overlays(
        challenger, phrase_keys(metadata), labels,
        set(policy["protected_decision_reasons"]), policy)
    track = []
    for text_id in sorted(fragments):
        source = str(metadata["asr"][str(text_id)]["text"])
        track.extend(overlay_tools.reproject_preserving_base(
            text_id, source, metadata["align"][str(text_id)],
            fragments[text_id], overlays.get(text_id, []), id_to_local))
    return {
        "schema_version": 1,
        "kind": "orator_frozen_speaker_candidate",
        "candidate_kind": "v21_complete_phrase_cross_prototype",
        "audio_sec": float(baseline["audio_sec"]),
        "sample_rate": int(baseline["sample_rate"]),
        "active_speaker_ids": active_ids,
        "source_turn_count": len(baseline.get("track", [])),
        "turn_count": len(track),
        "overlay_count": len(decisions),
        "policy": policy,
        "overlay_decisions": decisions,
        "track": track,
    }


def command_build(args):
    result = build_candidate(
        load_json(args.baseline), load_json(args.challenger),
        load_json(args.metadata), load_json(args.mapping),
        load_policy(args.config))
    result["sources"] = {
        name: {"path": os.path.abspath(path), "sha256": sha256_file(path)}
        for name, path in {
            "baseline": args.baseline,
            "challenger": args.challenger,
            "metadata": args.metadata,
            "mapping": args.mapping,
            "config": args.config,
        }.items()
    }
    with open(args.out, "w", encoding="utf-8") as output:
        json.dump(result, output, ensure_ascii=False, indent=2)
    print(json.dumps({
        "turns": result["turn_count"],
        "overlays": result["overlay_count"],
        "out": os.path.abspath(args.out),
    }, ensure_ascii=False))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", required=True)
    parser.add_argument("--challenger", required=True)
    parser.add_argument("--metadata", required=True)
    parser.add_argument("--mapping", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--out", required=True)
    command_build(parser.parse_args())


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as error:
        raise SystemExit(f"speaker complete-phrase cross-prototype: {error}")
