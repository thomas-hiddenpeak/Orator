#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[3]
TOOLS = ROOT / "tools" / "verify" / "py"
sys.path.insert(0, str(TOOLS))
MODULE_PATH = TOOLS / "speaker_expanded_local_run_phrase_candidate.py"
SPEC = importlib.util.spec_from_file_location("expanded_local_run", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class ExpandedLocalRunPhraseCandidateTest(unittest.TestCase):
    def setUp(self):
        self.policy = {
            "minimum_phrase_sec": 0.4,
            "maximum_phrase_sec": 3.0,
            "minimum_query_sec": 0.4,
            "maximum_query_sec": 3.0,
            "voiceprint": {
                "short_max_sec": 1.5,
                "short_min_score": 0.0,
                "short_min_margin": 0.04,
                "regular_min_score": 0.55,
                "regular_min_margin": 0.04,
            },
        }
        self.metadata = {
            "asr": {"0": {"text": "complete"}},
            "phrases": [{
                "evidence_id": "punctuation_phrase:0",
                "text_id": 0, "source_start": 0, "source_end": 8,
                "start": 1.0, "end": 1.8, "text": "complete",
            }],
        }
        self.baseline = {"track": [{
            "text_id": 0, "text": "complete", "speaker_id": "spk_1",
        }]}
        self.manifest = {"mapping": {"1": "spk_1", "2": "spk_2"}}

    @staticmethod
    def frames(run_start=1, run_end=16):
        output = []
        for index in range(18):
            local = 2 if run_start <= index < run_end else 1
            output.append({
                "frame": index, "time": 0.8 + 0.08 * index,
                "local": local, "probability": 0.7,
            })
        return output

    def enumerate(self, frames=None, baseline=None, manifest=None):
        return MODULE.enumerate_pieces(
            self.metadata, frames or self.frames(), 0.08, [(0.7, 2.2)],
            baseline or self.baseline, manifest or self.manifest, self.policy)

    def test_expands_query_to_maximal_same_channel_run(self):
        pieces = self.enumerate()
        self.assertEqual(len(pieces), 1)
        piece = pieces[0]
        self.assertAlmostEqual(piece["phrase_start"], 1.0)
        self.assertAlmostEqual(piece["phrase_end"], 1.8)
        self.assertAlmostEqual(piece["start"], 0.84)
        self.assertAlmostEqual(piece["end"], 2.04)
        self.assertEqual(piece["query_frame_start"], 1)
        self.assertEqual(piece["query_frame_end"], 16)
        self.assertTrue(piece["query_strictly_expands_phrase"])

    def test_rejects_unexpanded_or_overlong_maximal_run(self):
        exact = self.frames(run_start=3, run_end=13)
        self.assertEqual(self.enumerate(exact), [])
        long_frames = [
            {"frame": index, "time": 0.04 + 0.08 * index,
             "local": 2, "probability": 0.7}
            for index in range(50)
        ]
        self.assertEqual(MODULE.enumerate_pieces(
            self.metadata, long_frames, 0.08, [(0.0, 4.0)], self.baseline,
            self.manifest, self.policy), [])

    def test_rejects_missing_known_conflict_or_mapping(self):
        same = {"track": [{
            "text_id": 0, "text": "complete", "speaker_id": "spk_2",
        }]}
        self.assertEqual(self.enumerate(baseline=same), [])
        self.assertEqual(self.enumerate(manifest={"mapping": {"1": "spk_1"}}), [])

    @staticmethod
    def evidence(first="spk_2", second="spk_1"):
        if first is None:
            return {"status": "ok", "duration_sec": 2.0,
                    "scores": {"spk_2": 0.52, "spk_1": 0.50}}
        return {"status": "ok", "duration_sec": 2.0,
                "scores": {first: 0.7, second: 0.2}}

    def decide(self, current=None, robust=None, fragments=None, mapping=None):
        piece = {"local_speaker": 2}
        return MODULE.decide_piece(
            piece, current or self.evidence(), robust or self.evidence(),
            fragments or [{"entry": {"speaker_id": "spk_1"}}],
            ["spk_1", "spk_2"], mapping or {1: "spk_1", 2: "spk_2"},
            self.policy)

    def test_requires_dual_eligible_voiceprint_and_local_agreement(self):
        self.assertTrue(self.decide()["accepted"])
        self.assertFalse(self.decide(current=self.evidence(None))["accepted"])
        self.assertFalse(self.decide(
            robust=self.evidence("spk_1", "spk_2"))["accepted"])
        self.assertFalse(self.decide(
            mapping={1: "spk_2", 2: "spk_1"})["accepted"])

    def test_policy_requires_inherited_query_bounds(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "config.toml"
            text = (ROOT / "specs" / "013-industrial-closing-validation" /
                    "speaker-v21-expanded-local-run-phrase.toml").read_text(
                        encoding="ascii")
            path.write_text(text.replace(
                "maximum_query_sec = 3.0", "maximum_query_sec = 2.9"),
                encoding="ascii")
            with self.assertRaisesRegex(ValueError, "maximum differs"):
                MODULE.load_policy(path)


if __name__ == "__main__":
    unittest.main()
