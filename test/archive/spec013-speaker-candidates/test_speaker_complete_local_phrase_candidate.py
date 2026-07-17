#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[3]
TOOLS = ROOT / "tools" / "verify" / "py"
sys.path.insert(0, str(TOOLS))
MODULE_PATH = TOOLS / "speaker_complete_local_phrase_candidate.py"
SPEC = importlib.util.spec_from_file_location("complete_local_phrase", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class CompleteLocalPhraseCandidateTest(unittest.TestCase):
    def setUp(self):
        self.policy = {
            "minimum_phrase_sec": 0.4,
            "maximum_phrase_sec": 3.0,
            "voiceprint": {
                "short_max_sec": 1.5,
                "short_min_score": 0.0,
                "short_min_margin": 0.04,
                "regular_min_score": 0.55,
                "regular_min_margin": 0.04,
            },
        }
        self.metadata = {"phrases": [{
            "evidence_id": "punctuation_phrase:0",
            "text_id": 0,
            "source_start": 0,
            "source_end": 4,
            "start": 1.0,
            "end": 1.8,
            "text": "complete",
        }]}

    @staticmethod
    def frames(channels):
        return [
            {"frame": index, "time": 1.04 + 0.08 * index,
             "local": channel, "probability": 0.7}
            for index, channel in enumerate(channels)
        ]

    def test_enumerates_only_complete_vad_contained_single_channel_phrase(self):
        pieces = MODULE.enumerate_pieces(
            self.metadata, self.frames([2] * 10), [(0.9, 1.9)], self.policy)
        self.assertEqual(len(pieces), 1)
        self.assertEqual(pieces[0]["text"], "complete")
        self.assertEqual(pieces[0]["local_speaker"], 2)
        self.assertEqual(pieces[0]["source_start"], 0)
        self.assertEqual(pieces[0]["source_end"], 4)

    def test_rejects_partial_vad_container_and_channel_transition(self):
        self.assertEqual(MODULE.enumerate_pieces(
            self.metadata, self.frames([2] * 10), [(1.1, 1.9)], self.policy), [])
        self.assertEqual(MODULE.enumerate_pieces(
            self.metadata, self.frames([2] * 5 + [3] * 5),
            [(0.9, 1.9)], self.policy), [])

    @staticmethod
    def evidence(first=None, second="spk_1"):
        if first is None:
            return {"status": "ok", "duration_sec": 1.0,
                    "scores": {"spk_2": 0.3, "spk_1": 0.28}}
        return {"status": "ok", "duration_sec": 1.0,
                "scores": {first: 0.7, second: 0.2}}

    def decide(self, current=None, robust=None, fragments=None):
        piece = {"local_speaker": 2}
        if fragments is None:
            fragments = [{"entry": {"speaker_id": "spk_1"}}]
        return MODULE.decide_piece(
            piece, current or self.evidence(), robust or self.evidence(),
            fragments, ["spk_1", "spk_2"],
            {0: "spk_1", 2: "spk_2"}, self.policy,
            "complete_local_phrase")

    def test_accepts_uniform_conflict_when_both_voiceprints_abstain(self):
        result = self.decide()
        self.assertTrue(result["accepted"])
        self.assertEqual(result["selected_speaker_id"], "spk_2")

    def test_either_different_voiceprint_vetoes(self):
        session_veto = self.decide(current=self.evidence("spk_1", "spk_2"))
        robust_veto = self.decide(robust=self.evidence("spk_1", "spk_2"))
        self.assertFalse(session_veto["accepted"])
        self.assertEqual(session_veto["reason"],
                         "complete_local_phrase_session_voiceprint_veto")
        self.assertFalse(robust_veto["accepted"])
        self.assertEqual(robust_veto["reason"],
                         "complete_local_phrase_robust_voiceprint_veto")

    def test_top_ranked_baseline_vetoes_when_gate_abstains(self):
        abstaining_baseline_top = {
            "status": "ok", "duration_sec": 1.0,
            "scores": {"spk_1": 0.3, "spk_2": 0.28},
        }
        result = self.decide(current=abstaining_baseline_top)
        self.assertFalse(result["accepted"])
        self.assertEqual(
            result["reason"],
            "complete_local_phrase_session_top_ranked_baseline_veto")

    def test_rejects_unknown_or_mixed_baseline(self):
        unknown = self.decide(fragments=[{"entry": {"speaker_id": None}}])
        mixed = self.decide(fragments=[
            {"entry": {"speaker_id": "spk_1"}},
            {"entry": {"speaker_id": "spk_2"}},
        ])
        self.assertFalse(unknown["accepted"])
        self.assertFalse(mixed["accepted"])

    def test_policy_requires_inherited_duration_bounds(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "config.toml"
            text = (ROOT / "specs" / "013-industrial-closing-validation" /
                    "speaker-v21-complete-local-phrase.toml").read_text(
                        encoding="ascii")
            path.write_text(text.replace(
                "maximum_phrase_sec = 3.0", "maximum_phrase_sec = 2.9"),
                encoding="ascii")
            with self.assertRaisesRegex(ValueError, "maximum differs"):
                MODULE.load_policy(path)


if __name__ == "__main__":
    unittest.main()
