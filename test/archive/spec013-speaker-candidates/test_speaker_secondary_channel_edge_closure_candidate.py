#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import unittest


REPO = pathlib.Path(__file__).resolve().parents[3]
TOOLS = REPO / "tools" / "verify" / "py"
sys.path.insert(0, str(TOOLS))
MODULE_PATH = TOOLS / "speaker_secondary_channel_edge_closure_candidate.py"
SPEC = importlib.util.spec_from_file_location(
    "speaker_secondary_channel_edge_closure_candidate", MODULE_PATH)
candidate = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = candidate
SPEC.loader.exec_module(candidate)


class SpeakerSecondaryChannelEdgeClosureCandidateTest(unittest.TestCase):
    def fixture(self):
        baseline = {
            "kind": "orator_frozen_speaker_candidate",
            "audio_sec": 1.0, "sample_rate": 16000,
            "active_speaker_ids": ["spk_1", "spk_2"],
            "track": [
                {"text_id": 0, "text": "甲", "start": 0.1, "end": 0.5,
                 "speaker": 0, "speaker_id": "spk_1",
                 "speaker_uncertain": False,
                 "decision_reason":
                 "baseline_confirmed_short_direct_voiceprint"},
                {"text_id": 0, "text": "乙，", "start": 0.6, "end": 0.7,
                 "speaker": 1, "speaker_id": "spk_2",
                 "speaker_uncertain": False,
                 "decision_reason":
                 "baseline_confirmed_short_direct_voiceprint"},
            ],
        }
        metadata = {
            "kind": "orator_punctuation_phrase_spans",
            "asr": {"0": {"text": "甲乙，"}},
            "align": {"0": [
                {"text": "甲", "start": 0.1, "end": 0.5},
                {"text": "乙", "start": 0.6, "end": 0.7},
            ]},
            "phrases": [{
                "evidence_id": "phrase:0", "text_id": 0,
                "source_start": 0, "source_end": 3,
                "start": 0.1, "end": 0.7,
            }],
        }
        vad = [{"evidence_id": "vad:0", "start": 0.0, "end": 0.8}]
        frames = []
        for index in range(9):
            top1 = 1 if index in (6, 7) else 0
            frames.append({
                "frame": index, "time": index * 0.1, "top1": top1,
                "probabilities": {
                    0: 0.8, 1: 0.9 if top1 == 1 else 0.2,
                },
            })

        def evidence(first):
            second = "spk_2" if first == "spk_1" else "spk_1"
            return {"status": "ok", "duration_sec": 0.6,
                    "scores": {first: 0.30, second: 0.20}}

        session = {"phrase:0": evidence("spk_1")}
        robust = {"phrase:0": evidence("spk_1")}
        mapping = {"mapping": {"0": "spk_1", "1": "spk_2"}}
        policy = {
            "frame_activity_threshold": 0.5,
            "minimum_selected_channel_sec": 0.4,
            "positive_suffix_unit_count": 1,
            "allowed_anchor_reasons": [
                "baseline_confirmed_short_direct_voiceprint"],
            "allowed_competing_reasons": [
                "baseline_confirmed_short_direct_voiceprint"],
            "protected_decision_reasons": ["reviewed_repair"],
            "voiceprint": {
                "short_max_sec": 1.5, "short_min_score": 0.0,
                "short_min_margin": 0.04, "regular_min_score": 0.55,
                "regular_min_margin": 0.04,
            },
        }
        return [baseline, metadata, vad, frames, 0.1, session, robust,
                mapping, policy]

    def test_closes_one_timed_competing_suffix_unit(self):
        result = candidate.build_candidate(*self.fixture())
        self.assertEqual(result["accepted_closure_count"], 1)
        self.assertEqual("".join(x["text"] for x in result["track"]), "甲乙，")
        self.assertTrue(all(x["speaker_id"] == "spk_1"
                            for x in result["track"]))

    def test_abstains_for_two_timed_suffix_units(self):
        values = self.fixture()
        values[0]["track"][1]["text"] = "乙丙，"
        values[1]["asr"]["0"]["text"] = "甲乙丙，"
        values[1]["align"]["0"].append(
            {"text": "丙", "start": 0.7, "end": 0.8})
        values[1]["phrases"][0]["source_end"] = 4
        values[1]["phrases"][0]["end"] = 0.8
        result = candidate.build_candidate(*values)
        self.assertEqual(result["accepted_closure_count"], 0)

    def test_abstains_without_selected_tail_activity(self):
        values = self.fixture()
        values[3][6]["probabilities"][0] = 0.4
        result = candidate.build_candidate(*values)
        self.assertEqual(result["accepted_closure_count"], 0)

    def test_abstains_for_phrase_registry_disagreement(self):
        values = self.fixture()
        values[6]["phrase:0"]["scores"] = {"spk_1": 0.20, "spk_2": 0.30}
        result = candidate.build_candidate(*values)
        self.assertEqual(result["accepted_closure_count"], 0)

    def test_abstains_for_non_direct_anchor(self):
        values = self.fixture()
        values[0]["track"][0]["decision_reason"] = "baseline_score_below_gate"
        result = candidate.build_candidate(*values)
        self.assertEqual(result["accepted_closure_count"], 0)

    def test_abstains_when_vad_container_is_not_unique(self):
        values = self.fixture()
        values[2].append({"evidence_id": "vad:1", "start": 0.05,
                          "end": 0.75})
        result = candidate.build_candidate(*values)
        self.assertEqual(result["accepted_closure_count"], 0)


if __name__ == "__main__":
    unittest.main()
