#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[3]
TOOLS = ROOT / "tools" / "verify" / "py"
sys.path.insert(0, str(TOOLS))
MODULE_PATH = TOOLS / "speaker_vad_active_edge_candidate.py"
SPEC = importlib.util.spec_from_file_location("vad_active_edge", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class VadActiveEdgeCandidateTest(unittest.TestCase):
    def setUp(self):
        self.policy = {
            "minimum_run_sec": 0.4,
            "maximum_run_sec": 3.0,
            "frame_activity_threshold": 0.5,
            "voiceprint": {
                "short_max_sec": 1.5,
                "short_min_score": 0.0,
                "short_min_margin": 0.04,
                "regular_min_score": 0.55,
                "regular_min_margin": 0.04,
            },
        }

    @staticmethod
    def frames(channels, probability=0.8):
        return [
            {"frame": index, "time": 0.04 + 0.08 * index,
             "local": channel, "probability": probability}
            for index, channel in enumerate(channels)
        ]

    def test_requires_all_frames_active_on_both_sides(self):
        frames = self.frames([3] * 6 + [2] * 6)
        runs = MODULE.active_edge_runs(
            frames, 0.08, [(0.0, 0.96)], self.policy)
        self.assertEqual(len(runs), 2)
        frames[0]["probability"] = 0.49
        runs = MODULE.active_edge_runs(
            frames, 0.08, [(0.0, 0.96)], self.policy)
        self.assertEqual(runs, [])

    @staticmethod
    def evidence(first=None, second="spk_3"):
        if first is None:
            return {"status": "ok", "duration_sec": 1.0,
                    "scores": {"spk_1": 0.3, "spk_2": 0.28}}
        return {"status": "ok", "duration_sec": 1.0,
                "scores": {first: 0.7, second: 0.2}}

    def test_accepts_local_handoff_when_one_voiceprint_abstains(self):
        piece = {"local_speaker": 2}
        fragments = [{"entry": {"speaker_id": "spk_3"}}]
        result = MODULE.decide_piece(
            piece, self.evidence("spk_1"), self.evidence(), fragments,
            ["spk_1", "spk_2", "spk_3"], {2: "spk_2"}, self.policy,
            "vad_active_edge")
        self.assertTrue(result["accepted"])
        self.assertEqual(result["selected_speaker_id"], "spk_2")

    def test_vetoes_agreed_different_voiceprint(self):
        piece = {"local_speaker": 2}
        fragments = [{"entry": {"speaker_id": "spk_3"}}]
        result = MODULE.decide_piece(
            piece, self.evidence("spk_1"), self.evidence("spk_1"),
            fragments, ["spk_1", "spk_2", "spk_3"],
            {2: "spk_2"}, self.policy, "vad_active_edge")
        self.assertFalse(result["accepted"])
        self.assertTrue(result["agreed_different_voiceprint_veto"])

    def test_policy_requires_fr16j_activity_parity(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "config.toml"
            text = (ROOT / "specs" / "013-industrial-closing-validation" /
                    "speaker-v21-vad-active-edge.toml").read_text(
                        encoding="ascii")
            path.write_text(
                text.replace("frame_activity_threshold = 0.5\n\n[robust_gallery]",
                             "frame_activity_threshold = 0.4\n\n[robust_gallery]"),
                encoding="ascii")
            with self.assertRaisesRegex(ValueError, "activity differs"):
                MODULE.load_policy(path)


if __name__ == "__main__":
    unittest.main()
