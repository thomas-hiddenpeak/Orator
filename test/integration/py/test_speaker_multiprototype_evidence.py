#!/usr/bin/env python3

import importlib.util
import pathlib
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[3]
MODULE_PATH = ROOT / "tools/verify/py/speaker_multiprototype_evidence.py"
SPEC = importlib.util.spec_from_file_location("multiprototype", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def row(evidence_id="turn:0", start="1.0", score1="0.7", score2="0.2"):
    return {
        "evidence_id": evidence_id,
        "start_sec": start,
        "end_sec": "3.0",
        "duration_sec": "2.0",
        "status": "ok",
        "embed_start_sec": "1.0",
        "embed_end_sec": "3.0",
        "best_id": "spk_1",
        "best_score": score1,
        "second_id": "spk_2",
        "second_score": score2,
        "margin": str(float(score1) - float(score2)),
        "score_spk_1": score1,
        "score_spk_2": score2,
    }


class MultiprototypeEvidenceTest(unittest.TestCase):
    def test_same_identity_scores_are_reduced_by_maximum(self):
        result = MODULE.fuse_rows(
            [row(score1="0.70", score2="0.20")],
            [row(score1="0.60", score2="0.40")],
            ["score_spk_1", "score_spk_2"])[0]
        self.assertEqual(result["best_id"], "spk_1")
        self.assertAlmostEqual(float(result["score_spk_1"]), 0.70)
        self.assertAlmostEqual(float(result["score_spk_2"]), 0.40)
        self.assertAlmostEqual(float(result["margin"]), 0.30)

    def test_cross_identity_prototype_conflict_remains_in_margin(self):
        result = MODULE.fuse_rows(
            [row(score1="0.70", score2="0.20")],
            [row(score1="0.30", score2="0.65")],
            ["score_spk_1", "score_spk_2"])[0]
        self.assertEqual(result["best_id"], "spk_1")
        self.assertAlmostEqual(float(result["second_score"]), 0.65)
        self.assertAlmostEqual(float(result["margin"]), 0.05)

    def test_interval_mismatch_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "metadata mismatch"):
            MODULE.fuse_rows(
                [row(start="1.0")], [row(start="1.1")],
                ["score_spk_1", "score_spk_2"])

    def test_incomplete_gallery_is_rejected(self):
        incomplete = row()
        incomplete["score_spk_2"] = ""
        with self.assertRaisesRegex(ValueError, "incomplete prototype"):
            MODULE.fuse_rows(
                [row()], [incomplete],
                ["score_spk_1", "score_spk_2"])

    def test_policy_requires_maximum_and_complete_gallery(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "policy.toml"
            path.write_text(
                "[speaker_prototype_fusion]\n"
                "aggregation = \"maximum\"\n"
                "require_complete_gallery = true\n",
                encoding="ascii")
            self.assertEqual(MODULE.load_policy(path)["aggregation"],
                             "maximum")


if __name__ == "__main__":
    unittest.main()
