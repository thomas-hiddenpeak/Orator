#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[3]
TOOLS = ROOT / "tools" / "verify" / "py"
sys.path.insert(0, str(TOOLS))
MODULE_PATH = TOOLS / "speaker_bracketed_unknown_phrase_candidate.py"
SPEC = importlib.util.spec_from_file_location("bracketed_unknown", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)
DEFAULT = object()


class BracketedUnknownPhraseCandidateTest(unittest.TestCase):
    def setUp(self):
        self.policy = {"voiceprint": {
            "short_max_sec": 1.5,
            "short_min_score": 0.0,
            "short_min_margin": 0.04,
            "regular_min_score": 0.55,
            "regular_min_margin": 0.04,
        }}
        self.piece = {
            "source_start": 2, "source_end": 4, "local_speaker": 0,
        }
        self.left = {
            "source_start": 0, "source_end": 2,
            "entry": {"speaker_id": "spk_1"},
        }
        self.unknown = {
            "source_start": 2, "source_end": 4,
            "entry": {"speaker_id": None},
        }
        self.right = {
            "source_start": 4, "source_end": 6,
            "entry": {"speaker_id": "spk_1"},
        }

    @staticmethod
    def abstention():
        return {"status": "ok", "duration_sec": 1.0,
                "scores": {"spk_1": 0.3, "spk_2": 0.28}}

    @staticmethod
    def selected(identity, other):
        return {"status": "ok", "duration_sec": 1.0,
                "scores": {identity: 0.7, other: 0.2}}

    def decide(self, current=None, robust=None, left=DEFAULT, right=DEFAULT,
               overlaps=None):
        return MODULE.decide_piece(
            self.piece, current or self.abstention(),
            robust or self.abstention(),
            overlaps or [self.unknown],
            self.left if left is DEFAULT else left,
            self.right if right is DEFAULT else right,
            ["spk_1", "spk_2"], {0: "spk_1", 1: "spk_2"}, self.policy)

    def test_accepts_immediately_bracketed_unknown_phrase(self):
        result = self.decide()
        self.assertTrue(result["accepted"])
        self.assertEqual(result["selected_speaker_id"], "spk_1")

    def test_rejects_missing_or_disagreeing_adjacent_identity(self):
        self.assertFalse(self.decide(left=None)["accepted"])
        different = {**self.right, "entry": {"speaker_id": "spk_2"}}
        self.assertFalse(self.decide(right=different)["accepted"])

    def test_rejects_known_phrase_or_local_disagreement(self):
        known = {**self.unknown, "entry": {"speaker_id": "spk_1"}}
        self.assertFalse(self.decide(overlaps=[known])["accepted"])
        result = MODULE.decide_piece(
            self.piece, self.abstention(), self.abstention(), [self.unknown],
            self.left, self.right, ["spk_1", "spk_2"],
            {0: "spk_2", 1: "spk_1"}, self.policy)
        self.assertFalse(result["accepted"])

    def test_either_different_voiceprint_vetoes(self):
        self.assertFalse(self.decide(
            current=self.selected("spk_2", "spk_1"))["accepted"])
        self.assertFalse(self.decide(
            robust=self.selected("spk_2", "spk_1"))["accepted"])

    def test_adjacent_fragments_requires_exact_source_contact(self):
        fragments = [self.left, self.unknown, self.right]
        left, right = MODULE.adjacent_fragments(self.piece, fragments)
        self.assertIs(left, self.left)
        self.assertIs(right, self.right)
        shifted = {**self.right, "source_start": 5}
        self.assertEqual(
            MODULE.adjacent_fragments(
                self.piece, [self.left, self.unknown, shifted]),
            (None, None))


if __name__ == "__main__":
    unittest.main()
