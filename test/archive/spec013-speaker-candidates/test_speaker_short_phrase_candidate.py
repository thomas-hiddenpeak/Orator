#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import unittest


REPO = pathlib.Path(__file__).resolve().parents[3]
MODULE_PATH = (
    REPO / "tools" / "verify" / "py" /
    "speaker_short_phrase_candidate.py")
SPEC = importlib.util.spec_from_file_location(
    "speaker_short_phrase_candidate", MODULE_PATH)
speaker_short_phrase_candidate = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = speaker_short_phrase_candidate
SPEC.loader.exec_module(speaker_short_phrase_candidate)


def config():
    return {
        "diarizer": {"max_speakers": 2},
        "speaker": {
            "local_drift_competing_threshold": 0.72,
            "local_drift_competing_margin": 0.08,
        },
    }


def turn(turn_scores, segment_scores, baseline="spk_a"):
    return {
        "turn_id": "business_speaker:0",
        "duration_sec": 2.0,
        "baseline_output": {
            "local_speaker": 0,
            "speaker_id": baseline,
            "speaker_uncertain": False,
        },
        "titanet": {"status": "ok", "scores": turn_scores},
        "diar_segments": [{
            "evidence_id": "diarization:0",
            "start_sec": 0.0,
            "end_sec": 2.0,
            "overlap_sec": 2.0,
            "titanet": {"status": "ok", "scores": segment_scores},
        }],
    }


class SpeakerShortPhraseCandidateTest(unittest.TestCase):
    def test_consistent_strong_sources_override_baseline(self):
        item = turn(
            {"spk_a": 0.2, "spk_b": 0.9},
            {"spk_a": 0.3, "spk_b": 0.85})
        result = speaker_short_phrase_candidate.decide_turn(
            item, ["spk_a", "spk_b"], config())
        self.assertEqual(result["speaker_id"], "spk_b")
        self.assertTrue(result["changed"])
        self.assertEqual(
            result["reason"], "strong_combined_voiceprint_override")

    def test_conflicting_strong_sources_preserve_baseline(self):
        item = turn(
            {"spk_a": 0.2, "spk_b": 0.9},
            {"spk_a": 0.9, "spk_b": 0.2})
        result = speaker_short_phrase_candidate.decide_turn(
            item, ["spk_a", "spk_b"], config())
        self.assertEqual(result["speaker_id"], "spk_a")
        self.assertFalse(result["changed"])

    def test_weak_evidence_preserves_baseline(self):
        item = turn(
            {"spk_a": 0.65, "spk_b": 0.60},
            {"spk_a": 0.68, "spk_b": 0.62})
        result = speaker_short_phrase_candidate.decide_turn(
            item, ["spk_a", "spk_b"], config())
        self.assertEqual(result["speaker_id"], "spk_a")
        self.assertFalse(result["changed"])

    def test_strong_evidence_can_fill_unknown(self):
        item = turn(
            {"spk_a": 0.2, "spk_b": 0.9},
            {"spk_a": 0.3, "spk_b": 0.85},
            baseline=None)
        result = speaker_short_phrase_candidate.decide_turn(
            item, ["spk_a", "spk_b"], config())
        self.assertEqual(result["speaker_id"], "spk_b")
        self.assertEqual(result["reason"], "strong_combined_voiceprint_fill")


if __name__ == "__main__":
    unittest.main()
