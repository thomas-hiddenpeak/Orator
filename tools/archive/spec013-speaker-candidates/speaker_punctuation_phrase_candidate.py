#!/usr/bin/env python3
"""Build a reference-free TitaNet view over aligned ASR phrases."""

import argparse
import csv
import hashlib
import json
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
    phrase = document.get("punctuation_phrase", {})
    fusion = document.get("speaker_fusion", {})
    projection = document.get("punctuation_phrase_projection", {})
    phrase_required = {
        "minimum_duration_sec", "maximum_duration_sec",
        "minimum_visible_character_count", "punctuation",
    }
    fusion_required = {
        "short_max_sec", "short_min_score", "short_min_margin",
        "regular_min_score", "regular_min_margin",
    }
    projection_required = {
        "preserve_direct_anchors", "reject_on_direct_anchor_conflict",
        "require_agreeing_direct_anchor",
    }
    missing = sorted(
        (phrase_required - set(phrase)) |
        (fusion_required - set(fusion)) |
        (projection_required - set(projection)))
    if missing:
        raise ValueError("punctuation phrase policy missing: " + ",".join(missing))
    policy = {
        "minimum_duration_sec": float(phrase["minimum_duration_sec"]),
        "maximum_duration_sec": float(phrase["maximum_duration_sec"]),
        "minimum_visible_character_count": int(
            phrase["minimum_visible_character_count"]),
        "punctuation": str(phrase["punctuation"]),
        **{name: float(fusion[name]) for name in fusion_required},
        "preserve_direct_anchors": bool(projection["preserve_direct_anchors"]),
        "reject_on_direct_anchor_conflict": bool(
            projection["reject_on_direct_anchor_conflict"]),
        "require_agreeing_direct_anchor": bool(
            projection["require_agreeing_direct_anchor"]),
    }
    if (policy["minimum_duration_sec"] <= 0.0 or
            policy["maximum_duration_sec"] < policy["minimum_duration_sec"]):
        raise ValueError("invalid phrase duration bounds")
    if policy["minimum_visible_character_count"] < 1:
        raise ValueError("minimum_visible_character_count must be positive")
    if not policy["punctuation"]:
        raise ValueError("punctuation must not be empty")
    if not policy["preserve_direct_anchors"]:
        raise ValueError("direct anchor preservation is mandatory")
    if not policy["reject_on_direct_anchor_conflict"]:
        raise ValueError("direct anchor conflict rejection is mandatory")
    return policy


def decode_hex(value):
    return bytes.fromhex(value).decode("utf-8")


def read_asr(path):
    values = {}
    with open(path, encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source, delimiter="\t")
        for row in reader:
            text_id = int(row["text_id"])
            if text_id in values:
                raise ValueError(f"duplicate ASR text_id {text_id}")
            values[text_id] = {
                "start": float(row["start_sec"]),
                "end": float(row["end_sec"]),
                "text": decode_hex(row["text_hex"]),
            }
    return values


def read_align(path):
    values = defaultdict(list)
    with open(path, encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source, delimiter="\t")
        for row in reader:
            values[int(row["text_id"])].append({
                "start": float(row["unit_start_sec"]),
                "end": float(row["unit_end_sec"]),
                "text": decode_hex(row["text_hex"]),
            })
    return dict(values)


def phrase_ranges(source, punctuation):
    start = 0
    for index, character in enumerate(source):
        if character not in punctuation:
            continue
        end = index + 1
        if any(not value.isspace() and value not in punctuation
               for value in source[start:end]):
            yield start, end
        start = end
    if start < len(source) and any(
            not value.isspace() and value not in punctuation
            for value in source[start:]):
        yield start, len(source)


def aligned_character_times(source, units):
    times = [None] * len(source)
    cursor = 0
    for unit in units:
        for character in str(unit["text"]):
            found = source.find(character, cursor)
            if found < 0:
                raise ValueError("alignment unit is not a subsequence of ASR text")
            times[found] = {
                "start": float(unit["start"]),
                "end": float(unit["end"]),
            }
            cursor = found + 1
    return times


def enumerate_phrases(asr, align, policy):
    if set(asr) != set(align):
        raise ValueError("ASR and alignment text_id sets differ")
    punctuation = set(policy["punctuation"])
    phrases = []
    for text_id in sorted(asr):
        source = asr[text_id]["text"]
        times = aligned_character_times(source, align[text_id])
        for source_start, source_end in phrase_ranges(source, punctuation):
            aligned = [times[index] for index in range(source_start, source_end)
                       if times[index] is not None and
                       times[index]["end"] > times[index]["start"]]
            if not aligned:
                continue
            start = min(item["start"] for item in aligned)
            end = max(item["end"] for item in aligned)
            duration = end - start
            visible = sum(
                not value.isspace() and value not in punctuation
                for value in source[source_start:source_end])
            if (duration + EPSILON < policy["minimum_duration_sec"] or
                    duration > policy["maximum_duration_sec"] + EPSILON or
                    visible < policy["minimum_visible_character_count"]):
                continue
            phrases.append({
                "evidence_id": f"punctuation_phrase:{len(phrases)}",
                "text_id": text_id,
                "source_start": source_start,
                "source_end": source_end,
                "start": start,
                "end": end,
                "text": source[source_start:source_end],
            })
    return phrases


def write_spans(path, phrases):
    with open(path, "w", encoding="ascii", newline="") as output:
        writer = csv.writer(output, delimiter="\t", lineterminator="\n")
        writer.writerow(["evidence_id", "start_sec", "end_sec"])
        for phrase in phrases:
            writer.writerow([
                phrase["evidence_id"], phrase["start"], phrase["end"]])


def read_titanet(path):
    evidence = {}
    with open(path, encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source, delimiter="\t")
        score_fields = [name for name in reader.fieldnames or []
                        if name.startswith("score_")]
        for row in reader:
            evidence[row["evidence_id"]] = {
                "status": row["status"],
                "duration_sec": float(row["duration_sec"]),
                "scores": {
                    name[6:]: float(row[name]) for name in score_fields
                    if row.get(name) not in (None, "")
                },
            }
    return evidence


def select_identity(evidence, active_ids, policy):
    if evidence.get("status") != "ok":
        return None, "phrase_embedding_unavailable", []
    ranked = sorted(
        ((speaker_id, float(evidence["scores"][speaker_id]))
         for speaker_id in active_ids if speaker_id in evidence["scores"]),
        key=lambda item: (-item[1], item[0]))
    if len(ranked) < 2:
        return None, "phrase_active_scores_incomplete", ranked
    if evidence["duration_sec"] < policy["short_max_sec"]:
        score_gate = policy["short_min_score"]
        margin_gate = policy["short_min_margin"]
        kind = "short"
    else:
        score_gate = policy["regular_min_score"]
        margin_gate = policy["regular_min_margin"]
        kind = "regular"
    if ranked[0][1] < score_gate:
        return None, f"phrase_{kind}_score_below_gate", ranked
    if ranked[0][1] - ranked[1][1] < margin_gate:
        return None, f"phrase_{kind}_margin_below_gate", ranked
    return ranked[0][0], f"phrase_{kind}_voiceprint", ranked


def is_direct_anchor(decision):
    reason = str(decision.get("reason", ""))
    return (decision.get("speaker_id") is not None and
            "direct_voiceprint" in reason and
            "below_gate" not in reason)


def source_fragments(direct):
    track = direct.get("track", [])
    decisions = direct.get("decisions", [])
    if len(track) != len(decisions):
        raise ValueError("direct candidate track and decisions differ")
    grouped = defaultdict(list)
    for index, item in enumerate(track):
        grouped[int(item["text_id"])].append((index, item, decisions[index]))
    output = {}
    for text_id, fragments in grouped.items():
        cursor = 0
        values = []
        for index, item, decision in fragments:
            text = str(item.get("text", ""))
            values.append({
                "track_index": index,
                "source_start": cursor,
                "source_end": cursor + len(text),
                "entry": item,
                "decision": decision,
                "anchor": is_direct_anchor(decision),
            })
            cursor += len(text)
        output[text_id] = values
    return output


def phrase_anchor_ids(phrase, fragments):
    return {
        fragment["decision"]["speaker_id"]
        for fragment in fragments
        if fragment["anchor"] and
        fragment["source_start"] < phrase["source_end"] and
        fragment["source_end"] > phrase["source_start"]
    }


def phrase_overlay_accepted(selected, anchors, require_anchor):
    return (
        selected is not None and
        (bool(anchors) or not require_anchor) and
        all(anchor == selected for anchor in anchors))


def span_time(start, end, times, owners, fragments):
    aligned = [times[index] for index in range(start, end)
               if times[index] is not None and
               times[index]["end"] > times[index]["start"]]
    if aligned:
        return (min(item["start"] for item in aligned),
                max(item["end"] for item in aligned))
    owner_indices = {owners[index] for index in range(start, end)}
    return (
        min(float(fragments[index]["entry"]["start"])
            for index in owner_indices),
        max(float(fragments[index]["entry"]["end"])
            for index in owner_indices),
    )


def reproject_text(text_id, source, units, fragments, overlays, id_to_local):
    owners = []
    labels = []
    for owner, fragment in enumerate(fragments):
        item = fragment["entry"]
        for _ in str(item.get("text", "")):
            owners.append(owner)
            labels.append({
                "speaker_id": item.get("speaker_id"),
                "speaker": int(item.get("speaker", -1)),
                "speaker_uncertain": bool(
                    item.get("speaker_uncertain", False)),
                "reason": str(item.get("decision_reason", "baseline")),
            })
    if len(labels) != len(source):
        raise ValueError(f"candidate text differs for text_id {text_id}")
    for overlay in overlays:
        label = {
            "speaker_id": overlay["speaker_id"],
            "speaker": id_to_local[overlay["speaker_id"]],
            "speaker_uncertain": False,
            "reason": overlay["reason"],
        }
        for index in range(overlay["source_start"], overlay["source_end"]):
            labels[index] = label

    times = aligned_character_times(source, units)
    bounds = [0]
    for index in range(1, len(source)):
        left = labels[index - 1]
        right = labels[index]
        if (left["speaker_id"], left["speaker_uncertain"], left["reason"]) != (
                right["speaker_id"], right["speaker_uncertain"],
                right["reason"]):
            bounds.append(index)
    bounds.append(len(source))

    output = []
    for piece, (start, end) in enumerate(zip(bounds, bounds[1:])):
        if end <= start:
            continue
        span_start, span_end = span_time(
            start, end, times, owners, fragments)
        if span_end <= span_start + EPSILON:
            raise ValueError("reprojected phrase has non-positive duration")
        label = labels[start]
        output.append({
            "turn_id": f"punctuation_phrase:{text_id}:{piece}",
            "start": span_start,
            "end": span_end,
            "text_id": text_id,
            "text": source[start:end],
            "speaker": label["speaker"],
            "speaker_id": label["speaker_id"],
            "speaker_uncertain": label["speaker_uncertain"],
            "decision_reason": label["reason"],
        })
    if "".join(item["text"] for item in output) != source:
        raise ValueError("reprojected source text is not exact")
    return output


def build_candidate(direct, metadata, evidence, manifest, policy):
    if direct.get("kind") != "orator_frozen_speaker_candidate":
        raise ValueError("direct input is not a frozen speaker candidate")
    if metadata.get("kind") != "orator_punctuation_phrase_spans":
        raise ValueError("metadata is not punctuation phrase spans")
    mapping = {int(local): speaker_id
               for local, speaker_id in manifest["mapping"].items()}
    id_to_local = {speaker_id: local for local, speaker_id in mapping.items()}
    active_ids = sorted(set(mapping.values()))
    if sorted(direct.get("active_speaker_ids", [])) != active_ids:
        raise ValueError("active identities differ from mapping")

    fragments_by_text = source_fragments(direct)
    asr = metadata["asr"]
    align = metadata["align"]
    overlays_by_text = defaultdict(list)
    decisions = []
    for phrase in metadata["phrases"]:
        evidence_id = phrase["evidence_id"]
        if evidence_id not in evidence:
            raise ValueError(f"missing phrase evidence {evidence_id}")
        selected, reason, ranked = select_identity(
            evidence[evidence_id], active_ids, policy)
        text_id = int(phrase["text_id"])
        anchors = phrase_anchor_ids(phrase, fragments_by_text[text_id])
        accepted = phrase_overlay_accepted(
            selected, anchors, policy["require_agreeing_direct_anchor"])
        if selected is None:
            decision_reason = reason
        elif policy["require_agreeing_direct_anchor"] and not anchors:
            decision_reason = "phrase_direct_anchor_required"
        elif not accepted:
            decision_reason = "phrase_direct_anchor_conflict"
        else:
            decision_reason = reason + "_overlay"
            overlays_by_text[text_id].append({
                **phrase,
                "speaker_id": selected,
                "reason": decision_reason,
            })
        decisions.append({
            "evidence_id": evidence_id,
            "text_id": text_id,
            "start": phrase["start"],
            "end": phrase["end"],
            "speaker_id": selected,
            "direct_anchor_ids": sorted(anchors),
            "accepted": accepted,
            "reason": decision_reason,
            "ranked": [{"speaker_id": speaker_id, "score": score}
                       for speaker_id, score in ranked],
        })

    track = []
    for text_id in sorted(fragments_by_text):
        source = asr[str(text_id)]["text"]
        fragment_text = "".join(
            str(item["entry"].get("text", ""))
            for item in fragments_by_text[text_id])
        if fragment_text != source:
            raise ValueError(f"direct source text mismatch for {text_id}")
        track.extend(reproject_text(
            text_id, source, align[str(text_id)],
            fragments_by_text[text_id], overlays_by_text[text_id],
            id_to_local))

    return {
        "schema_version": 1,
        "kind": "orator_frozen_speaker_candidate",
        "candidate_kind": "v21_punctuation_phrase_voiceprint_overlay",
        "audio_sec": float(direct["audio_sec"]),
        "sample_rate": int(direct["sample_rate"]),
        "active_speaker_ids": active_ids,
        "source_turn_count": len(direct.get("track", [])),
        "turn_count": len(track),
        "phrase_count": len(metadata["phrases"]),
        "accepted_phrase_count": sum(
            decision["accepted"] for decision in decisions),
        "policy": policy,
        "decisions": decisions,
        "track": track,
    }


def command_spans(args, policy):
    asr = read_asr(args.asr)
    align = read_align(args.align)
    phrases = enumerate_phrases(asr, align, policy)
    write_spans(args.out, phrases)
    metadata = {
        "schema_version": 1,
        "kind": "orator_punctuation_phrase_spans",
        "policy": policy,
        "asr": {str(key): value for key, value in asr.items()},
        "align": {str(key): value for key, value in align.items()},
        "phrase_count": len(phrases),
        "phrases": phrases,
        "sources": {
            "asr": {"path": os.path.abspath(args.asr),
                    "sha256": sha256_file(args.asr)},
            "align": {"path": os.path.abspath(args.align),
                      "sha256": sha256_file(args.align)},
            "config": {"path": os.path.abspath(args.config),
                       "sha256": sha256_file(args.config)},
        },
    }
    with open(args.metadata, "w", encoding="utf-8") as output:
        json.dump(metadata, output, ensure_ascii=False, indent=2)
    print(json.dumps({
        "phrases": len(phrases),
        "spans": os.path.abspath(args.out),
        "metadata": os.path.abspath(args.metadata),
    }, ensure_ascii=False))


def command_build(args, policy):
    direct = load_json(args.direct)
    metadata = load_json(args.metadata)
    evidence = read_titanet(args.titanet)
    manifest = load_json(args.manifest)
    result = build_candidate(direct, metadata, evidence, manifest, policy)
    result["sources"] = {
        "direct": {"path": os.path.abspath(args.direct),
                   "sha256": sha256_file(args.direct)},
        "metadata": {"path": os.path.abspath(args.metadata),
                     "sha256": sha256_file(args.metadata)},
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
        "phrases": result["phrase_count"],
        "accepted_phrases": result["accepted_phrase_count"],
        "turns": result["turn_count"],
        "out": os.path.abspath(args.out),
    }, ensure_ascii=False))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    spans = subparsers.add_parser("spans")
    spans.add_argument("--asr", required=True)
    spans.add_argument("--align", required=True)
    spans.add_argument("--config", required=True)
    spans.add_argument("--out", required=True)
    spans.add_argument("--metadata", required=True)
    build = subparsers.add_parser("build")
    build.add_argument("--direct", required=True)
    build.add_argument("--metadata", required=True)
    build.add_argument("--titanet", required=True)
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
        raise SystemExit(f"speaker punctuation phrase candidate: {error}")
