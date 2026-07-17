#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[3]
TOOLS = ROOT / "tools" / "verify" / "py"
sys.path.insert(0, str(TOOLS))
MODULE_PATH = TOOLS / "speaker_bracketed_local_phrase_candidate.py"
SPEC = importlib.util.spec_from_file_location("bracketed_local", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class BracketedLocalPhraseCandidateTest(unittest.TestCase):
    def setUp(self):
        self.policy = {"voiceprint": {
            "short_max_sec": 1.5, "short_min_score": 0.0,
            "short_min_margin": 0.04, "regular_min_score": 0.55,
            "regular_min_margin": 0.04,
        }}
        self.piece = {"source_start": 2, "source_end": 4,
                      "local_speaker": 0}
        self.left = {"source_start": 0, "source_end": 2,
                     "entry": {"speaker_id": "spk_1"}}
        self.phrase = {"source_start": 2, "source_end": 4,
                       "entry": {"speaker_id": "spk_2"}}
        self.right = {"source_start": 4, "source_end": 6,
                      "entry": {"speaker_id": "spk_1"}}

    @staticmethod
    def evidence(first=None, second="spk_2"):
        if first is None:
            return {"status": "ok", "duration_sec": 1.0,
                    "scores": {"spk_1": 0.3, "spk_2": 0.28}}
        return {"status": "ok", "duration_sec": 1.0,
                "scores": {first: 0.7, second: 0.2}}

    def decide(self, current=None, robust=None, left=True, right=True,
               overlaps=None, mapping=None):
        return MODULE.decide_piece(
            self.piece, current or self.evidence(),
            robust or self.evidence(), overlaps or [self.phrase],
            self.left if left else None, self.right if right else None,
            ["spk_1", "spk_2"], mapping or {0: "spk_1", 1: "spk_2"},
            self.policy)

    def test_accepts_mapped_identity_bracketed_known_conflict(self):
        result = self.decide()
        self.assertTrue(result["accepted"])
        self.assertEqual(result["selected_speaker_id"], "spk_1")

    def test_rejects_missing_or_disagreeing_brackets(self):
        self.assertFalse(self.decide(left=False)["accepted"])
        other = {**self.right, "entry": {"speaker_id": "spk_2"}}
        result = MODULE.decide_piece(
            self.piece, self.evidence(), self.evidence(), [self.phrase],
            self.left, other, ["spk_1", "spk_2"],
            {0: "spk_1", 1: "spk_2"}, self.policy)
        self.assertFalse(result["accepted"])

    def test_rejects_phrase_without_known_conflict_or_local_agreement(self):
        same = {**self.phrase, "entry": {"speaker_id": "spk_1"}}
        self.assertFalse(self.decide(overlaps=[same])["accepted"])
        self.assertFalse(self.decide(mapping={0: "spk_2", 1: "spk_1"})[
            "accepted"])

    def test_either_eligible_voiceprint_conflict_vetoes(self):
        self.assertFalse(self.decide(
            current=self.evidence("spk_2", "spk_1"))["accepted"])
        self.assertFalse(self.decide(
            robust=self.evidence("spk_2", "spk_1"))["accepted"])

    def test_phrase_neighbors_require_uniform_known_complete_phrases(self):
        piece = {
            "left_phrase_source_start": 0,
            "left_phrase_source_end": 2,
            "right_phrase_source_start": 4,
            "right_phrase_source_end": 6,
        }
        left, right = MODULE.phrase_neighbors(
            piece, [self.left, self.phrase, self.right])
        self.assertEqual(left["entry"]["speaker_id"], "spk_1")
        self.assertEqual(right["entry"]["speaker_id"], "spk_1")
        mixed_right = {**self.right, "source_end": 5}
        tail = {"source_start": 5, "source_end": 6,
                "entry": {"speaker_id": "spk_2"}}
        self.assertIsNone(MODULE.phrase_neighbors(
            piece, [self.left, self.phrase, mixed_right, tail])[1])


if __name__ == "__main__":
    unittest.main()
