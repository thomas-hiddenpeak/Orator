#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import tempfile
import textwrap
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[3]
TOOLS = ROOT / "tools" / "verify" / "py"
sys.path.insert(0, str(TOOLS))
MODULE_PATH = TOOLS / "speaker_local_channel_island_candidate.py"
SPEC = importlib.util.spec_from_file_location("local_island", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class LocalChannelIslandCandidateTest(unittest.TestCase):
    def setUp(self):
        self.policy = {
            "minimum_island_run_sec": 0.4,
            "minimum_adjacent_run_sec": 0.4,
            "voiceprint": {
                "short_max_sec": 1.5,
                "short_min_score": 0.0,
                "short_min_margin": 0.04,
                "regular_min_score": 0.55,
                "regular_min_margin": 0.04,
            },
        }

    @staticmethod
    def frames(locals_):
        return [
            {"frame": index, "time": 0.05 + index * 0.1,
             "local": local, "probability": 0.7}
            for index, local in enumerate(locals_)
        ]

    def test_detects_complete_aba_island(self):
        frames = self.frames([0] * 5 + [2] * 4 + [0] * 5)
        islands = MODULE.channel_islands(
            frames, 0.1, [(0.0, 1.4)], self.policy)
        self.assertEqual(len(islands), 1)
        self.assertEqual(islands[0]["local_speaker"], 2)
        self.assertEqual(islands[0]["left_local_speaker"], 0)
        self.assertEqual(islands[0]["right_local_speaker"], 0)

    def test_rejects_abc_transition(self):
        frames = self.frames([0] * 5 + [2] * 4 + [1] * 5)
        self.assertEqual(
            MODULE.channel_islands(
                frames, 0.1, [(0.0, 1.4)], self.policy), [])

    def test_projection_requires_wholly_contained_units(self):
        metadata = {
            "asr": {"0": {"start": 0.0, "end": 2.0, "text": "abc"}},
            "align": {"0": [
                {"start": 0.8, "end": 1.1, "text": "a"},
                {"start": 1.1, "end": 1.3, "text": "b"},
                {"start": 1.3, "end": 1.6, "text": "c"},
            ]},
        }
        groups = MODULE.projected_groups(
            metadata, {"start": 1.0, "end": 1.5})
        self.assertEqual(len(groups), 1)
        self.assertEqual(groups[0]["text"], "b")

    @staticmethod
    def evidence(first="spk_2", second="spk_1"):
        return {
            "status": "ok",
            "duration_sec": 1.0,
            "scores": {first: 0.7, second: 0.2},
        }

    def test_accepts_three_view_known_conflict(self):
        piece = {"local_speaker": 2}
        fragments = [{"entry": {"speaker_id": "spk_1"}}]
        result = MODULE.decide_piece(
            piece, self.evidence(), self.evidence(), fragments,
            ["spk_1", "spk_2"], {0: "spk_1", 2: "spk_2"}, self.policy)
        self.assertTrue(result["accepted"])
        self.assertEqual(result["selected_speaker_id"], "spk_2")

    def test_rejects_registry_disagreement(self):
        piece = {"local_speaker": 2}
        fragments = [{"entry": {"speaker_id": "spk_1"}}]
        result = MODULE.decide_piece(
            piece, self.evidence(), self.evidence("spk_1", "spk_2"),
            fragments, ["spk_1", "spk_2"],
            {0: "spk_1", 2: "spk_2"}, self.policy)
        self.assertFalse(result["accepted"])
        self.assertEqual(result["reason"], "local_island_registry_disagreement")

    def test_policy_requires_fr16j_parity(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "config.toml"
            text = (ROOT / "specs" / "013-industrial-closing-validation" /
                    "speaker-v21-local-channel-island.toml").read_text(
                        encoding="ascii")
            path.write_text(
                text.replace("minimum_island_run_sec = 0.4",
                             "minimum_island_run_sec = 0.3"),
                encoding="ascii")
            with self.assertRaisesRegex(ValueError, "differs from FR16J"):
                MODULE.load_policy(path)

    def test_reads_monotonic_vad_span_tsv(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "vad.tsv"
            path.write_text(textwrap.dedent("""\
                evidence_id\tstart_sec\tend_sec\tduration_sec
                vad:0\t1.0\t2.0\t1.0
                vad:1\t3.0\t4.0\t1.0
                """), encoding="ascii")
            self.assertEqual(MODULE.read_vad(path), [(1.0, 2.0), (3.0, 4.0)])


if __name__ == "__main__":
    unittest.main()
