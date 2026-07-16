#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[3]
TOOLS = ROOT / "tools" / "verify" / "py"
sys.path.insert(0, str(TOOLS))
MODULE_PATH = TOOLS / "speaker_maximal_multislot_envelope_candidate.py"
SPEC = importlib.util.spec_from_file_location("maximal_envelope", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def run(start, end, local):
    return {"start": start, "end": end, "local_speaker": local}


def evidence(best, second):
    scores = {"spk_1": 0.10, "spk_2": 0.10, "spk_3": 0.10}
    scores[second] = 0.20
    scores[best] = 0.70
    return {"status": "ok", "duration_sec": 2.0, "scores": scores}


class MaximalMultislotEnvelopeCandidateTest(unittest.TestCase):
    def setUp(self):
        self.policy = MODULE.load_policy(
            ROOT / "specs" / "013-industrial-closing-validation" /
            "speaker-v21-maximal-multislot-envelope.toml")

    def test_query_uses_full_window_and_balances_outer_audio(self):
        bounds = MODULE.query_bounds(
            run(0.0, 6.0, 0), run(7.0, 13.0, 0), self.policy)
        self.assertAlmostEqual(bounds[0], 1.5)
        self.assertAlmostEqual(bounds[1], 11.5)

    def test_query_gives_unused_side_capacity_to_other_outer_run(self):
        bounds = MODULE.query_bounds(
            run(0.0, 1.0, 0), run(2.0, 12.0, 0), self.policy)
        self.assertAlmostEqual(bounds[0], 0.0)
        self.assertAlmostEqual(bounds[1], 10.0)

    def test_selects_farthest_fitting_same_channel_closure(self):
        runs = [
            run(0.0, 2.0, 0), run(2.0, 2.5, 1),
            run(2.5, 2.9, 2), run(2.9, 4.0, 0),
            run(4.0, 4.4, 1), run(4.4, 4.8, 2),
            run(4.8, 7.0, 0),
        ]
        envelopes = MODULE.maximal_envelopes(runs, self.policy)
        self.assertEqual(envelopes[0]["left_run_index"], 0)
        self.assertEqual(envelopes[0]["right_run_index"], 6)
        self.assertEqual(envelopes[0]["dominant_foreign_local_speaker"], 1)

    def test_single_foreign_channel_is_ineligible(self):
        runs = [run(0.0, 2.0, 0), run(2.0, 2.5, 1), run(2.5, 4.5, 0)]
        self.assertEqual(MODULE.maximal_envelopes(runs, self.policy), [])

    def test_foreign_duration_must_be_less_than_outer_duration(self):
        runs = [
            run(0.0, 0.5, 0), run(0.5, 1.5, 1),
            run(1.5, 2.5, 2), run(2.5, 3.0, 0),
        ]
        self.assertEqual(MODULE.maximal_envelopes(runs, self.policy), [])

    def test_tied_foreign_duration_has_no_arbitrary_dominant_channel(self):
        totals, dominant = MODULE.foreign_duration_summary(
            [run(1.0, 2.0, 1), run(2.0, 3.0, 2)], 0)
        self.assertEqual(totals, {1: 1.0, 2: 1.0})
        self.assertIsNone(dominant)

    def test_projection_keeps_complete_clauses_across_sources(self):
        metadata = {
            "asr": {
                "0": {"start": 0.0, "end": 2.0, "text": "甲句。"},
                "1": {"start": 2.0, "end": 4.0, "text": "乙句。"},
            },
            "align": {
                "0": [{"start": 1.0, "end": 1.2, "text": "甲句"}],
                "1": [{"start": 2.8, "end": 3.0, "text": "乙句"}],
            },
        }
        baseline = [
            {"text_id": 0, "text": "甲句。", "speaker_id": "spk_2"},
            {"text_id": 1, "text": "乙句。", "speaker_id": "spk_3"},
        ]
        fragments = MODULE.relative.source_fragments(baseline)
        envelope = {
            "query_start": 0.5, "query_end": 3.5,
            "interior_start": 0.8, "interior_end": 3.2,
        }
        clauses = MODULE.projection_clauses(
            metadata, envelope, fragments, "spk_1", self.policy)
        self.assertEqual([item["text_id"] for item in clauses], [0, 1])
        self.assertEqual([item["text"] for item in clauses], ["甲句。", "乙句。"])

    def test_incomplete_clause_alignment_is_not_projected(self):
        metadata = {
            "asr": {"0": {"start": 0.0, "end": 2.0, "text": "甲句。"}},
            "align": {"0": [
                {"start": 1.0, "end": 1.1, "text": "甲"},
            ]},
        }
        fragments = MODULE.relative.source_fragments([{
            "text_id": 0, "text": "甲句。", "speaker_id": "spk_2"}])
        envelope = {
            "query_start": 0.5, "query_end": 1.5,
            "interior_start": 0.8, "interior_end": 1.2,
        }
        self.assertEqual(MODULE.projection_clauses(
            metadata, envelope, fragments, "spk_1", self.policy), [])

    def test_dominant_foreign_dual_phrase_top_rank_vetoes(self):
        piece = {
            "mapped_dominant_foreign_identity": "spk_2",
            "intersecting_phrase_evidence_ids": ["phrase:0"],
        }
        values = {"phrase:0": evidence("spk_2", "spk_1")}
        veto, audit = MODULE.dominant_phrase_veto(
            piece, values, values, ["spk_1", "spk_2", "spk_3"])
        self.assertTrue(veto)
        self.assertTrue(audit[0]["veto"])

    def test_all_projection_clauses_must_share_one_vad_segment(self):
        clauses = [
            {"projection_start": 1.1, "projection_end": 1.4},
            {"projection_start": 1.6, "projection_end": 1.9},
        ]
        self.assertEqual(
            MODULE.containing_vad_segment(clauses, [(1.0, 2.0)]),
            (0, 1.0, 2.0))
        self.assertIsNone(MODULE.containing_vad_segment(
            clauses, [(1.0, 1.5), (1.5, 2.0)]))

    def test_phrase_top_rank_disagreement_does_not_veto(self):
        piece = {
            "mapped_dominant_foreign_identity": "spk_2",
            "intersecting_phrase_evidence_ids": ["phrase:0"],
        }
        session = {"phrase:0": evidence("spk_2", "spk_1")}
        robust = {"phrase:0": evidence("spk_3", "spk_2")}
        veto, _ = MODULE.dominant_phrase_veto(
            piece, session, robust, ["spk_1", "spk_2", "spk_3"])
        self.assertFalse(veto)

    def test_overlapping_source_ranges_conflict(self):
        left = {"projection_clauses": [
            {"text_id": 0, "source_start": 0, "source_end": 4}]}
        right = {"projection_clauses": [
            {"text_id": 0, "source_start": 3, "source_end": 6}]}
        self.assertTrue(MODULE.decision_intervals_overlap(left, right))
        right["projection_clauses"][0]["text_id"] = 1
        self.assertFalse(MODULE.decision_intervals_overlap(left, right))

    def test_policy_reuses_frozen_production_contracts(self):
        self.assertEqual(self.policy["frame_activity_threshold"], 0.5)
        self.assertEqual(self.policy["minimum_outer_run_sec"], 0.4)
        self.assertEqual(self.policy["maximum_query_sec"], 10.0)
        self.assertEqual(self.policy["minimum_distinct_foreign_channels"], 2)
        self.assertTrue(
            self.policy["require_projection_clauses_in_one_vad_segment"])
        self.assertEqual(self.policy["voiceprint"]["regular_min_score"], 0.55)
        self.assertEqual(self.policy["voiceprint"]["regular_min_margin"], 0.04)


if __name__ == "__main__":
    unittest.main()
