#!/usr/bin/env python3
"""Build VAD-bounded relative-top-1 phrase evidence without result scoring."""

import argparse
import bisect
import csv
from collections import defaultdict
import json
import os
import tomllib

import speaker_posterior_bounded_phrase_candidate as posterior
import speaker_punctuation_phrase_candidate as phrase_tools


EPSILON = 1e-9
REQUIRED_POLICY = {
    "enabled",
    "require_vad_support",
    "require_session_registry_agreement",
    "require_robust_gallery_agreement",
    "require_raw_local_identity_agreement",
    "require_known_baseline_conflict",
    "allow_exact_regular_anchor_challenge",
}


def load_policy(path):
    voiceprint = posterior.load_policy(path)
    with open(path, "rb") as source:
        document = tomllib.load(source)
    section = document.get("relative_top1_phrase", {})
    missing = sorted(REQUIRED_POLICY - set(section))
    if missing:
        raise ValueError("relative-top1 policy missing: " + ",".join(missing))
    policy = {name: bool(section[name]) for name in REQUIRED_POLICY}
    if not all(policy.values()):
        raise ValueError("all relative-top1 safety contracts are mandatory")
    for name in ("minimum_sustained_run_sec", "minimum_piece_duration_sec"):
        if name not in section:
            raise ValueError(f"relative-top1 policy missing: {name}")
        value = float(section[name])
        if abs(value - float(voiceprint[name])) > EPSILON:
            raise ValueError(f"relative-top1 {name} differs from FR16J")
        policy[name] = value
    policy["voiceprint"] = voiceprint
    return policy


def read_vad_timeline(path):
    path = os.fspath(path)
    if path.endswith(".tsv"):
        intervals = []
        with open(path, encoding="ascii", newline="") as source:
            reader = csv.DictReader(source, delimiter="\t")
            required = {"start_sec", "end_sec"}
            if not required.issubset(reader.fieldnames or []):
                raise ValueError("VAD span TSV is missing required columns")
            for row in reader:
                start = float(row["start_sec"])
                end = float(row["end_sec"])
                if start < 0.0 or end <= start:
                    raise ValueError("invalid VAD interval")
                if intervals and start < intervals[-1][1] - EPSILON:
                    raise ValueError("VAD intervals overlap or are not monotonic")
                intervals.append((start, end))
        if not intervals:
            raise ValueError("VAD span TSV is empty")
        return intervals
    document = posterior.load_json(path)
    timeline = document.get("timeline", document)
    tracks = timeline.get("tracks", [])
    vad_tracks = [item for item in tracks if item.get("kind") == "vad"]
    if len(vad_tracks) != 1:
        raise ValueError("timeline must contain exactly one VAD track")
    intervals = []
    for item in vad_tracks[0].get("entries", []):
        start = float(item["start"])
        end = float(item["end"])
        if start < 0.0 or end <= start:
            raise ValueError("invalid VAD interval")
        if intervals and start < intervals[-1][1] - EPSILON:
            raise ValueError("VAD intervals overlap or are not monotonic")
        intervals.append((start, end))
    if not intervals:
        raise ValueError("VAD track is empty")
    return intervals


def vad_contains(intervals, starts, time_sec):
    index = bisect.bisect_right(starts, time_sec + EPSILON) - 1
    return index >= 0 and time_sec < intervals[index][1] - EPSILON


def relative_top1_runs(frames, frame_sec, start, end, vad, policy):
    times = [item["time"] for item in frames]
    vad_starts = [item[0] for item in vad]
    first = bisect.bisect_left(times, start - 0.5 * frame_sec)
    limit = bisect.bisect_left(times, end + 0.5 * frame_sec)
    runs = []
    current = []

    def finish():
        if not current:
            return
        run_start = max(start, current[0]["time"] - 0.5 * frame_sec)
        run_end = min(end, current[-1]["time"] + 0.5 * frame_sec)
        if (run_end - run_start + EPSILON <
                policy["minimum_sustained_run_sec"]):
            return
        runs.append({
            "start": run_start,
            "end": run_end,
            "local_speaker": current[0]["local"],
            "frame_start": current[0]["frame"],
            "frame_end": current[-1]["frame"] + 1,
            "frame_count": len(current),
            "mean_probability": sum(
                item["probability"] for item in current) / len(current),
        })

    for item in frames[first:limit]:
        speech = vad_contains(vad, vad_starts, item["time"])
        if (not speech or
                (current and item["local"] != current[-1]["local"])):
            finish()
            current = []
        if speech:
            current.append(item)
    finish()
    return runs


def enumerate_pieces(metadata, frames, frame_sec, vad, policy):
    pieces = []
    voiceprint = policy["voiceprint"]
    for phrase in metadata["phrases"]:
        text_id = int(phrase["text_id"])
        source = metadata["asr"][str(text_id)]["text"]
        units = metadata["align"][str(text_id)]
        runs = relative_top1_runs(
            frames, frame_sec, float(phrase["start"]), float(phrase["end"]),
            vad, policy)
        for value in posterior.phrase_pieces(
                phrase, source, units, runs, voiceprint):
            pieces.append({
                "evidence_id": f"relative_top1_phrase:{len(pieces)}",
                "phrase_evidence_id": phrase["evidence_id"],
                "text_id": text_id,
                "text": source[value["source_start"]:value["source_end"]],
                **value,
            })
    return pieces


def known_conflicts(fragments, selected):
    return sorted({
        str(item["entry"]["speaker_id"])
        for item in fragments
        if item["entry"].get("speaker_id") is not None and
        item["entry"].get("speaker_id") != selected
    })


def source_fragments(track):
    grouped = defaultdict(list)
    cursors = defaultdict(int)
    for item in track:
        text_id = int(item["text_id"])
        text = str(item.get("text", ""))
        start = cursors[text_id]
        grouped[text_id].append({
            "source_start": start,
            "source_end": start + len(text),
            "entry": item,
        })
        cursors[text_id] += len(text)
    return dict(grouped)


def validate_source_text(fragments, metadata, label):
    expected_ids = {int(value) for value in metadata["asr"]}
    if set(fragments) != expected_ids:
        raise ValueError(f"{label} text ID set differs from metadata")
    for text_id, values in fragments.items():
        actual = "".join(str(value["entry"].get("text", ""))
                         for value in values)
        expected = str(metadata["asr"][str(text_id)]["text"])
        if actual != expected:
            raise ValueError(f"{label} source text mismatch for {text_id}")


def decide_piece(piece, current, robust, fragments, active_ids,
                 local_to_id, policy):
    voiceprint = policy["voiceprint"]
    current_id, current_reason, current_ranked = phrase_tools.select_identity(
        current, active_ids, voiceprint)
    robust_id, robust_reason, robust_ranked = phrase_tools.select_identity(
        robust, active_ids, voiceprint)
    local = int(piece["local_speaker"])
    mapped_id = local_to_id.get(local)
    conflicts = known_conflicts(fragments, current_id)
    accepted = False
    reason = "relative_top1_session_registry_abstention"
    if current_id is None:
        reason = "relative_top1_session_registry_abstention"
    elif robust_id is None:
        reason = "relative_top1_robust_gallery_abstention"
    elif current_id != robust_id:
        reason = "relative_top1_registry_disagreement"
    elif mapped_id != current_id:
        reason = "relative_top1_raw_local_identity_disagreement"
    elif not conflicts:
        reason = "relative_top1_known_baseline_conflict_missing"
    else:
        accepted = True
        reason = "relative_top1_dual_voiceprint_consensus"
    return {
        **piece,
        "mapped_speaker_id": mapped_id,
        "session_registry_speaker_id": current_id,
        "session_registry_reason": current_reason,
        "session_registry_ranked": [
            {"speaker_id": identity, "score": score}
            for identity, score in current_ranked
        ],
        "robust_gallery_speaker_id": robust_id,
        "robust_gallery_reason": robust_reason,
        "robust_gallery_ranked": [
            {"speaker_id": identity, "score": score}
            for identity, score in robust_ranked
        ],
        "known_baseline_conflict_ids": conflicts,
        "selected_speaker_id": current_id if accepted else None,
        "accepted": accepted,
        "reason": reason,
    }


def build_candidate(baseline, metadata, current_evidence, robust_evidence,
                    manifest, policy):
    if baseline.get("kind") != "orator_frozen_speaker_candidate":
        raise ValueError("baseline is not a frozen speaker candidate")
    if metadata.get("kind") != "orator_relative_top1_phrase_spans":
        raise ValueError("metadata is not relative-top1 phrase evidence")
    fragments_by_text = source_fragments(baseline.get("track", []))
    validate_source_text(fragments_by_text, metadata, "baseline")
    active_ids = [str(value) for value in baseline["active_speaker_ids"]]
    local_to_id = {
        int(local): str(identity)
        for local, identity in manifest["mapping"].items()
    }
    if sorted(local_to_id.values()) != sorted(active_ids):
        raise ValueError("manifest identity set differs from baseline")
    id_to_local = {identity: local for local, identity in local_to_id.items()}
    overlays = defaultdict(list)
    decisions = []
    for piece in metadata["pieces"]:
        evidence_id = str(piece["evidence_id"])
        if evidence_id not in current_evidence:
            raise ValueError(f"missing session evidence {evidence_id}")
        if evidence_id not in robust_evidence:
            raise ValueError(f"missing robust evidence {evidence_id}")
        text_id = int(piece["text_id"])
        fragments = posterior.overlapping_fragments(
            piece, fragments_by_text[text_id])
        decision = decide_piece(
            piece, current_evidence[evidence_id], robust_evidence[evidence_id],
            fragments, active_ids, local_to_id, policy)
        decisions.append(decision)
        if decision["accepted"]:
            overlays[text_id].append({
                "source_start": int(piece["source_start"]),
                "source_end": int(piece["source_end"]),
                "speaker_id": decision["selected_speaker_id"],
                "reason": decision["reason"],
            })
    track = posterior.project_track(
        metadata, fragments_by_text, overlays, id_to_local)
    validate_source_text(source_fragments(track), metadata, "projected")
    return {
        "schema_version": 1,
        "kind": "orator_frozen_speaker_candidate",
        "candidate_kind": "v21_relative_top1_dual_voiceprint_consensus",
        "audio_sec": float(baseline["audio_sec"]),
        "sample_rate": int(baseline["sample_rate"]),
        "active_speaker_ids": active_ids,
        "source_turn_count": len(baseline.get("track", [])),
        "turn_count": len(track),
        "piece_count": len(decisions),
        "accepted_piece_count": sum(item["accepted"] for item in decisions),
        "policy": {name: value for name, value in policy.items()
                   if name != "voiceprint"},
        "voiceprint_policy": policy["voiceprint"],
        "piece_decisions": decisions,
        "track": track,
    }


def command_spans(args, policy):
    phrases = posterior.load_json(args.phrases)
    frames, frame_sec = posterior.read_frames(args.frames)
    vad = read_vad_timeline(args.timeline)
    pieces = enumerate_pieces(phrases, frames, frame_sec, vad, policy)
    posterior.write_spans(args.out, pieces)
    result = {
        "schema_version": 1,
        "kind": "orator_relative_top1_phrase_spans",
        "frame_period_sec": frame_sec,
        "policy": {name: value for name, value in policy.items()
                   if name != "voiceprint"},
        "voiceprint_policy": policy["voiceprint"],
        "asr": phrases["asr"],
        "align": phrases["align"],
        "pieces": pieces,
        "piece_count": len(pieces),
        "sources": {
            name: {"path": os.path.abspath(path),
                   "sha256": posterior.sha256_file(path)}
            for name, path in {
                "phrases": args.phrases,
                "frames": args.frames,
                "timeline": args.timeline,
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
    result = build_candidate(
        posterior.load_json(args.baseline), posterior.load_json(args.metadata),
        phrase_tools.read_titanet(args.session_titanet),
        phrase_tools.read_titanet(args.robust_titanet),
        posterior.load_json(args.manifest), policy)
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
    spans.add_argument("--phrases", required=True)
    spans.add_argument("--frames", required=True)
    spans.add_argument("--timeline", required=True)
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
        raise SystemExit(f"speaker relative-top1 phrase candidate: {error}")
