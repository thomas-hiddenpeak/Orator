#!/usr/bin/env python3
"""Export typed, reference-free inputs for business-speaker replay."""

import argparse
import csv
import hashlib
import json
import os


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def encode_text(value):
    return str(value).encode("utf-8").hex()


def timeline_from(package):
    timeline = package.get("timeline", package)
    if not isinstance(timeline, dict) or "tracks" not in timeline:
        raise ValueError("timeline document has no tracks")
    return timeline


def tracks_by_kind(timeline):
    return {track["kind"]: track.get("entries", [])
            for track in timeline["tracks"]}


def slot_mapping(candidate):
    mapping = {}
    for session in candidate.get("sessions", {}).values():
        for slot in session.get("slots", []):
            local = int(slot["local_speaker"])
            speaker_id = str(slot["speaker_id"])
            if local in mapping and mapping[local] != speaker_id:
                raise ValueError(f"conflicting mapping for local {local}")
            mapping[local] = speaker_id
    if not mapping:
        raise ValueError("mapping candidate has no session slots")
    return mapping


def write_diar(path, diar_csv, mapping):
    count = 0
    with open(diar_csv, encoding="utf-8", newline="") as source, open(
            path, "w", encoding="utf-8", newline="") as output:
        reader = csv.DictReader(source)
        writer = csv.writer(output, delimiter="\t", lineterminator="\n")
        writer.writerow([
            "start_sec", "end_sec", "local_speaker", "confidence",
            "speaker_id"])
        for row in reader:
            local = int(row["local_speaker"])
            if local not in mapping:
                raise ValueError(f"no identity mapping for local {local}")
            writer.writerow([
                row["start_sec"], row["end_sec"], local,
                row["confidence"], mapping[local]])
            count += 1
    return count


def write_speaker_track(path, entries):
    with open(path, "w", encoding="utf-8", newline="") as output:
        writer = csv.writer(output, delimiter="\t", lineterminator="\n")
        writer.writerow([
            "start_sec", "end_sec", "local_speaker", "confidence",
            "speaker_id"])
        for entry in entries:
            writer.writerow([
                entry["start"], entry["end"], int(entry["speaker"]),
                entry["confidence"], entry.get("speaker_id", "")])
    return len(entries)


def write_asr(path, entries):
    with open(path, "w", encoding="ascii", newline="") as output:
        writer = csv.writer(output, delimiter="\t", lineterminator="\n")
        writer.writerow(["text_id", "start_sec", "end_sec", "text_hex"])
        for entry in entries:
            writer.writerow([
                int(entry["text_id"]), entry["start"], entry["end"],
                encode_text(entry.get("text", ""))])
    return len(entries)


def write_align(path, entries):
    count = 0
    with open(path, "w", encoding="ascii", newline="") as output:
        writer = csv.writer(output, delimiter="\t", lineterminator="\n")
        writer.writerow([
            "text_id", "group_start_sec", "group_end_sec", "unit_start_sec",
            "unit_end_sec", "text_hex"])
        for entry in entries:
            for unit in entry.get("units", []):
                writer.writerow([
                    int(entry["text_id"]), entry["start"], entry["end"],
                    unit["start"], unit["end"],
                    encode_text(unit.get("text", ""))])
                count += 1
    return count


def encode_scores(scores):
    return ",".join(
        f"{item['speaker_id']}:{item['score']}" for item in scores)


def write_voiceprint(path, entries):
    with open(path, "w", encoding="utf-8", newline="") as output:
        writer = csv.writer(output, delimiter="\t", lineterminator="\n")
        writer.writerow([
            "evidence_id", "kind", "text_id", "source_start",
            "source_end", "start", "end", "embedding_available",
            "session_gallery_complete", "robust_gallery_complete",
            "session_scores", "robust_scores"])
        for entry in entries:
            writer.writerow([
                entry["evidence_id"], entry["evidence_kind"],
                int(entry["text_id"]), int(entry["source_start"]),
                int(entry["source_end"]), entry["start"], entry["end"],
                int(bool(entry["embedding_available"])),
                int(bool(entry.get("session_gallery_complete", False))),
                int(bool(entry["robust_gallery_complete"])),
                encode_scores(entry.get("session_scores", [])),
                encode_scores(entry.get("robust_scores", []))])
    return len(entries)


def export_inputs(timeline_path, diar_path, mapping_path, out_prefix,
                  direct_timeline_tracks=False):
    with open(timeline_path, encoding="utf-8") as source:
        timeline_package = json.load(source)
    timeline = timeline_from(timeline_package)
    tracks = tracks_by_kind(timeline)
    if "asr" not in tracks or "align" not in tracks:
        raise ValueError("timeline requires asr and align tracks")

    outputs = {
        "diar": os.path.abspath(out_prefix + "-diar.tsv"),
        "asr": os.path.abspath(out_prefix + "-asr.tsv"),
        "align": os.path.abspath(out_prefix + "-align.tsv"),
    }
    if direct_timeline_tracks:
        required = {"diarization", "primary_speaker", "speaker_voiceprint"}
        missing = sorted(required - set(tracks))
        if missing:
            raise ValueError(
                "timeline is missing replay tracks: " + ", ".join(missing))
        outputs.update({
            "primary": os.path.abspath(out_prefix + "-primary.tsv"),
            "voiceprint": os.path.abspath(out_prefix + "-voiceprint.tsv"),
        })
    parent = os.path.dirname(outputs["diar"])
    if parent:
        os.makedirs(parent, exist_ok=True)
    if direct_timeline_tracks:
        mapping = {
            int(entry["speaker"]): str(entry.get("speaker_id", ""))
            for entry in tracks["diarization"]
        }
        counts = {
            "diar": write_speaker_track(
                outputs["diar"], tracks["diarization"]),
            "primary": write_speaker_track(
                outputs["primary"], tracks["primary_speaker"]),
            "asr": write_asr(outputs["asr"], tracks["asr"]),
            "align_units": write_align(outputs["align"], tracks["align"]),
            "voiceprint": write_voiceprint(
                outputs["voiceprint"], tracks["speaker_voiceprint"]),
        }
    else:
        if not diar_path or not mapping_path:
            raise ValueError(
                "legacy export requires diar segments and mapping candidate")
        with open(mapping_path, encoding="utf-8") as source:
            mapping_candidate = json.load(source)
        mapping = slot_mapping(mapping_candidate)
        counts = {
            "diar": write_diar(outputs["diar"], diar_path, mapping),
            "asr": write_asr(outputs["asr"], tracks["asr"]),
            "align_units": write_align(outputs["align"], tracks["align"]),
        }
    sources = {
        "timeline": {"path": os.path.abspath(timeline_path),
                     "sha256": sha256_file(timeline_path)},
    }
    if not direct_timeline_tracks:
        sources.update({
            "diar_segments": {"path": os.path.abspath(diar_path),
                              "sha256": sha256_file(diar_path)},
            "mapping_candidate": {"path": os.path.abspath(mapping_path),
                                  "sha256": sha256_file(mapping_path)},
        })
    manifest = {
        "schema_version": 2,
        "kind": "orator_business_speaker_replay_inputs",
        "source_mode": ("direct_timeline_tracks" if direct_timeline_tracks
                        else "legacy_offline_mapping"),
        "audio_sec": float(timeline["audio_sec"]),
        "sample_rate": int(timeline["sample_rate"]),
        "counts": counts,
        "mapping": {str(key): value for key, value in sorted(mapping.items())},
        "sources": sources,
        "outputs": {
            name: {"path": path, "sha256": sha256_file(path)}
            for name, path in outputs.items()
        },
    }
    manifest_path = os.path.abspath(out_prefix + "-manifest.json")
    with open(manifest_path, "w", encoding="utf-8") as output:
        json.dump(manifest, output, ensure_ascii=False, indent=2)
    return manifest_path, manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--timeline", required=True)
    parser.add_argument("--diar-segments")
    parser.add_argument("--mapping-candidate")
    parser.add_argument("--out-prefix", required=True)
    parser.add_argument("--direct-timeline-tracks", action="store_true")
    args = parser.parse_args()
    manifest_path, manifest = export_inputs(
        args.timeline, args.diar_segments, args.mapping_candidate,
        args.out_prefix, args.direct_timeline_tracks)
    print(json.dumps({
        "counts": manifest["counts"],
        "mapping": manifest["mapping"],
        "manifest": manifest_path,
    }, ensure_ascii=False))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, csv.Error) as error:
        raise SystemExit(f"speaker business replay inputs: {error}")
