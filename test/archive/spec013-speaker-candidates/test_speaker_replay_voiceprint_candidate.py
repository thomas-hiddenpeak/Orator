#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import unittest


REPO = pathlib.Path(__file__).resolve().parents[3]
MODULE_PATH = (
    REPO / "tools" / "verify" / "py" /
    "speaker_replay_voiceprint_candidate.py")
SPEC = importlib.util.spec_from_file_location(
    "speaker_replay_voiceprint_candidate", MODULE_PATH)
speaker_replay_voiceprint_candidate = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = speaker_replay_voiceprint_candidate
SPEC.loader.exec_module(speaker_replay_voiceprint_candidate)


POLICY = {
    "short_max_sec": 1.5,
    "short_min_score": 0.25,
    "short_min_margin": 0.05,
    "regular_min_score": 0.58,
    "regular_min_margin": 0.04,
}


class SpeakerReplayVoiceprintCandidateTest(unittest.TestCase):
    def test_short_gate_accepts_score_and_margin(self):
        selected, reason, _ = (
            speaker_replay_voiceprint_candidate.select_identity(
                {"status": "ok", "duration_sec": 0.88,
                 "scores": {"spk_1": 0.23, "spk_3": 0.29}},
                ["spk_1", "spk_3"], POLICY))
        self.assertEqual(selected, "spk_3")
        self.assertEqual(reason, "short_direct_voiceprint")

    def test_short_gate_rejects_small_margin(self):
        selected, reason, _ = (
            speaker_replay_voiceprint_candidate.select_identity(
                {"status": "ok", "duration_sec": 1.0,
                 "scores": {"spk_1": 0.27, "spk_3": 0.30}},
                ["spk_1", "spk_3"], POLICY))
        self.assertIsNone(selected)
        self.assertEqual(reason, "short_margin_below_gate")

    def test_regular_gate_keeps_existing_threshold(self):
        selected, reason, _ = (
            speaker_replay_voiceprint_candidate.select_identity(
                {"status": "ok", "duration_sec": 2.0,
                 "scores": {"spk_1": 0.50, "spk_3": 0.57}},
                ["spk_1", "spk_3"], POLICY))
        self.assertIsNone(selected)
        self.assertEqual(reason, "regular_score_below_gate")

    def test_inactive_identity_is_filtered(self):
        selected, _, ranked = (
            speaker_replay_voiceprint_candidate.select_identity(
                {"status": "ok", "duration_sec": 0.8,
                 "scores": {"stale": 0.95, "spk_1": 0.30,
                            "spk_3": 0.20}},
                ["spk_1", "spk_3"], POLICY))
        self.assertEqual(selected, "spk_1")
        self.assertNotIn("stale", {speaker_id for speaker_id, _ in ranked})


if __name__ == "__main__":
    unittest.main()
