#!/usr/bin/env python3

import importlib.util
import json
import pathlib
import sys
import tempfile
import unittest


REPO = pathlib.Path(__file__).resolve().parents[3]
MODULE_PATH = (
    REPO / "tools" / "verify" / "py" /
    "speaker_business_review_packet.py")
SPEC = importlib.util.spec_from_file_location(
    "speaker_business_review_packet", MODULE_PATH)
speaker_business_review_packet = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = speaker_business_review_packet
SPEC.loader.exec_module(speaker_business_review_packet)


class SpeakerBusinessReviewPacketTest(unittest.TestCase):
    def test_loads_frozen_candidate_with_decision_audit(self):
        candidate = {
            "kind": "orator_frozen_speaker_candidate",
            "audio_sec": 5.0,
            "track": [{
                "start": 1.0,
                "end": 3.5,
                "speaker_id": "spk_4",
                "speaker_uncertain": False,
                "decision_reason": "strong_turn_voiceprint",
                "text": "hello",
            }],
        }
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "candidate.json"
            path.write_text(json.dumps(candidate), encoding="utf-8")
            audio_sec, entries = speaker_business_review_packet._load_view(
                path)
        self.assertEqual(audio_sec, 5.0)
        self.assertEqual(entries[0].speaker, "spk_4")
        self.assertEqual(entries[0].reason, "strong_turn_voiceprint")
        self.assertFalse(entries[0].uncertain)

    def test_reference_parser_preserves_duplicate_and_backward_rows(self):
        text = """00:00:03 A
one
00:00:03 B
two
00:00:02 C
three
"""
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "reference.txt"
            path.write_text(text, encoding="utf-8")
            rows = speaker_business_review_packet._parse_ref(path, 10.0)
        self.assertEqual(len(rows), 3)
        self.assertEqual(rows[0].reference_id, "ref-0001")
        self.assertEqual(rows[0].interval_issue, "duplicate_source_timestamp")
        self.assertEqual(rows[1].interval_issue, "backward_source_timestamp")
        self.assertEqual(rows[2].interval_issue, "")

    def test_assignment_signature_ignores_reason_but_detects_speaker(self):
        entry = speaker_business_review_packet.ViewEntry(
            start=1.0,
            end=3.0,
            speaker="spk_1",
            text="hello",
            reason="old_reason",
        )
        same = speaker_business_review_packet.ViewEntry(
            start=1.0,
            end=3.0,
            speaker="spk_1",
            text="hello",
            reason="new_reason",
        )
        changed = speaker_business_review_packet.ViewEntry(
            start=1.0,
            end=3.0,
            speaker="spk_2",
            text="hello",
        )
        signature = speaker_business_review_packet._assignment_signature
        self.assertEqual(signature([entry], 0.0, 4.0),
                         signature([same], 0.0, 4.0))
        self.assertNotEqual(signature([entry], 0.0, 4.0),
                            signature([changed], 0.0, 4.0))

    def test_changed_only_reference_render_keeps_all_changed_rows(self):
        refs = [
            speaker_business_review_packet.RefBlock(
                "ref-0001", 0.0, 2.0, "A", "one"),
            speaker_business_review_packet.RefBlock(
                "ref-0002", 2.0, 4.0, "B", "two"),
        ]
        old = [
            speaker_business_review_packet.ViewEntry(
                0.0, 2.0, "spk_1", "one"),
            speaker_business_review_packet.ViewEntry(
                2.0, 4.0, "spk_1", "two"),
        ]
        new = [
            speaker_business_review_packet.ViewEntry(
                0.0, 2.0, "spk_1", "one"),
            speaker_business_review_packet.ViewEntry(
                2.0, 4.0, "spk_2", "two"),
        ]
        rendered = speaker_business_review_packet._render_by_reference(
            timeline_path=pathlib.Path("new.json"),
            reference_path=pathlib.Path("reference.txt"),
            audio_sec=4.0,
            refs=refs,
            entries=new,
            max_chars=100,
            comparison_path=pathlib.Path("old.json"),
            comparison_entries=old,
            only_changed=True,
        )
        self.assertNotIn("## ref-0001", rendered)
        self.assertIn("## ref-0002", rendered)
        self.assertIn("Displayed entries: `1`", rendered)

    def test_speaker_sequence_signature_ignores_boundary_refinement(self):
        coarse = [speaker_business_review_packet.ViewEntry(
            0.0, 4.0, "spk_1", "whole")]
        refined = [
            speaker_business_review_packet.ViewEntry(
                0.0, 2.0, "spk_1", "left"),
            speaker_business_review_packet.ViewEntry(
                2.0, 4.0, "spk_1", "right"),
        ]
        signature = speaker_business_review_packet._speaker_sequence_signature
        self.assertEqual(
            signature(coarse, 0.0, 4.0), signature(refined, 0.0, 4.0))


if __name__ == "__main__":
    unittest.main()
