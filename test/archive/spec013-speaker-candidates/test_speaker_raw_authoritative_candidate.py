#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[3]
TOOLS = ROOT / "tools" / "verify" / "py"
sys.path.insert(0, str(TOOLS))
MODULE_PATH = TOOLS / "speaker_raw_authoritative_candidate.py"
SPEC = importlib.util.spec_from_file_location("raw_authoritative", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class SpeakerRawAuthoritativeCandidateTest(unittest.TestCase):
    def test_source_fragments_preserve_character_offsets(self):
        values = MODULE.source_fragments([
            {"text_id": 2, "text": "ab"},
            {"text_id": 2, "text": "c"},
        ])
        self.assertEqual(values[2][0]["source_start"], 0)
        self.assertEqual(values[2][0]["source_end"], 2)
        self.assertEqual(values[2][1]["source_start"], 2)
        self.assertEqual(values[2][1]["source_end"], 3)

    def test_only_allowed_known_raw_range_is_authoritative(self):
        fragments = MODULE.source_fragments([
            {"text_id": 0, "text": "a", "speaker_id": "spk_1",
             "decision_reason": "sole_diar_support"},
            {"text_id": 0, "text": "b", "speaker_id": "spk_2",
             "decision_reason": "same_speaker_gap_fill"},
            {"text_id": 0, "text": "c", "speaker_id": None,
             "decision_reason": "sole_diar_support"},
        ])
        overlays, audit = MODULE.authoritative_overlays(
            fragments,
            {"require_known_identity": True,
             "allowed_reasons": ["sole_diar_support"]},
            {"spk_1", "spk_2"})
        self.assertEqual(len(overlays[0]), 1)
        self.assertEqual(overlays[0][0]["speaker_id"], "spk_1")
        self.assertEqual([item["accepted"] for item in audit],
                         [True, False, False])

    def test_policy_requires_known_identity(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "policy.toml"
            path.write_text(
                "[raw_authoritative]\n"
                "require_known_identity = false\n"
                "allowed_reasons = [\"sole_diar_support\"]\n",
                encoding="ascii")
            with self.assertRaisesRegex(ValueError, "known identity"):
                MODULE.load_policy(path)


if __name__ == "__main__":
    unittest.main()
