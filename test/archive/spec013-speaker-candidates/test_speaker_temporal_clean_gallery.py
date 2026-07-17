#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[3]
TOOLS = ROOT / "tools" / "verify" / "py"
sys.path.insert(0, str(TOOLS))
MODULE_PATH = TOOLS / "speaker_temporal_clean_gallery.py"
SPEC = importlib.util.spec_from_file_location("temporal_gallery", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class TemporalCleanGalleryTest(unittest.TestCase):
    @staticmethod
    def values():
        values = []
        for index in range(12):
            values.append({
                "evidence_id": f"d:{index}",
                "start": float(index * 10),
                "end": float(index * 10 + 3),
                "quality": float(index % 2),
            })
        return values

    def test_selects_quality_maximum_from_each_temporal_stratum(self):
        selected = MODULE.temporal_quality_strata(self.values(), 3)
        self.assertEqual(
            [value["evidence_id"] for value in selected],
            ["d:1", "d:5", "d:9"])
        self.assertEqual([value["start"] for value in selected],
                         sorted(value["start"] for value in selected))

    def test_selection_is_deterministic_for_unsorted_input(self):
        forward = MODULE.temporal_quality_strata(self.values(), 4)
        reverse = MODULE.temporal_quality_strata(
            list(reversed(self.values())), 4)
        self.assertEqual([value["evidence_id"] for value in forward],
                         [value["evidence_id"] for value in reverse])

    def test_rejects_incomplete_gallery(self):
        with self.assertRaisesRegex(ValueError, "too few"):
            MODULE.temporal_quality_strata(self.values()[:2], 3)

    def test_policy_requires_temporal_selection(self):
        policy = MODULE.load_policy(
            ROOT / "specs" / "013-industrial-closing-validation" /
            "speaker-v21-temporal-clean-gallery.toml")
        self.assertEqual(policy["selection"], "temporal_quality_strata")


if __name__ == "__main__":
    unittest.main()
