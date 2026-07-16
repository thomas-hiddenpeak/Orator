#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import unittest


REPO = pathlib.Path(__file__).resolve().parents[3]
TOOLS = REPO / "tools" / "verify" / "py"
sys.path.insert(0, str(TOOLS))
MODULE_PATH = TOOLS / "speaker_adjacent_subminimum_clause_candidate.py"
SPEC = importlib.util.spec_from_file_location(
    "speaker_adjacent_subminimum_clause_candidate", MODULE_PATH)
candidate = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = candidate
SPEC.loader.exec_module(candidate)


class SpeakerAdjacentSubminimumClauseCandidateTest(unittest.TestCase):
    def policy(self):
        return {
            "minimum_duration_sec": 0.5, "maximum_duration_sec": 4.0,
            "minimum_visible_character_count": 1,
            "punctuation": "，。？！；：、,.?!;:",
            "short_max_sec": 1.5, "short_min_score": 0.0,
            "short_min_margin": 0.04, "regular_min_score": 0.55,
            "regular_min_margin": 0.04, "clause_count": 2,
            "protected_decision_reasons": ["reviewed_repair"],
        }

    def alignment(self):
        return {
            "kind": "orator_punctuation_phrase_spans",
            "asr": {"0": {"text": "甲，乙。"}},
            "align": {"0": [
                {"text": "甲", "start": 0.0, "end": 0.2},
                {"text": "乙", "start": 0.6, "end": 0.8},
            ]},
        }

    def metadata(self):
        alignment = self.alignment()
        return {
            "kind": "orator_adjacent_subminimum_clause_spans",
            "asr": alignment["asr"], "align": alignment["align"],
            "envelopes": candidate.enumerate_envelopes(
                alignment, self.policy()),
        }

    def fixture(self, *, protected=False, mixed=False):
        second_id = "spk_2" if mixed else "spk_1"
        second_reason = "reviewed_repair" if protected else "baseline"
        baseline = {
            "kind": "orator_frozen_speaker_candidate", "audio_sec": 1.0,
            "sample_rate": 16000,
            "active_speaker_ids": ["spk_1", "spk_2"],
            "track": [
                {"text_id": 0, "text": "甲，", "start": 0.0, "end": 0.2,
                 "speaker": 0, "speaker_id": "spk_1",
                 "speaker_uncertain": False, "decision_reason": "baseline"},
                {"text_id": 0, "text": "乙。", "start": 0.6, "end": 0.8,
                 "speaker": 1 if mixed else 0, "speaker_id": second_id,
                 "speaker_uncertain": False,
                 "decision_reason": second_reason},
            ],
        }
        evidence = {"status": "ok", "duration_sec": 0.8,
                    "scores": {"spk_2": 0.6, "spk_1": 0.2}}
        session = {"adjacent_subminimum_clause:0": dict(evidence)}
        robust = {"adjacent_subminimum_clause:0": dict(evidence)}
        mapping = {"mapping": {"0": "spk_1", "1": "spk_2"}}
        return [baseline, self.metadata(), session, robust, mapping,
                self.policy()]

    def test_enumerates_exact_adjacent_micro_clause_pair(self):
        values = candidate.enumerate_envelopes(self.alignment(), self.policy())
        self.assertEqual(len(values), 1)
        self.assertEqual((values[0]["source_start"], values[0]["source_end"]),
                         (0, 4))
        self.assertEqual((values[0]["start"], values[0]["end"]), (0.0, 0.8))

    def test_projects_dual_voiceprint_consensus_exactly(self):
        result = candidate.build_candidate(*self.fixture())
        self.assertEqual(result["accepted_challenge_count"], 1)
        self.assertEqual("".join(item["text"] for item in result["track"]),
                         "甲，乙。")
        self.assertTrue(all(item["speaker_id"] == "spk_2"
                            for item in result["track"]))

    def test_abstains_when_voiceprint_views_disagree(self):
        values = self.fixture()
        values[3]["adjacent_subminimum_clause:0"]["scores"] = {
            "spk_1": 0.6, "spk_2": 0.2}
        result = candidate.build_candidate(*values)
        self.assertEqual(result["accepted_challenge_count"], 0)

    def test_abstains_for_mixed_baseline_identity(self):
        result = candidate.build_candidate(*self.fixture(mixed=True))
        self.assertEqual(result["accepted_challenge_count"], 0)

    def test_preserves_protected_overlay(self):
        result = candidate.build_candidate(*self.fixture(protected=True))
        self.assertEqual(result["accepted_challenge_count"], 0)


if __name__ == "__main__":
    unittest.main()
