#!/usr/bin/env python3

import importlib.util
import json
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


if __name__ == "__main__":
    unittest.main()
