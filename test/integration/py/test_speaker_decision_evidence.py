#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import unittest


REPO = pathlib.Path(__file__).resolve().parents[3]
MODULE_PATH = (
    REPO / "tools" / "verify" / "py" / "speaker_decision_evidence.py")
SPEC = importlib.util.spec_from_file_location(
    "speaker_decision_evidence", MODULE_PATH)
speaker_decision_evidence = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = speaker_decision_evidence
SPEC.loader.exec_module(speaker_decision_evidence)


class SpeakerDecisionEvidenceTest(unittest.TestCase):
    def test_competing_candidates_match_runtime_policy(self):
        entry = {
            "start": 4.0,
            "end": 6.0,
            "speaker": 1,
            "speaker_id": "spk_1",
        }
        diar = [
            {"start": 0.0, "end": 10.0, "speaker": 0,
             "speaker_id": "spk_0", "confidence": 0.4},
            {"start": 4.0, "end": 6.0, "speaker": 1,
             "speaker_id": "spk_1", "confidence": 0.75},
        ]
        result = speaker_decision_evidence.compute_decision(
            entry, diar, "forced_alignment")

        self.assertEqual(result["reason"],
                         "competing_diar_interval_policy")
        self.assertEqual(result["overlap_margin_sec"], 0.0)
        self.assertEqual(result["confidence_margin"], 0.35)
        self.assertEqual([candidate["speaker"]
                          for candidate in result["candidates"]], [1, 0])
        self.assertTrue(result["candidates"][0]["selected"])
        self.assertFalse(result["candidates"][1]["selected"])

    def test_gap_fill_uses_union_coverage_and_islands(self):
        entry = {"start": 0.0, "end": 3.0, "speaker": 0}
        diar = [
            {"start": 0.0, "end": 1.0, "speaker": 0,
             "confidence": 0.8},
            {"start": 2.0, "end": 3.0, "speaker": 0,
             "confidence": 0.6},
        ]
        result = speaker_decision_evidence.compute_decision(
            entry, diar, "asr_exact")

        self.assertEqual(result["reason"], "same_speaker_gap_fill")
        self.assertEqual(result["candidates"][0]["overlap_sec"], 2.0)
        self.assertEqual(result["candidates"][0]["coverage_ratio"], 0.666667)
        self.assertEqual(result["candidates"][0]["confidence"], 0.7)
        self.assertEqual(result["candidates"][0]["island_count"], 2)

    def test_existing_runtime_decision_must_match(self):
        package = {
            "timeline": {
                "audio_sec": 2.0,
                "resolved_config": {"timeline": {"gap_fill_enabled": True}},
                "tracks": [
                    {"kind": "diarization", "entries": [
                        {"start": 0.0, "end": 2.0, "speaker": 0,
                         "confidence": 0.8},
                    ]},
                    {"kind": "asr", "entries": [
                        {"start": 0.0, "end": 2.0, "text_id": 7},
                    ]},
                    {"kind": "align", "entries": []},
                    {"kind": "business_speaker", "entries": [
                        {"start": 0.0, "end": 2.0, "text_id": 7,
                         "speaker": 0, "text": "x"},
                    ]},
                ],
            },
        }
        decision = speaker_decision_evidence.compute_decision(
            package["timeline"]["tracks"][3]["entries"][0],
            package["timeline"]["tracks"][0]["entries"],
            "asr_exact")
        package["timeline"]["tracks"][3]["entries"][0][
            "speaker_decision"] = decision

        original_hash = speaker_decision_evidence.sha256_file
        speaker_decision_evidence.sha256_file = lambda _: "fixture"
        try:
            result = speaker_decision_evidence.build_evidence(
                package, "fixture.json")
            self.assertTrue(result["runtime_replay_match"])
            self.assertEqual(result["runtime_replay_mode"], "exact")
            package["timeline"]["tracks"][3]["entries"][0][
                "speaker_decision"]["candidates"][0]["confidence"] += 0.0004
            result = speaker_decision_evidence.build_evidence(
                package, "fixture.json")
            self.assertEqual(result["runtime_replay_mode"],
                             "terminal_quantization_envelope")
            package["timeline"]["tracks"][3]["entries"][0][
                "speaker_decision"]["reason"] = "wrong"
            with self.assertRaisesRegex(ValueError, "runtime decision mismatch"):
                speaker_decision_evidence.build_evidence(
                    package, "fixture.json")
        finally:
            speaker_decision_evidence.sha256_file = original_hash

    def test_projection_source_inference_uses_alignment_for_splits(self):
        business = [
            {"start": 0.0, "end": 1.0, "text_id": 3},
            {"start": 1.0, "end": 2.0, "text_id": 3},
            {"start": 2.0, "end": 3.0, "text_id": 4},
        ]
        asr = [
            {"start": 0.0, "end": 2.0, "text_id": 3},
            {"start": 2.0, "end": 3.0, "text_id": 4},
        ]
        align = [{"text_id": 3, "units": [
            {"start": 0.0, "end": 1.0, "text": "x"},
        ]}]

        self.assertEqual(
            speaker_decision_evidence.infer_projection_sources(
                business, asr, align),
            ["forced_alignment", "forced_alignment", "asr_exact"])


if __name__ == "__main__":
    unittest.main()
