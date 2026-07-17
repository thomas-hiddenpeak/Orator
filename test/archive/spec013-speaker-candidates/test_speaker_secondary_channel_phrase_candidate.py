#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[3]
TOOLS = ROOT / "tools" / "verify" / "py"
sys.path.insert(0, str(TOOLS))
MODULE_PATH = TOOLS / "speaker_secondary_channel_phrase_candidate.py"
SPEC = importlib.util.spec_from_file_location("secondary_channel", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class SecondaryChannelPhraseCandidateTest(unittest.TestCase):
    def setUp(self):
        self.voiceprint = {
            "short_max_sec": 1.5,
            "short_min_score": 0.0,
            "short_min_margin": 0.04,
            "regular_min_score": 0.55,
            "regular_min_margin": 0.04,
        }
        self.policy = {
            "minimum_selected_channel_sec": 0.4,
            "voiceprint": self.voiceprint,
        }

    @staticmethod
    def frame(index, top1, selected_probability):
        return {
            "frame": index, "time": 0.04 + 0.08 * index,
            "top1": top1,
            "probabilities": {0: 0.8, 1: selected_probability},
        }

    def test_sustained_secondary_channel_uses_common_clock_overlap(self):
        values = [self.frame(index, 0, 0.7) for index in range(6)]
        support, secondary = MODULE.channel_support(
            values, 0.08, 0.0, 0.48, 0.5)
        self.assertAlmostEqual(support["1"], 0.48)
        self.assertEqual(secondary["1"], 6)

    def test_activity_gap_breaks_sustained_support(self):
        values = [self.frame(index, 0, 0.7) for index in range(6)]
        values[3]["probabilities"][1] = 0.4
        support, _ = MODULE.channel_support(
            values, 0.08, 0.0, 0.48, 0.5)
        self.assertAlmostEqual(support["1"], 0.24)

    @staticmethod
    def evidence(first="spk_2", second="spk_3", margin=0.2):
        return {
            "status": "ok", "duration_sec": 2.0,
            "scores": {first: 0.3, second: 0.3 - margin, "spk_1": 0.0},
        }

    def decide(self, piece=None, current=None, robust=None):
        return MODULE.decide_piece(
            piece or {
                "raw_channel_sustained_sec": {"1": 0.4},
                "raw_channel_non_top1_frame_count": {"1": 5},
            },
            current or self.evidence(), robust or self.evidence(),
            [{"entry": {"speaker_id": "spk_1"}}],
            ["spk_1", "spk_2", "spk_3"],
            {0: "spk_1", 1: "spk_2", 2: "spk_3"}, self.policy)

    def test_accepts_subscore_dual_rank_with_sustained_secondary_channel(self):
        result = self.decide()
        self.assertTrue(result["accepted"])
        self.assertEqual(result["selected_speaker_id"], "spk_2")

    def test_rejects_margin_registry_or_channel_failure(self):
        self.assertFalse(self.decide(
            current=self.evidence(margin=0.03))["accepted"])
        self.assertFalse(self.decide(
            robust=self.evidence("spk_3", "spk_2"))["accepted"])
        self.assertFalse(self.decide(piece={
            "raw_channel_sustained_sec": {"1": 0.39},
            "raw_channel_non_top1_frame_count": {"1": 5},
        })["accepted"])

    def test_rejects_channel_that_is_always_top1(self):
        self.assertFalse(self.decide(piece={
            "raw_channel_sustained_sec": {"1": 0.4},
            "raw_channel_non_top1_frame_count": {"1": 0},
        })["accepted"])

    def test_policy_preserves_inherited_activity_and_duration(self):
        policy = MODULE.load_policy(
            ROOT / "specs" / "013-industrial-closing-validation" /
            "speaker-v21-secondary-channel-phrase.toml")
        self.assertEqual(policy["frame_activity_threshold"], 0.5)
        self.assertEqual(policy["minimum_selected_channel_sec"], 0.4)
        self.assertEqual(policy["maximum_duration_sec"], 3.0)


if __name__ == "__main__":
    unittest.main()
