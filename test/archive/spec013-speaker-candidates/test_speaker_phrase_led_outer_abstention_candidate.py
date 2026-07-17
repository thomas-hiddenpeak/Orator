#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import unittest


REPO = pathlib.Path(__file__).resolve().parents[3]
TOOLS = REPO / "tools" / "verify" / "py"
sys.path.insert(0, str(TOOLS))
MODULE_PATH = TOOLS / "speaker_phrase_led_outer_abstention_candidate.py"
SPEC = importlib.util.spec_from_file_location(
    "speaker_phrase_led_outer_abstention_candidate", MODULE_PATH)
candidate = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = candidate
SPEC.loader.exec_module(candidate)


class SpeakerPhraseLedOuterAbstentionCandidateTest(unittest.TestCase):
    def fixture(self):
        baseline = {
            "kind": "orator_frozen_speaker_candidate",
            "audio_sec": 2.0,
            "sample_rate": 16000,
            "active_speaker_ids": ["spk_1", "spk_2"],
            "track": [{
                "text_id": 0, "text": "甲，", "start": 0.2, "end": 0.9,
                "speaker": 1, "speaker_id": "spk_2",
                "speaker_uncertain": True,
                "decision_reason": "baseline_regular_score_below_gate",
            }],
        }
        metadata = {
            "kind": "orator_punctuation_phrase_spans",
            "asr": {"0": {"text": "甲，"}},
            "align": {"0": [
                {"text": "甲", "start": 0.2, "end": 0.9},
            ]},
            "phrases": [{
                "evidence_id": "phrase:0", "text_id": 0,
                "source_start": 0, "source_end": 2,
                "start": 0.2, "end": 0.9,
            }],
        }
        vad = [{"evidence_id": "vad:0", "start": 0.0, "end": 1.8}]
        primary = [{"start": 0.1, "end": 1.0, "local": 1,
                    "speaker_id": "spk_2"}]

        def evidence(duration, first, first_score, second_score):
            second = "spk_1" if first == "spk_2" else "spk_2"
            return {"status": "ok", "duration_sec": duration,
                    "scores": {first: first_score, second: second_score}}

        vad_session = {"vad:0": evidence(1.8, "spk_2", 0.53, 0.51)}
        vad_robust = {"vad:0": evidence(1.8, "spk_2", 0.54, 0.52)}
        phrase_session = {"phrase:0": evidence(
            0.7, "spk_1", 0.30, 0.20)}
        phrase_robust = {"phrase:0": evidence(
            0.7, "spk_1", 0.28, 0.26)}
        mapping = {"mapping": {"0": "spk_1", "1": "spk_2"}}
        policy = {
            "short_max_sec": 1.5, "short_min_score": 0.0,
            "short_min_margin": 0.04, "regular_min_score": 0.55,
            "regular_min_margin": 0.04,
            "boundary_tolerance_frames": 1,
            "allowed_baseline_reasons": [
                "baseline_regular_score_below_gate"],
            "protected_decision_reasons": ["reviewed_repair"],
        }
        return [baseline, metadata, vad, primary, 0.08, vad_session,
                vad_robust, phrase_session, phrase_robust, mapping, policy]

    def test_projects_exact_phrase_for_required_abstention_topology(self):
        result = candidate.build_candidate(*self.fixture())
        self.assertEqual(result["accepted_challenge_count"], 1)
        self.assertEqual(result["track"][0]["speaker_id"], "spk_1")
        self.assertEqual(result["track"][0]["text"], "甲，")

    def test_abstains_when_outer_vad_is_eligible(self):
        values = self.fixture()
        values[5]["vad:0"]["scores"] = {"spk_2": 0.70, "spk_1": 0.20}
        result = candidate.build_candidate(*values)
        self.assertEqual(result["accepted_challenge_count"], 0)

    def test_abstains_when_phrase_top_ranks_disagree(self):
        values = self.fixture()
        values[8]["phrase:0"]["scores"] = {"spk_1": 0.26, "spk_2": 0.28}
        result = candidate.build_candidate(*values)
        self.assertEqual(result["accepted_challenge_count"], 0)

    def test_abstains_for_non_margin_phrase_abstention(self):
        values = self.fixture()
        values[8]["phrase:0"] = {
            "status": "ok", "duration_sec": 1.6,
            "scores": {"spk_1": 0.50, "spk_2": 0.20},
        }
        result = candidate.build_candidate(*values)
        self.assertEqual(result["accepted_challenge_count"], 0)

    def test_abstains_for_unlisted_baseline_provenance(self):
        values = self.fixture()
        values[0]["track"][0]["decision_reason"] = "reviewed_repair"
        result = candidate.build_candidate(*values)
        self.assertEqual(result["accepted_challenge_count"], 0)

    def test_abstains_when_container_is_not_unique(self):
        values = self.fixture()
        values[2].append({"evidence_id": "vad:1", "start": 0.1,
                          "end": 1.7})
        result = candidate.build_candidate(*values)
        self.assertEqual(result["accepted_challenge_count"], 0)


if __name__ == "__main__":
    unittest.main()
