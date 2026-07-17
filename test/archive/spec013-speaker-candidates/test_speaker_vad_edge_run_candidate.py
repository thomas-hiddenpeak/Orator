#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[3]
TOOLS = ROOT / "tools" / "verify" / "py"
sys.path.insert(0, str(TOOLS))
MODULE_PATH = TOOLS / "speaker_vad_edge_run_candidate.py"
SPEC = importlib.util.spec_from_file_location("vad_edge_run", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class VadEdgeRunCandidateTest(unittest.TestCase):
    def setUp(self):
        self.policy = {
            "minimum_run_sec": 0.4,
            "maximum_run_sec": 3.0,
            "require_below_activity_threshold": False,
            "frame_activity_threshold": 0.5,
            "voiceprint": {},
        }

    @staticmethod
    def frames(channels):
        return [
            {"frame": index, "time": 0.04 + 0.08 * index,
             "local": channel, "probability": 0.7}
            for index, channel in enumerate(channels)
        ]

    def test_retains_sustained_start_and_end_runs(self):
        runs = MODULE.edge_runs(
            self.frames([2] * 5 + [3] * 8), 0.08,
            [(0.0, 1.04)], self.policy)
        self.assertEqual(len(runs), 2)
        self.assertEqual(runs[0]["edge"], "start")
        self.assertEqual(runs[0]["local_speaker"], 2)
        self.assertEqual(runs[1]["edge"], "end")
        self.assertEqual(runs[1]["local_speaker"], 3)

    def test_rejects_edge_with_short_adjacent_run(self):
        runs = MODULE.edge_runs(
            self.frames([2] * 5 + [3] * 3), 0.08,
            [(0.0, 0.64)], self.policy)
        self.assertEqual(runs, [])

    def test_rejects_single_run_vad(self):
        runs = MODULE.edge_runs(
            self.frames([2] * 10), 0.08, [(0.0, 0.8)], self.policy)
        self.assertEqual(runs, [])

    def test_low_activity_guard_uses_existing_threshold(self):
        guarded = {**self.policy, "require_below_activity_threshold": True}
        frames = self.frames([2] * 5 + [3] * 8)
        for item in frames:
            item["probability"] = 0.49
        self.assertEqual(
            len(MODULE.edge_runs(frames, 0.08, [(0.0, 1.04)], guarded)), 2)
        frames[0]["probability"] = 0.5
        runs = MODULE.edge_runs(frames, 0.08, [(0.0, 1.04)], guarded)
        self.assertEqual(len(runs), 1)
        self.assertEqual(runs[0]["edge"], "end")

    def test_does_not_emit_internal_run(self):
        runs = MODULE.edge_runs(
            self.frames([0] * 5 + [2] * 5 + [3] * 5), 0.08,
            [(0.0, 1.2)], self.policy)
        self.assertEqual([item["local_speaker"] for item in runs], [0, 3])

    def test_projects_alignment_without_punctuation_phrase_filter(self):
        metadata = {
            "asr": {"0": {"start": 0.0, "end": 2.0,
                            "text": "四层，嗯，哦，十五"}},
            "align": {"0": [
                {"start": 0.10, "end": 0.20, "text": "四"},
                {"start": 0.20, "end": 0.30, "text": "层"},
                {"start": 0.60, "end": 0.70, "text": "嗯"},
                {"start": 0.90, "end": 1.00, "text": "哦"},
                {"start": 1.10, "end": 1.20, "text": "十"},
                {"start": 1.20, "end": 1.30, "text": "五"},
            ]},
        }
        pieces, runs = MODULE.enumerate_pieces(
            metadata, self.frames([2] * 5 + [3] * 8), 0.08,
            [(0.0, 1.04)], self.policy)
        self.assertEqual(len(runs), 2)
        self.assertTrue(any(item["text"] == "四层" for item in pieces))

    def test_policy_requires_inherited_floor_and_window(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "config.toml"
            text = (ROOT / "specs" / "013-industrial-closing-validation" /
                    "speaker-v21-vad-edge-run.toml").read_text(
                        encoding="ascii")
            path.write_text(
                text.replace("maximum_run_sec = 3.0",
                             "maximum_run_sec = 2.0"),
                encoding="ascii")
            with self.assertRaisesRegex(ValueError, "differs from embedding"):
                MODULE.load_policy(path)


if __name__ == "__main__":
    unittest.main()
