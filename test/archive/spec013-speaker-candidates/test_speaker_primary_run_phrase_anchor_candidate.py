#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import unittest


REPO = pathlib.Path(__file__).resolve().parents[3]
TOOLS = REPO / "tools" / "verify" / "py"
sys.path.insert(0, str(TOOLS))
MODULE_PATH = TOOLS / "speaker_primary_run_phrase_anchor_candidate.py"
SPEC = importlib.util.spec_from_file_location(
    "speaker_primary_run_phrase_anchor_candidate", MODULE_PATH)
candidate = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = candidate
SPEC.loader.exec_module(candidate)


class SpeakerPrimaryRunPhraseAnchorCandidateTest(unittest.TestCase):
    def fixture(self, *, robust_challenge_ok=False, protected=False,
                add_third=False):
        challenge_reason = "reviewed_repair" if protected else "baseline"
        baseline = {
            "kind": "orator_frozen_speaker_candidate", "audio_sec": 2.0,
            "sample_rate": 16000,
            "active_speaker_ids": ["spk_1", "spk_2"],
            "track": [
                {"text_id": 0, "text": "甲，", "start": 0.1, "end": 0.5,
                 "speaker": 0, "speaker_id": "spk_1",
                 "speaker_uncertain": False,
                 "decision_reason": challenge_reason},
                {"text_id": 0, "text": "乙，", "start": 0.6, "end": 1.0,
                 "speaker": 1, "speaker_id": "spk_2",
                 "speaker_uncertain": False, "decision_reason": "baseline"},
            ],
        }
        source = "甲，乙，"
        phrases = [
            {"evidence_id": "phrase:0", "text_id": 0,
             "source_start": 0, "source_end": 2,
             "start": 0.1, "end": 0.5},
            {"evidence_id": "phrase:1", "text_id": 0,
             "source_start": 2, "source_end": 4,
             "start": 0.6, "end": 1.0},
        ]
        if add_third:
            source += "丙。"
            phrases.append({"evidence_id": "phrase:2", "text_id": 0,
                            "source_start": 4, "source_end": 6,
                            "start": 1.1, "end": 1.4})
            baseline["track"].append({
                "text_id": 0, "text": "丙。", "start": 1.1, "end": 1.4,
                "speaker": 1, "speaker_id": "spk_2",
                "speaker_uncertain": False, "decision_reason": "baseline"})
        metadata = {
            "kind": "orator_punctuation_phrase_spans",
            "asr": {"0": {"text": source}},
            "align": {"0": [
                {"text": "甲", "start": 0.1, "end": 0.5},
                {"text": "乙", "start": 0.6, "end": 1.0},
                *([{"text": "丙", "start": 1.1, "end": 1.4}]
                  if add_third else []),
            ]},
            "phrases": phrases,
        }
        primary = [{"start": 0.0, "end": 1.5, "local": 1,
                    "speaker_id": "spk_2"}]

        def ok(target):
            return {"status": "ok", "duration_sec": 0.4,
                    "scores": {target: 0.6,
                               "spk_1" if target == "spk_2" else "spk_2":
                               0.2}}

        session = {"phrase:0": ok("spk_1"), "phrase:1": ok("spk_2")}
        robust = {
            "phrase:0": (ok("spk_1") if robust_challenge_ok else
                          {"status": "insufficient_duration",
                           "duration_sec": 0.4, "scores": {}}),
            "phrase:1": ok("spk_2"),
        }
        if add_third:
            session["phrase:2"] = ok("spk_2")
            robust["phrase:2"] = ok("spk_2")
        mapping = {"mapping": {"0": "spk_1", "1": "spk_2"}}
        policy = {
            "short_max_sec": 1.5, "short_min_score": 0.0,
            "short_min_margin": 0.04, "regular_min_score": 0.55,
            "regular_min_margin": 0.04, "contained_phrase_count": 2,
            "boundary_tolerance_frames": 1,
            "protected_decision_reasons": ["reviewed_repair"],
        }
        return [baseline, metadata, primary, 0.08, session, robust, mapping,
                policy]

    def test_expands_from_dual_confirmed_adjacent_anchor(self):
        result = candidate.build_candidate(*self.fixture())
        self.assertEqual(result["accepted_expansion_count"], 1)
        self.assertEqual("".join(item["text"] for item in result["track"]),
                         "甲，乙，")
        self.assertTrue(all(item["speaker_id"] == "spk_2"
                            for item in result["track"]))

    def test_abstains_for_dual_eligible_competitor(self):
        result = candidate.build_candidate(*self.fixture(
            robust_challenge_ok=True))
        self.assertEqual(result["accepted_expansion_count"], 0)

    def test_abstains_when_run_contains_third_phrase(self):
        result = candidate.build_candidate(*self.fixture(add_third=True))
        self.assertEqual(result["accepted_expansion_count"], 0)

    def test_preserves_protected_challenge(self):
        result = candidate.build_candidate(*self.fixture(protected=True))
        self.assertEqual(result["accepted_expansion_count"], 0)

    def test_abstains_when_anchor_lacks_mapped_identity(self):
        values = self.fixture()
        values[0]["track"][1]["speaker_id"] = "spk_1"
        values[0]["track"][1]["speaker"] = 0
        result = candidate.build_candidate(*values)
        self.assertEqual(result["accepted_expansion_count"], 0)


if __name__ == "__main__":
    unittest.main()
