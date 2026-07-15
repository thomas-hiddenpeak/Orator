#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import unittest
from unittest import mock


REPO = pathlib.Path(__file__).resolve().parents[3]
MODULE_PATH = (
    REPO / "tools" / "verify" / "py" / "speaker_sequence_candidate.py")
SPEC = importlib.util.spec_from_file_location(
    "speaker_sequence_candidate", MODULE_PATH)
speaker_sequence_candidate = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = speaker_sequence_candidate
SPEC.loader.exec_module(speaker_sequence_candidate)


class SpeakerSequenceCandidateTest(unittest.TestCase):
    def test_build_candidate_preserves_source_extent(self):
        evidence = {
            "turns": [],
            "resolved_config": {},
            "audio_sec": 3615.12,
            "sample_rate": 16000,
        }
        with mock.patch.multiple(
                speaker_sequence_candidate,
                discover_active_ids=mock.DEFAULT,
                build_turn_features=mock.DEFAULT,
                build_anchors=mock.DEFAULT,
                stable_local_priors=mock.DEFAULT) as patched:
            patched["discover_active_ids"].return_value = ([], {})
            patched["build_turn_features"].return_value = []
            patched["build_anchors"].return_value = {}
            patched["stable_local_priors"].return_value = ({}, {})
            result = speaker_sequence_candidate.build_candidate(evidence)
        self.assertEqual(result["audio_sec"], 3615.12)
        self.assertEqual(result["sample_rate"], 16000)

    def test_score_gate_requires_threshold_and_margin(self):
        self.assertTrue(speaker_sequence_candidate.score_gate(
            [("spk_1", 0.8), ("spk_2", 0.6)], 0.72, 0.08))
        self.assertFalse(speaker_sequence_candidate.score_gate(
            [("spk_1", 0.7), ("spk_2", 0.5)], 0.72, 0.08))
        self.assertFalse(speaker_sequence_candidate.score_gate(
            [("spk_1", 0.8), ("spk_2", 0.75)], 0.72, 0.08))

    def test_nearest_anchor_rejects_equidistant_identity_conflict(self):
        anchors = {0: [
            {"time_sec": 90.0, "speaker_id": "spk_1",
             "turn_id": "a"},
            {"time_sec": 110.0, "speaker_id": "spk_2",
             "turn_id": "b"},
        ]}
        result = speaker_sequence_candidate.nearest_anchor_prior(
            0, 100.0, anchors, 120.0, 4.0)
        self.assertIsNone(result)

    def test_nearest_anchor_returns_closest_supported_identity(self):
        anchors = {0: [
            {"time_sec": 80.0, "speaker_id": "spk_1",
             "turn_id": "a"},
            {"time_sec": 110.0, "speaker_id": "spk_2",
             "turn_id": "b"},
        ]}
        result = speaker_sequence_candidate.nearest_anchor_prior(
            0, 100.0, anchors, 120.0, 4.0)
        self.assertEqual(result["speaker_id"], "spk_2")
        self.assertEqual(result["anchor_turn_id"], "b")

    def test_ranked_scores_can_filter_inactive_registry_ids(self):
        turn = {"titanet": {"scores": {
            "spk_0": 0.9, "spk_1": 0.8, "spk_2": 0.7}}}
        result = speaker_sequence_candidate.ranked_scores(
            turn, {"spk_1", "spk_2"})
        self.assertEqual(result, [("spk_1", 0.8), ("spk_2", 0.7)])

    def test_dominant_local_rejects_inactive_top_one_frames(self):
        turn = {"sortformer": {
            "active_top1_duration_sec": [0.2, 0.0],
            "active_duration_sec": [0.2, 0.0],
            "any_active_duration_sec": 2.0,
        }}
        local, dominance, activity = (
            speaker_sequence_candidate.dominant_local(turn, 0.5))
        self.assertIsNone(local)
        self.assertAlmostEqual(dominance, 0.1)
        self.assertAlmostEqual(activity, 0.1)

    def test_segment_scores_use_only_dominant_local_overlap(self):
        turn = {"diar_segments": [
            {
                "evidence_id": "diarization:1",
                "local_speaker": 0,
                "overlap_sec": 2.0,
                "titanet": {"status": "ok", "scores": {
                    "spk_1": 0.8, "spk_2": 0.2}},
            },
            {
                "evidence_id": "diarization:2",
                "local_speaker": 1,
                "overlap_sec": 3.0,
                "titanet": {"status": "ok", "scores": {
                    "spk_1": 0.1, "spk_2": 0.9}},
            },
        ]}
        ranked, evidence_ids = (
            speaker_sequence_candidate.aggregate_segment_scores(
                turn, 0, ["spk_1", "spk_2"]))
        self.assertEqual(ranked, [("spk_1", 0.8), ("spk_2", 0.2)])
        self.assertEqual(evidence_ids, ["diarization:1"])

    def test_direct_voiceprint_rejects_two_strong_identities(self):
        sources = [
            {"name": "turn", "ranked": [("a", 0.9), ("b", 0.2)],
             "support_ids": ["turn:1"], "strong": True,
             "candidate": True},
            {"name": "diar_segment",
             "ranked": [("b", 0.85), ("a", 0.2)],
             "support_ids": ["diar:1"], "strong": True,
             "candidate": True},
        ]
        result = speaker_sequence_candidate.select_direct_voiceprint(sources)
        self.assertEqual(result["status"], "conflict")
        self.assertEqual(result["strength"], "strong")


if __name__ == "__main__":
    unittest.main()
