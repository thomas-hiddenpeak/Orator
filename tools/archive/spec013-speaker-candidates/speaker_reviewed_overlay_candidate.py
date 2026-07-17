#!/usr/bin/env python3
"""Compose exact, allowlisted reviewed overlays onto a frozen candidate."""

import argparse
import hashlib
import json
import os
import tomllib
from collections import defaultdict

import speaker_punctuation_phrase_candidate as phrase_tools


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
    section = document.get("reviewed_speaker_overlay", {})
    required = {"enabled", "allowed_reasons"}
    missing = sorted(required - set(section))
    if missing:
        raise ValueError("reviewed overlay policy missing: " +
                         ",".join(missing))
    if section["enabled"] is not True:
        raise ValueError("reviewed speaker overlays must be enabled")
    reasons = section["allowed_reasons"]
    if not isinstance(reasons, list) or not reasons:
        raise ValueError("reviewed overlay allowlist must be non-empty")
    normalized = [str(reason) for reason in reasons]
    if any(not reason for reason in normalized):
        raise ValueError("reviewed overlay reason must be non-empty")
    if len(set(normalized)) != len(normalized):
        raise ValueError("reviewed overlay allowlist contains duplicates")
    forbidden_tokens = ("baseline", "neighbour", "neighbor", "propagation")
    if any(token in reason.lower()
           for reason in normalized for token in forbidden_tokens):
        raise ValueError("reviewed overlay allowlist contains generic reason")
    return {"enabled": True, "allowed_reasons": normalized}


def read_mapping(document):
    raw = document.get("mapping")
    if not isinstance(raw, dict) or not raw:
        raise ValueError("mapping document has no local-slot mapping")
    mapping = {int(local): str(speaker_id)
               for local, speaker_id in raw.items()}
    if len(mapping) != len(set(mapping.values())):
        raise ValueError("mapping identities are not one-to-one")
    return mapping


def candidate_fragments(candidate):
    if candidate.get("kind") != "orator_frozen_speaker_candidate":
        raise ValueError("input is not a frozen speaker candidate")
    grouped = defaultdict(list)
    last_text_id = None
    for index, entry in enumerate(candidate.get("track", [])):
        text_id = int(entry["text_id"])
        if last_text_id is not None and text_id < last_text_id:
            raise ValueError("candidate track is not ordered by text_id")
        last_text_id = text_id
        grouped[text_id].append((index, entry))
    if not grouped:
        raise ValueError("candidate track is empty")
    output = {}
    for text_id, entries in grouped.items():
        cursor = 0
        fragments = []
        for index, entry in entries:
            text = str(entry.get("text", ""))
            if not text:
                raise ValueError("candidate contains an empty source fragment")
            fragments.append({
                "track_index": index,
                "source_start": cursor,
                "source_end": cursor + len(text),
                "entry": entry,
            })
            cursor += len(text)
        output[text_id] = fragments
    return output


def source_text(fragments):
    return "".join(str(item["entry"]["text"]) for item in fragments)


def validate_candidate_sources(fragments_by_text, metadata):
    asr = metadata.get("asr")
    align = metadata.get("align")
    if not isinstance(asr, dict) or not isinstance(align, dict):
        raise ValueError("metadata has no ASR/alignment source")
    expected_ids = {int(value) for value in asr}
    if set(fragments_by_text) != expected_ids:
        raise ValueError("candidate and metadata text_id sets differ")
    if set(align) != set(asr):
        raise ValueError("metadata ASR/alignment text_id sets differ")
    for text_id, fragments in fragments_by_text.items():
        if source_text(fragments) != str(asr[str(text_id)]["text"]):
            raise ValueError(f"candidate source differs for text_id {text_id}")


def collect_overlays(retained_fragments, allowed_reasons, active_ids):
    allowed = set(allowed_reasons)
    overlays = defaultdict(list)
    decisions = []
    for text_id in sorted(retained_fragments):
        for fragment in retained_fragments[text_id]:
            entry = fragment["entry"]
            reason = str(entry.get("decision_reason", ""))
            if reason not in allowed:
                continue
            speaker_id = entry.get("speaker_id")
            if speaker_id not in active_ids:
                raise ValueError("allowlisted overlay has no active identity")
            overlay = {
                "text_id": text_id,
                "source_start": fragment["source_start"],
                "source_end": fragment["source_end"],
                "speaker_id": str(speaker_id),
                "reason": reason,
                "retained_track_index": fragment["track_index"],
            }
            overlays[text_id].append(overlay)
            decisions.append(dict(overlay))
    validate_overlays(overlays)
    return dict(overlays), decisions


def validate_overlays(overlays_by_text):
    for text_id, overlays in overlays_by_text.items():
        previous_end = -1
        for overlay in sorted(
                overlays,
                key=lambda item: (item["source_start"], item["source_end"])):
            start = int(overlay["source_start"])
            end = int(overlay["source_end"])
            if start < 0 or end <= start:
                raise ValueError(f"overlay has invalid range for text_id {text_id}")
            if start < previous_end:
                raise ValueError(f"reviewed overlays conflict for text_id {text_id}")
            previous_end = end


def partial_span_time(source_start, source_end, times, fragment):
    entry = fragment["entry"]
    aligned = [
        times[index] for index in range(source_start, source_end)
        if times[index] is not None and
        float(times[index]["end"]) > float(times[index]["start"])
    ]
    if aligned:
        start = max(float(entry["start"]),
                    min(float(item["start"]) for item in aligned))
        end = min(float(entry["end"]),
                  max(float(item["end"]) for item in aligned))
        if end > start + EPSILON:
            return start, end

    owner_start = int(fragment["source_start"])
    owner_end = int(fragment["source_end"])
    width = owner_end - owner_start
    if width <= 0:
        raise ValueError("base fragment has no source width")
    duration = float(entry["end"]) - float(entry["start"])
    start_ratio = (source_start - owner_start) / width
    end_ratio = (source_end - owner_start) / width
    return (float(entry["start"]) + duration * start_ratio,
            float(entry["start"]) + duration * end_ratio)


def reproject_preserving_base(text_id, source, units, fragments, overlays,
                              id_to_local):
    owners = []
    labels = []
    for owner, fragment in enumerate(fragments):
        entry = fragment["entry"]
        label = {
            "speaker_id": entry.get("speaker_id"),
            "speaker": int(entry.get("speaker", -1)),
            "speaker_uncertain": bool(
                entry.get("speaker_uncertain", False)),
            "reason": str(entry.get("decision_reason", "baseline")),
        }
        for _ in str(entry.get("text", "")):
            owners.append(owner)
            labels.append(dict(label))
    if len(labels) != len(source):
        raise ValueError(f"base source differs for text_id {text_id}")

    for item in overlays:
        label = {
            "speaker_id": item["speaker_id"],
            "speaker": id_to_local[item["speaker_id"]],
            "speaker_uncertain": False,
            "reason": item["reason"],
        }
        for index in range(item["source_start"], item["source_end"]):
            labels[index] = dict(label)

    bounds = [0]
    for index in range(1, len(source)):
        left = labels[index - 1]
        right = labels[index]
        if (owners[index - 1] != owners[index] or
                (left["speaker_id"], left["speaker_uncertain"],
                 left["reason"]) !=
                (right["speaker_id"], right["speaker_uncertain"],
                 right["reason"])):
            bounds.append(index)
    bounds.append(len(source))

    times = phrase_tools.aligned_character_times(source, units)
    output = []
    for piece, (start, end) in enumerate(zip(bounds, bounds[1:])):
        if end <= start:
            continue
        owner = owners[start]
        if any(value != owner for value in owners[start:end]):
            raise ValueError("composed fragment crosses a base owner")
        fragment = fragments[owner]
        if (start == fragment["source_start"] and
                end == fragment["source_end"]):
            span_start = float(fragment["entry"]["start"])
            span_end = float(fragment["entry"]["end"])
        else:
            span_start, span_end = partial_span_time(
                start, end, times, fragment)
        if span_end <= span_start + EPSILON:
            raise ValueError("composed fragment has non-positive duration")
        label = labels[start]
        output.append({
            "turn_id": f"reviewed_overlay:{text_id}:{piece}",
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
        raise ValueError("composed candidate changed immutable source text")
    return output


def build_candidate(base, retained, metadata, mapping_document, policy):
    mapping = read_mapping(mapping_document)
    id_to_local = {speaker_id: local
                   for local, speaker_id in mapping.items()}
    active_ids = sorted(id_to_local)
    retained_ids = sorted(retained.get("active_speaker_ids", []))
    if retained_ids != active_ids:
        raise ValueError("retained candidate identities differ from mapping")

    base_fragments = candidate_fragments(base)
    retained_fragments = candidate_fragments(retained)
    validate_candidate_sources(base_fragments, metadata)
    validate_candidate_sources(retained_fragments, metadata)
    overlays_by_text, decisions = collect_overlays(
        retained_fragments, policy["allowed_reasons"], set(active_ids))

    for text_id, fragments in base_fragments.items():
        for fragment in fragments:
            entry = fragment["entry"]
            speaker_id = entry.get("speaker_id")
            if speaker_id is None:
                continue
            if speaker_id not in id_to_local:
                raise ValueError("base candidate has an unmapped identity")
            if int(entry.get("speaker", -1)) != id_to_local[speaker_id]:
                raise ValueError("base candidate local/global identity mismatch")

    track = []
    for text_id in sorted(base_fragments):
        source = str(metadata["asr"][str(text_id)]["text"])
        pieces = reproject_preserving_base(
            text_id, source, metadata["align"][str(text_id)],
            base_fragments[text_id], overlays_by_text.get(text_id, []),
            id_to_local)
        track.extend(pieces)

    by_text = defaultdict(str)
    for entry in track:
        by_text[int(entry["text_id"])] += str(entry["text"])
    for text_id, text in by_text.items():
        if text != str(metadata["asr"][str(text_id)]["text"]):
            raise ValueError("composed candidate changed immutable source text")

    return {
        "schema_version": 1,
        "kind": "orator_frozen_speaker_candidate",
        "candidate_kind": "v21_primary_top1_reviewed_overlay",
        "audio_sec": float(base["audio_sec"]),
        "sample_rate": int(base["sample_rate"]),
        "active_speaker_ids": active_ids,
        "source_turn_count": len(base["track"]),
        "turn_count": len(track),
        "overlay_count": len(decisions),
        "policy": policy,
        "overlay_decisions": decisions,
        "track": track,
    }


def command_build(args):
    base = load_json(args.base)
    retained = load_json(args.retained)
    metadata = load_json(args.metadata)
    mapping = load_json(args.mapping)
    policy = load_policy(args.config)
    result = build_candidate(base, retained, metadata, mapping, policy)
    result["sources"] = {
        "base": {"path": os.path.abspath(args.base),
                 "sha256": sha256_file(args.base)},
        "retained": {"path": os.path.abspath(args.retained),
                     "sha256": sha256_file(args.retained)},
        "metadata": {"path": os.path.abspath(args.metadata),
                     "sha256": sha256_file(args.metadata)},
        "mapping": {"path": os.path.abspath(args.mapping),
                    "sha256": sha256_file(args.mapping)},
        "config": {"path": os.path.abspath(args.config),
                   "sha256": sha256_file(args.config)},
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
    parser.add_argument("--base", required=True)
    parser.add_argument("--retained", required=True)
    parser.add_argument("--metadata", required=True)
    parser.add_argument("--mapping", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--out", required=True)
    command_build(parser.parse_args())


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as error:
        raise SystemExit(f"speaker reviewed overlay candidate: {error}")
