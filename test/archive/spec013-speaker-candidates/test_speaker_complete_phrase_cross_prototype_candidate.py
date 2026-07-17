#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import unittest


REPO = pathlib.Path(__file__).resolve().parents[3]
TOOLS = REPO / "tools" / "verify" / "py"
sys.path.insert(0, str(TOOLS))
MODULE_PATH = TOOLS / "speaker_complete_phrase_cross_prototype_candidate.py"
SPEC = importlib.util.spec_from_file_location(
    "speaker_complete_phrase_cross_prototype_candidate", MODULE_PATH)
candidate = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = candidate
SPEC.loader.exec_module(candidate)


class SpeakerCompletePhraseCrossPrototypeCandidateTest(unittest.TestCase):
    def fixture(self, reason="baseline", challenge_end=2):
        baseline = {
            "kind": "orator_frozen_speaker_candidate",
            "audio_sec": 4.0,
            "sample_rate": 16000,
            "active_speaker_ids": ["spk_1", "spk_2"],
            "track": [{
                "text_id": 0, "text": "甲，乙。", "start": 0.0, "end": 4.0,
                "speaker": 1, "speaker_id": "spk_2",
                "speaker_uncertain": False, "decision_reason": reason,
            }],
        }
        challenger = {
            "candidate_kind": "v21_prototype_local_veto",
            "challenge_decisions": [{
                "accepted": True,
                "reason": "cross_prototype_margin_veto",
                "text_id": 0,
                "source_start": 0,
                "source_end": challenge_end,
                "initial_speaker_id": "spk_1",
                "source_speaker_id": "spk_1",
                "mapped_speaker_id": "spk_1",
                "terminal_speaker_id": "spk_2",
                "initial_evidence_id": "initial:0",
                "local_evidence_id": "local:0",
                "phrase_evidence_id": "phrase:0",
            }],
        }
        metadata = {
            "kind": "orator_punctuation_phrase_spans",
            "asr": {"0": {"text": "甲，乙。"}},
            "align": {"0": [
                {"text": "甲", "start": 0.0, "end": 1.0},
                {"text": "乙", "start": 2.0, "end": 3.0},
            ]},
            "phrases": [{
                "text_id": 0, "source_start": 0, "source_end": 2,
            }, {
                "text_id": 0, "source_start": 2, "source_end": 4,
            }],
        }
        mapping = {"mapping": {"0": "spk_1", "1": "spk_2"}}
        policy = {
            "required_candidate_kind": "v21_prototype_local_veto",
            "required_decision_reason": "cross_prototype_margin_veto",
            "protected_decision_reasons": ["reviewed_repair"],
        }
        return baseline, challenger, metadata, mapping, policy

    def test_applies_only_exact_complete_phrase(self):
        result = candidate.build_candidate(*self.fixture())
        self.assertEqual(result["overlay_count"], 1)
        self.assertEqual(
            [(item["text"], item["speaker_id"], item["decision_reason"])
             for item in result["track"]],
            [("甲，", "spk_1", candidate.DECISION_REASON),
             ("乙。", "spk_2", "baseline")])

    def test_rejects_partial_phrase(self):
        result = candidate.build_candidate(*self.fixture(challenge_end=1))
        self.assertEqual(result["overlay_count"], 0)

    def test_preserves_protected_overlay(self):
        result = candidate.build_candidate(*self.fixture(reason="reviewed_repair"))
        self.assertEqual(result["overlay_count"], 0)

    def test_rejects_identity_parity_failure(self):
        values = list(self.fixture())
        values[1]["challenge_decisions"][0]["mapped_speaker_id"] = "spk_2"
        with self.assertRaises(ValueError):
            candidate.build_candidate(*values)


if __name__ == "__main__":
    unittest.main()
