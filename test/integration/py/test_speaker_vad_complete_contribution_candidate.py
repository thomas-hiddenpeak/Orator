#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[3]
TOOLS = ROOT / "tools" / "verify" / "py"
sys.path.insert(0, str(TOOLS))
MODULE_PATH = TOOLS / "speaker_vad_complete_contribution_candidate.py"
SPEC = importlib.util.spec_from_file_location("vad_complete", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def run(start, end, local):
    return {"start": start, "end": end, "local_speaker": local}


def evidence(best, second):
    scores = {"spk_1": 0.10, "spk_2": 0.10, "spk_3": 0.10}
    scores[second] = 0.20
    scores[best] = 0.70
    return {"status": "ok", "duration_sec": 2.0, "scores": scores}


class VadCompleteContributionCandidateTest(unittest.TestCase):
    def setUp(self):
        self.policy = MODULE.load_policy(
            ROOT / "specs" / "013-industrial-closing-validation" /
            "speaker-v21-vad-complete-contribution.toml")

    def test_clips_stable_runs_to_vad_and_reapplies_duration_floor(self):
        runs = [run(0.0, 1.0, 0), run(1.0, 2.0, 1)]
        clipped = MODULE.clipped_stable_runs(runs, 0.8, 1.8, 0.4)
        self.assertEqual(len(clipped), 1)
        self.assertEqual(clipped[0]["local_speaker"], 1)
        self.assertAlmostEqual(clipped[0]["start"], 1.0)
        self.assertAlmostEqual(clipped[0]["end"], 1.8)

    def test_strict_duration_majority_selects_one_local_channel(self):
        totals, dominant = MODULE.strict_duration_majority([
            run(0.0, 1.5, 0), run(1.5, 2.0, 1), run(2.0, 2.4, 2),
        ], 2)
        self.assertEqual(dominant, 0)
        self.assertEqual(set(totals), {0, 1, 2})
        self.assertAlmostEqual(totals[0], 1.5)
        self.assertAlmostEqual(totals[1], 0.5)
        self.assertAlmostEqual(totals[2], 0.4)

    def test_tie_or_nonmajority_abstains(self):
        _, tied = MODULE.strict_duration_majority([
            run(0.0, 1.0, 0), run(1.0, 2.0, 1)], 2)
        _, no_majority = MODULE.strict_duration_majority([
            run(0.0, 1.0, 0), run(1.0, 1.6, 1), run(1.6, 2.2, 2)], 2)
        self.assertIsNone(tied)
        self.assertIsNone(no_majority)

    def test_requires_configured_number_of_distinct_channels(self):
        _, dominant = MODULE.strict_duration_majority([
            run(0.0, 2.0, 0)], 2)
        self.assertIsNone(dominant)

    def test_phrase_must_be_wholly_contained_in_vad(self):
        metadata = {"phrases": [
            {"evidence_id": "p0", "start": 1.0, "end": 2.0},
            {"evidence_id": "p1", "start": 1.5, "end": 2.5},
        ]}
        self.assertEqual(
            MODULE.contained_phrase_ids(metadata, 0.9, 2.1), ["p0"])

    def test_zero_duration_alignment_clause_has_no_acoustic_query(self):
        clauses = [
            {"projection_start": 1.0, "projection_end": 1.0},
            {"projection_start": 1.0, "projection_end": 1.2},
        ]
        self.assertEqual(
            MODULE.positive_duration_clauses(clauses), [clauses[1]])

    def test_dual_different_phrase_top_rank_vetoes(self):
        piece = {
            "mapped_dominant_identity": "spk_1",
            "contained_phrase_evidence_ids": ["p0"],
        }
        values = {"p0": evidence("spk_2", "spk_1")}
        veto, audit = MODULE.different_phrase_veto(
            piece, values, values, ["spk_1", "spk_2", "spk_3"])
        self.assertTrue(veto)
        self.assertTrue(audit[0]["veto"])

    def test_phrase_top_rank_disagreement_does_not_veto(self):
        piece = {
            "mapped_dominant_identity": "spk_1",
            "contained_phrase_evidence_ids": ["p0"],
        }
        session = {"p0": evidence("spk_2", "spk_1")}
        robust = {"p0": evidence("spk_3", "spk_2")}
        veto, _ = MODULE.different_phrase_veto(
            piece, session, robust, ["spk_1", "spk_2", "spk_3"])
        self.assertFalse(veto)

    def test_expected_phrase_top_rank_does_not_veto(self):
        piece = {
            "mapped_dominant_identity": "spk_1",
            "contained_phrase_evidence_ids": ["p0"],
        }
        values = {"p0": evidence("spk_1", "spk_2")}
        veto, _ = MODULE.different_phrase_veto(
            piece, values, values, ["spk_1", "spk_2", "spk_3"])
        self.assertFalse(veto)

    def test_written_clause_requires_both_galleries_and_vad_identity(self):
        piece = {
            "mapped_dominant_identity": "spk_1",
            "projection_clauses": [{
                "clause_evidence_id": "c0",
                "conflicting_fragments": [{
                    "fragment_evidence_id": "f0",
                    "baseline_speaker_id": "spk_2",
                }],
            }],
        }
        values = {
            "c0": evidence("spk_1", "spk_2"),
            "f0": evidence("spk_1", "spk_2"),
        }
        selected, audit = MODULE.selected_clauses(
            piece, values, values, ["spk_1", "spk_2", "spk_3"],
            self.policy)
        self.assertEqual(len(selected), 1)
        self.assertTrue(audit[0]["accepted"])

    def test_clause_gallery_disagreement_abstains(self):
        piece = {
            "mapped_dominant_identity": "spk_1",
            "projection_clauses": [{
                "clause_evidence_id": "c0",
                "conflicting_fragments": [{
                    "fragment_evidence_id": "f0",
                    "baseline_speaker_id": "spk_2",
                }],
            }],
        }
        session = {
            "c0": evidence("spk_1", "spk_2"),
            "f0": evidence("spk_1", "spk_2"),
        }
        robust = {
            "c0": evidence("spk_2", "spk_1"),
            "f0": evidence("spk_1", "spk_2"),
        }
        selected, audit = MODULE.selected_clauses(
            piece, session, robust, ["spk_1", "spk_2", "spk_3"],
            self.policy)
        self.assertEqual(selected, [])
        self.assertFalse(audit[0]["accepted"])

    def test_conflicting_fragment_must_match_clause_identity(self):
        piece = {
            "mapped_dominant_identity": "spk_1",
            "projection_clauses": [{
                "clause_evidence_id": "c0",
                "conflicting_fragments": [{
                    "fragment_evidence_id": "f0",
                    "baseline_speaker_id": "spk_2",
                }],
            }],
        }
        values = {
            "c0": evidence("spk_1", "spk_2"),
            "f0": evidence("spk_2", "spk_1"),
        }
        selected, audit = MODULE.selected_clauses(
            piece, values, values, ["spk_1", "spk_2", "spk_3"],
            self.policy)
        self.assertEqual(selected, [])
        self.assertFalse(audit[0]["fragment_audit"][0]["accepted"])

    def test_policy_reuses_production_contracts(self):
        self.assertEqual(self.policy["frame_activity_threshold"], 0.5)
        self.assertEqual(self.policy["minimum_stable_run_sec"], 0.4)
        self.assertEqual(self.policy["maximum_query_sec"], 10.0)
        self.assertEqual(self.policy["minimum_distinct_local_channels"], 2)
        self.assertEqual(self.policy["voiceprint"]["regular_min_score"], 0.55)
        self.assertEqual(self.policy["voiceprint"]["regular_min_margin"], 0.04)


if __name__ == "__main__":
    unittest.main()
