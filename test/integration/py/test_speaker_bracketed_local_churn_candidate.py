#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[3]
TOOLS = ROOT / "tools" / "verify" / "py"
sys.path.insert(0, str(TOOLS))
MODULE_PATH = TOOLS / "speaker_bracketed_local_churn_candidate.py"
SPEC = importlib.util.spec_from_file_location("bracketed_churn", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def evidence(best, second, duration=2.0):
    scores = {"spk_1": 0.10, "spk_2": 0.10, "spk_3": 0.10}
    scores[second] = 0.20
    scores[best] = 0.70
    return {
        "status": "ok",
        "duration_sec": duration,
        "scores": scores,
    }


class BracketedLocalChurnCandidateTest(unittest.TestCase):
    def setUp(self):
        self.policy = MODULE.load_policy(
            ROOT / "specs" / "013-industrial-closing-validation" /
            "speaker-v21-bracketed-local-churn.toml")
        self.source = "甲句。乙句。丙句。"
        self.metadata = {
            "kind": "orator_punctuation_phrase_spans",
            "asr": {"0": {"start": 0.0, "end": 2.0,
                            "text": self.source}},
            "align": {"0": [
                {"start": 0.10, "end": 0.20, "text": "甲"},
                {"start": 0.20, "end": 0.30, "text": "句"},
                {"start": 0.60, "end": 0.70, "text": "乙"},
                {"start": 0.70, "end": 0.80, "text": "句"},
                {"start": 1.10, "end": 1.20, "text": "丙"},
                {"start": 1.20, "end": 1.30, "text": "句"},
            ]},
            "phrases": [
                {"evidence_id": "punctuation_phrase:0", "text_id": 0,
                 "source_start": 0, "source_end": 3,
                 "start": 0.10, "end": 0.30, "text": "甲句。"},
                {"evidence_id": "punctuation_phrase:1", "text_id": 0,
                 "source_start": 3, "source_end": 6,
                 "start": 0.60, "end": 0.80, "text": "乙句。"},
                {"evidence_id": "punctuation_phrase:2", "text_id": 0,
                 "source_start": 6, "source_end": 9,
                 "start": 1.10, "end": 1.30, "text": "丙句。"},
            ],
        }
        self.baseline = {
            "kind": "orator_frozen_speaker_candidate",
            "audio_sec": 2.0,
            "sample_rate": 16000,
            "active_speaker_ids": ["spk_1", "spk_2", "spk_3"],
            "track": [{
                "turn_id": "base:0", "start": 0.1, "end": 1.3,
                "text_id": 0, "text": self.source, "speaker": 1,
                "speaker_id": "spk_2", "speaker_uncertain": False,
                "decision_reason": "baseline",
            }],
        }
        self.mapping = {0: "spk_1", 1: "spk_2", 2: "spk_3"}

    def frames(self):
        values = []
        for index in range(20):
            if index < 5 or index >= 15:
                local = 0
            elif index < 10:
                local = 1
            else:
                local = 2
            values.append({
                "frame": index, "time": 0.05 + 0.1 * index,
                "local": local, "probability": 0.8,
                "margin": 0.6 if local == 0 else 0.2,
                "active_count": 1,
            })
        return values

    def test_enumerates_complete_phrases_inside_multichannel_churn(self):
        pieces, runs = MODULE.enumerate_pieces(
            self.metadata, self.frames(), 0.1, self.baseline,
            self.mapping, self.policy)
        self.assertEqual(len(runs), 4)
        self.assertEqual(len(pieces), 1)
        self.assertEqual(pieces[0]["mapped_outer_identity"], "spk_1")
        self.assertEqual(pieces[0]["inner_local_speakers"], [1, 2])
        self.assertTrue(pieces[0]["inner_mean_margins_below_outer"])
        self.assertTrue(pieces[0]["inner_runs_single_active_channel"])
        self.assertEqual(
            [item["text"] for item in pieces[0]["projection_phrases"]],
            ["乙句。", "丙句。"])

    def test_incomplete_alignment_rejects_only_incomplete_phrase(self):
        self.metadata["align"]["0"] = self.metadata["align"]["0"][:-1]
        pieces, _ = MODULE.enumerate_pieces(
            self.metadata, self.frames(), 0.1, self.baseline,
            self.mapping, self.policy)
        self.assertEqual(
            [item["text"] for item in pieces[0]["projection_phrases"]],
            ["乙句。"])

    def test_regular_query_gate_does_not_use_short_score_policy(self):
        weak = {
            "status": "ok", "duration_sec": 0.8,
            "scores": {"spk_1": 0.40, "spk_2": 0.10},
        }
        selected, reason, _ = MODULE.select_regular(
            weak, ["spk_1", "spk_2"], self.policy["voiceprint"])
        self.assertIsNone(selected)
        self.assertEqual(reason, "regular_score_below_gate")

    def test_single_inner_channel_dual_phrase_support_vetoes(self):
        piece = {
            "inner_local_speakers": [1],
            "projection_phrases": [{
                "phrase_evidence_id": "punctuation_phrase:1"}],
        }
        phrase = {"punctuation_phrase:1": evidence("spk_2", "spk_1", 0.8)}
        veto, audit = MODULE.has_single_channel_phrase_veto(
            piece, phrase, phrase, self.baseline["active_speaker_ids"],
            self.mapping, self.policy["voiceprint"])
        self.assertTrue(veto)
        self.assertTrue(audit[0]["veto"])

    def test_multichannel_churn_does_not_use_single_channel_veto(self):
        piece = {
            "inner_local_speakers": [1, 2],
            "projection_phrases": [{
                "phrase_evidence_id": "punctuation_phrase:1"}],
        }
        veto, audit = MODULE.has_single_channel_phrase_veto(
            piece, {}, {}, self.baseline["active_speaker_ids"],
            self.mapping, self.policy["voiceprint"])
        self.assertFalse(veto)
        self.assertEqual(audit, [])

    def test_phrase_guard_requires_uniform_conflict_and_dual_outer_rank(self):
        piece = {"projection_phrases": [{
            "phrase_evidence_id": "punctuation_phrase:1",
            "baseline_identity_ids": ["spk_2"],
        }]}
        phrase = {"punctuation_phrase:1": evidence("spk_1", "spk_2")}
        selected, audit = MODULE.guarded_projection_phrases(
            piece, phrase, phrase, self.baseline["active_speaker_ids"],
            "spk_1", self.policy["voiceprint"])
        self.assertEqual(len(selected), 1)
        self.assertTrue(audit[0]["dual_outer_top_rank"])
        piece["projection_phrases"][0]["baseline_identity_ids"] = [
            "spk_2", "spk_3"]
        selected, _ = MODULE.guarded_projection_phrases(
            piece, phrase, phrase, self.baseline["active_speaker_ids"],
            "spk_1", self.policy["voiceprint"])
        self.assertEqual(selected, [])

    def test_phrase_guard_rejects_one_gallery_top_rank_disagreement(self):
        piece = {"projection_phrases": [{
            "phrase_evidence_id": "punctuation_phrase:1",
            "baseline_identity_ids": ["spk_2"],
        }]}
        current = {"punctuation_phrase:1": evidence("spk_1", "spk_2")}
        robust = {"punctuation_phrase:1": evidence("spk_3", "spk_1")}
        selected, audit = MODULE.guarded_projection_phrases(
            piece, current, robust, self.baseline["active_speaker_ids"],
            "spk_1", self.policy["voiceprint"])
        self.assertEqual(selected, [])
        self.assertFalse(audit[0]["dual_outer_top_rank"])

    def test_same_different_phrase_top_vetoes_complete_clause_expansion(self):
        audit = [{
            "session_top_ranked_identity": "spk_2",
            "robust_top_ranked_identity": "spk_2",
        }]
        self.assertTrue(MODULE.has_dual_agreed_different_phrase_top(
            audit, "spk_1"))
        audit[0]["robust_top_ranked_identity"] = "spk_3"
        self.assertFalse(MODULE.has_dual_agreed_different_phrase_top(
            audit, "spk_1"))

    def test_overlap_marks_inner_run_ineligible_for_expansion(self):
        frames = self.frames()
        for frame in frames[5:10]:
            frame["active_count"] = 2
        pieces, _ = MODULE.enumerate_pieces(
            self.metadata, frames, 0.1, self.baseline,
            self.mapping, self.policy)
        self.assertFalse(pieces[0]["inner_runs_single_active_channel"])

    def test_conflicting_source_ranges_overlap(self):
        left = {"text_id": 0, "projection_phrases": [
            {"source_start": 3, "source_end": 6}]}
        right = {"text_id": 0, "projection_phrases": [
            {"source_start": 5, "source_end": 9}]}
        self.assertTrue(MODULE.intervals_overlap(left, right))
        right["text_id"] = 1
        self.assertFalse(MODULE.intervals_overlap(left, right))

    def test_policy_reuses_production_contracts(self):
        self.assertEqual(self.policy["frame_activity_threshold"], 0.5)
        self.assertEqual(self.policy["minimum_outer_run_sec"], 0.4)
        self.assertEqual(self.policy["maximum_query_sec"], 10.0)
        self.assertEqual(self.policy["voiceprint"]["regular_min_score"], 0.55)
        self.assertEqual(self.policy["voiceprint"]["regular_min_margin"], 0.04)


if __name__ == "__main__":
    unittest.main()
