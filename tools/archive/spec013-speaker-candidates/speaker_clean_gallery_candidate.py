#!/usr/bin/env python3
"""Build a reference-free clean multi-prototype speaker gallery candidate."""

import argparse
import csv
import json
import math
import os
import tomllib
from collections import defaultdict

import speaker_posterior_bounded_phrase_candidate as posterior
import speaker_replay_voiceprint_candidate as direct_tools


EPSILON = 1e-9


def load_policy(path):
    with open(path, "rb") as source:
        document = tomllib.load(source)
    gallery = document.get("speaker_gallery", {})
    fusion = document.get("speaker_fusion", {})
    gallery_required = {
        "minimum_prototype_duration_sec", "minimum_diar_confidence",
        "overlap_epsilon_sec", "prototype_min_score",
        "prototype_min_margin", "max_prototypes_per_identity",
        "aggregation", "require_initial_terminal_agreement",
        "require_raw_local_identity_agreement", "require_complete_gallery",
    }
    fusion_required = {
        "short_max_sec", "short_min_score", "short_min_margin",
        "regular_min_score", "regular_min_margin",
    }
    missing = sorted(
        (gallery_required - set(gallery)) | (fusion_required - set(fusion)))
    if missing:
        raise ValueError("clean gallery policy missing: " + ",".join(missing))
    policy = {
        **{name: float(gallery[name]) for name in (
            "minimum_prototype_duration_sec", "minimum_diar_confidence",
            "overlap_epsilon_sec", "prototype_min_score",
            "prototype_min_margin")},
        "max_prototypes_per_identity": int(
            gallery["max_prototypes_per_identity"]),
        "aggregation": str(gallery["aggregation"]),
        **{name: bool(gallery[name]) for name in (
            "require_initial_terminal_agreement",
            "require_raw_local_identity_agreement",
            "require_complete_gallery")},
        **{name: float(fusion[name]) for name in fusion_required},
    }
    if policy["minimum_prototype_duration_sec"] <= 0.0:
        raise ValueError("prototype duration must be positive")
    if not 0.0 <= policy["minimum_diar_confidence"] <= 1.0:
        raise ValueError("diar confidence must be between zero and one")
    if policy["overlap_epsilon_sec"] < 0.0:
        raise ValueError("overlap epsilon must be non-negative")
    if policy["max_prototypes_per_identity"] < 1:
        raise ValueError("prototype cap must be positive")
    if policy["aggregation"] != "maximum":
        raise ValueError("clean gallery aggregation must be maximum")
    mandatory = (
        "require_initial_terminal_agreement",
        "require_raw_local_identity_agreement",
        "require_complete_gallery",
    )
    if not all(policy[name] for name in mandatory):
        raise ValueError("all clean gallery identity contracts are mandatory")
    return policy


def read_diar(path, mapping=None):
    values = []
    with open(path, encoding="utf-8", newline="") as source:
        delimiter = "," if str(path).endswith(".csv") else "\t"
        reader = csv.DictReader(source, delimiter=delimiter)
        required = {
            "start_sec", "end_sec", "local_speaker", "confidence",
        }
        if not required.issubset(reader.fieldnames or []):
            raise ValueError("diar TSV is missing required columns")
        for row in reader:
            local = int(row["local_speaker"])
            speaker_id = row.get("speaker_id")
            if not speaker_id and mapping is not None:
                speaker_id = mapping.get(local)
            if not speaker_id:
                raise ValueError(f"no stable identity for local speaker {local}")
            values.append({
                "start": float(row["start_sec"]),
                "end": float(row["end_sec"]),
                "local_speaker": local,
                "confidence": float(row["confidence"]),
                "speaker_id": str(speaker_id),
            })
    if str(path).endswith(".csv"):
        values.sort(key=lambda item: (
            item["start"], item["end"], item["local_speaker"]))
    for index, value in enumerate(values):
        value["evidence_id"] = f"diarization:{index}"
    return values


def read_score_evidence(path):
    values = {}
    with open(path, encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source, delimiter="\t")
        score_fields = [name for name in reader.fieldnames or []
                        if name.startswith("score_")]
        for row in reader:
            evidence_id = row["evidence_id"]
            if evidence_id in values:
                raise ValueError(f"duplicate score evidence {evidence_id}")
            values[evidence_id] = {
                "start": float(row["start_sec"]),
                "end": float(row["end_sec"]),
                "status": row["status"],
                "embed_start": (
                    float(row["embed_start_sec"])
                    if row.get("embed_start_sec") else None),
                "embed_end": (
                    float(row["embed_end_sec"])
                    if row.get("embed_end_sec") else None),
                "scores": {
                    name[6:]: float(row[name]) for name in score_fields
                    if row.get(name) not in (None, "")
                },
            }
    return values


def ranked_active(evidence, active_ids):
    scores = evidence.get("scores", {})
    if any(speaker_id not in scores for speaker_id in active_ids):
        return []
    return sorted(
        ((speaker_id, scores[speaker_id]) for speaker_id in active_ids),
        key=lambda item: (-item[1], item[0]))


def strong_identity(evidence, active_ids, policy):
    if evidence.get("status") != "ok":
        return None
    ranked = ranked_active(evidence, active_ids)
    if len(ranked) < 2:
        return None
    if ranked[0][1] + EPSILON < policy["prototype_min_score"]:
        return None
    if (ranked[0][1] - ranked[1][1] + EPSILON <
            policy["prototype_min_margin"]):
        return None
    return ranked[0][0]


def overlap_contaminated(segment, all_segments, evidence, epsilon):
    start = evidence.get("embed_start")
    end = evidence.get("embed_end")
    if start is None or end is None or end <= start:
        return True
    for other in all_segments:
        if other is segment or other["local_speaker"] == segment["local_speaker"]:
            continue
        overlap = max(0.0, min(end, other["end"]) - max(start, other["start"]))
        if overlap > epsilon + EPSILON:
            return True
    return False


def prototype_record(segment, initial, terminal, active_ids, policy,
                     all_segments):
    duration = segment["end"] - segment["start"]
    if duration + EPSILON < policy["minimum_prototype_duration_sec"]:
        return None, "duration_below_gate"
    if segment["confidence"] + EPSILON < policy["minimum_diar_confidence"]:
        return None, "confidence_below_gate"
    if (abs(initial["start"] - segment["start"]) > 1e-6 or
            abs(initial["end"] - segment["end"]) > 1e-6 or
            abs(terminal["start"] - segment["start"]) > 1e-6 or
            abs(terminal["end"] - segment["end"]) > 1e-6):
        raise ValueError("prototype score interval mismatch")
    if overlap_contaminated(
            segment, all_segments, terminal, policy["overlap_epsilon_sec"]):
        return None, "overlap_contaminated"
    initial_id = strong_identity(initial, active_ids, policy)
    terminal_id = strong_identity(terminal, active_ids, policy)
    mapped_id = segment["speaker_id"]
    if initial_id is None or terminal_id is None:
        return None, "voiceprint_gate_failed"
    if policy["require_initial_terminal_agreement"] and initial_id != terminal_id:
        return None, "registry_identity_disagreement"
    if (policy["require_raw_local_identity_agreement"] and
            (initial_id != mapped_id or terminal_id != mapped_id)):
        return None, "raw_local_identity_disagreement"
    return {
        **segment,
        "identity": mapped_id,
        "quality": segment["confidence"] * duration,
        "initial_speaker_id": initial_id,
        "terminal_speaker_id": terminal_id,
    }, "eligible"


def select_prototypes(segments, initial_scores, terminal_scores, active_ids,
                      policy):
    eligible = defaultdict(list)
    audit = []
    for segment in segments:
        evidence_id = segment["evidence_id"]
        if evidence_id not in initial_scores or evidence_id not in terminal_scores:
            raise ValueError(f"missing prototype score evidence {evidence_id}")
        record, reason = prototype_record(
            segment, initial_scores[evidence_id], terminal_scores[evidence_id],
            active_ids, policy, segments)
        audit.append({"evidence_id": evidence_id, "accepted": record is not None,
                      "reason": reason})
        if record is not None:
            eligible[record["identity"]].append(record)
    selected = []
    for speaker_id in active_ids:
        values = sorted(
            eligible[speaker_id],
            key=lambda item: (-item["quality"], item["start"],
                              item["evidence_id"]))
        kept = values[:policy["max_prototypes_per_identity"]]
        if policy["require_complete_gallery"] and len(kept) < policy[
                "max_prototypes_per_identity"]:
            raise ValueError(f"incomplete clean gallery for {speaker_id}")
        selected.extend(kept)
    return selected, audit


def write_spans(path, prototypes):
    with open(path, "w", encoding="ascii", newline="") as output:
        writer = csv.writer(output, delimiter="\t", lineterminator="\n")
        writer.writerow(["evidence_id", "start_sec", "end_sec"])
        for value in prototypes:
            writer.writerow([value["evidence_id"], value["start"], value["end"]])


def read_embeddings(path):
    values = {}
    with open(path, encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source, delimiter="\t")
        fields = [name for name in reader.fieldnames or []
                  if name.startswith("emb_")]
        if not fields:
            raise ValueError("embedding TSV has no vector columns")
        for row in reader:
            evidence_id = row["evidence_id"]
            embedding = ([float(row[name]) for name in fields]
                         if row["status"] == "ok" else [])
            if embedding:
                norm = math.sqrt(sum(value * value for value in embedding))
                if abs(norm - 1.0) > 1e-3:
                    raise ValueError(f"embedding is not normalized: {evidence_id}")
            values[evidence_id] = {
                "status": row["status"],
                "start": float(row["start_sec"]),
                "end": float(row["end_sec"]),
                "embedding": embedding,
            }
    return values


def dot(left, right):
    if len(left) != len(right):
        raise ValueError("embedding dimensions differ")
    return sum(a * b for a, b in zip(left, right))


def gallery_scores(query, gallery):
    scores = {}
    supporting = {}
    for speaker_id, prototypes in gallery.items():
        ranked = sorted(
            ((dot(query, value["embedding"]), value["evidence_id"])
             for value in prototypes),
            key=lambda item: (-item[0], item[1]))
        scores[speaker_id] = ranked[0][0]
        supporting[speaker_id] = ranked[0][1]
    return scores, supporting


def load_gallery(metadata, prototype_embeddings, active_ids, policy):
    gallery = defaultdict(list)
    for item in metadata["prototypes"]:
        evidence_id = item["evidence_id"]
        value = prototype_embeddings.get(evidence_id)
        if value is None or value["status"] != "ok":
            raise ValueError(f"missing prototype embedding {evidence_id}")
        if (abs(value["start"] - item["start"]) > 1e-6 or
                abs(value["end"] - item["end"]) > 1e-6):
            raise ValueError(f"prototype interval mismatch {evidence_id}")
        gallery[item["identity"]].append({
            "evidence_id": evidence_id,
            "embedding": value["embedding"],
        })
    expected_count = policy["max_prototypes_per_identity"]
    if any(len(gallery[speaker_id]) != expected_count
           for speaker_id in active_ids):
        raise ValueError("clean gallery prototype count differs")
    return gallery


def build_candidate(source, metadata, prototype_embeddings, query_embeddings,
                    manifest, policy):
    mapping = {int(local): str(speaker_id)
               for local, speaker_id in manifest["mapping"].items()}
    active_ids = sorted(mapping.values())
    source_active = source.get("active_speaker_ids")
    if source_active is not None and sorted(source_active) != active_ids:
        raise ValueError("manifest active identities differ")
    id_to_local = {speaker_id: local for local, speaker_id in mapping.items()}
    gallery = load_gallery(
        metadata, prototype_embeddings, active_ids, policy)

    track = []
    decisions = []
    for index, item in enumerate(source.get("track", [])):
        evidence_id = f"business_replay:{index}"
        query = query_embeddings.get(evidence_id)
        if query is None:
            raise ValueError(f"missing query embedding record {evidence_id}")
        if (abs(query["start"] - float(item["start"])) > 1e-6 or
                abs(query["end"] - float(item["end"])) > 1e-6):
            raise ValueError(f"query interval mismatch {evidence_id}")
        ranked = []
        supporting = {}
        selected = None
        voiceprint_reason = "insufficient_duration"
        if query["status"] == "ok":
            scores, supporting = gallery_scores(query["embedding"], gallery)
            evidence = {
                "status": "ok",
                "duration_sec": float(item["end"]) - float(item["start"]),
                "scores": scores,
            }
            selected, voiceprint_reason, ranked = direct_tools.select_identity(
                evidence, active_ids, policy)
        baseline_id = item.get("speaker_id")
        changed = selected is not None and selected != baseline_id
        if selected is None:
            selected = baseline_id
            reason = f"baseline_{voiceprint_reason}"
        elif changed:
            reason = f"{voiceprint_reason}_override"
        else:
            reason = f"baseline_confirmed_{voiceprint_reason}"
        output = dict(item)
        output.update({
            "speaker": id_to_local.get(selected, int(item.get("speaker", -1))),
            "speaker_id": selected,
            "speaker_uncertain": (
                bool(item.get("speaker_uncertain", False))
                if not changed else False),
            "decision_reason": reason,
        })
        track.append(output)
        decisions.append({
            "turn_id": item.get("turn_id", evidence_id),
            "evidence_id": evidence_id,
            "baseline_speaker_id": baseline_id,
            "speaker_id": selected,
            "changed": changed,
            "reason": reason,
            "ranked": [
                {"speaker_id": speaker_id, "score": score,
                 "prototype_evidence_id": supporting.get(speaker_id)}
                for speaker_id, score in ranked
            ],
        })
    return {
        "schema_version": 1,
        "kind": "orator_frozen_speaker_candidate",
        "candidate_kind": "v21_clean_gallery_direct_voiceprint",
        "audio_sec": float(source["audio_sec"]),
        "sample_rate": int(source["sample_rate"]),
        "active_speaker_ids": active_ids,
        "prototype_count": sum(len(values) for values in gallery.values()),
        "turn_count": len(track),
        "policy": policy,
        "decisions": decisions,
        "track": track,
    }


def command_spans(args):
    policy = load_policy(args.config)
    manifest = posterior.load_json(args.manifest)
    mapping = {int(local): str(value)
               for local, value in manifest["mapping"].items()}
    active_ids = sorted(mapping.values())
    selected, audit = select_prototypes(
        read_diar(args.diar, mapping), read_score_evidence(args.initial_titanet),
        read_score_evidence(args.terminal_titanet), active_ids, policy)
    write_spans(args.out, selected)
    result = {
        "schema_version": 1,
        "kind": "orator_clean_speaker_gallery_spans",
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
        "prototypes": len(selected),
        "out": os.path.abspath(args.out),
        "metadata": os.path.abspath(args.metadata),
    }, ensure_ascii=False))


def command_build(args):
    policy = load_policy(args.config)
    result = build_candidate(
        posterior.load_json(args.source), posterior.load_json(args.metadata),
        read_embeddings(args.prototype_embeddings),
        read_embeddings(args.query_embeddings),
        posterior.load_json(args.manifest), policy)
    result["sources"] = {
        name: {"path": os.path.abspath(path),
               "sha256": posterior.sha256_file(path)}
        for name, path in {
            "source": args.source,
            "metadata": args.metadata,
            "prototype_embeddings": args.prototype_embeddings,
            "query_embeddings": args.query_embeddings,
            "manifest": args.manifest,
            "config": args.config,
        }.items()
    }
    with open(args.out, "w", encoding="utf-8") as output:
        json.dump(result, output, ensure_ascii=False, indent=2)
    print(json.dumps({
        "prototypes": result["prototype_count"],
        "turns": result["turn_count"],
        "out": os.path.abspath(args.out),
    }, ensure_ascii=False))


def command_score(args):
    policy = load_policy(args.config)
    metadata = posterior.load_json(args.metadata)
    active_ids = [str(value) for value in metadata["active_speaker_ids"]]
    gallery = load_gallery(
        metadata, read_embeddings(args.prototype_embeddings),
        active_ids, policy)
    queries = read_embeddings(args.query_embeddings)
    with open(args.out, "w", encoding="utf-8", newline="") as output:
        writer = csv.writer(output, delimiter="\t", lineterminator="\n")
        writer.writerow([
            "evidence_id", "start_sec", "end_sec", "duration_sec",
            "status", "embed_start_sec", "embed_end_sec", "best_id",
            "best_score", "second_id", "second_score", "margin",
            *[f"score_{speaker_id}" for speaker_id in active_ids],
        ])
        for evidence_id, query in queries.items():
            start = query["start"]
            end = query["end"]
            if query["status"] != "ok":
                writer.writerow([
                    evidence_id, start, end, end - start, query["status"],
                    "", "", "", "", "", "", "",
                    *["" for _ in active_ids],
                ])
                continue
            scores, _ = gallery_scores(query["embedding"], gallery)
            ranked = sorted(scores.items(), key=lambda item: (-item[1], item[0]))
            writer.writerow([
                evidence_id, start, end, end - start, "ok", start, end,
                ranked[0][0], ranked[0][1], ranked[1][0], ranked[1][1],
                ranked[0][1] - ranked[1][1],
                *[scores[speaker_id] for speaker_id in active_ids],
            ])
    print(json.dumps({
        "queries": len(queries),
        "out": os.path.abspath(args.out),
    }, ensure_ascii=False))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    spans = commands.add_parser("spans")
    spans.add_argument("--diar", required=True)
    spans.add_argument("--initial-titanet", required=True)
    spans.add_argument("--terminal-titanet", required=True)
    spans.add_argument("--manifest", required=True)
    spans.add_argument("--config", required=True)
    spans.add_argument("--out", required=True)
    spans.add_argument("--metadata", required=True)
    build = commands.add_parser("build")
    build.add_argument("--source", required=True)
    build.add_argument("--metadata", required=True)
    build.add_argument("--prototype-embeddings", required=True)
    build.add_argument("--query-embeddings", required=True)
    build.add_argument("--manifest", required=True)
    build.add_argument("--config", required=True)
    build.add_argument("--out", required=True)
    score = commands.add_parser("score")
    score.add_argument("--metadata", required=True)
    score.add_argument("--prototype-embeddings", required=True)
    score.add_argument("--query-embeddings", required=True)
    score.add_argument("--config", required=True)
    score.add_argument("--out", required=True)
    args = parser.parse_args()
    if args.command == "spans":
        command_spans(args)
    elif args.command == "build":
        command_build(args)
    else:
        command_score(args)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, csv.Error) as error:
        raise SystemExit(f"speaker clean gallery candidate: {error}")
