#!/usr/bin/env python3
"""Build complete punctuation contributions inside active VAD edge runs."""

import argparse
import csv
import json
import os
import tomllib

import speaker_posterior_bounded_phrase_candidate as posterior
import speaker_punctuation_phrase_candidate as phrase_tools
import speaker_relative_top1_phrase_candidate as relative
import speaker_vad_active_edge_candidate as active
import speaker_vad_utterance_candidate as vad_tools


EPSILON = 1e-9
REQUIRED_POLICY = {
    "enabled",
    "require_complete_punctuation_clause",
    "require_terminal_edge_run",
    "require_all_nonseparator_characters_aligned",
    "require_clause_wholly_inside_edge_run",
    "merge_only_adjacent_complete_clauses",
    "require_uniform_known_baseline_identity",
    "reuse_active_edge_identity_decision",
    "project_complete_clause_group",
}


def load_policy(path):
    policy = active.load_policy(path)
    with open(path, "rb") as source:
        document = tomllib.load(source)
    section = document.get("complete_edge_contribution", {})
    missing = sorted(REQUIRED_POLICY - set(section))
    if missing:
        raise ValueError(
            "complete-edge policy missing: " + ",".join(missing))
    contracts = {name: bool(section[name]) for name in REQUIRED_POLICY}
    if not all(contracts.values()):
        raise ValueError("all complete-edge contracts are mandatory")
    punctuation = str(section.get("punctuation", ""))
    if not punctuation:
        raise ValueError("complete-edge punctuation is missing")
    policy["complete_edge_contribution"] = contracts
    policy["punctuation"] = punctuation
    return policy


def complete_clause(source, times, source_start, source_end, run,
                    punctuation):
    separators = set(punctuation) | set(" \t\n\r")
    aligned = []
    for index in range(source_start, source_end):
        if source[index] in separators:
            continue
        interval = times[index]
        if interval is None:
            return None
        if (float(interval["start"]) < float(run["start"]) - EPSILON or
                float(interval["end"]) > float(run["end"]) + EPSILON):
            return None
        aligned.append(interval)
    if not aligned:
        return None
    return {
        "source_start": source_start,
        "source_end": source_end,
        "projection_start": min(float(item["start"]) for item in aligned),
        "projection_end": max(float(item["end"]) for item in aligned),
    }


def group_adjacent(clauses):
    groups = []
    for clause in clauses:
        if groups and int(groups[-1][-1]["source_end"]) == int(
                clause["source_start"]):
            groups[-1].append(clause)
        else:
            groups.append([clause])
    return groups


def uniform_known_identity(piece, fragments):
    overlaps = posterior.overlapping_fragments(piece, fragments)
    identities = [item["entry"].get("speaker_id") for item in overlaps]
    known = {str(value) for value in identities if value is not None}
    if (not overlaps or any(value is None for value in identities) or
            len(known) != 1):
        return None
    return next(iter(known))


def source_evidence_by_run(active_metadata):
    values = {}
    for piece in active_metadata.get("pieces", []):
        run_id = str(piece["active_edge_run_id"])
        values.setdefault(run_id, str(piece["evidence_id"]))
    return values


def enumerate_pieces(active_metadata, baseline, policy):
    if active_metadata.get("kind") != "orator_vad_active_edge_spans":
        raise ValueError("active metadata kind differs from source contract")
    fragments = relative.source_fragments(baseline.get("track", []))
    relative.validate_source_text(fragments, active_metadata, "baseline")
    punctuation = policy["punctuation"]
    source_evidence = source_evidence_by_run(active_metadata)
    pieces = []
    for run_index, run in enumerate(active_metadata.get(
            "active_edge_runs", [])):
        if str(run.get("edge")) != "end":
            continue
        run_id = f"vad_active_edge:{run_index}"
        if run_id not in source_evidence:
            continue
        for text_id in sorted(int(value) for value in active_metadata["asr"]):
            asr = active_metadata["asr"][str(text_id)]
            if (float(asr["end"]) < float(run["start"]) - EPSILON or
                    float(asr["start"]) > float(run["end"]) + EPSILON):
                continue
            source = str(asr["text"])
            times = phrase_tools.aligned_character_times(
                source, active_metadata["align"][str(text_id)])
            clauses = []
            for source_start, source_end in phrase_tools.phrase_ranges(
                    source, punctuation):
                clause = complete_clause(
                    source, times, source_start, source_end, run, punctuation)
                if clause is not None:
                    clauses.append(clause)
            for group in group_adjacent(clauses):
                source_start = int(group[0]["source_start"])
                source_end = int(group[-1]["source_end"])
                piece = {
                    "evidence_id": (
                        f"complete_edge_contribution:{len(pieces)}"),
                    "source_evidence_id": source_evidence[run_id],
                    "active_edge_run_id": run_id,
                    **run,
                    "text_id": text_id,
                    "source_start": source_start,
                    "source_end": source_end,
                    "text": source[source_start:source_end],
                    "projection_start": min(
                        float(item["projection_start"]) for item in group),
                    "projection_end": max(
                        float(item["projection_end"]) for item in group),
                    "complete_clause_count": len(group),
                }
                baseline_id = uniform_known_identity(
                    piece, fragments[text_id])
                if baseline_id is None:
                    continue
                piece["baseline_speaker_id"] = baseline_id
                pieces.append(piece)
    return pieces


def remap_evidence(path, metadata):
    source = phrase_tools.read_titanet(path)
    output = {}
    for piece in metadata.get("pieces", []):
        source_id = str(piece["source_evidence_id"])
        if source_id not in source:
            raise ValueError(f"missing source evidence {source_id}")
        evidence = source[source_id]
        if abs(float(evidence["duration_sec"]) - (
                float(piece["end"]) - float(piece["start"]))) > 1e-5:
            raise ValueError(f"source evidence duration differs for {source_id}")
        output[str(piece["evidence_id"])] = evidence
    return output


def command_spans(args, policy):
    active_metadata = posterior.load_json(args.active_metadata)
    baseline = posterior.load_json(args.baseline)
    pieces = enumerate_pieces(active_metadata, baseline, policy)
    posterior.write_spans(args.out, pieces)
    result = {
        "schema_version": 1,
        "kind": "orator_complete_edge_contribution_spans",
        "frame_period_sec": active_metadata["frame_period_sec"],
        "policy": {name: value for name, value in policy.items()
                   if name != "voiceprint"},
        "voiceprint_policy": policy["voiceprint"],
        "asr": active_metadata["asr"],
        "align": active_metadata["align"],
        "pieces": pieces,
        "piece_count": len(pieces),
        "sources": {
            name: {"path": os.path.abspath(path),
                   "sha256": posterior.sha256_file(path)}
            for name, path in {
                "active_metadata": args.active_metadata,
                "baseline": args.baseline,
                "config": args.config,
            }.items()
        },
    }
    with open(args.metadata, "w", encoding="utf-8") as output:
        json.dump(result, output, ensure_ascii=False, indent=2)
    print(json.dumps({
        "pieces": len(pieces),
        "spans": os.path.abspath(args.out),
        "metadata": os.path.abspath(args.metadata),
    }, ensure_ascii=False))


def command_build(args, policy):
    metadata = posterior.load_json(args.metadata)
    result = vad_tools.build_candidate(
        posterior.load_json(args.baseline), metadata,
        remap_evidence(args.session_titanet, metadata),
        remap_evidence(args.robust_titanet, metadata),
        posterior.load_json(args.manifest), policy,
        expected_metadata_kind="orator_complete_edge_contribution_spans",
        candidate_kind="v21_complete_edge_contribution_handoff",
        reason_prefix="complete_edge_contribution",
        decision_function=active.decide_piece)
    result["sources"] = {
        name: {"path": os.path.abspath(path),
               "sha256": posterior.sha256_file(path)}
        for name, path in {
            "baseline": args.baseline,
            "metadata": args.metadata,
            "session_titanet": args.session_titanet,
            "robust_titanet": args.robust_titanet,
            "manifest": args.manifest,
            "config": args.config,
        }.items()
    }
    with open(args.out, "w", encoding="utf-8") as output:
        json.dump(result, output, ensure_ascii=False, indent=2)
    print(json.dumps({
        "pieces": result["piece_count"],
        "accepted_pieces": result["accepted_piece_count"],
        "turns": result["turn_count"],
        "out": os.path.abspath(args.out),
    }, ensure_ascii=False))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    spans = commands.add_parser("spans")
    spans.add_argument("--active-metadata", required=True)
    spans.add_argument("--baseline", required=True)
    spans.add_argument("--config", required=True)
    spans.add_argument("--out", required=True)
    spans.add_argument("--metadata", required=True)
    build = commands.add_parser("build")
    build.add_argument("--baseline", required=True)
    build.add_argument("--metadata", required=True)
    build.add_argument("--session-titanet", required=True)
    build.add_argument("--robust-titanet", required=True)
    build.add_argument("--manifest", required=True)
    build.add_argument("--config", required=True)
    build.add_argument("--out", required=True)
    args = parser.parse_args()
    policy = load_policy(args.config)
    if args.command == "spans":
        command_spans(args, policy)
    else:
        command_build(args, policy)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, csv.Error) as error:
        raise SystemExit(f"speaker complete-edge candidate: {error}")
