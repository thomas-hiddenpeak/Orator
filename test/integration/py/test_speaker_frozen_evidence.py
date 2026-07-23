#!/usr/bin/env python3

import importlib.util
import csv
import pathlib
import sys
import tempfile
import unittest


REPO = pathlib.Path(__file__).resolve().parents[3]
MODULE_PATH = (
    REPO / "tools" / "verify" / "py" / "speaker_frozen_evidence.py")
SPEC = importlib.util.spec_from_file_location(
    "speaker_frozen_evidence", MODULE_PATH)
speaker_frozen_evidence = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = speaker_frozen_evidence
SPEC.loader.exec_module(speaker_frozen_evidence)


class SpeakerFrozenEvidenceTest(unittest.TestCase):
    def test_frame_summary_uses_exact_overlap_weights(self):
        rows = [
            {"time_sec": 0.0, "probs": [0.8, 0.2], "top1": 0,
             "margin": 0.6, "active_count": 1},
            {"time_sec": 0.08, "probs": [0.3, 0.7], "top1": 1,
             "margin": 0.4, "active_count": 2},
        ]
        result = speaker_frozen_evidence.summarize_frames(
            rows, [0.0, 0.08], 0.08, 0.04, 0.12, 0.45)
        self.assertAlmostEqual(result["frame_coverage_sec"], 0.08)
        self.assertEqual(result["frame_start_index"], 0)
        self.assertEqual(result["frame_end_index"], 2)
        self.assertAlmostEqual(result["mean_probs"][0], 0.55)
        self.assertAlmostEqual(result["mean_probs"][1], 0.45)
        self.assertAlmostEqual(result["overlap_duration_sec"], 0.04)
        self.assertAlmostEqual(result["active_top1_duration_sec"][0], 0.04)
        self.assertAlmostEqual(result["active_top1_duration_sec"][1], 0.04)

    def test_interval_metrics_merges_overlapping_support(self):
        entries = [
            {"start": 1.0, "end": 2.0},
            {"start": 1.5, "end": 2.5},
            {"start": 3.0, "end": 3.5},
        ]
        result = speaker_frozen_evidence.interval_metrics(
            0.5, 4.0, entries)
        self.assertAlmostEqual(result["coverage_sec"], 2.0)
        self.assertEqual(result["island_count"], 2)
        self.assertAlmostEqual(result["max_gap_sec"], 0.5)
        self.assertEqual(len(result["evidence"]), 3)

    def test_alignment_evidence_preserves_point_units_once(self):
        align_by_id = {7: {"units": [
            {"start": 1.0, "end": 1.0, "text": "起"},
            {"start": 1.5, "end": 1.5, "text": "中"},
            {"start": 1.75, "end": 1.9, "text": "段"},
            {"start": 2.0, "end": 2.0, "text": "界"},
        ]}}

        left = speaker_frozen_evidence.alignment_evidence(
            align_by_id, 7, 1.0, 2.0)
        right = speaker_frozen_evidence.alignment_evidence(
            align_by_id, 7, 2.0, 3.0)

        self.assertEqual(
            [unit["text"] for unit in left["units"]], ["起", "中", "段"])
        self.assertEqual(
            [unit["text"] for unit in right["units"]], ["界"])
        self.assertEqual(left["units"][1]["evidence_id"], "align:7:1")

    def test_diar_span_export_has_stable_evidence_ids(self):
        entries = [
            {"start": 1.25, "end": 2.5},
            {"start": 3.0, "end": 4.75},
        ]
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "spans.tsv"
            speaker_frozen_evidence.write_diar_spans(path, entries)
            with path.open(encoding="utf-8", newline="") as source:
                rows = list(csv.DictReader(source, delimiter="\t"))
        self.assertEqual(
            [row["evidence_id"] for row in rows],
            ["diarization:0", "diarization:1"])
        self.assertEqual(rows[1]["end_sec"], "4.75")

    def test_diar_query_export_matches_replay_probe_schema(self):
        entries = [
            {"start": 1.25, "end": 2.5},
            {"start": 3.0, "end": 4.75},
        ]
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "queries.tsv"
            speaker_frozen_evidence.write_diar_queries(path, entries)
            with path.open(encoding="utf-8", newline="") as source:
                rows = list(csv.DictReader(source, delimiter="\t"))
        self.assertEqual(
            list(rows[0]),
            [
                "evidence_id", "kind", "text_id", "source_start",
                "source_end", "start", "end",
            ])
        self.assertEqual(rows[0]["evidence_id"], "diarization:0")
        self.assertEqual(rows[0]["kind"], "diarization")
        self.assertEqual(rows[0]["text_id"], "-1")
        self.assertEqual(rows[0]["source_start"], "0")
        self.assertEqual(rows[0]["source_end"], "0")
        self.assertEqual(rows[1]["end"], "4.75")

    def test_business_span_export_has_stable_evidence_ids(self):
        entries = [
            {"start": 5.5, "end": 7.25, "speaker_id": "spk_1"},
            {"start": 8.0, "end": 9.0, "speaker_id": "spk_2"},
        ]
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "business-spans.tsv"
            speaker_frozen_evidence.write_business_spans(path, entries)
            with path.open(encoding="utf-8", newline="") as source:
                rows = list(csv.DictReader(source, delimiter="\t"))
        self.assertEqual(
            [row["evidence_id"] for row in rows],
            ["business_speaker:0", "business_speaker:1"])
        self.assertEqual(rows[0]["start_sec"], "5.5")

    def test_external_diar_segments_are_canonicalized(self):
        content = (
            "start_sec,end_sec,session,local_speaker,confidence,mean_margin\n"
            "3.0,4.0,0,2,0.8,0.3\n"
            "1.0,2.0,0,1,0.9,0.4\n"
        )
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "segments.csv"
            path.write_text(content, encoding="utf-8")
            entries = speaker_frozen_evidence.read_diar_segments(path)
        self.assertEqual(
            [(entry["start"], entry["speaker"]) for entry in entries],
            [(1.0, 1), (3.0, 2)])
        self.assertEqual(entries[0]["source_session"], 0)
        self.assertAlmostEqual(entries[0]["source_mean_margin"], 0.4)

    def test_candidate_toml_overrides_resolved_diarizer(self):
        timeline = {
            "resolved_config": {
                "diarizer": {
                    "model_weights": "old.safetensors",
                    "fifo_len": 0,
                },
                "speaker": {"min_embed_sec": 3.0},
            },
        }
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "candidate.toml"
            path.write_text(
                "[diarizer]\n"
                "model_weights = \"new.safetensors\"\n"
                "fifo_len = 188\n",
                encoding="utf-8",
            )
            result = speaker_frozen_evidence.candidate_resolved_config(
                timeline, path)
        self.assertEqual(
            result["diarizer"]["model_weights"], "new.safetensors")
        self.assertEqual(result["diarizer"]["fifo_len"], 188)
        self.assertEqual(result["speaker"]["min_embed_sec"], 3.0)


if __name__ == "__main__":
    unittest.main()
