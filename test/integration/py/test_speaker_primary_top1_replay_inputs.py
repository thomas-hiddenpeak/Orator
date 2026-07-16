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
    "speaker_primary_top1_replay_inputs.py")
SPEC = importlib.util.spec_from_file_location(
    "speaker_primary_top1_replay_inputs", MODULE_PATH)
primary = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = primary
SPEC.loader.exec_module(primary)


class SpeakerPrimaryTop1ReplayInputsTest(unittest.TestCase):
    def write_fixture(self, root):
        frames = root / "frames.csv"
        with frames.open("w", encoding="utf-8", newline="") as output:
            writer = csv.writer(output)
            writer.writerow([
                "frame", "time_sec", "session", "top1", "top1_prob",
            ])
            for frame in range(20):
                local = 3
                probability = 0.1
                if 1 <= frame <= 4:
                    local, probability = 0, 0.8
                elif frame in (6, 7, 9, 10):
                    local, probability = 1, 0.9
                elif frame == 8:
                    local, probability = 2, 0.9
                elif 12 <= frame <= 15:
                    local, probability = 2, 0.7
                writer.writerow([
                    frame, f"{frame * 0.1:.6f}", 0, local, probability,
                ])

        vad = root / "vad.tsv"
        with vad.open("w", encoding="utf-8", newline="") as output:
            writer = csv.writer(output, delimiter="\t", lineterminator="\n")
            writer.writerow(["start_sec", "end_sec"])
            writer.writerow([0.05, 0.55])
            writer.writerow([0.55, 1.05])
            writer.writerow([1.15, 1.65])

        mapping = root / "mapping.json"
        mapping.write_text(json.dumps({
            "mapping": {"0": "spk_1", "1": "spk_2", "2": "spk_3"},
        }), encoding="utf-8")
        config = root / "config.toml"
        config.write_text(
            "[speaker_primary_top1]\n"
            "enabled = true\n"
            "require_vad_support = true\n"
            "minimum_probability = 0.5\n"
            "minimum_run_sec = 0.4\n",
            encoding="utf-8")
        return frames, vad, mapping, config

    def test_exports_only_sustained_vad_contained_runs(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            frames, vad, mapping, config = self.write_fixture(root)
            out = root / "primary.tsv"
            embedding_spans = root / "primary-spans.tsv"
            manifest_path = root / "manifest.json"
            manifest = primary.export_inputs(
                str(frames), str(vad), str(mapping), str(config), str(out),
                str(manifest_path), str(embedding_spans))
            with out.open(encoding="utf-8", newline="") as source:
                rows = list(csv.DictReader(source, delimiter="\t"))
            self.assertEqual(len(rows), 2)
            self.assertEqual(
                (rows[0]["start_sec"], rows[0]["end_sec"],
                 rows[0]["local_speaker"], rows[0]["speaker_id"]),
                ("0.100000", "0.500000", "0", "spk_1"))
            self.assertEqual(
                (rows[1]["start_sec"], rows[1]["end_sec"],
                 rows[1]["local_speaker"], rows[1]["speaker_id"]),
                ("1.200000", "1.600000", "2", "spk_3"))
            self.assertEqual(manifest["counts"]["primary_runs"], 2)
            with embedding_spans.open(encoding="utf-8", newline="") as source:
                span_rows = list(csv.DictReader(source, delimiter="\t"))
            self.assertEqual(
                span_rows,
                [{"evidence_id": "primary_top1:0",
                  "start_sec": "0.100000", "end_sec": "0.500000"},
                 {"evidence_id": "primary_top1:1",
                  "start_sec": "1.200000", "end_sec": "1.600000"}])
            self.assertIn("embedding_spans", manifest)
            first_hash = manifest["output"]["sha256"]
            second = primary.export_inputs(
                str(frames), str(vad), str(mapping), str(config), str(out),
                str(manifest_path))
            self.assertEqual(first_hash, second["output"]["sha256"])

    def test_rejects_threshold_drift(self):
        with tempfile.TemporaryDirectory() as temp:
            config = pathlib.Path(temp) / "config.toml"
            config.write_text(
                "[speaker_primary_top1]\n"
                "enabled = true\n"
                "require_vad_support = true\n"
                "minimum_probability = 0.49\n"
                "minimum_run_sec = 0.4\n",
                encoding="utf-8")
            with self.assertRaises(ValueError):
                primary.load_policy(str(config))

    def test_rejects_unmapped_retained_run(self):
        frames = [
            {"frame": index, "time": index * 0.1, "session": 0,
             "local": 4, "probability": 0.9}
            for index in range(5)
        ]
        with self.assertRaises(ValueError):
            primary.primary_runs(
                frames, 0.1, [(0.0, 0.5)], {0: "spk_1"},
                {"minimum_probability": 0.5, "minimum_run_sec": 0.4})


if __name__ == "__main__":
    unittest.main()
