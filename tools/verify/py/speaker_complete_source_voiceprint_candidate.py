#!/usr/bin/env python3
"""Project accepted bounded voiceprint evidence over one complete ASR source."""

import argparse
import csv
import json
import os
import tomllib

import speaker_bounded_local_run_voiceprint_candidate as bounded
import speaker_posterior_bounded_phrase_candidate as posterior
import speaker_punctuation_phrase_candidate as phrase_tools
import speaker_relative_top1_phrase_candidate as relative
import speaker_vad_utterance_candidate as vad_tools


EPSILON = 1e-9
REQUIRED_POLICY = {
    "enabled",
    "require_single_indexed_phrase_in_source",
    "require_complete_source_alignment_in_query",
    "require_uniform_known_source_identity",
    "reuse_bounded_local_run_voiceprint_decision",
    "project_complete_asr_source",
}
SEPARATORS = set("，。？！；、,.?!;:： \t\n\r")


def load_policy(path):
    policy = bounded.load_policy(path)
    with open(path, "rb") as source:
        document = tomllib.load(source)
    section = document.get("complete_source_voiceprint", {})
    missing = sorted(REQUIRED_POLICY - set(section))
    if missing:
        raise ValueError(
            "complete-source policy missing: " + ",".join(missing))
    contracts = {name: bool(section[name]) for name in REQUIRED_POLICY}
    if not all(contracts.values()):
        raise ValueError("all complete-source contracts are mandatory")
    policy["complete_source_voiceprint"] = contracts
    return policy


def complete_alignment_in_query(source, units, query_start, query_end):
    times = phrase_tools.aligned_character_times(source, units)
    for index, character in enumerate(source):
        if character in SEPARATORS:
            continue
        interval = times[index]
        if interval is None:
            return False
        if (float(interval["start"]) < query_start - EPSILON or
                float(interval["end"]) > query_end + EPSILON):
            return False
    return True


def enumerate_pieces(bounded_metadata, alignment, baseline):
    if bounded_metadata.get("kind") != (
            "orator_bounded_local_run_voiceprint_spans"):
        raise ValueError("bounded metadata kind differs from source contract")
    if alignment.get("kind") != "orator_punctuation_phrase_spans":
        raise ValueError("alignment is not punctuation-phrase metadata")
    if bounded_metadata.get("asr") != alignment.get("asr"):
        raise ValueError("bounded and phrase ASR metadata differ")
    if bounded_metadata.get("align") != alignment.get("align"):
        raise ValueError("bounded and phrase alignment metadata differ")

    fragments = relative.source_fragments(baseline.get("track", []))
    relative.validate_source_text(fragments, alignment, "baseline")
    phrases_by_text = {}
    for phrase in alignment.get("phrases", []):
        phrases_by_text.setdefault(int(phrase["text_id"]), []).append(phrase)

    pieces = []
    for piece in bounded_metadata.get("pieces", []):
        text_id = int(piece["text_id"])
        indexed = phrases_by_text.get(text_id, [])
        if len(indexed) != 1:
            continue
        if str(indexed[0]["evidence_id"]) != str(
                piece["phrase_evidence_id"]):
            continue
        source = str(alignment["asr"][str(text_id)]["text"])
        query_start = float(piece["start"])
        query_end = float(piece["end"])
        if not complete_alignment_in_query(
                source, alignment["align"][str(text_id)],
                query_start, query_end):
            continue
        expanded = {
            **piece,
            "source_start": 0,
            "source_end": len(source),
            "text": source,
            "projection_start": 0,
            "projection_end": len(source),
            "indexed_phrase_count": 1,
            "complete_source_alignment": True,
        }
        if bounded.uniform_known_identity(
                expanded, fragments[text_id]) is None:
            continue
        pieces.append(expanded)
    return pieces


def command_spans(args, policy):
    bounded_metadata = posterior.load_json(args.bounded_metadata)
    alignment = posterior.load_json(args.alignment)
    baseline = posterior.load_json(args.baseline)
    pieces = enumerate_pieces(bounded_metadata, alignment, baseline)
    posterior.write_spans(args.out, pieces)
    result = {
        "schema_version": 1,
        "kind": "orator_complete_source_voiceprint_spans",
        "frame_period_sec": bounded_metadata["frame_period_sec"],
        "policy": {name: value for name, value in policy.items()
                   if name != "voiceprint"},
        "voiceprint_policy": policy["voiceprint"],
        "asr": alignment["asr"],
        "align": alignment["align"],
        "pieces": pieces,
        "piece_count": len(pieces),
        "sources": {
            name: {"path": os.path.abspath(path),
                   "sha256": posterior.sha256_file(path)}
            for name, path in {
                "bounded_metadata": args.bounded_metadata,
                "alignment": args.alignment,
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
    result = vad_tools.build_candidate(
        posterior.load_json(args.baseline), posterior.load_json(args.metadata),
        phrase_tools.read_titanet(args.session_titanet),
        phrase_tools.read_titanet(args.robust_titanet),
        posterior.load_json(args.manifest), policy,
        expected_metadata_kind="orator_complete_source_voiceprint_spans",
        candidate_kind="v21_complete_source_voiceprint_override",
        reason_prefix="complete_source_voiceprint",
        decision_function=bounded.decide_piece)
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
    spans.add_argument("--bounded-metadata", required=True)
    spans.add_argument("--alignment", required=True)
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
        raise SystemExit(f"speaker complete-source candidate: {error}")
