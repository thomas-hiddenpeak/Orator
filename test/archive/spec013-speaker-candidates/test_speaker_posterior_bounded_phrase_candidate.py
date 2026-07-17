#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import unittest


REPO = pathlib.Path(__file__).resolve().parents[3]
TOOLS = REPO / "tools" / "verify" / "py"
sys.path.insert(0, str(TOOLS))
MODULE_PATH = TOOLS / "speaker_posterior_bounded_phrase_candidate.py"
SPEC = importlib.util.spec_from_file_location(
    "speaker_posterior_bounded_phrase_candidate", MODULE_PATH)
candidate = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = candidate
SPEC.loader.exec_module(candidate)


POLICY = {
    "frame_activity_threshold": 0.5,
    "minimum_sustained_run_sec": 0.4,
    "minimum_piece_duration_sec": 0.4,
    "regular_piece_min_sec": 1.5,
    "preserve_regular_direct_anchors": True,
    "allow_regular_consensus_over_short_anchor": True,
}


def fragment(start, end, speaker_id, reason):
    return {
        "source_start": start,
        "source_end": end,
        "anchor": "direct_voiceprint" in reason,
        "decision": {"speaker_id": speaker_id, "reason": reason},
    }


class SpeakerPosteriorBoundedPhraseCandidateTest(unittest.TestCase):
    def test_active_runs_drop_short_transition(self):
        frames = [
            {"frame": index, "time": 0.04 + 0.08 * index,
             "local": 1 if index == 5 else 0, "probability": 0.9}
            for index in range(12)
        ]
        runs = candidate.active_runs(frames, 0.08, 0.0, 1.0, POLICY)
        self.assertEqual([item["local_speaker"] for item in runs], [0, 0])
        self.assertTrue(all(item["frame_count"] >= 5 for item in runs))

    def test_phrase_pieces_split_on_sustained_local_transition(self):
        phrase = {"source_start": 0, "source_end": 4}
        source = "甲乙丙丁"
        units = [
            {"text": "甲", "start": 0.0, "end": 0.5},
            {"text": "乙", "start": 0.5, "end": 1.0},
            {"text": "丙", "start": 1.0, "end": 1.5},
            {"text": "丁", "start": 1.5, "end": 2.0},
        ]
        runs = [
            {"start": 0.0, "end": 1.0, "local_speaker": 0,
             "frame_start": 0, "frame_end": 12, "frame_count": 12,
             "mean_probability": 0.9},
            {"start": 1.0, "end": 2.0, "local_speaker": 3,
             "frame_start": 12, "frame_end": 25, "frame_count": 13,
             "mean_probability": 0.9},
        ]
        pieces = candidate.phrase_pieces(
            phrase, source, units, runs, POLICY)
        self.assertEqual(
            [(item["source_start"], item["source_end"],
              item["local_speaker"]) for item in pieces],
            [(0, 2, 0), (2, 4, 3)])

    def test_phrase_piece_keeps_trailing_unaligned_punctuation(self):
        phrase = {"source_start": 0, "source_end": 3}
        units = [
            {"text": "甲", "start": 0.0, "end": 0.5},
            {"text": "乙", "start": 0.5, "end": 1.0},
        ]
        runs = [{
            "start": 0.0, "end": 1.0, "local_speaker": 0,
            "frame_start": 0, "frame_end": 12, "frame_count": 12,
            "mean_probability": 0.9,
        }]
        pieces = candidate.phrase_pieces(
            phrase, "甲乙。", units, runs, POLICY)
        self.assertEqual(pieces[0]["source_end"], 3)

    def test_local_and_voiceprint_must_agree(self):
        piece = {"start": 0.0, "end": 2.0}
        accepted, reason = candidate.accept_piece(
            piece, "spk_2", "spk_1", [], POLICY)
        self.assertFalse(accepted)
        self.assertEqual(reason, "piece_posterior_voiceprint_conflict")

    def test_regular_direct_anchor_is_immutable(self):
        piece = {"start": 0.0, "end": 2.0}
        fragments = [fragment(
            0, 4, "spk_1", "baseline_confirmed_regular_direct_voiceprint")]
        accepted, reason = candidate.accept_piece(
            piece, "spk_2", "spk_2", fragments, POLICY)
        self.assertFalse(accepted)
        self.assertEqual(reason, "piece_regular_direct_anchor_conflict")

    def test_regular_consensus_can_replace_short_anchor(self):
        piece = {"start": 0.0, "end": 2.0,
                 "source_start": 0, "source_end": 4}
        fragments = [fragment(
            0, 4, "spk_1", "short_direct_voiceprint_override")]
        accepted, _ = candidate.accept_piece(
            piece, "spk_2", "spk_2", fragments, POLICY)
        self.assertTrue(accepted)
        self.assertEqual(
            candidate.overlay_ranges(piece, "spk_2", fragments, POLICY),
            [(0, 4)])


if __name__ == "__main__":
    unittest.main()
