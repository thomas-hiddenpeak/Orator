#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import unittest


REPO = pathlib.Path(__file__).resolve().parents[3]
MODULE_PATH = (
    REPO / "tools" / "verify" / "py" / "speaker_rotation_candidate.py")
SPEC = importlib.util.spec_from_file_location(
    "speaker_rotation_candidate", MODULE_PATH)
speaker_rotation_candidate = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = speaker_rotation_candidate
SPEC.loader.exec_module(speaker_rotation_candidate)


def config():
    return {
        "diarizer": {"max_speakers": 2},
        "speaker": {
            "local_drift_competing_threshold": 0.72,
            "local_drift_competing_margin": 0.08,
            "max_embed_window_sec": 10.0,
        },
        "timeline": {"speaker_support_min_coverage_ratio": 0.5},
    }


def voiceprint(a, b):
    return {
        "status": "ok",
        "scores": {"spk_a": a, "spk_b": b},
    }


def turn(turn_id, local, evidence, start=0.0):
    segment = {
        "evidence_id": f"diarization:{turn_id}",
        "start_sec": start,
        "end_sec": start + 4.0,
        "local_speaker": local,
        "confidence": 0.9,
        "overlap_sec": 4.0,
        "titanet": evidence,
    }
    return {
        "turn_id": f"business_speaker:{turn_id}",
        "start_sec": start,
        "end_sec": start + 4.0,
        "duration_sec": 4.0,
        "text_id": turn_id,
        "text": "x",
        "diar_segments": [segment],
        "titanet": evidence,
        "vad": {"coverage_sec": 4.0},
    }


class SpeakerRotationCandidateTest(unittest.TestCase):
    def test_same_slot_index_is_qualified_by_session(self):
        turns = [
            turn(0, 0, voiceprint(0.9, 0.2), 0.0),
            turn(1, 1, voiceprint(0.2, 0.9), 5.0),
            turn(2, 2, voiceprint(0.2, 0.9), 90.0),
            turn(3, 3, voiceprint(0.9, 0.2), 95.0),
        ]
        assignments, sessions = (
            speaker_rotation_candidate.assign_session_slots(
                turns, ["spk_a", "spk_b"], config()))
        self.assertEqual(assignments[0]["speaker_id"], "spk_a")
        self.assertEqual(assignments[2]["speaker_id"], "spk_b")
        self.assertEqual(set(sessions), {"0", "1"})

    def test_one_to_one_constraint_completes_unobserved_slot(self):
        observed = turn(0, 0, voiceprint(0.9, 0.2))
        unobserved = turn(1, 1, {"status": "insufficient_duration"}, 5.0)
        assignments, _ = speaker_rotation_candidate.assign_session_slots(
            [observed, unobserved], ["spk_a", "spk_b"], config())
        self.assertEqual(assignments[0]["speaker_id"], "spk_a")
        self.assertEqual(assignments[1]["speaker_id"], "spk_b")
        self.assertEqual(
            assignments[1]["reason"], "cannot_link_completion")

    def test_turn_prefers_mapped_diar_support(self):
        item = turn(0, 0, voiceprint(0.2, 0.9))
        assignments = {0: {
            "speaker_id": "spk_a", "score": 0.8, "margin": 0.3,
            "evidence_ids": ["diarization:0"],
        }}
        decision = speaker_rotation_candidate.decide_turn(
            item, assignments, ["spk_a", "spk_b"], config())
        self.assertEqual(decision["speaker_id"], "spk_a")
        self.assertEqual(
            decision["reason"], "rotated_sortformer_titanet_mapping")

    def test_turn_without_diar_requires_strong_voiceprint_and_vad(self):
        item = turn(0, 0, voiceprint(0.9, 0.2))
        item["diar_segments"] = []
        decision = speaker_rotation_candidate.decide_turn(
            item, {}, ["spk_a", "spk_b"], config())
        self.assertEqual(decision["speaker_id"], "spk_a")
        item["vad"]["coverage_sec"] = 0.0
        decision = speaker_rotation_candidate.decide_turn(
            item, {}, ["spk_a", "spk_b"], config())
        self.assertIsNone(decision["speaker_id"])


if __name__ == "__main__":
    unittest.main()
