#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import unittest


REPO = pathlib.Path(__file__).resolve().parents[3]
TOOLS = REPO / "tools" / "verify" / "py"
sys.path.insert(0, str(TOOLS))
MODULE_PATH = TOOLS / "speaker_edge_anchor_trimmed_phrase_candidate.py"
SPEC = importlib.util.spec_from_file_location(
    "speaker_edge_anchor_trimmed_phrase_candidate", MODULE_PATH)
challenge = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = challenge
SPEC.loader.exec_module(challenge)


class SpeakerEdgeAnchorTrimmedPhraseCandidateTest(unittest.TestCase):
    def fixture(self, *, gap=0.2, protected=False):
        anchor_reason = "baseline_confirmed_regular_direct_voiceprint"
        remainder_reason = "reviewed_repair" if protected else "baseline"
        baseline = {
            "kind": "orator_frozen_speaker_candidate", "audio_sec": 2.0,
            "sample_rate": 16000,
            "active_speaker_ids": ["spk_1", "spk_2"],
            "track": [
                {"text_id": 0, "text": "甲", "start": 0.0, "end": 0.2,
                 "speaker": 0, "speaker_id": "spk_1",
                 "speaker_uncertain": False,
                 "decision_reason": anchor_reason},
                {"text_id": 0, "text": "乙丙。", "start": 0.2 + gap,
                 "end": 1.0, "speaker": 0, "speaker_id": "spk_1",
                 "speaker_uncertain": False,
                 "decision_reason": remainder_reason},
            ],
        }
        metadata = {
            "kind": "orator_punctuation_phrase_spans",
            "asr": {"0": {"text": "甲乙丙。"}},
            "align": {"0": [
                {"text": "甲", "start": 0.0, "end": 0.2},
                {"text": "乙", "start": 0.2 + gap, "end": 0.5 + gap},
                {"text": "丙", "start": 0.5 + gap, "end": 0.8 + gap},
            ]},
            "phrases": [{"evidence_id": "phrase:0", "text_id": 0,
                         "source_start": 0, "source_end": 4,
                         "start": 0.0, "end": 0.8 + gap}],
        }

        def evidence(target="spk_2"):
            scores = {"spk_1": 0.2, "spk_2": 0.6}
            if target == "spk_1":
                scores = {"spk_1": 0.6, "spk_2": 0.2}
            return {"status": "ok", "duration_sec": 1.0,
                    "scores": scores}

        session = {"phrase:0": evidence()}
        robust = {"phrase:0": evidence()}
        mapping = {"mapping": {"0": "spk_1", "1": "spk_2"}}
        policy = {
            "short_max_sec": 1.5, "short_min_score": 0.0,
            "short_min_margin": 0.04, "regular_min_score": 0.55,
            "regular_min_margin": 0.04,
            "boundary_separation_frames": 1,
            "protected_decision_reasons": ["reviewed_repair"],
        }
        return [baseline, metadata, session, robust, mapping, 0.08, policy]

    def test_trims_competing_prefix_and_preserves_source(self):
        result = challenge.build_candidate(*self.fixture())
        self.assertEqual(result["accepted_challenge_count"], 1)
        self.assertEqual("".join(item["text"] for item in result["track"]),
                         "甲乙丙。")
        self.assertEqual(result["track"][0]["speaker_id"], "spk_1")
        self.assertTrue(all(item["speaker_id"] == "spk_2"
                            for item in result["track"][1:]))
        accepted = next(item for item in result["challenge_decisions"]
                        if item["accepted"])
        self.assertEqual((accepted["source_start"], accepted["source_end"]),
                         (1, 4))

    def test_abstains_without_one_frame_separation(self):
        result = challenge.build_candidate(*self.fixture(gap=0.04))
        self.assertEqual(result["accepted_challenge_count"], 0)

    def test_abstains_when_registries_disagree(self):
        values = self.fixture()
        values[3]["phrase:0"]["scores"] = {"spk_1": 0.6, "spk_2": 0.2}
        result = challenge.build_candidate(*values)
        self.assertEqual(result["accepted_challenge_count"], 0)

    def test_abstains_with_anchors_on_both_edges(self):
        values = self.fixture()
        values[0]["track"] = [
            values[0]["track"][0],
            {**values[0]["track"][1], "text": "乙丙", "end": 0.8},
            {**values[0]["track"][0], "text": "。", "start": 0.8,
             "end": 1.0},
        ]
        result = challenge.build_candidate(*values)
        self.assertEqual(result["accepted_challenge_count"], 0)

    def test_preserves_protected_remainder(self):
        result = challenge.build_candidate(*self.fixture(protected=True))
        self.assertEqual(result["accepted_challenge_count"], 0)


if __name__ == "__main__":
    unittest.main()
