#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import unittest


REPO = pathlib.Path(__file__).resolve().parents[3]
MODULE_PATH = (
    REPO / "tools" / "verify" / "py" /
    "speaker_phrase_reaggregation_candidate.py")
SPEC = importlib.util.spec_from_file_location(
    "speaker_phrase_reaggregation_candidate", MODULE_PATH)
speaker_phrase_reaggregation_candidate = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = speaker_phrase_reaggregation_candidate
SPEC.loader.exec_module(speaker_phrase_reaggregation_candidate)


POLICY = {
    "max_alignment_gap_sec": 0.25,
    "max_bridge_sec": 1.5,
    "require_two_sided_agreement": True,
}


def item(index, speaker_id=None, start=None, end=None, text_id=7):
    return {
        "turn_id": f"turn:{index}",
        "start": float(index if start is None else start),
        "end": float(index + 0.2 if end is None else end),
        "text_id": text_id,
        "speaker": 0,
        "speaker_id": speaker_id,
        "speaker_uncertain": speaker_id is None,
        "text": str(index),
    }


def decision(speaker_id=None, anchor=False):
    return {
        "speaker_id": speaker_id,
        "reason": (
            "baseline_confirmed_short_direct_voiceprint"
            if anchor else "baseline_insufficient_duration"),
    }


class SpeakerPhraseReaggregationCandidateTest(unittest.TestCase):
    def test_two_sided_same_identity_bridges_short_fragment(self):
        track = [
            item(0, "spk_1", 0.0, 0.5),
            item(1, "spk_4", 0.5, 0.7),
            item(2, "spk_1", 0.7, 1.2),
        ]
        decisions = [
            decision("spk_1", True), decision("spk_4"),
            decision("spk_1", True),
        ]
        runs = {7: [(0.0, 1.2)]}
        bridges = speaker_phrase_reaggregation_candidate.find_bridges(
            track, decisions, runs, POLICY)
        self.assertEqual(len(bridges), 1)
        self.assertEqual(bridges[0]["indices"], [1])
        self.assertEqual(bridges[0]["speaker_id"], "spk_1")

    def test_conflicting_anchors_preserve_middle(self):
        track = [item(0, "spk_1"), item(1), item(2, "spk_2")]
        decisions = [
            decision("spk_1", True), decision(), decision("spk_2", True)]
        runs = {7: [(0.0, 2.2)]}
        self.assertEqual(
            speaker_phrase_reaggregation_candidate.find_bridges(
                track, decisions, runs, POLICY), [])

    def test_direct_anchor_is_not_part_of_bridge(self):
        track = [
            item(0, "spk_1", 0.0, 0.4),
            item(1, "spk_2", 0.4, 0.8),
            item(2, "spk_1", 0.8, 1.2),
        ]
        decisions = [
            decision("spk_1", True), decision("spk_2", True),
            decision("spk_1", True),
        ]
        runs = {7: [(0.0, 1.2)]}
        self.assertEqual(
            speaker_phrase_reaggregation_candidate.find_bridges(
                track, decisions, runs, POLICY), [])

    def test_overlong_bridge_is_rejected(self):
        track = [
            item(0, "spk_1", 0.0, 0.4),
            item(1, None, 0.4, 2.1),
            item(2, "spk_1", 2.1, 2.5),
        ]
        decisions = [
            decision("spk_1", True), decision(), decision("spk_1", True)]
        runs = {7: [(0.0, 2.5)]}
        self.assertEqual(
            speaker_phrase_reaggregation_candidate.find_bridges(
                track, decisions, runs, POLICY), [])

    def test_alignment_run_boundary_blocks_bridge(self):
        track = [
            item(0, "spk_1", 0.0, 0.4),
            item(1, None, 0.4, 0.6),
            item(2, "spk_1", 1.0, 1.4),
        ]
        decisions = [
            decision("spk_1", True), decision(), decision("spk_1", True)]
        runs = {7: [(0.0, 0.6), (1.0, 1.4)]}
        self.assertEqual(
            speaker_phrase_reaggregation_candidate.find_bridges(
                track, decisions, runs, POLICY), [])


if __name__ == "__main__":
    unittest.main()
