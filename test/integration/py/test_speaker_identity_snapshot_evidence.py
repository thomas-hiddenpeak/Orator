#!/usr/bin/env python3

import csv
import hashlib
import importlib.util
import json
import pathlib
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[3]
MODULE_PATH = (
    ROOT / "tools" / "verify" / "py" /
    "speaker_identity_snapshot_evidence.py")
SPEC = importlib.util.spec_from_file_location(
    "speaker_identity_snapshot_evidence", MODULE_PATH)
speaker_identity_snapshot_evidence = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(speaker_identity_snapshot_evidence)


class SpeakerIdentitySnapshotEvidenceTest(unittest.TestCase):
    def write_artifact(self, root):
        path = root / "artifact.json"
        path.write_text(json.dumps({
            "events": [
                {"type": "vad", "segments": []},
                {"type": "diar", "segments": [
                    {"start": 1.0, "end": 2.0, "speaker": "speaker_0",
                     "confidence": 0.75},
                    {"start": 1.5, "end": 2.5, "speaker": "speaker_1",
                     "speaker_id": "spk_7", "confidence": 0.5},
                ]},
                {"type": "diar", "segments": []},
            ],
        }), encoding="utf-8")
        return path

    def test_exports_capture_order_with_empty_snapshot_marker(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            artifact = self.write_artifact(root)
            output = root / "snapshots.tsv"
            manifest_path = root / "manifest.json"
            manifest = speaker_identity_snapshot_evidence.export_snapshots(
                artifact, output, manifest_path)

            with output.open(encoding="utf-8", newline="") as source:
                rows = list(csv.DictReader(source, delimiter="\t"))
            self.assertEqual(len(rows), 3)
            self.assertEqual(rows[0], {
                "snapshot_index": "0",
                "start_sec": "1.000000000",
                "end_sec": "2.000000000",
                "local_speaker": "0",
                "confidence": "0.750000000",
                "captured_speaker_id": "",
            })
            self.assertEqual(rows[1]["captured_speaker_id"], "spk_7")
            self.assertEqual(rows[2]["snapshot_index"], "1")
            self.assertEqual(rows[2]["start_sec"], "")
            self.assertEqual(manifest["snapshot_count"], 2)
            self.assertEqual(manifest["segment_row_count"], 2)
            self.assertEqual(
                manifest["output"]["sha256"],
                hashlib.sha256(output.read_bytes()).hexdigest())

    def test_output_is_deterministic(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            artifact = self.write_artifact(root)
            first = root / "first.tsv"
            second = root / "second.tsv"
            speaker_identity_snapshot_evidence.export_snapshots(
                artifact, first, root / "first.json")
            speaker_identity_snapshot_evidence.export_snapshots(
                artifact, second, root / "second.json")
            self.assertEqual(first.read_bytes(), second.read_bytes())

    def test_rejects_unordered_segments(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            artifact = self.write_artifact(root)
            package = json.loads(artifact.read_text(encoding="utf-8"))
            package["events"][1]["segments"].reverse()
            artifact.write_text(json.dumps(package), encoding="utf-8")
            with self.assertRaises(ValueError):
                speaker_identity_snapshot_evidence.export_snapshots(
                    artifact, root / "out.tsv", root / "manifest.json")

    def test_rejects_invalid_speaker_label(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            artifact = self.write_artifact(root)
            package = json.loads(artifact.read_text(encoding="utf-8"))
            package["events"][1]["segments"][0]["speaker"] = "spk_0"
            artifact.write_text(json.dumps(package), encoding="utf-8")
            with self.assertRaises(ValueError):
                speaker_identity_snapshot_evidence.export_snapshots(
                    artifact, root / "out.tsv", root / "manifest.json")


if __name__ == "__main__":
    unittest.main()
