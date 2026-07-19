#!/usr/bin/env python3

import csv
import importlib.util
import json
import pathlib
import sys
import tempfile
import unittest


REPO = pathlib.Path(__file__).resolve().parents[3]
MODULE_PATH = (
    REPO / "tools" / "verify" / "py" /
    "speaker_short_primary_evidence.py")
SPEC = importlib.util.spec_from_file_location(
    "speaker_short_primary_evidence", MODULE_PATH)
speaker_short_primary_evidence = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = speaker_short_primary_evidence
SPEC.loader.exec_module(speaker_short_primary_evidence)


class SpeakerShortPrimaryEvidenceTest(unittest.TestCase):
    def write_fixture(self, root):
        timeline_path = root / "timeline.json"
        timeline = {
            "audio_sec": 6.0,
            "sample_rate": 16000,
            "resolved_config": {
                "speaker_fusion": {"short_max_sec": 1.5},
            },
            "tracks": [
                {"kind": "primary_speaker", "entries": [
                    {"start": 1.0, "end": 1.4, "speaker": 0,
                     "speaker_id": "spk_a", "confidence": 0.8},
                    {"start": 2.1, "end": 2.4, "speaker": 1,
                     "speaker_id": "spk_c", "confidence": 0.7},
                    {"start": 3.0, "end": 4.6, "speaker": 0,
                     "speaker_id": "spk_a", "confidence": 0.9},
                    {"start": 5.0, "end": 5.3, "speaker": 0,
                     "speaker_id": "spk_a", "confidence": 0.6},
                ]},
                {"kind": "diarization", "entries": [
                    {"start": 0.8, "end": 1.5, "speaker": 0,
                     "speaker_id": "spk_a", "confidence": 0.8},
                    {"start": 2.0, "end": 2.5, "speaker": 1,
                     "speaker_id": "spk_c", "confidence": 0.7},
                ]},
                {"kind": "asr", "entries": [
                    {"text_id": 7, "start": 0.8, "end": 2.8,
                     "text": "abcd"},
                ]},
                {"kind": "align", "entries": [
                    {"text_id": 7, "start": 0.8, "end": 2.8,
                     "units": [
                         {"start": 0.9, "end": 1.2, "text": "a"},
                         {"start": 1.3, "end": 1.3, "text": "b"},
                         {"start": 1.8, "end": 2.0, "text": "c"},
                         {"start": 2.5, "end": 2.7, "text": "d"},
                     ]},
                ]},
                {"kind": "vad", "entries": [
                    {"start": 0.8, "end": 2.8},
                ]},
                {"kind": "speaker_voiceprint", "entries": [
                    {"evidence_id": "complete_source:7",
                     "evidence_kind": "complete_source", "text_id": 7,
                     "source_start": 0, "source_end": 4,
                     "start": 0.8, "end": 2.8,
                     "embedding_available": True,
                     "session_gallery_complete": True,
                     "robust_gallery_complete": True,
                     "session_scores": [], "robust_scores": []},
                ]},
                {"kind": "business_speaker", "entries": [
                    {"start": 0.9, "end": 1.4, "text_id": 7,
                     "text": "ab", "speaker": 2,
                     "speaker_id": "spk_b"},
                    {"start": 1.8, "end": 2.5, "text_id": 7,
                     "text": "cd", "speaker": 3,
                     "speaker_id": "spk_d"},
                ]},
            ],
        }
        timeline_path.write_text(
            json.dumps({"timeline": timeline}), encoding="utf-8")

        frames_path = root / "frames.csv"
        with frames_path.open("w", encoding="utf-8", newline="") as output:
            writer = csv.writer(output)
            writer.writerow([
                "frame", "time_sec", "session", "top1", "top1_prob",
                "top2", "top2_prob", "margin", "active_count",
                "spk0", "spk1",
            ])
            for frame in range(60):
                time_sec = frame * 0.1
                writer.writerow([
                    frame, time_sec, 0, 0, 0.8, 1, 0.2, 0.6, 1,
                    0.8, 0.2,
                ])

        evidence_path = root / "primary-evidence.tsv"
        with evidence_path.open(
                "w", encoding="utf-8", newline="") as output:
            writer = csv.writer(output, delimiter="\t", lineterminator="\n")
            writer.writerow([
                "evidence_id", "kind", "text_id", "source_start",
                "source_end", "start", "end", "embedding_available",
                "session_gallery_complete", "robust_gallery_complete",
                "session_scores", "robust_scores",
            ])
            for index, primary in enumerate(
                    timeline["tracks"][0]["entries"]):
                writer.writerow([
                    f"primary_run:{index}", "primary_run", -1, 0, 1,
                    primary["start"], primary["end"], 1, 1, 1,
                    "spk_a:0.8,spk_b:0.2", "spk_a:0.7,spk_b:0.3",
                ])

        epochs_path = root / "epochs.tsv"
        epochs_path.write_text(
            "local_speaker\tstart_sec\tend_sec\tspeaker_id\n"
            "0\t0.0\t6.0\tspk_a\n"
            "1\t0.0\t6.0\tspk_c\n",
            encoding="utf-8")
        return timeline_path, frames_path, evidence_path, epochs_path

    def test_exports_positive_point_and_gap_conflicts_with_controls(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            inputs = self.write_fixture(root)
            out_path = root / "evidence.json"
            result = speaker_short_primary_evidence.export_inventory(
                *map(str, inputs), str(out_path))

            self.assertEqual(result["record_count"], 2)
            self.assertEqual(
                [item["record_id"] for item in result["records"]],
                ["short_primary:0", "short_primary:1"])
            first_kinds = {
                item["kind"]
                for item in result["records"][0]["alignment_relations"]
            }
            second_kinds = {
                item["kind"]
                for item in result["records"][1]["alignment_relations"]
            }
            self.assertEqual(
                first_kinds,
                {"positive_unit", "zero_duration_unit", "alignment_gap"})
            self.assertEqual(second_kinds, {"alignment_gap"})
            self.assertEqual(
                result["records"][0]["identity_epochs"][0]["speaker_id"],
                "spk_a")
            self.assertEqual(
                [item["index"] for item in
                 result["records"][0]["controls"]["business"]],
                [0, 1])
            rendered = json.dumps(result["records"], ensure_ascii=False)
            for forbidden in ("reference_id", "expected_speaker",
                              "correctness", "verdict"):
                self.assertNotIn(forbidden, rendered)
            self.assertTrue(out_path.exists())

    def test_rejects_primary_evidence_bound_mismatch(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            inputs = list(self.write_fixture(root))
            evidence = inputs[2]
            content = evidence.read_text(encoding="utf-8")
            evidence.write_text(
                content.replace("\t1.0\t1.4\t", "\t1.0\t1.41\t", 1),
                encoding="utf-8")
            with self.assertRaises(ValueError):
                speaker_short_primary_evidence.export_inventory(
                    *map(str, inputs), str(root / "evidence.json"))


if __name__ == "__main__":
    unittest.main()
