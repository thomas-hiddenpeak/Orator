#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[3]
TOOLS = ROOT / "tools" / "verify" / "py"
sys.path.insert(0, str(TOOLS))
MODULE_PATH = TOOLS / "speaker_robust_gallery_evidence.py"
SPEC = importlib.util.spec_from_file_location("robust_gallery", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class SpeakerRobustGalleryEvidenceTest(unittest.TestCase):
    def test_top_half_mean_uses_highest_half(self):
        query = [1.0, 0.0]
        prototypes = [
            [1.0, 0.0],
            [0.8, 0.6],
            [0.2, 0.979795897],
            [0.0, 1.0],
        ]
        self.assertAlmostEqual(
            MODULE.top_half_mean(query, prototypes), 0.9, places=6)

    def test_top_half_rejects_odd_gallery(self):
        with self.assertRaisesRegex(ValueError, "even prototype count"):
            MODULE.top_half_mean([1.0, 0.0], [[1.0, 0.0]] * 3)

    def test_score_query_preserves_identity_set(self):
        query = {"status": "ok", "embedding": [1.0, 0.0]}
        gallery = {
            "spk_1": [[1.0, 0.0], [0.8, 0.6]],
            "spk_2": [[0.0, 1.0], [0.6, 0.8]],
        }
        result = MODULE.score_query(query, gallery, ["spk_1", "spk_2"])
        self.assertEqual(set(result), {"spk_1", "spk_2"})
        self.assertGreater(result["spk_1"], result["spk_2"])

    def test_policy_rejects_maximum_aggregation(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "policy.toml"
            path.write_text(
                "[robust_gallery]\n"
                "aggregation = \"maximum\"\n"
                "require_complete_gallery = true\n",
                encoding="ascii")
            with self.assertRaisesRegex(ValueError, "top_half_mean"):
                MODULE.load_policy(path)


if __name__ == "__main__":
    unittest.main()
