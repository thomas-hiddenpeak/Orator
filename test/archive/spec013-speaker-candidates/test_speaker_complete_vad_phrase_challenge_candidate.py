#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import unittest


REPO = pathlib.Path(__file__).resolve().parents[3]
TOOLS = REPO / "tools" / "verify" / "py"
sys.path.insert(0, str(TOOLS))
MODULE_PATH = TOOLS / "speaker_complete_vad_phrase_challenge_candidate.py"
SPEC = importlib.util.spec_from_file_location(
    "speaker_complete_vad_phrase_challenge_candidate", MODULE_PATH)
challenge = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = challenge
SPEC.loader.exec_module(challenge)


class SpeakerCompleteVadPhraseChallengeCandidateTest(unittest.TestCase):
    def fixture(self, *, mapped="spk_1", phrase_robust="spk_2",
                phrase_start=0.0, reason="baseline"):
        baseline = {
            "kind": "orator_frozen_speaker_candidate", "audio_sec": 2.0,
            "sample_rate": 16000,
            "active_speaker_ids": ["spk_1", "spk_2"],
            "track": [{
                "text_id": 0, "text": "甲乙。", "start": 0.0, "end": 1.0,
                "speaker": 0, "speaker_id": "spk_1",
                "speaker_uncertain": False, "decision_reason": reason,
            }],
        }
        vad_metadata = {
            "kind": "orator_vad_utterance_spans", "frame_period_sec": 0.08,
            "pieces": [{
                "evidence_id": "vad:0", "start": 0.04, "end": 1.04,
                "local_speaker": 0, "text_id": 0,
                "source_start": 1, "source_end": 2,
            }],
        }
        phrase_metadata = {
            "kind": "orator_punctuation_phrase_spans",
            "asr": {"0": {"text": "甲乙。"}},
            "align": {"0": [
                {"text": "甲", "start": 0.0, "end": 0.4},
                {"text": "乙", "start": 0.4, "end": 0.8},
            ]},
            "phrases": [{
                "evidence_id": "phrase:0", "text_id": 0,
                "source_start": 0, "source_end": 3,
                "start": phrase_start, "end": 0.8,
            }],
        }

        def evidence(identity, top_score=0.7):
            other = "spk_1" if identity == "spk_2" else "spk_2"
            return {"status": "ok", "duration_sec": 1.0,
                    "scores": {identity: top_score, other: 0.3}}

        vad_session = {"vad:0": evidence("spk_2")}
        vad_robust = {"vad:0": evidence("spk_2")}
        phrase_session = {"phrase:0": evidence("spk_2")}
        phrase_robust_values = {"phrase:0": evidence(phrase_robust)}
        other_mapping = "spk_1" if mapped == "spk_2" else "spk_2"
        mapping = {"mapping": {"0": mapped, "1": other_mapping}}
        policy = {
            "short_max_sec": 1.5, "short_min_score": 0.0,
            "short_min_margin": 0.04, "regular_min_score": 0.55,
            "regular_min_margin": 0.04, "boundary_tolerance_frames": 1,
            "protected_decision_reasons": ["reviewed_repair"],
        }
        return (baseline, vad_metadata, phrase_metadata, vad_session,
                vad_robust, phrase_session, phrase_robust_values,
                mapping, policy)

    def test_accepts_complete_four_view_identity_challenge(self):
        result = challenge.build_candidate(*self.fixture())
        self.assertEqual(result["accepted_challenge_count"], 1)
        self.assertEqual("".join(item["text"] for item in result["track"]),
                         "甲乙。")
        self.assertTrue(all(item["speaker_id"] == "spk_2"
                            for item in result["track"]))

    def test_abstains_when_raw_local_mapping_agrees(self):
        result = challenge.build_candidate(*self.fixture(mapped="spk_2"))
        self.assertEqual(result["accepted_challenge_count"], 0)

    def test_abstains_on_four_view_disagreement(self):
        result = challenge.build_candidate(*self.fixture(
            phrase_robust="spk_1"))
        self.assertEqual(result["accepted_challenge_count"], 0)

    def test_abstains_when_vad_does_not_meet_regular_score_floor(self):
        values = list(self.fixture())
        values[3]["vad:0"]["scores"] = {"spk_2": 0.5, "spk_1": 0.3}
        result = challenge.build_candidate(*values)
        self.assertEqual(result["accepted_challenge_count"], 0)

    def test_rejects_phrase_beyond_one_frame_tolerance(self):
        result = challenge.build_candidate(*self.fixture(phrase_start=-0.05))
        self.assertEqual(result["accepted_challenge_count"], 0)

    def test_preserves_protected_overlay(self):
        result = challenge.build_candidate(*self.fixture(
            reason="reviewed_repair"))
        self.assertEqual(result["accepted_challenge_count"], 0)


if __name__ == "__main__":
    unittest.main()
