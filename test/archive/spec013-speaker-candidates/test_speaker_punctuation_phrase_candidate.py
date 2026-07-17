#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import unittest


REPO = pathlib.Path(__file__).resolve().parents[3]
MODULE_PATH = (
    REPO / "tools" / "verify" / "py" /
    "speaker_punctuation_phrase_candidate.py")
SPEC = importlib.util.spec_from_file_location(
    "speaker_punctuation_phrase_candidate", MODULE_PATH)
speaker_punctuation_phrase_candidate = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = speaker_punctuation_phrase_candidate
SPEC.loader.exec_module(speaker_punctuation_phrase_candidate)


POLICY = {
    "short_max_sec": 1.5,
    "short_min_score": 0.0,
    "short_min_margin": 0.04,
    "regular_min_score": 0.55,
    "regular_min_margin": 0.04,
}


class SpeakerPunctuationPhraseCandidateTest(unittest.TestCase):
    def test_phrase_ranges_keep_terminal_punctuation(self):
        self.assertEqual(
            list(speaker_punctuation_phrase_candidate.phrase_ranges(
                "第一句，第二句。尾句", set("，。"))),
            [(0, 4), (4, 8), (8, 10)])

    def test_alignment_is_subsequence_of_punctuated_source(self):
        units = [
            {"start": 1.0, "end": 1.2, "text": "你"},
            {"start": 1.2, "end": 1.4, "text": "好"},
            {"start": 2.0, "end": 2.2, "text": "吗"},
        ]
        result = speaker_punctuation_phrase_candidate.aligned_character_times(
            "你好，吗？", units)
        self.assertEqual(result[0]["start"], 1.0)
        self.assertIsNone(result[2])
        self.assertEqual(result[3]["end"], 2.2)

    def test_short_identity_uses_margin_without_absolute_score(self):
        selected, reason, _ = (
            speaker_punctuation_phrase_candidate.select_identity(
                {"status": "ok", "duration_sec": 0.8,
                 "scores": {"spk_1": 0.10, "spk_2": 0.15}},
                ["spk_1", "spk_2"], POLICY))
        self.assertEqual(selected, "spk_2")
        self.assertEqual(reason, "phrase_short_voiceprint")

    def test_regular_identity_keeps_production_score_gate(self):
        selected, reason, _ = (
            speaker_punctuation_phrase_candidate.select_identity(
                {"status": "ok", "duration_sec": 2.0,
                 "scores": {"spk_1": 0.40, "spk_2": 0.50}},
                ["spk_1", "spk_2"], POLICY))
        self.assertIsNone(selected)
        self.assertEqual(reason, "phrase_regular_score_below_gate")

    def test_phrase_anchor_ids_exposes_conflict(self):
        phrase = {"source_start": 2, "source_end": 8}
        fragments = [
            {"source_start": 0, "source_end": 4, "anchor": True,
             "decision": {"speaker_id": "spk_1"}},
            {"source_start": 4, "source_end": 9, "anchor": True,
             "decision": {"speaker_id": "spk_2"}},
        ]
        self.assertEqual(
            speaker_punctuation_phrase_candidate.phrase_anchor_ids(
                phrase, fragments),
            {"spk_1", "spk_2"})

    def test_anchored_projection_requires_agreeing_current_audio(self):
        accepted = (
            speaker_punctuation_phrase_candidate.phrase_overlay_accepted)
        self.assertFalse(accepted("spk_1", set(), True))
        self.assertTrue(accepted("spk_1", {"spk_1"}, True))
        self.assertFalse(accepted("spk_1", {"spk_1", "spk_2"}, True))

    def test_reprojection_preserves_exact_source_text(self):
        source = "你好，继续。"
        fragments = [{
            "entry": {
                "start": 1.0, "end": 3.0, "text": source,
                "speaker": 0, "speaker_id": "spk_1",
                "speaker_uncertain": False,
                "decision_reason": "baseline",
            }
        }]
        units = [
            {"start": 1.0, "end": 1.2, "text": "你"},
            {"start": 1.2, "end": 1.4, "text": "好"},
            {"start": 2.0, "end": 2.2, "text": "继"},
            {"start": 2.2, "end": 2.4, "text": "续"},
        ]
        overlays = [{
            "source_start": 3, "source_end": len(source),
            "speaker_id": "spk_2", "reason": "phrase_overlay",
        }]
        result = speaker_punctuation_phrase_candidate.reproject_text(
            3, source, units, fragments, overlays,
            {"spk_1": 0, "spk_2": 1})
        self.assertEqual("".join(item["text"] for item in result), source)
        self.assertEqual(result[-1]["speaker_id"], "spk_2")


if __name__ == "__main__":
    unittest.main()
