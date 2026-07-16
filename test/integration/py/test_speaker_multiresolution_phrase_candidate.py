#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import unittest


REPO = pathlib.Path(__file__).resolve().parents[3]
TOOLS = REPO / "tools" / "verify" / "py"
sys.path.insert(0, str(TOOLS))
MODULE_PATH = TOOLS / "speaker_multiresolution_phrase_candidate.py"
SPEC = importlib.util.spec_from_file_location(
    "speaker_multiresolution_phrase_candidate", MODULE_PATH)
candidate = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = candidate
SPEC.loader.exec_module(candidate)


POLICY = {
    "frame_activity_threshold": 0.5,
    "minimum_sustained_run_sec": 0.4,
    "minimum_piece_duration_sec": 0.4,
    "regular_piece_min_sec": 1.5,
    "short_max_sec": 1.5,
    "regular_min_score": 0.55,
    "minimum_micro_run_sec": 0.08,
    "allow_micro_local_phrase_consensus": True,
    "allow_three_view_regular_anchor_split": True,
    "allow_regular_score_dual_voiceprint_override": True,
    "allow_isolated_unanchored_short_override": True,
    "reject_competing_direct_anchors_for_dual_override": True,
}


def fragment(speaker_id):
    return {
        "anchor": True,
        "decision": {
            "speaker_id": speaker_id,
            "reason": "baseline_confirmed_regular_direct_voiceprint",
        },
    }


def decision(mapped="spk_1", selected="spk_2", score=0.4,
             reason="phrase_short_voiceprint:piece_posterior_voiceprint_conflict"):
    return {
        "mapped_speaker_id": mapped,
        "speaker_id": selected,
        "ranked": [
            {"speaker_id": selected, "score": score},
            {"speaker_id": "spk_3", "score": score - 0.1},
        ],
        "reason": reason,
    }


class SpeakerMultiresolutionPhraseCandidateTest(unittest.TestCase):
    def test_three_view_consensus_can_split_regular_anchor(self):
        value = decision(
            mapped="spk_2", selected="spk_2", score=0.3,
            reason=("phrase_short_voiceprint:"
                    "piece_regular_direct_anchor_conflict"))
        accepted, reason = candidate.correction_for_piece(
            value, {"speaker_id": "spk_2"}, [fragment("spk_1")],
            {"start": 0.0, "end": 2.0}, POLICY)
        self.assertTrue(accepted)
        self.assertEqual(reason, "three_view_regular_anchor_split")

    def test_strong_dual_voiceprint_rejects_competing_anchors(self):
        value = decision(score=0.7)
        accepted, reason = candidate.correction_for_piece(
            value, {"speaker_id": "spk_2"},
            [fragment("spk_1"), fragment("spk_3")],
            {"start": 0.0, "end": 0.8}, POLICY)
        self.assertFalse(accepted)
        self.assertEqual(reason, "multiresolution_correction_rejected")

    def test_isolated_unanchored_short_run_can_correct_local(self):
        value = decision(score=0.3)
        accepted, reason = candidate.correction_for_piece(
            value, {"speaker_id": "spk_2"}, [],
            {"start": 4.0, "end": 5.0}, POLICY)
        self.assertTrue(accepted)
        self.assertEqual(
            reason, "isolated_unanchored_short_voiceprint_override")
        accepted, _ = candidate.correction_for_piece(
            value, {"speaker_id": "spk_2"}, [],
            {"start": 4.0, "end": 6.0}, POLICY)
        self.assertFalse(accepted)

    def test_micro_consensus_preserves_sole_conflicting_anchor(self):
        piece = {"source_start": 0, "source_end": 1}
        accepted, reason = candidate.micro_decision(
            piece, {"speaker_id": "spk_2"}, "spk_2",
            [fragment("spk_1")], POLICY)
        self.assertFalse(accepted)
        self.assertEqual(reason, "micro_conflicting_sole_direct_anchor")
        accepted, reason = candidate.micro_decision(
            piece, {"speaker_id": "spk_2"}, "spk_2",
            [fragment("spk_1"), fragment("spk_2")], POLICY)
        self.assertTrue(accepted)
        self.assertEqual(reason, "micro_local_phrase_consensus")

    def test_micro_enumeration_keeps_only_sub_embedding_runs(self):
        phrases = {
            "asr": {"1": {"text": "甲乙。"}},
            "align": {"1": [
                {"text": "甲", "start": 0.0, "end": 0.16},
                {"text": "乙", "start": 0.16, "end": 0.32},
            ]},
            "phrases": [{
                "evidence_id": "phrase:1", "text_id": 1,
                "source_start": 0, "source_end": 3,
                "start": 0.0, "end": 0.32,
            }],
        }
        frames = [
            {"frame": index, "time": 0.04 + 0.08 * index,
             "local": 2, "probability": 0.9}
            for index in range(4)
        ]
        pieces = candidate.enumerate_micro_pieces(
            phrases, frames, 0.08, POLICY)
        self.assertEqual(len(pieces), 1)
        self.assertEqual(pieces[0]["source_end"], 3)
        self.assertEqual(pieces[0]["local_speaker"], 2)


if __name__ == "__main__":
    unittest.main()
