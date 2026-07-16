#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import unittest


REPO = pathlib.Path(__file__).resolve().parents[3]
MODULE_PATH = (
    REPO / "tools" / "verify" / "py" /
    "speaker_direct_fusion_candidate.py")
SPEC = importlib.util.spec_from_file_location(
    "speaker_direct_fusion_candidate", MODULE_PATH)
speaker_direct_fusion_candidate = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = speaker_direct_fusion_candidate
SPEC.loader.exec_module(speaker_direct_fusion_candidate)


def config():
    return {
        "speaker": {
            "local_drift_competing_threshold": 0.72,
            "local_drift_competing_margin": 0.08,
            "local_drift_competing_candidate_threshold": 0.58,
            "local_drift_competing_candidate_margin": 0.04,
        },
        "diarizer": {"max_speakers": 4},
    }


def turn(turn_scores, segment_scores, baseline="spk_1"):
    return {
        "turn_id": "business_speaker:0",
        "start_sec": 10.0,
        "end_sec": 12.0,
        "duration_sec": 2.0,
        "text_id": 1,
        "text": "text",
        "baseline_output": {
            "speaker_id": baseline,
            "local_speaker": 0,
            "speaker_uncertain": False,
        },
        "titanet": {"status": "ok", "scores": turn_scores},
        "diar_segments": [{
            "evidence_id": "diarization:0",
            "start_sec": 10.0,
            "end_sec": 12.0,
            "overlap_sec": 2.0,
            "titanet": {"status": "ok", "scores": segment_scores},
        }],
    }


class SpeakerDirectFusionCandidateTest(unittest.TestCase):
    def test_candidate_consensus_overrides_baseline(self):
        item = turn(
            {"spk_1": 0.50, "spk_2": 0.64},
            {"spk_1": 0.52, "spk_2": 0.65})
        decision = speaker_direct_fusion_candidate.decide_turn(
            item, ["spk_1", "spk_2"], config())
        self.assertEqual(decision["speaker_id"], "spk_2")
        self.assertTrue(decision["changed"])
        self.assertIn("candidate_combined_voiceprint", decision["reason"])

    def test_direct_conflict_preserves_baseline(self):
        item = turn(
            {"spk_1": 0.50, "spk_2": 0.64},
            {"spk_1": 0.66, "spk_2": 0.50})
        decision = speaker_direct_fusion_candidate.decide_turn(
            item, ["spk_1", "spk_2"], config())
        self.assertEqual(decision["speaker_id"], "spk_1")
        self.assertFalse(decision["changed"])
        self.assertIn("conflict", decision["reason"])

    def test_weak_evidence_preserves_baseline_uncertainty(self):
        item = turn(
            {"spk_1": 0.54, "spk_2": 0.56},
            {"spk_1": 0.55, "spk_2": 0.56})
        item["baseline_output"]["speaker_uncertain"] = True
        decision = speaker_direct_fusion_candidate.decide_turn(
            item, ["spk_1", "spk_2"], config())
        self.assertEqual(decision["speaker_id"], "spk_1")
        self.assertTrue(decision["speaker_uncertain"])
        self.assertFalse(decision["changed"])

    def test_inactive_registry_identity_is_filtered(self):
        item = turn(
            {"spk_0": 0.95, "spk_1": 0.50, "spk_2": 0.64},
            {"spk_0": 0.94, "spk_1": 0.52, "spk_2": 0.65})
        decision = speaker_direct_fusion_candidate.decide_turn(
            item, ["spk_1", "spk_2"], config())
        self.assertEqual(decision["speaker_id"], "spk_2")


if __name__ == "__main__":
    unittest.main()
