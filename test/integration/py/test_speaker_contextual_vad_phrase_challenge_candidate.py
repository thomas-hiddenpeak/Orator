#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import unittest


REPO = pathlib.Path(__file__).resolve().parents[3]
TOOLS = REPO / "tools" / "verify" / "py"
sys.path.insert(0, str(TOOLS))
MODULE_PATH = TOOLS / "speaker_contextual_vad_phrase_challenge_candidate.py"
SPEC = importlib.util.spec_from_file_location(
    "speaker_contextual_vad_phrase_challenge_candidate", MODULE_PATH)
challenge = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = challenge
SPEC.loader.exec_module(challenge)


class SpeakerContextualVadPhraseChallengeCandidateTest(unittest.TestCase):
    def fixture(self, *, outer_score=0.6, primary_id="spk_1",
                reason="baseline"):
        baseline = {
            "kind": "orator_frozen_speaker_candidate", "audio_sec": 2.0,
            "sample_rate": 16000,
            "active_speaker_ids": ["spk_1", "spk_2"],
            "track": [{"text_id": 0, "text": "甲乙。", "start": 0.0,
                       "end": 0.8, "speaker": 0, "speaker_id": "spk_1",
                       "speaker_uncertain": False,
                       "decision_reason": reason}],
        }
        metadata = {
            "kind": "orator_punctuation_phrase_spans",
            "asr": {"0": {"text": "甲乙。"}},
            "align": {"0": [{"text": "甲", "start": 0.0, "end": 0.4},
                              {"text": "乙", "start": 0.4, "end": 0.8}]},
            "phrases": [{"evidence_id": "phrase:0", "text_id": 0,
                         "source_start": 0, "source_end": 3,
                         "start": 0.0, "end": 0.8}],
        }
        vad = [{"evidence_id": "vad:0", "start": 0.0, "end": 1.2}]
        primary = [{"start": 0.0, "end": 1.0, "local": 0,
                    "speaker_id": primary_id}]

        def evidence(score):
            return {"status": "ok", "duration_sec": 1.0,
                    "scores": {"spk_2": score, "spk_1": score - 0.2}}

        vad_session = {"vad:0": evidence(outer_score)}
        vad_robust = {"vad:0": evidence(0.5)}
        phrase_session = {"phrase:0": evidence(0.45)}
        phrase_robust = {"phrase:0": evidence(0.4)}
        mapping = {"mapping": {"0": "spk_1", "1": "spk_2"}}
        policy = {
            "short_max_sec": 1.5, "short_min_score": 0.0,
            "short_min_margin": 0.04, "regular_min_score": 0.55,
            "regular_min_margin": 0.04, "view_margin": 0.04,
            "outer_regular_score": 0.55, "boundary_tolerance_frames": 1,
            "protected_decision_reasons": ["reviewed_repair"],
        }
        return (baseline, metadata, vad, primary, 0.08, vad_session,
                vad_robust, phrase_session, phrase_robust, mapping, policy)

    def test_accepts_contextual_exact_phrase_challenge(self):
        result = challenge.build_candidate(*self.fixture())
        self.assertEqual(result["accepted_challenge_count"], 1)
        self.assertTrue(all(item["speaker_id"] == "spk_2"
                            for item in result["track"]))
        self.assertEqual("".join(item["text"] for item in result["track"]),
                         "甲乙。")

    def test_abstains_without_outer_regular_score(self):
        result = challenge.build_candidate(*self.fixture(outer_score=0.5))
        self.assertEqual(result["accepted_challenge_count"], 0)

    def test_abstains_when_primary_mapping_agrees(self):
        values = list(self.fixture())
        values[3][0]["local"] = 1
        values[3][0]["speaker_id"] = "spk_2"
        result = challenge.build_candidate(*values)
        self.assertEqual(result["accepted_challenge_count"], 0)

    def test_abstains_with_multiple_containing_vad_intervals(self):
        values = list(self.fixture())
        values[2].append({"evidence_id": "vad:1", "start": 0.0, "end": 1.0})
        result = challenge.build_candidate(*values)
        self.assertEqual(result["accepted_challenge_count"], 0)

    def test_preserves_protected_overlay(self):
        result = challenge.build_candidate(*self.fixture(
            reason="reviewed_repair"))
        self.assertEqual(result["accepted_challenge_count"], 0)


if __name__ == "__main__":
    unittest.main()
