#!/usr/bin/env python3
"""Prepare and validate Spec 013 full-session manual review ledgers."""

import argparse
import datetime
import hashlib
import json
import os
import re
import subprocess
import sys


TIMESTAMP_RE = re.compile(r"^(\d{2}):(\d{2}):(\d{2})\s+(.+?)\s*$")
JUDGMENT_TEMPLATE = {
    "result": "unreviewed",
    "speaker_eval": "unreviewed",
    "system_speakers": [],
    "correct_speaker_intervals": [],
    "boundary_start_offset_sec": None,
    "boundary_end_offset_sec": None,
    "confident_wrong": None,
    "evidence_ids": [],
    "context_notes": "",
    "reviewer": "",
    "reviewed_utc": None,
}


def utc_now():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def content_sha256(value):
    copy = dict(value)
    copy.pop("content_sha256", None)
    payload = json.dumps(
        copy, ensure_ascii=False, sort_keys=True,
        separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(payload).hexdigest()


def load_json(path):
    with open(path, encoding="utf-8") as source:
        return json.load(source)


def write_json(path, value):
    value["content_sha256"] = content_sha256(value)
    parent = os.path.dirname(os.path.abspath(path))
    os.makedirs(parent, exist_ok=True)
    with open(path, "w", encoding="utf-8") as output:
        json.dump(value, output, ensure_ascii=False, indent=2)


def audio_duration(path):
    result = subprocess.run(
        ["ffprobe", "-v", "error", "-show_entries", "format=duration",
         "-of", "default=noprint_wrappers=1:nokey=1", path],
        capture_output=True, text=True, check=False)
    if result.returncode != 0:
        raise RuntimeError("ffprobe failed: " + result.stderr.strip())
    return float(result.stdout.strip())


def parse_reference(path, final_end):
    rows = []
    current = None
    with open(path, encoding="utf-8") as source:
        for line_number, raw in enumerate(source, start=1):
            line = raw.strip()
            match = TIMESTAMP_RE.match(line)
            if match:
                if current is not None:
                    rows.append(current)
                hours, minutes, seconds, speaker = match.groups()
                start = int(hours) * 3600 + int(minutes) * 60 + int(seconds)
                current = {
                    "source_line": line_number,
                    "source_timestamp": f"{hours}:{minutes}:{seconds}",
                    "source_start_sec": float(start),
                    "source_speaker": speaker.strip(),
                    "text_lines": [],
                }
            elif current is not None and line and not line.startswith("【内容由"):
                current["text_lines"].append(line)
    if current is not None:
        rows.append(current)

    entries = []
    for index, row in enumerate(rows):
        provisional_end = (
            rows[index + 1]["source_start_sec"]
            if index + 1 < len(rows) else final_end)
        source_text = "".join(row.pop("text_lines"))
        source = {
            **row,
            "source_text": source_text,
            "provisional_end_sec": provisional_end,
        }
        source_payload = json.dumps(
            source, ensure_ascii=False, sort_keys=True,
            separators=(",", ":")).encode("utf-8")
        entries.append({
            "reference_id": f"ref-{index + 1:04d}",
            **source,
            "source_sha256": hashlib.sha256(source_payload).hexdigest(),
            "adjudication": {
                "audible_start_sec": None,
                "audible_end_sec": None,
                "overlap_participants": [],
                "criticality": "unreviewed",
                "ambiguity": "unreviewed",
                "acceptable_speakers": [row["source_speaker"]],
                "acceptable_semantic_equivalents": [],
                "context_summary": "",
                "notes": "",
                "reviewer": "",
                "adjudicated_utc": None,
            },
        })
    return entries


def init_ledger(args):
    duration = args.audio_sec or audio_duration(args.audio)
    entries = parse_reference(args.reference, duration)
    if len(entries) != args.expected_entries:
        raise ValueError(
            f"expected {args.expected_entries} reference entries, got "
            f"{len(entries)}")
    ledger = {
        "schema_version": 1,
        "kind": "orator_reference_ledger",
        "created_utc": utc_now(),
        "reference": {
            "path": args.reference,
            "sha256": sha256_file(args.reference),
            "entry_count": len(entries),
        },
        "audio": {
            "path": args.audio,
            "sha256": sha256_file(args.audio),
            "duration_sec": duration,
            "sample_rate": 16000,
        },
        "entries": entries,
    }
    write_json(args.out, ledger)


def overlap(a_start, a_end, b_start, b_end):
    return max(0.0, min(a_end, b_end) - max(a_start, b_start))


def terminal_timeline(package):
    if isinstance(package.get("timeline"), dict):
        return package["timeline"]
    return package


def track_map(timeline):
    return {
        track.get("kind"): track.get("entries", [])
        for track in timeline.get("tracks", [])
        if isinstance(track, dict) and track.get("kind")
    }


def evidence_id(kind, item, index):
    text_id = item.get("text_id") if isinstance(item, dict) else None
    return f"{kind}:{text_id if text_id is not None else index}"


def select_evidence(entries, start, end, kind):
    selected = []
    for index, item in enumerate(entries):
        if not isinstance(item, dict):
            continue
        item_start = float(item.get("start", 0.0))
        item_end = float(item.get("end", item_start))
        if overlap(start, end, item_start, item_end) <= 0.0:
            continue
        selected.append({"evidence_id": evidence_id(kind, item, index), **item})
    return selected


def prepare_review(args):
    ledger = load_json(args.ledger)
    validate_ledger(ledger, require_complete=True)
    package = load_json(args.timeline)
    timeline = terminal_timeline(package)
    tracks = track_map(timeline)
    business = tracks.get("business_speaker", timeline.get("comprehensive", []))
    review_entries = []
    for entry in ledger["entries"]:
        adjudication = entry["adjudication"]
        start = adjudication["audible_start_sec"]
        end = adjudication["audible_end_sec"]
        if start is None:
            start = entry["source_start_sec"]
        if end is None:
            end = entry["provisional_end_sec"]
        evidence = {
            "business_speaker": select_evidence(
                business, start, end, "business_speaker"),
            "diarization": select_evidence(
                tracks.get("diarization", []), start, end, "diarization"),
            "asr": select_evidence(tracks.get("asr", []), start, end, "asr"),
            "align": select_evidence(
                tracks.get("align", []), start, end, "align"),
            "vad": select_evidence(tracks.get("vad", []), start, end, "vad"),
        }
        review_entries.append({
            "reference_id": entry["reference_id"],
            "evidence": evidence,
            "chronological_pass": dict(JUDGMENT_TEMPLATE),
            "reverse_block_pass": dict(JUDGMENT_TEMPLATE),
            "final": dict(JUDGMENT_TEMPLATE),
        })
    review = {
        "schema_version": 1,
        "kind": "orator_run_review",
        "created_utc": utc_now(),
        "ledger_path": args.ledger,
        "ledger_sha256": ledger["content_sha256"],
        "timeline_path": args.timeline,
        "timeline_sha256": sha256_file(args.timeline),
        "resolved_config_sha256": package.get("meta", {}).get(
            "resolved_config_sha256"),
        "entry_count": len(review_entries),
        "entries": review_entries,
    }
    write_json(args.out, review)


def validate_hash(value, name):
    expected = value.get("content_sha256")
    actual = content_sha256(value)
    if expected != actual:
        raise ValueError(f"{name} content hash mismatch")


def validate_ledger(ledger, require_complete):
    validate_hash(ledger, "ledger")
    entries = ledger.get("entries", [])
    if len(entries) != ledger.get("reference", {}).get("entry_count"):
        raise ValueError("ledger entry count mismatch")
    reference = ledger.get("reference", {})
    audio = ledger.get("audio", {})
    if sha256_file(reference.get("path", "")) != reference.get("sha256"):
        raise ValueError("reference file hash mismatch")
    if sha256_file(audio.get("path", "")) != audio.get("sha256"):
        raise ValueError("audio file hash mismatch")
    source_entries = parse_reference(
        reference["path"], float(audio["duration_sec"]))
    if len(source_entries) != len(entries):
        raise ValueError("current reference source count differs from ledger")
    errors = []
    previous_id = None
    immutable_fields = (
        "reference_id", "source_line", "source_timestamp", "source_start_sec",
        "source_speaker", "source_text", "provisional_end_sec", "source_sha256")
    for index, (entry, source_entry) in enumerate(
            zip(entries, source_entries)):
        expected_id = f"ref-{index + 1:04d}"
        for field in immutable_fields:
            if entry.get(field) != source_entry.get(field):
                errors.append(f"{expected_id}: immutable {field} changed")
        if entry.get("reference_id") != expected_id:
            errors.append(f"{expected_id}: unstable reference id")
        if previous_id == entry.get("reference_id"):
            errors.append(f"{expected_id}: duplicate reference id")
        previous_id = entry.get("reference_id")
        adjudication = entry.get("adjudication", {})
        if require_complete:
            start = adjudication.get("audible_start_sec")
            end = adjudication.get("audible_end_sec")
            if not isinstance(start, (int, float)) or not isinstance(
                    end, (int, float)) or end <= start:
                errors.append(f"{expected_id}: invalid audible interval")
            if adjudication.get("criticality") not in (
                    "critical", "noncritical"):
                errors.append(f"{expected_id}: criticality unreviewed")
            if adjudication.get("ambiguity") not in (
                    "none", "timestamp", "speaker", "text", "audio",
                    "overlap"):
                errors.append(f"{expected_id}: ambiguity unreviewed")
            if not adjudication.get("context_summary"):
                errors.append(f"{expected_id}: context summary missing")
            if not adjudication.get("reviewer"):
                errors.append(f"{expected_id}: adjudicator missing")
    if errors:
        raise ValueError("\n".join(errors[:50]))


def intervals_duration(intervals, start, end):
    clipped = []
    for interval in intervals:
        if not isinstance(interval, list) or len(interval) != 2:
            raise ValueError("correct_speaker_intervals must be [start,end] pairs")
        left = max(start, float(interval[0]))
        right = min(end, float(interval[1]))
        if right <= left:
            raise ValueError("correct speaker interval is empty or out of bounds")
        clipped.append((left, right))
    clipped.sort()
    merged = []
    for left, right in clipped:
        if not merged or left > merged[-1][1]:
            merged.append([left, right])
        else:
            merged[-1][1] = max(merged[-1][1], right)
    return sum(right - left for left, right in merged)


def validate_judgment(judgment, reference_id):
    if judgment.get("result") not in ("correct", "incorrect", "ambiguous"):
        raise ValueError(f"{reference_id}: result unreviewed")
    if judgment.get("speaker_eval") not in (
            "accurate", "mostly_accurate", "minor_confusion",
            "major_confusion", "unusable", "not_scorable"):
        raise ValueError(f"{reference_id}: speaker_eval unreviewed")
    if not isinstance(judgment.get("confident_wrong"), bool):
        raise ValueError(f"{reference_id}: confident_wrong unreviewed")
    if not judgment.get("reviewer") or not judgment.get("reviewed_utc"):
        raise ValueError(f"{reference_id}: review signature missing")


def validate_review(review, ledger, require_complete):
    validate_hash(review, "review")
    if review.get("ledger_sha256") != ledger.get("content_sha256"):
        raise ValueError("review points to a different reference ledger")
    review_entries = review.get("entries", [])
    if len(review_entries) != len(ledger.get("entries", [])):
        raise ValueError("review entry count mismatch")
    if require_complete:
        for expected, row in zip(ledger["entries"], review_entries):
            reference_id = expected["reference_id"]
            if row.get("reference_id") != reference_id:
                raise ValueError(f"{reference_id}: review order/id mismatch")
            validate_judgment(row["chronological_pass"], reference_id)
            validate_judgment(row["reverse_block_pass"], reference_id)
            validate_judgment(row["final"], reference_id)


def summarize(review, ledger):
    validate_ledger(ledger, require_complete=True)
    validate_review(review, ledger, require_complete=True)
    total_turns = len(ledger["entries"])
    correct_turns = 0
    critical_total = 0
    critical_correct = 0
    confident_wrong = 0
    total_sec = 0.0
    correct_sec = 0.0
    blocks = {}
    for ref, row in zip(ledger["entries"], review["entries"]):
        adjudication = ref["adjudication"]
        judgment = row["final"]
        start = float(adjudication["audible_start_sec"])
        end = float(adjudication["audible_end_sec"])
        duration = end - start
        correct_duration = intervals_duration(
            judgment["correct_speaker_intervals"], start, end)
        total_sec += duration
        correct_sec += correct_duration
        is_correct = judgment["result"] == "correct"
        correct_turns += int(is_correct)
        confident_wrong += int(judgment["confident_wrong"])
        if adjudication["criticality"] == "critical":
            critical_total += 1
            critical_correct += int(is_correct)
        block = int(start // 600)
        bucket = blocks.setdefault(
            str(block), {"total_turns": 0, "correct_turns": 0,
                         "total_sec": 0.0, "correct_sec": 0.0})
        bucket["total_turns"] += 1
        bucket["correct_turns"] += int(is_correct)
        bucket["total_sec"] += duration
        bucket["correct_sec"] += correct_duration

    for bucket in blocks.values():
        bucket["turn_accuracy"] = (
            bucket["correct_turns"] / bucket["total_turns"])
        bucket["speaker_time_accuracy"] = (
            bucket["correct_sec"] / bucket["total_sec"]
            if bucket["total_sec"] else 0.0)
    return {
        "turns": total_turns,
        "correct_turns": correct_turns,
        "natural_turn_accuracy": correct_turns / total_turns,
        "speaker_time_sec": total_sec,
        "correct_speaker_time_sec": correct_sec,
        "speaker_time_accuracy": correct_sec / total_sec if total_sec else 0.0,
        "critical_turns": critical_total,
        "critical_correct": critical_correct,
        "critical_accuracy": (
            critical_correct / critical_total if critical_total else None),
        "confident_wrong_turns": confident_wrong,
        "blocks": blocks,
    }


def command_validate(args):
    ledger = load_json(args.ledger)
    validate_ledger(ledger, args.require_complete)
    if args.review:
        review = load_json(args.review)
        validate_review(review, ledger, args.require_complete)


def command_summary(args):
    result = summarize(load_json(args.review), load_json(args.ledger))
    print(json.dumps(result, ensure_ascii=False, indent=2))


def add_common_paths(parser):
    parser.add_argument("--ledger", required=True)
    parser.add_argument("--review")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    init = subparsers.add_parser("init-reference")
    init.add_argument("--reference", required=True)
    init.add_argument("--audio", required=True)
    init.add_argument("--audio-sec", type=float)
    init.add_argument("--expected-entries", type=int, default=556)
    init.add_argument("--out", required=True)
    init.set_defaults(func=init_ledger)

    prepare = subparsers.add_parser("prepare-review")
    prepare.add_argument("--ledger", required=True)
    prepare.add_argument("--timeline", required=True)
    prepare.add_argument("--out", required=True)
    prepare.set_defaults(func=prepare_review)

    validate = subparsers.add_parser("validate")
    add_common_paths(validate)
    validate.add_argument("--require-complete", action="store_true")
    validate.set_defaults(func=command_validate)

    summary = subparsers.add_parser("summary")
    add_common_paths(summary)
    summary.set_defaults(func=command_summary)

    args = parser.parse_args(argv)
    args.func(args)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, RuntimeError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        raise SystemExit(1)
