#!/usr/bin/env python3
"""Prepare and validate Spec 013 full-session manual review ledgers."""

import argparse
import copy
import datetime
import hashlib
import json
import math
import os
import re
import statistics
import subprocess
import sys


TIMESTAMP_RE = re.compile(r"^(\d{2}):(\d{2}):(\d{2})\s+(.+?)\s*$")
EXPECTED_REFERENCE_ENTRIES = 556
BLOCK_SEC = 600.0
JUDGMENT_TEMPLATE = {
    "result": "unreviewed",
    "speaker_eval": "unreviewed",
    "system_speakers": [],
    "correct_speaker_intervals": [],
    "boundary_start_offset_sec": None,
    "boundary_end_offset_sec": None,
    "boundary_offset_notes": "",
    "confident_wrong": None,
    "uncertain_output": None,
    "evidence_ids": [],
    "context_notes": "",
    "reviewer": "",
    "reviewed_utc": None,
}


def fresh_judgment():
    return copy.deepcopy(JUDGMENT_TEMPLATE)


def is_finite_number(value):
    return (isinstance(value, (int, float)) and
            not isinstance(value, bool) and math.isfinite(value))


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
                "overlap_intervals": [],
                "criticality": "unreviewed",
                "ambiguity": "unreviewed",
                "canonical_speaker": row["source_speaker"],
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
        "schema_version": 2,
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
    suffix = f":text:{text_id}" if text_id is not None else ""
    return f"{kind}:{index}{suffix}"


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
            "chronological_pass": fresh_judgment(),
            "reverse_block_pass": fresh_judgment(),
            "final": fresh_judgment(),
            "reconciliation_notes": "",
        })
    review = {
        "schema_version": 2,
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
    if ledger.get("schema_version") != 2:
        raise ValueError("unsupported ledger schema version")
    if ledger.get("kind") != "orator_reference_ledger":
        raise ValueError("input is not a reference ledger")
    entries = ledger.get("entries", [])
    if len(entries) != ledger.get("reference", {}).get("entry_count"):
        raise ValueError("ledger entry count mismatch")
    if len(entries) != EXPECTED_REFERENCE_ENTRIES:
        raise ValueError(
            f"reference ledger must contain {EXPECTED_REFERENCE_ENTRIES} "
            f"entries, got {len(entries)}")
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
    audio_duration_sec = float(audio["duration_sec"])
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
            valid_audible_interval = (
                is_finite_number(start) and is_finite_number(end) and
                start >= 0.0 and end > start and
                end <= audio_duration_sec)
            if not valid_audible_interval:
                errors.append(f"{expected_id}: invalid audible interval")
            if adjudication.get("criticality") not in (
                    "critical", "noncritical"):
                errors.append(f"{expected_id}: criticality unreviewed")
            if adjudication.get("ambiguity") not in (
                    "none", "timestamp", "speaker", "text", "audio",
                    "overlap"):
                errors.append(f"{expected_id}: ambiguity unreviewed")
            canonical = adjudication.get("canonical_speaker")
            acceptable = adjudication.get("acceptable_speakers")
            if not isinstance(canonical, str) or not canonical:
                errors.append(f"{expected_id}: canonical speaker missing")
            if (not isinstance(acceptable, list) or not acceptable or
                    any(not isinstance(value, str) or not value
                        for value in acceptable)):
                errors.append(f"{expected_id}: acceptable speakers invalid")
            elif canonical not in acceptable:
                errors.append(
                    f"{expected_id}: canonical speaker is not acceptable")
            participants = adjudication.get("overlap_participants")
            if (not isinstance(participants, list) or
                    any(not isinstance(value, str) or not value
                        for value in participants)):
                errors.append(
                    f"{expected_id}: overlap participants invalid")
            valid_participants = (
                participants if isinstance(participants, list) else [])
            overlap_intervals = adjudication.get("overlap_intervals")
            if not isinstance(overlap_intervals, list):
                errors.append(f"{expected_id}: overlap intervals invalid")
            else:
                for overlap_index, interval in enumerate(overlap_intervals):
                    if not isinstance(interval, dict):
                        errors.append(
                            f"{expected_id}: overlap interval "
                            f"{overlap_index} invalid")
                        continue
                    interval_speaker = interval.get("speaker")
                    interval_start = interval.get("start_sec")
                    interval_end = interval.get("end_sec")
                    if (not isinstance(interval_speaker, str) or
                            not interval_speaker or
                            interval_speaker not in valid_participants or
                            not is_finite_number(interval_start) or
                            not is_finite_number(interval_end) or
                            not valid_audible_interval or
                            interval_start < start or interval_end > end or
                            interval_end <= interval_start):
                        errors.append(
                            f"{expected_id}: overlap interval "
                            f"{overlap_index} invalid")
            equivalents = adjudication.get(
                "acceptable_semantic_equivalents")
            if (not isinstance(equivalents, list) or
                    any(not isinstance(value, str) or not value
                        for value in equivalents)):
                errors.append(
                    f"{expected_id}: semantic equivalents invalid")
            if not adjudication.get("context_summary"):
                errors.append(f"{expected_id}: context summary missing")
            if not adjudication.get("reviewer"):
                errors.append(f"{expected_id}: adjudicator missing")
            if not adjudication.get("adjudicated_utc"):
                errors.append(f"{expected_id}: adjudication time missing")
    if errors:
        raise ValueError("\n".join(errors[:50]))


def normalize_intervals(intervals, start, end):
    if not isinstance(intervals, list):
        raise ValueError("correct_speaker_intervals must be a list")
    clipped = []
    for interval in intervals:
        if not isinstance(interval, list) or len(interval) != 2:
            raise ValueError("correct_speaker_intervals must be [start,end] pairs")
        left = interval[0]
        right = interval[1]
        if (not is_finite_number(left) or not is_finite_number(right) or
                left < start or right > end or right <= left):
            raise ValueError("correct speaker interval is empty or out of bounds")
        clipped.append((float(left), float(right)))
    clipped.sort()
    merged = []
    for left, right in clipped:
        if not merged or left > merged[-1][1]:
            merged.append([left, right])
        else:
            merged[-1][1] = max(merged[-1][1], right)
    return merged


def intervals_duration(intervals, start, end):
    merged = normalize_intervals(intervals, start, end)
    return sum(right - left for left, right in merged)


def validate_judgment(judgment, reference_id, start, end,
                      available_evidence_ids):
    if judgment.get("result") not in ("correct", "incorrect", "ambiguous"):
        raise ValueError(f"{reference_id}: result unreviewed")
    if judgment.get("speaker_eval") not in (
            "accurate", "mostly_accurate", "minor_confusion",
            "major_confusion", "unusable", "not_scorable"):
        raise ValueError(f"{reference_id}: speaker_eval unreviewed")
    if not isinstance(judgment.get("confident_wrong"), bool):
        raise ValueError(f"{reference_id}: confident_wrong unreviewed")
    if not isinstance(judgment.get("uncertain_output"), bool):
        raise ValueError(f"{reference_id}: uncertain output unreviewed")
    if (judgment.get("uncertain_output") and
            judgment.get("result") == "correct"):
        raise ValueError(
            f"{reference_id}: uncertain output cannot count as correct")
    system_speakers = judgment.get("system_speakers")
    if (not isinstance(system_speakers, list) or
            any(not isinstance(value, str) or not value
                for value in system_speakers)):
        raise ValueError(f"{reference_id}: system speakers invalid")
    evidence_ids = judgment.get("evidence_ids")
    if (not isinstance(evidence_ids, list) or
            any(not isinstance(value, str) or not value
                for value in evidence_ids)):
        raise ValueError(f"{reference_id}: evidence IDs invalid")
    unknown_evidence = set(evidence_ids) - available_evidence_ids
    if unknown_evidence:
        raise ValueError(
            f"{reference_id}: judgment cites unavailable evidence "
            f"{sorted(unknown_evidence)[:3]}")
    duration = intervals_duration(
        judgment.get("correct_speaker_intervals"), start, end)
    if judgment.get("result") == "correct" and duration <= 0.0:
        raise ValueError(
            f"{reference_id}: correct result has no correct speaker time")
    offsets = []
    for field in ("boundary_start_offset_sec", "boundary_end_offset_sec"):
        value = judgment.get(field)
        if value is not None and not is_finite_number(value):
            raise ValueError(f"{reference_id}: {field} invalid")
        if value is not None:
            offsets.append(abs(float(value)))
    boundary_notes = judgment.get("boundary_offset_notes")
    if not isinstance(boundary_notes, str):
        raise ValueError(f"{reference_id}: boundary offset notes invalid")
    if any(value > 1.0 for value in offsets) and not boundary_notes.strip():
        raise ValueError(
            f"{reference_id}: boundary offset over 1 second is unannotated")
    if not judgment.get("context_notes"):
        raise ValueError(f"{reference_id}: context notes missing")
    if not judgment.get("reviewer") or not judgment.get("reviewed_utc"):
        raise ValueError(f"{reference_id}: review signature missing")


def judgment_decision_signature(judgment):
    fields = (
        "result", "speaker_eval", "system_speakers",
        "correct_speaker_intervals", "boundary_start_offset_sec",
        "boundary_end_offset_sec", "confident_wrong", "uncertain_output",
    )
    return tuple(
        json.dumps(judgment.get(field), ensure_ascii=False, sort_keys=True)
        for field in fields)


def validate_review(review, ledger, require_complete):
    validate_hash(review, "review")
    if review.get("schema_version") != 2:
        raise ValueError("unsupported review schema version")
    if review.get("kind") != "orator_run_review":
        raise ValueError("input is not a run review")
    if review.get("ledger_sha256") != ledger.get("content_sha256"):
        raise ValueError("review points to a different reference ledger")
    if sha256_file(review.get("timeline_path", "")) != review.get(
            "timeline_sha256"):
        raise ValueError("review timeline hash mismatch")
    if require_complete and not review.get("resolved_config_sha256"):
        raise ValueError("review resolved configuration hash missing")
    review_entries = review.get("entries", [])
    if len(review_entries) != len(ledger.get("entries", [])):
        raise ValueError("review entry count mismatch")
    if require_complete:
        for expected, row in zip(ledger["entries"], review_entries):
            reference_id = expected["reference_id"]
            if row.get("reference_id") != reference_id:
                raise ValueError(f"{reference_id}: review order/id mismatch")
            adjudication = expected["adjudication"]
            start = float(adjudication["audible_start_sec"])
            end = float(adjudication["audible_end_sec"])
            evidence = row.get("evidence", {})
            available_evidence_ids = {
                item.get("evidence_id")
                for items in evidence.values() if isinstance(items, list)
                for item in items if isinstance(item, dict) and
                isinstance(item.get("evidence_id"), str)
            }
            validate_judgment(
                row["chronological_pass"], reference_id, start, end,
                available_evidence_ids)
            validate_judgment(
                row["reverse_block_pass"], reference_id, start, end,
                available_evidence_ids)
            validate_judgment(
                row["final"], reference_id, start, end,
                available_evidence_ids)
            if (judgment_decision_signature(row["chronological_pass"]) !=
                    judgment_decision_signature(row["reverse_block_pass"]) and
                    not str(row.get("reconciliation_notes", "")).strip()):
                raise ValueError(
                    f"{reference_id}: pass disagreement is not reconciled")


def ratio(numerator, denominator):
    return numerator / denominator if denominator else None


def nearest_rank_percentile(values, percentile):
    if not values:
        return None
    ordered = sorted(values)
    index = max(0, math.ceil(percentile * len(ordered)) - 1)
    return ordered[index]


def make_bucket(start, end, full_block=None):
    bucket = {
        "start_sec": start,
        "end_sec": end,
        "total_turns": 0,
        "correct_turns": 0,
        "speaker_time_sec": 0.0,
        "correct_speaker_time_sec": 0.0,
    }
    if full_block is not None:
        bucket["full_600_sec_block"] = full_block
    return bucket


def finish_bucket(bucket):
    bucket["turn_accuracy"] = ratio(
        bucket["correct_turns"], bucket["total_turns"])
    bucket["speaker_time_accuracy"] = ratio(
        bucket["correct_speaker_time_sec"], bucket["speaker_time_sec"])


def overlap_duration(intervals, start, end):
    return sum(
        max(0.0, min(right, end) - max(left, start))
        for left, right in intervals)


def summarize_validated(review, ledger):
    audio_end = float(ledger["audio"]["duration_sec"])
    block_count = max(1, math.ceil(audio_end / BLOCK_SEC))
    blocks = {
        str(index): make_bucket(
            index * BLOCK_SEC,
            min(audio_end, (index + 1) * BLOCK_SEC),
            (index + 1) * BLOCK_SEC <= audio_end)
        for index in range(block_count)
    }
    speakers = {}
    total_turns = len(ledger["entries"])
    correct_turns = 0
    critical_total = 0
    critical_correct = 0
    critical_confident_wrong = 0
    confident_wrong = 0
    uncertain_output = 0
    total_sec = 0.0
    correct_sec = 0.0
    boundary_offsets = []
    missing_boundary_offsets = 0
    unannotated_large_offsets = []
    pass_disagreement_rows = []

    for ref, row in zip(ledger["entries"], review["entries"]):
        adjudication = ref["adjudication"]
        judgment = row["final"]
        if (judgment_decision_signature(row["chronological_pass"]) !=
                judgment_decision_signature(row["reverse_block_pass"])):
            pass_disagreement_rows.append(ref["reference_id"])
        start = float(adjudication["audible_start_sec"])
        end = float(adjudication["audible_end_sec"])
        duration = end - start
        intervals = normalize_intervals(
            judgment["correct_speaker_intervals"], start, end)
        correct_duration = sum(right - left for left, right in intervals)
        is_correct = judgment["result"] == "correct"
        is_critical = adjudication["criticality"] == "critical"
        is_confident_wrong = judgment["confident_wrong"]
        is_uncertain_output = judgment["uncertain_output"]
        speaker = adjudication["canonical_speaker"]

        total_sec += duration
        correct_sec += correct_duration
        correct_turns += int(is_correct)
        critical_total += int(is_critical)
        critical_correct += int(is_critical and is_correct)
        confident_wrong += int(is_confident_wrong)
        uncertain_output += int(is_uncertain_output)
        critical_confident_wrong += int(is_critical and is_confident_wrong)

        speaker_bucket = speakers.setdefault(
            speaker, make_bucket(0.0, audio_end))
        speaker_bucket["total_turns"] += 1
        speaker_bucket["correct_turns"] += int(is_correct)
        speaker_bucket["speaker_time_sec"] += duration
        speaker_bucket["correct_speaker_time_sec"] += correct_duration

        turn_block = min(int(start // BLOCK_SEC), block_count - 1)
        blocks[str(turn_block)]["total_turns"] += 1
        blocks[str(turn_block)]["correct_turns"] += int(is_correct)
        first_block = max(0, int(start // BLOCK_SEC))
        last_block = min(block_count - 1, int(math.nextafter(
            end, -math.inf) // BLOCK_SEC))
        for block_index in range(first_block, last_block + 1):
            bucket = blocks[str(block_index)]
            bucket["speaker_time_sec"] += max(
                0.0, min(end, bucket["end_sec"]) -
                max(start, bucket["start_sec"]))
            bucket["correct_speaker_time_sec"] += overlap_duration(
                intervals, bucket["start_sec"], bucket["end_sec"])

        for field in (
                "boundary_start_offset_sec", "boundary_end_offset_sec"):
            offset = judgment[field]
            if offset is None:
                missing_boundary_offsets += 1
                continue
            absolute = abs(float(offset))
            boundary_offsets.append(absolute)
            if (absolute > 1.0 and
                    not judgment["boundary_offset_notes"].strip()):
                unannotated_large_offsets.append({
                    "reference_id": ref["reference_id"],
                    "field": field,
                    "absolute_offset_sec": absolute,
                })

    for bucket in blocks.values():
        finish_bucket(bucket)
    for bucket in speakers.values():
        finish_bucket(bucket)

    natural_turn_accuracy = ratio(correct_turns, total_turns)
    speaker_time_accuracy = ratio(correct_sec, total_sec)
    critical_accuracy = ratio(critical_correct, critical_total)
    confident_wrong_rate = ratio(confident_wrong, total_turns)
    boundary = {
        "offset_count": len(boundary_offsets),
        "missing_offset_count": missing_boundary_offsets,
        "median_absolute_offset_sec": (
            statistics.median(boundary_offsets)
            if boundary_offsets else None),
        "p95_absolute_offset_sec": (
            nearest_rank_percentile(boundary_offsets, 0.95)),
        "over_1_sec_count": sum(
            value > 1.0 for value in boundary_offsets),
        "unannotated_over_1_sec": unannotated_large_offsets,
    }

    full_blocks = [
        bucket for bucket in blocks.values()
        if bucket["full_600_sec_block"]
    ]
    speaker_gates = {
        "natural_turn_accuracy_at_least_90": (
            natural_turn_accuracy is not None and
            natural_turn_accuracy >= 0.90),
        "speaker_time_accuracy_at_least_90": (
            speaker_time_accuracy is not None and
            speaker_time_accuracy >= 0.90),
        "every_full_600_sec_block_at_least_90": all(
            bucket["turn_accuracy"] is not None and
            bucket["speaker_time_accuracy"] is not None and
            bucket["turn_accuracy"] >= 0.90 and
            bucket["speaker_time_accuracy"] >= 0.90
            for bucket in full_blocks),
        "every_speaker_recall_at_least_90": all(
            bucket["turn_accuracy"] is not None and
            bucket["speaker_time_accuracy"] is not None and
            bucket["turn_accuracy"] >= 0.90 and
            bucket["speaker_time_accuracy"] >= 0.90
            for bucket in speakers.values()),
        "critical_turns_100_percent": (
            critical_accuracy is not None and critical_accuracy == 1.0),
        "critical_confident_wrong_zero": critical_confident_wrong == 0,
        "confident_wrong_at_most_2_percent": (
            confident_wrong_rate is not None and
            confident_wrong_rate <= 0.02),
        "boundary_offsets_complete": missing_boundary_offsets == 0,
        "boundary_median_at_most_0_25_sec": (
            boundary["median_absolute_offset_sec"] is not None and
            boundary["median_absolute_offset_sec"] <= 0.25),
        "boundary_p95_at_most_0_80_sec": (
            boundary["p95_absolute_offset_sec"] is not None and
            boundary["p95_absolute_offset_sec"] <= 0.80),
        "boundary_over_1_sec_annotated": not unannotated_large_offsets,
    }
    speaker_gates["all_pass"] = all(speaker_gates.values())

    return {
        "turns": total_turns,
        "correct_turns": correct_turns,
        "natural_turn_accuracy": natural_turn_accuracy,
        "speaker_time_sec": total_sec,
        "correct_speaker_time_sec": correct_sec,
        "speaker_time_accuracy": speaker_time_accuracy,
        "critical_turns": critical_total,
        "critical_correct": critical_correct,
        "critical_accuracy": critical_accuracy,
        "critical_confident_wrong_turns": critical_confident_wrong,
        "confident_wrong_turns": confident_wrong,
        "confident_wrong_rate": confident_wrong_rate,
        "uncertain_output_turns": uncertain_output,
        "pass_disagreement_count": len(pass_disagreement_rows),
        "pass_disagreement_reference_ids": pass_disagreement_rows,
        "blocks": blocks,
        "speakers": speakers,
        "boundary_offsets": boundary,
        "speaker_acceptance_gates": speaker_gates,
        "notes": {
            "turn_block_assignment": "audible start block",
            "speaker_time_block_assignment": "exact interval clipping",
            "final_partial_block_is_reported_not_gated": True,
        },
    }


def summarize(review, ledger):
    validate_ledger(ledger, require_complete=True)
    validate_review(review, ledger, require_complete=True)
    return summarize_validated(review, ledger)


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
    init.add_argument(
        "--expected-entries", type=int,
        default=EXPECTED_REFERENCE_ENTRIES)
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
