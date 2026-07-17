#!/usr/bin/env python3
"""Export and compose adjacent subminimum-clause speaker evidence."""

import argparse
import csv
import hashlib
import json
import os
import tomllib
from collections import defaultdict

import speaker_punctuation_phrase_candidate as phrase_tools
import speaker_reviewed_overlay_candidate as overlay_tools


EPSILON = 1e-9
DECISION_REASON = "adjacent_subminimum_clause_dual_voiceprint_challenge"


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
    phrase = document.get("punctuation_phrase", {})
    fusion = document.get("speaker_fusion", {})
    section = document.get("adjacent_subminimum_clause", {})
    phrase_names = (
        "minimum_duration_sec", "maximum_duration_sec",
        "minimum_visible_character_count", "punctuation",
    )
    fusion_names = (
        "short_max_sec", "short_min_score", "short_min_margin",
        "regular_min_score", "regular_min_margin",
    )
    safeguards = {
        "enabled", "require_complete_adjacent_clauses",
        "require_each_below_phrase_minimum",
        "require_combined_within_phrase_bounds",
        "require_dual_voiceprint_consensus",
        "require_uniform_known_conflicting_baseline",
        "reject_conflicting_overlays", "reject_overlapping_proposals",
    }
    required = safeguards | {"clause_count", "protected_decision_reasons"}
    missing = sorted((set(phrase_names) - set(phrase)) |
                     (set(fusion_names) - set(fusion)) |
                     (required - set(section)))
    if missing:
        raise ValueError("adjacent-clause policy missing: " + ",".join(missing))
    if any(section[name] is not True for name in safeguards):
        raise ValueError("adjacent-clause safeguards are mandatory")
    if int(section["clause_count"]) != 2:
        raise ValueError("adjacent-clause envelope must contain exactly two clauses")
    minimum = float(phrase["minimum_duration_sec"])
    maximum = float(phrase["maximum_duration_sec"])
    if minimum <= 0.0 or maximum < minimum:
        raise ValueError("invalid inherited punctuation duration bounds")
    visible = int(phrase["minimum_visible_character_count"])
    punctuation = str(phrase["punctuation"])
    if visible < 1 or not punctuation:
        raise ValueError("invalid inherited punctuation policy")
    protected = [str(value)
                 for value in section["protected_decision_reasons"]]
    if not protected or len(set(protected)) != len(protected):
        raise ValueError("protected decision reasons are invalid")
    return {
        "minimum_duration_sec": minimum,
        "maximum_duration_sec": maximum,
        "minimum_visible_character_count": visible,
        "punctuation": punctuation,
        **{name: float(fusion[name]) for name in fusion_names},
        "clause_count": 2,
        "protected_decision_reasons": protected,
    }


def aligned_bounds(times, start, end):
    values = [times[index] for index in range(start, end)
              if times[index] is not None and
              float(times[index]["end"]) > float(times[index]["start"])]
    if not values:
        return None
    return (min(float(value["start"]) for value in values),
            max(float(value["end"]) for value in values))


def visible_count(source, start, end, punctuation):
    return sum(not character.isspace() and character not in punctuation
               for character in source[start:end])


def enumerate_envelopes(alignment, policy):
    if alignment.get("kind") != "orator_punctuation_phrase_spans":
        raise ValueError("alignment input is not punctuation phrase metadata")
    if set(alignment.get("asr", {})) != set(alignment.get("align", {})):
        raise ValueError("ASR/alignment text ID sets differ")
    punctuation = set(policy["punctuation"])
    envelopes = []
    for text_id_text in sorted(alignment["asr"], key=int):
        text_id = int(text_id_text)
        source = str(alignment["asr"][text_id_text]["text"])
        times = phrase_tools.aligned_character_times(
            source, alignment["align"][text_id_text])
        clauses = []
        for source_start, source_end in phrase_tools.phrase_ranges(
                source, policy["punctuation"]):
            bounds = aligned_bounds(times, source_start, source_end)
            if bounds is None:
                continue
            if visible_count(source, source_start, source_end, punctuation) < \
                    policy["minimum_visible_character_count"]:
                continue
            clauses.append({
                "source_start": source_start, "source_end": source_end,
                "start": bounds[0], "end": bounds[1],
                "duration_sec": bounds[1] - bounds[0],
            })
        for left, right in zip(clauses, clauses[1:]):
            if left["source_end"] != right["source_start"]:
                continue
            if (left["duration_sec"] + EPSILON >=
                    policy["minimum_duration_sec"] or
                    right["duration_sec"] + EPSILON >=
                    policy["minimum_duration_sec"]):
                continue
            duration = right["end"] - left["start"]
            if (duration + EPSILON < policy["minimum_duration_sec"] or
                    duration > policy["maximum_duration_sec"] + EPSILON):
                continue
            envelopes.append({
                "evidence_id": f"adjacent_subminimum_clause:{len(envelopes)}",
                "text_id": text_id,
                "source_start": left["source_start"],
                "source_end": right["source_end"],
                "start": left["start"], "end": right["end"],
                "left_clause": left, "right_clause": right,
            })
    return envelopes


def write_spans(path, envelopes):
    with open(path, "w", encoding="ascii", newline="") as output:
        writer = csv.writer(output, delimiter="\t", lineterminator="\n")
        writer.writerow(["evidence_id", "start_sec", "end_sec"])
        for item in envelopes:
            writer.writerow([item["evidence_id"], item["start"], item["end"]])


def labels_for_source(fragments, source):
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


def overlaps(left, right):
    return (int(left["source_start"]) < int(right["source_end"]) and
            int(left["source_end"]) > int(right["source_start"]))


def build_candidate(baseline, metadata, session_evidence, robust_evidence,
                    mapping_document, policy):
    if metadata.get("kind") != "orator_adjacent_subminimum_clause_spans":
        raise ValueError("metadata is not adjacent subminimum-clause evidence")
    mapping = overlay_tools.read_mapping(mapping_document)
    id_to_local = {speaker_id: local for local, speaker_id in mapping.items()}
    active_ids = sorted(id_to_local)
    if sorted(baseline.get("active_speaker_ids", [])) != active_ids:
        raise ValueError("baseline identities differ from mapping")
    fragments = overlay_tools.candidate_fragments(baseline)
    overlay_tools.validate_candidate_sources(fragments, metadata)
    labels = {
        text_id: labels_for_source(
            values, str(metadata["asr"][str(text_id)]["text"]))
        for text_id, values in fragments.items()
    }
    protected = set(policy["protected_decision_reasons"])
    decisions = []
    proposals = defaultdict(list)
    for envelope in metadata.get("envelopes", []):
        evidence_id = str(envelope["evidence_id"])
        if evidence_id not in session_evidence or evidence_id not in robust_evidence:
            raise ValueError("missing adjacent-clause voiceprint evidence")
        session_id, session_reason, session_ranked = phrase_tools.select_identity(
            session_evidence[evidence_id], active_ids, policy)
        robust_id, robust_reason, robust_ranked = phrase_tools.select_identity(
            robust_evidence[evidence_id], active_ids, policy)
        decision = {
            "evidence_id": evidence_id, "text_id": int(envelope["text_id"]),
            "source_start": int(envelope["source_start"]),
            "source_end": int(envelope["source_end"]),
            "start": float(envelope["start"]), "end": float(envelope["end"]),
            "session_identity": session_id, "session_reason": session_reason,
            "session_ranked": [{"speaker_id": identity, "score": score}
                               for identity, score in session_ranked],
            "robust_identity": robust_id, "robust_reason": robust_reason,
            "robust_ranked": [{"speaker_id": identity, "score": score}
                              for identity, score in robust_ranked],
            "selected_speaker_id": None, "accepted": False,
            "reason": "dual_voiceprint_abstention",
        }
        decisions.append(decision)
        if session_id is None or robust_id is None:
            continue
        if session_id != robust_id:
            decision["reason"] = "dual_voiceprint_disagreement"
            continue
        text_id = int(envelope["text_id"])
        start = int(envelope["source_start"])
        end = int(envelope["source_end"])
        range_labels = labels[text_id][start:end]
        baseline_ids = {value["speaker_id"] for value in range_labels}
        if None in baseline_ids or len(baseline_ids) != 1 or \
                session_id in baseline_ids:
            decision["reason"] = "uniform_known_conflict_missing"
            continue
        if any(value["reason"] in protected for value in range_labels):
            decision["reason"] = "protected_overlay_conflict"
            continue
        decision["selected_speaker_id"] = session_id
        decision["accepted"] = True
        decision["reason"] = DECISION_REASON
        proposals[text_id].append(decision)

    for text_id, values in proposals.items():
        conflicted = set()
        ordered = sorted(values, key=lambda item: (
            item["source_start"], item["source_end"], item["evidence_id"]))
        for index, left in enumerate(ordered):
            for right in ordered[index + 1:]:
                if right["source_start"] >= left["source_end"]:
                    break
                if overlaps(left, right):
                    conflicted.add(left["evidence_id"])
                    conflicted.add(right["evidence_id"])
        for item in values:
            if item["evidence_id"] in conflicted:
                item["accepted"] = False
                item["selected_speaker_id"] = None
                item["reason"] = "overlapping_proposal_abstention"

    overlays = defaultdict(list)
    for values in proposals.values():
        for item in values:
            if item["accepted"]:
                overlays[item["text_id"]].append({
                    "text_id": item["text_id"],
                    "source_start": item["source_start"],
                    "source_end": item["source_end"],
                    "speaker_id": item["selected_speaker_id"],
                    "reason": DECISION_REASON,
                })
    overlay_tools.validate_overlays(overlays)
    track = []
    for text_id in sorted(fragments):
        source = str(metadata["asr"][str(text_id)]["text"])
        track.extend(overlay_tools.reproject_preserving_base(
            text_id, source, metadata["align"][str(text_id)],
            fragments[text_id], overlays.get(text_id, []), id_to_local))
    return {
        "schema_version": 1, "kind": "orator_frozen_speaker_candidate",
        "candidate_kind": "v21_adjacent_subminimum_clause_challenge",
        "audio_sec": float(baseline["audio_sec"]),
        "sample_rate": int(baseline["sample_rate"]),
        "active_speaker_ids": active_ids,
        "source_turn_count": len(baseline.get("track", [])),
        "turn_count": len(track),
        "accepted_challenge_count": sum(item["accepted"] for item in decisions),
        "policy": policy, "challenge_decisions": decisions, "track": track,
    }


def spans_command(args):
    alignment = load_json(args.alignment)
    policy = load_policy(args.config)
    envelopes = enumerate_envelopes(alignment, policy)
    metadata = {
        "schema_version": 1,
        "kind": "orator_adjacent_subminimum_clause_spans",
        "asr": alignment["asr"], "align": alignment["align"],
        "envelopes": envelopes, "policy": policy,
        "sources": {
            "alignment": {"path": os.path.abspath(args.alignment),
                          "sha256": sha256_file(args.alignment)},
            "config": {"path": os.path.abspath(args.config),
                       "sha256": sha256_file(args.config)},
        },
    }
    with open(args.metadata_out, "w", encoding="utf-8") as output:
        json.dump(metadata, output, ensure_ascii=False, indent=2)
    write_spans(args.spans_out, envelopes)
    print(json.dumps({"envelopes": len(envelopes),
                      "metadata": os.path.abspath(args.metadata_out),
                      "spans": os.path.abspath(args.spans_out)},
                     ensure_ascii=False))


def build_command(args):
    result = build_candidate(
        load_json(args.baseline), load_json(args.metadata),
        phrase_tools.read_titanet(args.session_evidence),
        phrase_tools.read_titanet(args.robust_evidence),
        load_json(args.mapping), load_policy(args.config))
    result["sources"] = {
        name: {"path": os.path.abspath(path), "sha256": sha256_file(path)}
        for name, path in {
            "baseline": args.baseline, "metadata": args.metadata,
            "session_evidence": args.session_evidence,
            "robust_evidence": args.robust_evidence,
            "mapping": args.mapping, "config": args.config,
        }.items()
    }
    with open(args.out, "w", encoding="utf-8") as output:
        json.dump(result, output, ensure_ascii=False, indent=2)
    print(json.dumps({
        "accepted_challenges": result["accepted_challenge_count"],
        "turns": result["turn_count"], "out": os.path.abspath(args.out),
    }, ensure_ascii=False))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    spans = commands.add_parser("spans")
    spans.add_argument("--alignment", required=True)
    spans.add_argument("--config", required=True)
    spans.add_argument("--metadata-out", required=True)
    spans.add_argument("--spans-out", required=True)
    spans.set_defaults(handler=spans_command)
    build = commands.add_parser("build")
    build.add_argument("--baseline", required=True)
    build.add_argument("--metadata", required=True)
    build.add_argument("--session-evidence", required=True)
    build.add_argument("--robust-evidence", required=True)
    build.add_argument("--mapping", required=True)
    build.add_argument("--config", required=True)
    build.add_argument("--out", required=True)
    build.set_defaults(handler=build_command)
    args = parser.parse_args()
    args.handler(args)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError) as error:
        raise SystemExit(f"speaker adjacent-clause candidate: {error}")
