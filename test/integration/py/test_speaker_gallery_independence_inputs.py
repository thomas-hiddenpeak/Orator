#!/usr/bin/env python3

import csv
import hashlib
import importlib.util
import json
import pathlib
import sys
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[3]
MODULE_PATH = (
    ROOT / "tools" / "verify" / "py" /
    "speaker_gallery_independence_inputs.py")
SPEC = importlib.util.spec_from_file_location(
    "speaker_gallery_independence_inputs", MODULE_PATH)
speaker_gallery_independence_inputs = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = speaker_gallery_independence_inputs
SPEC.loader.exec_module(speaker_gallery_independence_inputs)


class SpeakerGalleryIndependenceInputsTest(unittest.TestCase):
    def write_fixture(self, root):
        artifact = root / "artifact.json"
        artifact.write_text(json.dumps({
            "timeline": {"tracks": [{
                "kind": "speaker_voiceprint",
                "entries": [
                    {"evidence_id": "primary_run:0",
                     "evidence_kind": "primary_run", "text_id": -1,
                     "source_start": 0, "source_end": 0,
                     "start": 1.0, "end": 2.0,
                     "embedding_available": True,
                     "session_gallery_complete": True,
                     "robust_gallery_complete": True,
                     "session_scores": [
                         {"speaker_id": "spk_0", "score": 0.5}],
                     "robust_scores": [
                         {"speaker_id": "spk_0", "score": 0.4}]},
                    {"evidence_id": "phrase:7:0:2",
                     "evidence_kind": "phrase", "text_id": 7,
                     "source_start": 0, "source_end": 2,
                     "start": 4.0, "end": 5.0,
                     "embedding_available": False,
                     "session_gallery_complete": False,
                     "robust_gallery_complete": False,
                     "session_scores": [], "robust_scores": []},
                    {"evidence_id": "primary_run:1",
                     "evidence_kind": "primary_run", "text_id": -1,
                     "source_start": 0, "source_end": 0,
                     "start": 8.0, "end": 9.0},
                ],
            }]},
        }), encoding="utf-8")
        contexts = root / "contexts.tsv"
        contexts.write_text(
            "context_id\tstart_sec\tend_sec\tfocus_refs\tcontrol_refs\n"
            "context-a\t1.500\t4.500\t0001\t0002\n",
            encoding="utf-8")
        return artifact, contexts

    def write_replayed_evidence(self, queries_path, evidence_path,
                                first_score="0.500000000"):
        with queries_path.open(encoding="utf-8", newline="") as source:
            queries = list(csv.DictReader(source, delimiter="\t"))
        columns = (
            speaker_gallery_independence_inputs.INCLUSIVE_EVIDENCE_COLUMNS)
        with evidence_path.open("w", encoding="utf-8", newline="") as output:
            writer = csv.DictWriter(
                output, fieldnames=columns,
                delimiter="\t", lineterminator="\n")
            writer.writeheader()
            for index, query in enumerate(queries):
                writer.writerow({
                    **query,
                    "embedding_available": "1" if index == 0 else "0",
                    "session_gallery_complete": "1" if index == 0 else "0",
                    "robust_gallery_complete": "1" if index == 0 else "0",
                    "session_scores": (
                        f"spk_0:{first_score}" if index == 0 else ""),
                    "robust_scores": (
                        "spk_0:0.400000000" if index == 0 else ""),
                })

    def test_copies_only_intersecting_queries_in_track_order(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            artifact, contexts = self.write_fixture(root)
            output = root / "queries.tsv"
            manifest_path = root / "manifest.json"
            manifest = speaker_gallery_independence_inputs.export_inputs(
                artifact, contexts, output, manifest_path)

            with output.open(encoding="utf-8", newline="") as source:
                rows = list(csv.DictReader(source, delimiter="\t"))
            self.assertEqual(
                [row["evidence_id"] for row in rows],
                ["primary_run:0", "phrase:7:0:2"])
            self.assertEqual(rows[0]["text_id"], "-1")
            self.assertEqual(rows[0]["source_start"], "0")
            self.assertEqual(manifest["context_count"], 1)
            self.assertEqual(manifest["query_count"], 2)
            self.assertEqual(
                manifest["output"]["sha256"],
                hashlib.sha256(output.read_bytes()).hexdigest())

    def test_output_is_deterministic(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            artifact, contexts = self.write_fixture(root)
            first = root / "first.tsv"
            second = root / "second.tsv"
            speaker_gallery_independence_inputs.export_inputs(
                artifact, contexts, first, root / "first.json")
            speaker_gallery_independence_inputs.export_inputs(
                artifact, contexts, second, root / "second.json")
            self.assertEqual(first.read_bytes(), second.read_bytes())

    def test_verifies_inclusive_replay_as_a_mechanical_contract(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            artifact, contexts = self.write_fixture(root)
            queries = root / "queries.tsv"
            speaker_gallery_independence_inputs.export_inputs(
                artifact, contexts, queries, root / "first-manifest.json")
            evidence = root / "evidence.tsv"
            self.write_replayed_evidence(queries, evidence)
            manifest = speaker_gallery_independence_inputs.export_inputs(
                artifact, contexts, queries, root / "manifest.json", evidence)
            self.assertEqual(
                manifest["inclusive_replay_contract"]["row_count"], 2)
            self.assertLessEqual(
                manifest["inclusive_replay_contract"][
                    "maximum_score_delta"], 1e-9)

    def test_rejects_inclusive_replay_score_drift(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            artifact, contexts = self.write_fixture(root)
            queries = root / "queries.tsv"
            speaker_gallery_independence_inputs.export_inputs(
                artifact, contexts, queries, root / "first-manifest.json")
            evidence = root / "evidence.tsv"
            self.write_replayed_evidence(queries, evidence, "0.600000000")
            with self.assertRaises(ValueError):
                speaker_gallery_independence_inputs.export_inputs(
                    artifact, contexts, queries, root / "manifest.json",
                    evidence)

    def test_rejects_context_columns_that_could_carry_judgments(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            artifact, contexts = self.write_fixture(root)
            contexts.write_text(
                contexts.read_text(encoding="utf-8").replace(
                    "\tcontrol_refs\n", "\tcontrol_refs\tverdict\n").replace(
                    "\t0002\n", "\t0002\tvalue\n"),
                encoding="utf-8")
            with self.assertRaises(ValueError):
                speaker_gallery_independence_inputs.export_inputs(
                    artifact, contexts, root / "out.tsv",
                    root / "manifest.json")

    def test_rejects_invalid_reference_free_source_range(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            artifact, contexts = self.write_fixture(root)
            package = json.loads(artifact.read_text(encoding="utf-8"))
            package["timeline"]["tracks"][0]["entries"][0][
                "source_end"] = 1
            artifact.write_text(json.dumps(package), encoding="utf-8")
            with self.assertRaises(ValueError):
                speaker_gallery_independence_inputs.export_inputs(
                    artifact, contexts, root / "out.tsv",
                    root / "manifest.json")


if __name__ == "__main__":
    unittest.main()
