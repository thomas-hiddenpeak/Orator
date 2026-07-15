#!/usr/bin/env python3

import contextlib
import importlib.util
import io
import pathlib
import sys
import tempfile
import unittest


REPO = pathlib.Path(__file__).resolve().parents[3]
MODULE_PATH = REPO / "tools" / "verify" / "py" / "closing_ledger.py"
SPEC = importlib.util.spec_from_file_location("closing_ledger", MODULE_PATH)
closing_ledger = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = closing_ledger
SPEC.loader.exec_module(closing_ledger)


class ClosingLedgerTest(unittest.TestCase):
    def test_reference_parser_uses_stable_ids_and_final_audio_end(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            reference = pathlib.Path(temp_dir) / "reference.txt"
            reference.write_text(
                "00:00:01 A\nfirst\n00:00:05 B\nsecond\n",
                encoding="utf-8")
            entries = closing_ledger.parse_reference(reference, 9.5)
            self.assertEqual([e["reference_id"] for e in entries],
                             ["ref-0001", "ref-0002"])
            self.assertEqual(entries[0]["provisional_end_sec"], 5.0)
            self.assertEqual(entries[1]["provisional_end_sec"], 9.5)

    def test_manual_intervals_are_merged_without_double_counting(self):
        duration = closing_ledger.intervals_duration(
            [[1.0, 3.0], [2.0, 4.0], [5.0, 6.0]], 0.0, 10.0)
        self.assertEqual(duration, 4.0)

    def test_manual_intervals_cannot_be_silently_clipped(self):
        with self.assertRaises(ValueError):
            closing_ledger.intervals_duration(
                [[-1.0, 3.0]], 0.0, 10.0)

    def test_hash_detects_post_signature_changes(self):
        document = {"kind": "test", "entries": []}
        document["content_sha256"] = closing_ledger.content_sha256(document)
        closing_ledger.validate_hash(document, "test")
        document["entries"].append({"changed": True})
        with self.assertRaises(ValueError):
            closing_ledger.validate_hash(document, "test")

    def test_prepare_evidence_does_not_assign_a_judgment(self):
        selected = closing_ledger.select_evidence(
            [{"start": 1.0, "end": 3.0, "speaker": 2}], 2.0, 4.0,
            "diarization")
        self.assertEqual(len(selected), 1)
        self.assertNotIn("result", selected[0])
        self.assertEqual(closing_ledger.JUDGMENT_TEMPLATE["result"],
                         "unreviewed")

    def test_evidence_ids_remain_unique_when_text_ids_repeat(self):
        entries = [
            {"start": 1.0, "end": 2.0, "text_id": 7},
            {"start": 2.0, "end": 3.0, "text_id": 7},
        ]
        selected = closing_ledger.select_evidence(
            entries, 0.0, 4.0, "business_speaker")
        self.assertEqual(
            [item["evidence_id"] for item in selected],
            ["business_speaker:0:text:7", "business_speaker:1:text:7"])

    def test_uncertain_output_cannot_count_as_correct(self):
        judgment = closing_ledger.fresh_judgment()
        judgment.update({
            "result": "correct",
            "speaker_eval": "accurate",
            "system_speakers": ["spk_1"],
            "correct_speaker_intervals": [[1.0, 2.0]],
            "confident_wrong": False,
            "uncertain_output": True,
            "boundary_start_offset_sec": 0.0,
            "boundary_end_offset_sec": 0.0,
            "context_notes": "manual context judgment",
            "reviewer": "reviewer",
            "reviewed_utc": "2026-07-14T00:00:00+00:00",
        })
        with self.assertRaises(ValueError):
            closing_ledger.validate_judgment(
                judgment, "ref-0001", 1.0, 2.0, set())

    def test_automated_summary_command_is_not_available(self):
        with contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit) as raised:
                closing_ledger.main(["summary"])
        self.assertEqual(raised.exception.code, 2)


if __name__ == "__main__":
    unittest.main()
