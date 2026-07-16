#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[3]
TOOLS = ROOT / "tools" / "verify" / "py"
sys.path.insert(0, str(TOOLS))
MODULE_PATH = TOOLS / "speaker_vad_utterance_candidate.py"
SPEC = importlib.util.spec_from_file_location("vad_utterance", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class VadUtteranceCandidateTest(unittest.TestCase):
    def setUp(self):
        self.policy = {
            "minimum_duration_sec": 0.4,
            "maximum_duration_sec": 1.5,
            "voiceprint": {
                "short_max_sec": 1.5,
                "short_min_score": 0.0,
                "short_min_margin": 0.04,
                "regular_min_score": 0.55,
                "regular_min_margin": 0.04,
            },
        }
        self.metadata = {
            "asr": {"0": {"start": 0.0, "end": 2.0, "text": "啥帽子"}},
            "align": {"0": [
                {"start": 1.00, "end": 1.12, "text": "啥"},
                {"start": 1.12, "end": 1.27, "text": "帽"},
                {"start": 1.27, "end": 1.42, "text": "子"},
            ]},
        }

    @staticmethod
    def frames(channels):
        return [
            {"frame": index, "time": 1.05 + 0.1 * index,
             "local": channel, "probability": 0.3}
            for index, channel in enumerate(channels)
        ]

    def test_complete_short_vad_projects_three_character_utterance(self):
        pieces, utterances = MODULE.enumerate_pieces(
            self.metadata, self.frames([2, 2, 2, 2]),
            [(0.96, 1.46)], self.policy)
        self.assertEqual(len(utterances), 1)
        self.assertEqual(len(pieces), 1)
        self.assertEqual(pieces[0]["text"], "啥帽子")
        self.assertEqual(pieces[0]["local_speaker"], 2)
        self.assertAlmostEqual(pieces[0]["start"], 0.96)
        self.assertAlmostEqual(pieces[0]["end"], 1.46)

    def test_rejects_local_channel_change(self):
        pieces, utterances = MODULE.enumerate_pieces(
            self.metadata, self.frames([2, 2, 1, 1]),
            [(0.96, 1.46)], self.policy)
        self.assertEqual(utterances, [])
        self.assertEqual(pieces, [])

    def test_rejects_partial_alignment_unit(self):
        pieces, utterances = MODULE.enumerate_pieces(
            self.metadata, self.frames([2, 2, 2, 2]),
            [(1.13, 1.53)], self.policy)
        self.assertEqual(len(utterances), 1)
        self.assertEqual(len(pieces), 1)
        self.assertEqual(pieces[0]["text"], "子")

    @staticmethod
    def evidence(first="spk_2", second="spk_1"):
        return {
            "status": "ok",
            "duration_sec": 1.0,
            "scores": {first: 0.7, second: 0.2},
        }

    def test_accepts_three_view_known_conflict(self):
        piece = {"local_speaker": 2}
        fragments = [{"entry": {"speaker_id": "spk_1"}}]
        result = MODULE.decide_piece(
            piece, self.evidence(), self.evidence(), fragments,
            ["spk_1", "spk_2"], {0: "spk_1", 2: "spk_2"}, self.policy)
        self.assertTrue(result["accepted"])
        self.assertEqual(result["selected_speaker_id"], "spk_2")

    def test_rejects_robust_gallery_disagreement(self):
        piece = {"local_speaker": 2}
        fragments = [{"entry": {"speaker_id": "spk_1"}}]
        result = MODULE.decide_piece(
            piece, self.evidence(), self.evidence("spk_1", "spk_2"),
            fragments, ["spk_1", "spk_2"],
            {0: "spk_1", 2: "spk_2"}, self.policy)
        self.assertFalse(result["accepted"])
        self.assertEqual(result["reason"], "vad_utterance_registry_disagreement")

    def test_policy_requires_inherited_bounds(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "config.toml"
            text = (ROOT / "specs" / "013-industrial-closing-validation" /
                    "speaker-v21-vad-utterance.toml").read_text(
                        encoding="ascii")
            path.write_text(
                text.replace("minimum_duration_sec = 0.4",
                             "minimum_duration_sec = 0.3"),
                encoding="ascii")
            with self.assertRaisesRegex(ValueError, "differs from FR16J"):
                MODULE.load_policy(path)


if __name__ == "__main__":
    unittest.main()
