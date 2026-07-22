#!/usr/bin/env python3

import csv
import hashlib
import importlib.util
import json
import pathlib
import sys
import tempfile
import unittest


REPO = pathlib.Path(__file__).resolve().parents[3]
MODULE_PATH = (
    REPO / "tools" / "verify" / "py" /
    "speaker_residual_evidence_packet.py")
SPEC = importlib.util.spec_from_file_location(
    "speaker_residual_evidence_packet", MODULE_PATH)
speaker_residual_evidence_packet = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = speaker_residual_evidence_packet
SPEC.loader.exec_module(speaker_residual_evidence_packet)


class SpeakerResidualEvidencePacketTest(unittest.TestCase):
    def write_fixture(self, root):
        reference_path = root / "reference.txt"
        reference_path.write_text(
            "00:00:00 Alpha\nfirst\n"
            "00:00:02 Beta\nsecond\n"
            "00:00:04 Gamma\nthird\n",
            encoding="utf-8")

        contexts_path = root / "contexts.tsv"
        contexts_path.write_text(
            "context_id\tstart_sec\tend_sec\tfocus_refs\tcontrol_refs\n"
            "ref-0002\t1.000\t4.500\t0002\t0001,0003\n",
            encoding="utf-8")

        tracks = [
            {"kind": "diarization", "entries": [
                {"start": 0.5, "end": 1.5, "speaker": 0,
                 "speaker_id": "spk_a", "confidence": 0.8},
                {"start": 2.0, "end": 3.0, "speaker": 1,
                 "speaker_id": "spk_b", "confidence": 0.7},
                {"start": 3.5, "end": 4.2, "speaker": 0,
                 "speaker_id": "spk_c", "confidence": 0.6},
            ]},
            {"kind": "primary_speaker", "entries": [
                {"start": 1.0, "end": 2.0, "speaker": 0,
                 "speaker_id": "spk_a", "confidence": 0.8},
            ]},
            {"kind": "asr", "entries": [
                {"text_id": 7, "start": 1.2, "end": 4.0,
                 "text": "source text"},
            ]},
            {"kind": "vad", "entries": [
                {"start": 1.1, "end": 4.1},
            ]},
            {"kind": "align", "entries": [
                {"text_id": 7, "start": 1.2, "end": 4.0,
                 "units": [{"start": 1.2, "end": 1.4, "text": "x"}]},
            ]},
            {"kind": "speaker_voiceprint", "entries": [
                {"evidence_id": "complete_source:7",
                 "evidence_kind": "complete_source", "text_id": 7,
                 "source_start": 0, "source_end": 2,
                 "start": 0.0, "end": 0.5,
                 "embedding_available": True,
                 "session_gallery_complete": True,
                 "robust_gallery_complete": True,
                 "session_scores": [{"speaker_id": "spk_a", "score": 0.7}],
                 "robust_scores": []},
                {"evidence_id": "vad:9", "evidence_kind": "vad",
                 "text_id": -1, "source_start": 0, "source_end": 0,
                 "start": 1.0, "end": 1.3,
                 "embedding_available": False,
                 "session_gallery_complete": False,
                 "robust_gallery_complete": False,
                 "session_scores": [], "robust_scores": []},
                {"evidence_id": "complete_source:8",
                 "evidence_kind": "complete_source", "text_id": 8,
                 "source_start": 0, "source_end": 1,
                 "start": 5.0, "end": 5.5,
                 "embedding_available": True,
                 "session_gallery_complete": True,
                 "robust_gallery_complete": True,
                 "session_scores": [], "robust_scores": []},
            ]},
            {"kind": "business_speaker", "entries": [
                {"start": 1.2, "end": 4.0, "text_id": 7,
                 "text": "source text", "speaker": 0,
                 "speaker_id": "spk_a",
                 "speaker_decision": {"reason": "raw_reason"}},
            ]},
        ]
        timeline = {
            "audio_sec": 6.0,
            "sample_rate": 16000,
            "resolved_config": {
                "speaker_fusion": {"frame_activity_threshold": 0.5},
            },
            "tracks": tracks,
            "comprehensive": tracks[-1]["entries"],
        }
        artifact_path = root / "artifact.json"
        artifact_path.write_text(
            json.dumps({
                "meta": {"resolved_config_sha256": "config-id"},
                "timeline": timeline,
            }),
            encoding="utf-8")
        artifact_sha256 = hashlib.sha256(artifact_path.read_bytes()).hexdigest()
        artifact_manifest_path = root / "artifact.json.manifest.json"
        artifact_manifest_path.write_text(json.dumps({
            "artifact_id": "fixture-a",
            "artifact": {"sha256": artifact_sha256},
            "resolved_config_sha256": "config-id",
            "git": {"commit": "fixture", "dirty": False},
            "source_audio": {"samples": 96000, "sample_rate": 16000},
        }), encoding="utf-8")

        posterior_path = root / "frames.csv"
        with posterior_path.open("w", encoding="utf-8", newline="") as output:
            writer = csv.writer(output, lineterminator="\n")
            writer.writerow([
                "frame", "time_sec", "session", "top1", "top1_prob",
                "top2", "top2_prob", "margin", "active_count",
                "spk0", "spk1",
            ])
            for frame in range(60):
                active = 10 <= frame < 20
                top1_prob = "0.800000" if active else "0.100000"
                top2_prob = "0.200000" if active else "0.050000"
                writer.writerow([
                    frame, f"{frame * 0.1:.6f}", 0, 0, top1_prob, 1,
                    top2_prob, "0.600000" if active else "0.050000",
                    1 if active else 0, top1_prob, top2_prob,
                ])
        return (
            artifact_path,
            artifact_manifest_path,
            reference_path,
            contexts_path,
            posterior_path,
            tracks,
        )

    def test_exports_complete_raw_context_and_source_related_voiceprints(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            inputs = self.write_fixture(root)
            out_dir = root / "packet"
            result = speaker_residual_evidence_packet.export_packet(
                *inputs[:5], out_dir)

            context_dir = out_dir / "ref-0002"
            self.assertTrue((out_dir / "content.sha256").is_file())
            self.assertEqual(result["scope"], "display_only_cross_pipeline_evidence")
            self.assertTrue(result["posterior_primary_producer_check"]
                            ["local_slot_and_order_match"])
            rendered = (context_dir / "reference-sections.md").read_text(
                encoding="utf-8")
            for reference_id in ("ref-0001", "ref-0002", "ref-0003"):
                self.assertIn(f"## {reference_id}", rendered)

            typed = json.loads(
                (context_dir / "typed-tracks.json").read_text(encoding="utf-8"))
            self.assertEqual(
                [track["kind"] for track in typed],
                speaker_residual_evidence_packet.TRACK_KINDS)
            by_kind = {track["kind"]: track["entries"] for track in typed}
            voiceprints = by_kind["speaker_voiceprint"]
            self.assertEqual(
                [item["entry"]["evidence_id"] for item in voiceprints],
                ["complete_source:7", "vad:9"])
            self.assertEqual(
                by_kind["business_speaker"][0]["entry"],
                inputs[5][-1]["entries"][0])

            posterior_lines = (
                context_dir / "sortformer-frames.csv").read_text(
                    encoding="utf-8").splitlines()
            self.assertEqual(posterior_lines[1].split(",")[1], "1.000000")
            self.assertEqual(posterior_lines[-1].split(",")[1], "4.400000")

            epochs = (
                context_dir / "local-identity-epochs.tsv").read_text(
                    encoding="utf-8")
            self.assertIn("0\t0.000000000\t3.500000000\tspk_a", epochs)
            self.assertIn("0\t3.500000000\t6.000000000\tspk_c", epochs)

    def test_output_is_deterministic(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            inputs = self.write_fixture(root)
            first = root / "first"
            second = root / "second"
            speaker_residual_evidence_packet.export_packet(*inputs[:5], first)
            speaker_residual_evidence_packet.export_packet(*inputs[:5], second)
            self.assertEqual(
                (first / "content.sha256").read_text(encoding="utf-8"),
                (second / "content.sha256").read_text(encoding="utf-8"))

    def test_rejects_context_columns_that_could_carry_judgments(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            inputs = self.write_fixture(root)
            contexts_path = inputs[3]
            contexts_path.write_text(
                contexts_path.read_text(encoding="utf-8").replace(
                    "\tcontrol_refs\n", "\tcontrol_refs\tverdict\n").replace(
                    "\t0001,0003\n", "\t0001,0003\tvalue\n"),
                encoding="utf-8")
            with self.assertRaises(ValueError):
                speaker_residual_evidence_packet.export_packet(
                    inputs[0], inputs[1], inputs[2], inputs[3], inputs[4],
                    root / "packet")

    def test_rejects_artifact_identity_mismatch(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            inputs = self.write_fixture(root)
            manifest = json.loads(inputs[1].read_text(encoding="utf-8"))
            manifest["artifact"]["sha256"] = "0" * 64
            inputs[1].write_text(json.dumps(manifest), encoding="utf-8")
            with self.assertRaises(ValueError):
                speaker_residual_evidence_packet.export_packet(
                    *inputs[:5], root / "packet")

    def test_rejects_posterior_that_does_not_reproduce_primary(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            inputs = self.write_fixture(root)
            posterior = inputs[4]
            posterior.write_text(
                posterior.read_text(encoding="utf-8").replace(
                    "10,1.000000,0,0,0.800000",
                    "10,1.000000,0,1,0.800000"),
                encoding="utf-8")
            with self.assertRaises(ValueError):
                speaker_residual_evidence_packet.export_packet(
                    *inputs[:5], root / "packet")


if __name__ == "__main__":
    unittest.main()
