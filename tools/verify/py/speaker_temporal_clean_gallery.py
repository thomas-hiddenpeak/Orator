#!/usr/bin/env python3
"""Select clean speaker prototypes across the full session clock."""

import argparse
import csv
from collections import defaultdict
import json
import os
import tomllib

import speaker_clean_gallery_candidate as clean
import speaker_posterior_bounded_phrase_candidate as posterior


def load_policy(path):
    policy = clean.load_policy(path)
    with open(path, "rb") as source:
        document = tomllib.load(source)
    selection = document.get("speaker_gallery", {}).get("selection")
    if selection != "temporal_quality_strata":
        raise ValueError("temporal gallery selection is not configured")
    policy["selection"] = selection
    return policy


def temporal_quality_strata(values, count):
    ordered = sorted(values, key=lambda item: (
        item["start"], item["end"], item["evidence_id"]))
    if len(ordered) < count:
        raise ValueError("temporal gallery has too few eligible prototypes")
    selected = []
    for index in range(count):
        first = index * len(ordered) // count
        limit = (index + 1) * len(ordered) // count
        stratum = ordered[first:limit]
        if not stratum:
            raise ValueError("temporal gallery stratum is empty")
        selected.append(min(stratum, key=lambda item: (
            -item["quality"], item["start"], item["evidence_id"])))
    return selected


def select_prototypes(segments, initial_scores, terminal_scores, active_ids,
                      policy):
    eligible = defaultdict(list)
    audit = []
    for segment in segments:
        evidence_id = segment["evidence_id"]
        if evidence_id not in initial_scores or evidence_id not in terminal_scores:
            raise ValueError(f"missing prototype score evidence {evidence_id}")
        record, reason = clean.prototype_record(
            segment, initial_scores[evidence_id], terminal_scores[evidence_id],
            active_ids, policy, segments)
        audit.append({
            "evidence_id": evidence_id,
            "eligible": record is not None,
            "selected": False,
            "reason": reason,
        })
        if record is not None:
            eligible[record["identity"]].append(record)

    selected = []
    selected_ids = set()
    count = policy["max_prototypes_per_identity"]
    for speaker_id in active_ids:
        kept = temporal_quality_strata(eligible[speaker_id], count)
        selected.extend(kept)
        selected_ids.update(value["evidence_id"] for value in kept)
    for value in audit:
        if value["evidence_id"] in selected_ids:
            value["selected"] = True
            value["reason"] = "temporal_quality_stratum_selected"
    return selected, audit


def command_spans(args):
    policy = load_policy(args.config)
    manifest = posterior.load_json(args.manifest)
    mapping = {
        int(local): str(identity)
        for local, identity in manifest["mapping"].items()
    }
    active_ids = sorted(mapping.values())
    selected, audit = select_prototypes(
        clean.read_diar(args.diar, mapping),
        clean.read_score_evidence(args.initial_titanet),
        clean.read_score_evidence(args.terminal_titanet), active_ids, policy)
    clean.write_spans(args.out, selected)
    result = {
        "schema_version": 1,
        "kind": "orator_clean_speaker_gallery_spans",
        "selection": "temporal_quality_strata",
        "active_speaker_ids": active_ids,
        "policy": policy,
        "prototype_count": len(selected),
        "prototypes": selected,
        "eligibility_audit": audit,
        "sources": {
            name: {"path": os.path.abspath(path),
                   "sha256": posterior.sha256_file(path)}
            for name, path in {
                "diar": args.diar,
                "initial_titanet": args.initial_titanet,
                "terminal_titanet": args.terminal_titanet,
                "manifest": args.manifest,
                "config": args.config,
            }.items()
        },
    }
    with open(args.metadata, "w", encoding="utf-8") as output:
        json.dump(result, output, ensure_ascii=False, indent=2)
    print(json.dumps({
        "prototypes": len(selected), "out": os.path.abspath(args.out),
        "metadata": os.path.abspath(args.metadata)}, ensure_ascii=False))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--diar", required=True)
    parser.add_argument("--initial-titanet", required=True)
    parser.add_argument("--terminal-titanet", required=True)
    parser.add_argument("--manifest", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--metadata", required=True)
    command_spans(parser.parse_args())


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, csv.Error) as error:
        raise SystemExit(f"speaker temporal clean gallery: {error}")
