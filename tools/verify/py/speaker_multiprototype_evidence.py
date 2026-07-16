#!/usr/bin/env python3
"""Fuse same-identity TitaNet prototypes without evaluating a result."""

import argparse
import csv
import hashlib
import json
import os
import tomllib


METADATA_FIELDS = (
    "evidence_id", "start_sec", "end_sec", "duration_sec", "status",
    "embed_start_sec", "embed_end_sec",
)
DECISION_FIELDS = {
    "best_id", "best_score", "second_id", "second_score", "margin",
}


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_policy(path):
    with open(path, "rb") as source:
        document = tomllib.load(source)
    section = document.get("speaker_prototype_fusion", {})
    if section.get("aggregation") != "maximum":
        raise ValueError("speaker prototype aggregation must be maximum")
    if section.get("require_complete_gallery") is not True:
        raise ValueError("complete prototype galleries must be required")
    return {
        "aggregation": "maximum",
        "require_complete_gallery": True,
    }


def read_evidence(path):
    with open(path, encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source, delimiter="\t")
        fields = list(reader.fieldnames or [])
        missing = [name for name in METADATA_FIELDS if name not in fields]
        if missing:
            raise ValueError("evidence missing fields: " + ",".join(missing))
        score_fields = [name for name in fields if name.startswith("score_")]
        if not score_fields:
            raise ValueError("evidence has no identity score fields")
        rows = list(reader)
    return fields, score_fields, rows


def same_metadata(left, right, field):
    if field in {"evidence_id", "status"}:
        return left.get(field, "") == right.get(field, "")
    left_value = left.get(field, "")
    right_value = right.get(field, "")
    if left_value == "" or right_value == "":
        return left_value == right_value
    return abs(float(left_value) - float(right_value)) <= 1e-6


def fuse_rows(primary, secondary, score_fields):
    if len(primary) != len(secondary):
        raise ValueError("prototype evidence row counts differ")
    output = []
    for index, (left, right) in enumerate(zip(primary, secondary)):
        for field in METADATA_FIELDS:
            if not same_metadata(left, right, field):
                raise ValueError(
                    f"prototype evidence metadata mismatch at row {index}: "
                    f"{field}")
        row = dict(left)
        scores = {}
        for field in score_fields:
            left_value = left.get(field, "")
            right_value = right.get(field, "")
            if (left_value == "") != (right_value == ""):
                raise ValueError(
                    f"incomplete prototype gallery at row {index}: {field}")
            if left_value != "":
                scores[field] = max(float(left_value), float(right_value))
                row[field] = f"{scores[field]:.9f}"
        if row["status"] == "ok" and len(scores) < 2:
            raise ValueError(f"incomplete identity scores at row {index}")
        ranked = sorted(scores.items(), key=lambda item: (-item[1], item[0]))
        if ranked:
            row["best_id"] = ranked[0][0].removeprefix("score_")
            row["best_score"] = f"{ranked[0][1]:.9f}"
            row["second_id"] = (
                ranked[1][0].removeprefix("score_")
                if len(ranked) > 1 else "")
            row["second_score"] = (
                f"{ranked[1][1]:.9f}" if len(ranked) > 1 else "")
            row["margin"] = (
                f"{ranked[0][1] - ranked[1][1]:.9f}"
                if len(ranked) > 1 else "")
        else:
            for field in DECISION_FIELDS:
                row[field] = ""
        output.append(row)
    return output


def fuse(primary_path, secondary_path, config_path, output_path,
         manifest_path):
    policy = load_policy(config_path)
    fields, score_fields, primary = read_evidence(primary_path)
    secondary_fields, secondary_scores, secondary = read_evidence(
        secondary_path)
    if fields != secondary_fields or score_fields != secondary_scores:
        raise ValueError("prototype evidence schemas differ")
    rows = fuse_rows(primary, secondary, score_fields)
    with open(output_path, "w", encoding="utf-8", newline="") as output:
        writer = csv.DictWriter(
            output, fieldnames=fields, delimiter="\t", lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)
    manifest = {
        "schema_version": 1,
        "kind": "orator_speaker_multiprototype_evidence",
        "policy": policy,
        "evidence_count": len(rows),
        "identity_score_fields": score_fields,
        "sources": {
            "primary": {"path": os.path.abspath(primary_path),
                        "sha256": sha256_file(primary_path)},
            "secondary": {"path": os.path.abspath(secondary_path),
                          "sha256": sha256_file(secondary_path)},
            "config": {"path": os.path.abspath(config_path),
                       "sha256": sha256_file(config_path)},
        },
        "output": {"path": os.path.abspath(output_path),
                   "sha256": sha256_file(output_path)},
    }
    with open(manifest_path, "w", encoding="utf-8") as output:
        json.dump(manifest, output, ensure_ascii=False, indent=2)
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--primary", required=True)
    parser.add_argument("--secondary", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--manifest", required=True)
    args = parser.parse_args()
    result = fuse(args.primary, args.secondary, args.config, args.out,
                  args.manifest)
    print(json.dumps({
        "evidence_count": result["evidence_count"],
        "identity_score_fields": result["identity_score_fields"],
        "out": result["output"]["path"],
        "manifest": os.path.abspath(args.manifest),
    }, ensure_ascii=False))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, csv.Error) as error:
        raise SystemExit(f"speaker multiprototype evidence: {error}")
