#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[3]
TOOLS = ROOT / "tools" / "verify" / "py"
sys.path.insert(0, str(TOOLS))
MODULE_PATH = TOOLS / "speaker_complete_source_unanimous_phrase_candidate.py"
SPEC = importlib.util.spec_from_file_location("source_unanimous", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def evidence(best, second):
    scores = {"spk_1": 0.10, "spk_2": 0.10, "spk_3": 0.10}
    scores[second] = 0.20
    scores[best] = 0.70
    return {"status": "ok", "duration_sec": 2.0, "scores": scores}


class CompleteSourceUnanimousPhraseCandidateTest(unittest.TestCase):
    def setUp(self):
        self.policy = MODULE.load_policy(
            ROOT / "specs" / "013-industrial-closing-validation" /
            "speaker-v21-complete-source-unanimous-phrase.toml")

    def test_phrase_requires_one_containing_vad(self):
        vad = [(1.0, 2.0), (2.2, 3.0)]
        self.assertEqual(MODULE.containing_vad_index(1.2, 1.8, vad), 0)
        self.assertIsNone(MODULE.containing_vad_index(1.8, 2.4, vad))

    def test_stable_support_reapplies_duration_floor_inside_source(self):
        runs = [
            {"start": 0.0, "end": 1.0, "local_speaker": 0},
            {"start": 1.0, "end": 2.0, "local_speaker": 1},
        ]
        locals_, records = MODULE.stable_source_locals(
            runs, 0.8, 1.8, 0.4)
        self.assertEqual(locals_, [1])
        self.assertEqual(len(records), 1)

    def test_phrase_unanimity_requires_both_top_ranks(self):
        piece = {"indexed_phrases": [{"evidence_id": "p0"}]}
        values = {"p0": evidence("spk_1", "spk_2")}
        unanimous, audit = MODULE.phrase_unanimity(
            piece, values, values, ["spk_1", "spk_2", "spk_3"], "spk_1")
        self.assertTrue(unanimous)
        self.assertTrue(audit[0]["accepted"])

    def test_phrase_top_rank_disagreement_abstains(self):
        piece = {"indexed_phrases": [{"evidence_id": "p0"}]}
        session = {"p0": evidence("spk_1", "spk_2")}
        robust = {"p0": evidence("spk_2", "spk_1")}
        unanimous, _ = MODULE.phrase_unanimity(
            piece, session, robust, ["spk_1", "spk_2", "spk_3"], "spk_1")
        self.assertFalse(unanimous)

    def test_every_phrase_must_match_source_identity(self):
        piece = {"indexed_phrases": [
            {"evidence_id": "p0"}, {"evidence_id": "p1"},
        ]}
        values = {
            "p0": evidence("spk_1", "spk_2"),
            "p1": evidence("spk_2", "spk_1"),
        }
        unanimous, audit = MODULE.phrase_unanimity(
            piece, values, values, ["spk_1", "spk_2", "spk_3"], "spk_1")
        self.assertFalse(unanimous)
        self.assertFalse(audit[1]["accepted"])

    def test_overlapping_phrase_ranges_conflict_only_in_same_source(self):
        left = {"text_id": 0, "selected_projection_phrases": [
            {"source_start": 0, "source_end": 3}]}
        right = {"text_id": 0, "selected_projection_phrases": [
            {"source_start": 2, "source_end": 5}]}
        self.assertTrue(MODULE.intervals_overlap(left, right))
        right["text_id"] = 1
        self.assertFalse(MODULE.intervals_overlap(left, right))

    def test_phrase_ownership_records_both_baseline_boundaries(self):
        phrase = {"source_start": 0, "source_end": 4}
        fragments = [
            {"source_start": 0, "source_end": 1,
             "entry": {"speaker_id": "spk_1"}},
            {"source_start": 1, "source_end": 3,
             "entry": {"speaker_id": "spk_2"}},
            {"source_start": 3, "source_end": 4,
             "entry": {"speaker_id": "spk_1"}},
        ]
        identities, unknown, first, last = MODULE.phrase_baseline_ownership(
            phrase, fragments)
        self.assertEqual(identities, ["spk_1", "spk_2"])
        self.assertFalse(unknown)
        self.assertEqual(first, "spk_1")
        self.assertEqual(last, "spk_1")

    def test_policy_reuses_production_contracts(self):
        self.assertEqual(self.policy["frame_activity_threshold"], 0.5)
        self.assertEqual(self.policy["minimum_stable_run_sec"], 0.4)
        self.assertEqual(self.policy["maximum_query_sec"], 10.0)
        self.assertEqual(self.policy["voiceprint"]["regular_min_score"], 0.55)
        self.assertEqual(self.policy["voiceprint"]["regular_min_margin"], 0.04)


if __name__ == "__main__":
    unittest.main()
