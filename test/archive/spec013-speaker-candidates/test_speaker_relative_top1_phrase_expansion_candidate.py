#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import unittest


REPO = pathlib.Path(__file__).resolve().parents[3]
TOOLS = REPO / "tools" / "verify" / "py"
sys.path.insert(0, str(TOOLS))
MODULE_PATH = TOOLS / "speaker_relative_top1_phrase_expansion_candidate.py"
SPEC = importlib.util.spec_from_file_location(
    "speaker_relative_top1_phrase_expansion_candidate", MODULE_PATH)
expansion = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = expansion
SPEC.loader.exec_module(expansion)


class SpeakerRelativeTop1PhraseExpansionCandidateTest(unittest.TestCase):
    def fixture(self, robust_scores=None, reason="baseline"):
        baseline = {
            "kind": "orator_frozen_speaker_candidate",
            "audio_sec": 2.0,
            "sample_rate": 16000,
            "active_speaker_ids": ["spk_1", "spk_2"],
            "track": [
                {"text_id": 0, "text": "甲", "start": 0.0, "end": 0.5,
                 "speaker": 0, "speaker_id": "spk_1",
                 "speaker_uncertain": False, "decision_reason": reason},
                {"text_id": 0, "text": "乙丙。", "start": 0.5, "end": 2.0,
                 "speaker": 1, "speaker_id": "spk_2",
                 "speaker_uncertain": False, "decision_reason": "baseline"},
            ],
        }
        relative = {
            "candidate_kind": "v21_relative_top1_dual_voiceprint_consensus",
            "piece_decisions": [{
                "accepted": True,
                "reason": "relative_top1_dual_voiceprint_consensus",
                "evidence_id": "relative:0",
                "phrase_evidence_id": "phrase:0",
                "text_id": 0, "source_start": 1, "source_end": 2,
                "selected_speaker_id": "spk_1", "local_speaker": 0,
            }],
        }
        metadata = {
            "kind": "orator_punctuation_phrase_spans",
            "asr": {"0": {"text": "甲乙丙。"}},
            "align": {"0": [
                {"text": "甲", "start": 0.0, "end": 0.5},
                {"text": "乙", "start": 0.5, "end": 1.0},
                {"text": "丙", "start": 1.0, "end": 1.5},
            ]},
            "phrases": [{
                "evidence_id": "phrase:0", "text_id": 0,
                "source_start": 0, "source_end": 4,
                "start": 0.0, "end": 2.0,
            }],
        }
        session = {"phrase:0": {
            "status": "ok", "duration_sec": 2.0,
            "scores": {"spk_1": 0.4, "spk_2": 0.3},
        }}
        robust = {"phrase:0": {
            "status": "ok", "duration_sec": 2.0,
            "scores": robust_scores or {"spk_1": 0.35, "spk_2": 0.2},
        }}
        mapping = {"mapping": {"0": "spk_1", "1": "spk_2"}}
        policy = {
            "required_candidate_kind":
                "v21_relative_top1_dual_voiceprint_consensus",
            "required_piece_reason": "relative_top1_dual_voiceprint_consensus",
            "phrase_margin": 0.04,
            "protected_decision_reasons": ["reviewed_repair"],
        }
        return baseline, relative, metadata, session, robust, mapping, policy

    def test_expands_accepted_piece_to_complete_phrase(self):
        result = expansion.build_candidate(*self.fixture())
        self.assertEqual(result["accepted_expansion_count"], 1)
        self.assertEqual("".join(item["text"] for item in result["track"]),
                         "甲乙丙。")
        self.assertTrue(all(item["speaker_id"] == "spk_1"
                            for item in result["track"]))
        self.assertTrue(all(item["decision_reason"] == expansion.DECISION_REASON
                            for item in result["track"]))

    def test_abstains_on_phrase_registry_disagreement(self):
        result = expansion.build_candidate(*self.fixture(
            robust_scores={"spk_1": 0.2, "spk_2": 0.4}))
        self.assertEqual(result["accepted_expansion_count"], 0)

    def test_preserves_reviewed_overlay(self):
        result = expansion.build_candidate(*self.fixture(reason="reviewed_repair"))
        self.assertEqual(result["accepted_expansion_count"], 0)

    def test_rejects_piece_outside_phrase(self):
        values = list(self.fixture())
        values[1]["piece_decisions"][0]["source_end"] = 5
        with self.assertRaises(ValueError):
            expansion.build_candidate(*values)


if __name__ == "__main__":
    unittest.main()
