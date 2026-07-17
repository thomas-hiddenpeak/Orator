#!/usr/bin/env python3
"""Build robust clean-gallery scores from frozen normalized embeddings."""

import argparse
import csv
import hashlib
import json
import math
import os
import tomllib
from collections import defaultdict


EPSILON = 1e-9


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
    section = document.get("robust_gallery", {})
    if section.get("aggregation") != "top_half_mean":
        raise ValueError("robust gallery aggregation must be top_half_mean")
    if section.get("require_complete_gallery") is not True:
        raise ValueError("robust gallery must require a complete gallery")
    return {
        "aggregation": "top_half_mean",
        "require_complete_gallery": True,
    }


def read_embeddings(path):
    values = {}
    with open(path, encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source, delimiter="\t")
        fields = list(reader.fieldnames or [])
        embedding_fields = [name for name in fields if name.startswith("emb_")]
        required = {
            "evidence_id", "start_sec", "end_sec", "duration_sec", "status",
            "embed_start_sec", "embed_end_sec",
        }
        if not required.issubset(fields) or not embedding_fields:
            raise ValueError("embedding table schema is incomplete")
        for row in reader:
            evidence_id = row["evidence_id"]
            if evidence_id in values:
                raise ValueError(f"duplicate embedding {evidence_id}")
            vector = (
                [float(row[name]) for name in embedding_fields]
                if row["status"] == "ok" else [])
            if vector:
                norm = math.sqrt(sum(value * value for value in vector))
                if abs(norm - 1.0) > 1e-3:
                    raise ValueError(f"embedding is not normalized: {evidence_id}")
            values[evidence_id] = {
                "evidence_id": evidence_id,
                "start_sec": float(row["start_sec"]),
                "end_sec": float(row["end_sec"]),
                "duration_sec": float(row["duration_sec"]),
                "status": row["status"],
                "embed_start_sec": (
                    float(row["embed_start_sec"])
                    if row["embed_start_sec"] else None),
                "embed_end_sec": (
                    float(row["embed_end_sec"])
                    if row["embed_end_sec"] else None),
                "embedding": vector,
            }
    return values


def load_gallery(metadata, embeddings):
    gallery = defaultdict(list)
    for record in metadata.get("prototypes", []):
        evidence_id = str(record["evidence_id"])
        value = embeddings.get(evidence_id)
        if value is None or value["status"] != "ok":
            raise ValueError(f"missing clean prototype {evidence_id}")
        if (abs(value["start_sec"] - float(record["start"])) > 1e-6 or
                abs(value["end_sec"] - float(record["end"])) > 1e-6):
            raise ValueError(f"prototype interval differs: {evidence_id}")
        gallery[str(record["identity"])].append(value["embedding"])
    active_ids = [str(value) for value in metadata["active_speaker_ids"]]
    if sorted(gallery) != sorted(active_ids):
        raise ValueError("clean gallery identity set is incomplete")
    counts = {len(gallery[speaker_id]) for speaker_id in active_ids}
    if len(counts) != 1:
        raise ValueError("clean gallery prototype counts differ")
    count = next(iter(counts), 0)
    if count < 2 or count % 2 != 0:
        raise ValueError("top-half aggregation requires an even complete gallery")
    return active_ids, dict(gallery)


def dot(left, right):
    if len(left) != len(right):
        raise ValueError("embedding dimensions differ")
    return sum(a * b for a, b in zip(left, right))


def top_half_mean(query, prototypes):
    if len(prototypes) < 2 or len(prototypes) % 2 != 0:
        raise ValueError("top-half aggregation requires an even prototype count")
    scores = sorted((dot(query, value) for value in prototypes), reverse=True)
    keep = len(scores) // 2
    return sum(scores[:keep]) / keep


def score_query(query, gallery, active_ids):
    if query["status"] != "ok":
        return {}
    return {
        speaker_id: top_half_mean(query["embedding"], gallery[speaker_id])
        for speaker_id in active_ids
    }


def write_evidence(path, queries, gallery, active_ids):
    score_fields = [f"score_{speaker_id}" for speaker_id in active_ids]
    fields = [
        "evidence_id", "start_sec", "end_sec", "duration_sec", "status",
        "embed_start_sec", "embed_end_sec", "best_id", "best_score",
        "second_id", "second_score", "margin", *score_fields,
    ]
    with open(path, "w", encoding="utf-8", newline="") as output:
        writer = csv.DictWriter(
            output, fieldnames=fields, delimiter="\t", lineterminator="\n")
        writer.writeheader()
        for query in queries.values():
            scores = score_query(query, gallery, active_ids)
            ranked = sorted(scores.items(), key=lambda item: (-item[1], item[0]))
            row = {
                name: query[name] for name in (
                    "evidence_id", "start_sec", "end_sec", "duration_sec",
                    "status", "embed_start_sec", "embed_end_sec")
            }
            if ranked:
                row.update({
                    "best_id": ranked[0][0],
                    "best_score": ranked[0][1],
                    "second_id": ranked[1][0],
                    "second_score": ranked[1][1],
                    "margin": ranked[0][1] - ranked[1][1],
                })
                row.update({f"score_{speaker_id}": scores[speaker_id]
                            for speaker_id in active_ids})
            writer.writerow(row)


def build(metadata_path, prototype_path, query_path, config_path, output_path,
          manifest_path):
    policy = load_policy(config_path)
    metadata = load_json(metadata_path)
    if metadata.get("kind") != "orator_clean_speaker_gallery_spans":
        raise ValueError("metadata is not a clean speaker gallery")
    prototypes = read_embeddings(prototype_path)
    queries = read_embeddings(query_path)
    active_ids, gallery = load_gallery(metadata, prototypes)
    dimensions = {
        len(value) for values in gallery.values() for value in values
    }
    if len(dimensions) != 1:
        raise ValueError("prototype embedding dimensions differ")
    for query in queries.values():
        if query["embedding"] and len(query["embedding"]) not in dimensions:
            raise ValueError(f"query embedding dimension differs: {query['evidence_id']}")
    write_evidence(output_path, queries, gallery, active_ids)
    manifest = {
        "schema_version": 1,
        "kind": "orator_robust_clean_gallery_evidence",
        "policy": policy,
        "active_speaker_ids": active_ids,
        "prototype_count_per_identity": len(gallery[active_ids[0]]),
        "retained_count_per_identity": len(gallery[active_ids[0]]) // 2,
        "query_count": len(queries),
        "sources": {
            name: {"path": os.path.abspath(path),
                   "sha256": sha256_file(path)}
            for name, path in {
                "metadata": metadata_path,
                "prototype_embeddings": prototype_path,
                "query_embeddings": query_path,
                "config": config_path,
            }.items()
        },
        "output": {"path": os.path.abspath(output_path),
                   "sha256": sha256_file(output_path)},
    }
    with open(manifest_path, "w", encoding="utf-8") as output:
        json.dump(manifest, output, ensure_ascii=False, indent=2)
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--metadata", required=True)
    parser.add_argument("--prototype-embeddings", required=True)
    parser.add_argument("--query-embeddings", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--manifest", required=True)
    args = parser.parse_args()
    result = build(
        args.metadata, args.prototype_embeddings, args.query_embeddings,
        args.config, args.out, args.manifest)
    print(json.dumps({
        "query_count": result["query_count"],
        "prototype_count_per_identity": result[
            "prototype_count_per_identity"],
        "retained_count_per_identity": result[
            "retained_count_per_identity"],
        "out": result["output"]["path"],
        "manifest": os.path.abspath(args.manifest),
    }, ensure_ascii=False))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, csv.Error) as error:
        raise SystemExit(f"speaker robust gallery evidence: {error}")
