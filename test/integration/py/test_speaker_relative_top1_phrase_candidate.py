#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import tempfile
import textwrap
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[3]
TOOLS = ROOT / "tools" / "verify" / "py"
sys.path.insert(0, str(TOOLS))
MODULE_PATH = TOOLS / "speaker_relative_top1_phrase_candidate.py"
SPEC = importlib.util.spec_from_file_location("relative_top1", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class RelativeTop1PhraseCandidateTest(unittest.TestCase):
    def setUp(self):
        self.policy = {
            "enabled": True,
            "require_vad_support": True,
            "require_session_registry_agreement": True,
            "require_robust_gallery_agreement": True,
            "require_raw_local_identity_agreement": True,
            "require_known_baseline_conflict": True,
            "allow_exact_regular_anchor_challenge": True,
            "minimum_sustained_run_sec": 0.4,
            "minimum_piece_duration_sec": 0.4,
            "voiceprint": {
                "short_max_sec": 1.5,
                "short_min_score": 0.0,
                "short_min_margin": 0.04,
                "regular_min_score": 0.55,
                "regular_min_margin": 0.04,
            },
        }

    def test_relative_run_accepts_below_activity_threshold(self):
        frames = [
            {"frame": index, "time": 1.0 + index * 0.1,
             "local": 1, "probability": 0.3}
            for index in range(5)
        ]
        runs = MODULE.relative_top1_runs(
            frames, 0.1, 0.95, 1.55, [(0.0, 2.0)], self.policy)
        self.assertEqual(len(runs), 1)
        self.assertEqual(runs[0]["local_speaker"], 1)
        self.assertAlmostEqual(runs[0]["mean_probability"], 0.3)

    def test_vad_gap_splits_and_rejects_short_runs(self):
        frames = [
            {"frame": index, "time": 1.0 + index * 0.1,
             "local": 1, "probability": 0.9}
            for index in range(8)
        ]
        runs = MODULE.relative_top1_runs(
            frames, 0.1, 0.95, 1.85,
            [(0.0, 1.25), (1.55, 2.0)], self.policy)
        self.assertEqual(runs, [])

    def evidence(self, first="spk_1", second="spk_2", margin=0.4):
        return {
            "status": "ok",
            "duration_sec": 1.0,
            "scores": {first: 0.7, second: 0.7 - margin},
        }

    def piece(self):
        return {
            "evidence_id": "relative_top1_phrase:0",
            "text_id": 0,
            "source_start": 0,
            "source_end": 2,
            "start": 1.0,
            "end": 2.0,
            "local_speaker": 0,
        }

    def fragments(self, speaker_id="spk_2"):
        return [{
            "source_start": 0,
            "source_end": 2,
            "entry": {"speaker_id": speaker_id,
                      "decision_reason":
                          "baseline_confirmed_regular_direct_voiceprint"},
        }]

    def test_accepts_three_view_known_conflict(self):
        result = MODULE.decide_piece(
            self.piece(), self.evidence(), self.evidence(), self.fragments(),
            ["spk_1", "spk_2"], {0: "spk_1", 1: "spk_2"}, self.policy)
        self.assertTrue(result["accepted"])
        self.assertEqual(result["selected_speaker_id"], "spk_1")

    def test_rejects_registry_disagreement(self):
        result = MODULE.decide_piece(
            self.piece(), self.evidence(),
            self.evidence("spk_2", "spk_1"), self.fragments(),
            ["spk_1", "spk_2"], {0: "spk_1", 1: "spk_2"}, self.policy)
        self.assertFalse(result["accepted"])
        self.assertEqual(result["reason"],
                         "relative_top1_registry_disagreement")

    def test_rejects_local_mapping_disagreement(self):
        result = MODULE.decide_piece(
            self.piece(), self.evidence(), self.evidence(), self.fragments(),
            ["spk_1", "spk_2"], {0: "spk_2", 1: "spk_1"}, self.policy)
        self.assertFalse(result["accepted"])
        self.assertEqual(result["reason"],
                         "relative_top1_raw_local_identity_disagreement")

    def test_rejects_unknown_only_baseline(self):
        result = MODULE.decide_piece(
            self.piece(), self.evidence(), self.evidence(),
            self.fragments(None), ["spk_1", "spk_2"],
            {0: "spk_1", 1: "spk_2"}, self.policy)
        self.assertFalse(result["accepted"])
        self.assertEqual(result["reason"],
                         "relative_top1_known_baseline_conflict_missing")

    def test_policy_requires_fr16j_parity(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "config.toml"
            text = (ROOT / "specs" / "013-industrial-closing-validation" /
                    "speaker-v21-relative-top1-phrase.toml").read_text(
                        encoding="ascii")
            path.write_text(
                text.replace(
                    "minimum_sustained_run_sec = 0.4\n"
                    "minimum_piece_duration_sec = 0.4\n",
                    "minimum_sustained_run_sec = 0.3\n"
                    "minimum_piece_duration_sec = 0.4\n",
                    1),
                encoding="ascii")
            with self.assertRaisesRegex(ValueError, "differs from FR16J"):
                MODULE.load_policy(path)

    def test_source_fragments_accepts_layered_track_without_decisions(self):
        fragments = MODULE.source_fragments([
            {"text_id": 7, "text": "ab", "speaker_id": "spk_1"},
            {"text_id": 7, "text": "c", "speaker_id": "spk_2"},
        ])
        self.assertEqual(
            [(item["source_start"], item["source_end"])
             for item in fragments[7]],
            [(0, 2), (2, 3)])

    def test_reads_monotonic_vad_span_tsv(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "vad.tsv"
            path.write_text(textwrap.dedent("""\
                evidence_id\tstart_sec\tend_sec\tduration_sec
                vad:0\t1.0\t2.0\t1.0
                vad:1\t3.0\t4.0\t1.0
                """), encoding="ascii")
            self.assertEqual(
                MODULE.read_vad_timeline(path), [(1.0, 2.0), (3.0, 4.0)])


if __name__ == "__main__":
    unittest.main()
