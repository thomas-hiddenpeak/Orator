#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[3]
TOOLS = ROOT / "tools" / "verify" / "py"
sys.path.insert(0, str(TOOLS))
MODULE_PATH = TOOLS / "speaker_complete_source_voiceprint_candidate.py"
SPEC = importlib.util.spec_from_file_location("complete_source", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class CompleteSourceVoiceprintCandidateTest(unittest.TestCase):
    def setUp(self):
        self.source = "prefix，suffix。"
        self.alignment = {
            "kind": "orator_punctuation_phrase_spans",
            "asr": {"0": {"text": self.source}},
            "align": {"0": [
                {"start": 1.0 + 0.1 * index,
                 "end": 1.1 + 0.1 * index, "text": character}
                for index, character in enumerate(self.source)
                if character not in MODULE.SEPARATORS
            ]},
            "phrases": [{
                "evidence_id": "punctuation_phrase:0", "text_id": 0,
                "source_start": 0, "source_end": 7,
                "start": 1.0, "end": 1.6, "text": "prefix，",
            }],
        }
        self.bounded = {
            "kind": "orator_bounded_local_run_voiceprint_spans",
            "frame_period_sec": 0.08,
            "asr": self.alignment["asr"],
            "align": self.alignment["align"],
            "pieces": [{
                "evidence_id": "bounded_local_run_voiceprint:7",
                "phrase_evidence_id": "punctuation_phrase:0",
                "text_id": 0, "source_start": 0, "source_end": 7,
                "start": 0.9, "end": 2.5, "text": "prefix，",
            }],
        }
        self.baseline = {"track": [{
            "text_id": 0, "text": self.source, "speaker_id": "spk_1",
        }]}

    def test_expands_single_fully_aligned_source_and_preserves_evidence(self):
        pieces = MODULE.enumerate_pieces(
            self.bounded, self.alignment, self.baseline)
        self.assertEqual(len(pieces), 1)
        self.assertEqual(
            pieces[0]["evidence_id"],
            "bounded_local_run_voiceprint:7")
        self.assertEqual(pieces[0]["source_start"], 0)
        self.assertEqual(pieces[0]["source_end"], len(self.source))
        self.assertEqual(pieces[0]["text"], self.source)

    def test_rejects_source_with_more_than_one_indexed_phrase(self):
        self.alignment["phrases"].append({
            "evidence_id": "punctuation_phrase:1", "text_id": 0,
            "source_start": 7, "source_end": len(self.source),
            "start": 1.7, "end": 2.3, "text": "suffix。",
        })
        self.assertEqual(MODULE.enumerate_pieces(
            self.bounded, self.alignment, self.baseline), [])

    def test_rejects_unaligned_nonseparator_character(self):
        self.alignment["align"]["0"].pop()
        self.bounded["align"] = self.alignment["align"]
        self.assertEqual(MODULE.enumerate_pieces(
            self.bounded, self.alignment, self.baseline), [])

    def test_rejects_alignment_outside_query(self):
        self.alignment["align"]["0"][-1]["end"] = 2.6
        self.bounded["align"] = self.alignment["align"]
        self.assertEqual(MODULE.enumerate_pieces(
            self.bounded, self.alignment, self.baseline), [])

    def test_rejects_nonuniform_known_source(self):
        self.baseline["track"] = [
            {"text_id": 0, "text": "prefix，", "speaker_id": "spk_1"},
            {"text_id": 0, "text": "suffix。", "speaker_id": "spk_2"},
        ]
        self.assertEqual(MODULE.enumerate_pieces(
            self.bounded, self.alignment, self.baseline), [])

    def test_production_policy_requires_all_projection_contracts(self):
        policy = MODULE.load_policy(
            ROOT / "specs" / "013-industrial-closing-validation" /
            "speaker-v21-production-window-local-run.toml")
        self.assertTrue(policy["complete_source_voiceprint"][
            "project_complete_asr_source"])


if __name__ == "__main__":
    unittest.main()
