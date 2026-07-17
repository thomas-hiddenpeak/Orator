#!/usr/bin/env python3
"""Build direct TitaNet evidence on production-replayed business intervals."""

import argparse
import csv
import hashlib
import json
import os
import tomllib


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
    values = document.get("speaker_fusion", {})
    required = {
        "short_max_sec", "short_min_score", "short_min_margin",
        "regular_min_score", "regular_min_margin",
    }
    missing = sorted(required - set(values))
    if missing:
        raise ValueError("speaker_fusion missing: " + ",".join(missing))
    return {name: float(values[name]) for name in required}


def export_spans(timeline_path, output_path):
    timeline = load_json(timeline_path)
    if timeline.get("kind") != "orator_frozen_speaker_candidate":
        raise ValueError("timeline is not a frozen speaker candidate")
    track = timeline.get("track", [])
    with open(output_path, "w", encoding="utf-8", newline="") as output:
        writer = csv.writer(output, delimiter="\t", lineterminator="\n")
        writer.writerow(["evidence_id", "start_sec", "end_sec"])
        for index, item in enumerate(track):
            writer.writerow([
                f"business_replay:{index}", item["start"], item["end"]])
    return len(track)


def read_titanet(path):
    evidence = {}
    with open(path, encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source, delimiter="\t")
        score_fields = [name for name in reader.fieldnames or []
                        if name.startswith("score_")]
        for row in reader:
            evidence_id = row["evidence_id"]
            scores = {
                name[6:]: float(row[name]) for name in score_fields
                if row.get(name) not in (None, "")
            }
            evidence[evidence_id] = {
                "status": row["status"],
                "duration_sec": float(row["duration_sec"]),
                "scores": scores,
            }
    return evidence


def rank_scores(scores, active_ids):
    return sorted(
        ((speaker_id, float(scores[speaker_id])) for speaker_id in active_ids
         if speaker_id in scores),
        key=lambda item: (-item[1], item[0]))


def select_identity(evidence, active_ids, policy):
    if evidence.get("status") != "ok":
        return None, "insufficient_duration", []
    ranked = rank_scores(evidence.get("scores", {}), active_ids)
    if len(ranked) < 2:
        return None, "incomplete_active_identity_scores", ranked
    duration = float(evidence["duration_sec"])
    if duration < policy["short_max_sec"]:
        threshold = policy["short_min_score"]
        margin = policy["short_min_margin"]
        gate = "short"
    else:
        threshold = policy["regular_min_score"]
        margin = policy["regular_min_margin"]
        gate = "regular"
    if ranked[0][1] < threshold:
        return None, f"{gate}_score_below_gate", ranked
    if ranked[0][1] - ranked[1][1] < margin:
        return None, f"{gate}_margin_below_gate", ranked
    return ranked[0][0], f"{gate}_direct_voiceprint", ranked


def build_candidate(timeline, evidence, manifest, policy):
    mapping = {int(local): speaker_id
               for local, speaker_id in manifest["mapping"].items()}
    active_ids = sorted(set(mapping.values()))
    id_to_local = {speaker_id: local for local, speaker_id in mapping.items()}
    track = []
    decisions = []
    source_track = timeline.get("track", [])
    for index, item in enumerate(source_track):
        evidence_id = f"business_replay:{index}"
        if evidence_id not in evidence:
            raise ValueError(f"missing TitaNet evidence {evidence_id}")
        selected, reason, ranked = select_identity(
            evidence[evidence_id], active_ids, policy)
        baseline_id = item.get("speaker_id")
        changed = selected is not None and selected != baseline_id
        if selected is None:
            selected = baseline_id
            decision_reason = f"baseline_{reason}"
        elif changed:
            decision_reason = f"{reason}_override"
        else:
            decision_reason = f"baseline_confirmed_{reason}"
        speaker = (
            id_to_local[selected] if selected in id_to_local
            else int(item.get("speaker", -1)))
        uncertain = (
            bool(item.get("speaker_uncertain", False))
            if not changed else False)
        output = dict(item)
        output.update({
            "speaker": speaker,
            "speaker_id": selected,
            "speaker_uncertain": uncertain,
            "decision_reason": decision_reason,
        })
        track.append(output)
        decisions.append({
            "turn_id": item.get("turn_id", evidence_id),
            "evidence_id": evidence_id,
            "baseline_speaker_id": baseline_id,
            "speaker_id": selected,
            "changed": changed,
            "reason": decision_reason,
            "ranked": [{"speaker_id": speaker_id, "score": score}
                       for speaker_id, score in ranked],
        })
    return {
        "schema_version": 1,
        "kind": "orator_frozen_speaker_candidate",
        "candidate_kind": "v21_replay_interval_direct_voiceprint",
        "audio_sec": float(timeline["audio_sec"]),
        "sample_rate": int(timeline["sample_rate"]),
        "active_speaker_ids": active_ids,
        "turn_count": len(track),
        "decisions": decisions,
        "track": track,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    spans = subparsers.add_parser("spans")
    spans.add_argument("--timeline", required=True)
    spans.add_argument("--out", required=True)
    build = subparsers.add_parser("build")
    build.add_argument("--timeline", required=True)
    build.add_argument("--titanet", required=True)
    build.add_argument("--manifest", required=True)
    build.add_argument("--config", required=True)
    build.add_argument("--out", required=True)
    args = parser.parse_args()

    if args.command == "spans":
        count = export_spans(args.timeline, args.out)
        print(json.dumps({"spans": count, "out": os.path.abspath(args.out)}))
        return

    timeline = load_json(args.timeline)
    manifest = load_json(args.manifest)
    evidence = read_titanet(args.titanet)
    policy = load_policy(args.config)
    result = build_candidate(timeline, evidence, manifest, policy)
    result["sources"] = {
        "timeline": {"path": os.path.abspath(args.timeline),
                     "sha256": sha256_file(args.timeline)},
        "titanet": {"path": os.path.abspath(args.titanet),
                    "sha256": sha256_file(args.titanet)},
        "manifest": {"path": os.path.abspath(args.manifest),
                     "sha256": sha256_file(args.manifest)},
        "config": {"path": os.path.abspath(args.config),
                   "sha256": sha256_file(args.config)},
    }
    with open(args.out, "w", encoding="utf-8") as output:
        json.dump(result, output, ensure_ascii=False, indent=2)
    print(json.dumps({
        "turns": result["turn_count"],
        "active_speaker_ids": result["active_speaker_ids"],
        "out": os.path.abspath(args.out),
    }, ensure_ascii=False))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, csv.Error) as error:
        raise SystemExit(f"speaker replay voiceprint candidate: {error}")
