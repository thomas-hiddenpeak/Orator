#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[3]
TOOLS = ROOT / "tools" / "verify" / "py"
sys.path.insert(0, str(TOOLS))
MODULE_PATH = TOOLS / "speaker_bounded_local_run_voiceprint_candidate.py"
SPEC = importlib.util.spec_from_file_location("bounded_local_run", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class BoundedLocalRunVoiceprintCandidateTest(unittest.TestCase):
    def setUp(self):
        self.policy = {
            "minimum_phrase_sec": 0.4,
            "maximum_phrase_sec": 3.0,
            "minimum_query_sec": 0.4,
            "maximum_query_sec": 3.0,
            "minimum_selected_channel_sec": 0.4,
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
                "start": 2.0, "end": 2.8, "text": "complete",
            }],
        }
        self.baseline = {"track": [{
            "text_id": 0, "text": "complete", "speaker_id": "spk_1",
        }]}
        self.manifest = {"mapping": {
            "0": "spk_1", "1": "spk_2", "2": "spk_3",
        }}

    @staticmethod
    def frames():
        return [
            {"frame": index, "time": 0.04 + 0.08 * index,
             "local": 1, "probability": 0.8}
            for index in range(60)
        ]

    def test_long_run_uses_deterministic_phrase_centered_max_window(self):
        pieces = MODULE.enumerate_pieces(
            self.metadata, self.frames(), 0.08,
            {index: 2 for index in range(60)}, [(0.0, 4.8)], [{
                "start": 2.0, "end": 2.5, "local_speaker": 2,
            }],
            self.baseline, self.manifest, self.policy)
        self.assertEqual(len(pieces), 1)
        piece = pieces[0]
        self.assertAlmostEqual(piece["start"], 0.9)
        self.assertAlmostEqual(piece["end"], 3.9)
        self.assertAlmostEqual(piece["duration_sec"], 3.0)
        self.assertAlmostEqual(piece["full_run_start"], 0.0)
        self.assertAlmostEqual(piece["full_run_end"], 4.8)
        self.assertTrue(piece["query_was_bounded"])
        self.assertAlmostEqual(piece["raw_channel_support_sec"]["2"], 0.5)

    def test_short_run_uses_whole_context(self):
        phrase = {"evidence_id": "p", "start": 1.0, "end": 1.8}
        run = {"query_start": 0.8, "query_end": 2.1}
        self.assertEqual(MODULE.bounded_query(phrase, run, 3.0), (0.8, 2.1))

    def test_rejects_run_that_does_not_cover_complete_phrase(self):
        frames = self.frames()
        frames[0]["local"] = 0
        metadata = {
            "asr": {"0": {"text": "complete"}},
            "phrases": [{
                "evidence_id": "punctuation_phrase:0", "text_id": 0,
                "source_start": 0, "source_end": 8, "start": 0.01,
                "end": 0.81, "text": "complete",
            }],
        }
        self.assertEqual(MODULE.enumerate_pieces(
            metadata, frames, 0.08, {index: 2 for index in range(60)},
            [(0.0, 4.8)], [], self.baseline,
            self.manifest, self.policy), [])

    @staticmethod
    def evidence(first="spk_3", second="spk_1"):
        if first is None:
            return {"status": "ok", "duration_sec": 2.0,
                    "scores": {"spk_3": 0.52, "spk_1": 0.50,
                               "spk_2": 0.2}}
        return {"status": "ok", "duration_sec": 2.0,
                "scores": {first: 0.7, second: 0.2, "spk_2": 0.1}}

    def decide(self, current=None, robust=None, fragments=None, mapping=None):
        return MODULE.decide_piece(
            {"local_speaker": 1,
             "raw_channel_support_sec": {"2": 0.4},
             "raw_top2_frame_count": {}, "query_frame_count": 10},
            current or self.evidence(),
            robust or self.evidence(),
            fragments or [{"entry": {"speaker_id": "spk_1"}}],
            ["spk_1", "spk_2", "spk_3"],
            mapping or {0: "spk_1", 1: "spk_2", 2: "spk_3"}, self.policy)

    def test_accepts_only_dual_conflict_with_baseline_and_local(self):
        result = self.decide()
        self.assertTrue(result["accepted"])
        self.assertEqual(result["selected_speaker_id"], "spk_3")
        self.assertFalse(self.decide(current=self.evidence(None))["accepted"])
        self.assertFalse(self.decide(
            robust=self.evidence("spk_2", "spk_3"))["accepted"])
        self.assertFalse(self.decide(
            current=self.evidence("spk_1", "spk_3"),
            robust=self.evidence("spk_1", "spk_3"))["accepted"])
        self.assertFalse(self.decide(
            current=self.evidence("spk_2", "spk_3"),
            robust=self.evidence("spk_2", "spk_3"))["accepted"])

    def test_rejects_missing_or_short_selected_channel_support(self):
        piece = {"local_speaker": 1, "raw_channel_support_sec": {"2": 0.39},
                 "raw_top2_frame_count": {"2": 9}, "query_frame_count": 10}
        result = MODULE.decide_piece(
            piece, self.evidence(), self.evidence(),
            [{"entry": {"speaker_id": "spk_1"}}],
            ["spk_1", "spk_2", "spk_3"],
            {0: "spk_1", 1: "spk_2", 2: "spk_3"}, self.policy)
        self.assertFalse(result["accepted"])

    def test_accepts_unanimous_top2_without_lowering_activity_gate(self):
        piece = {"local_speaker": 1, "raw_channel_support_sec": {},
                 "raw_top2_frame_count": {"2": 10}, "query_frame_count": 10}
        result = MODULE.decide_piece(
            piece, self.evidence(), self.evidence(),
            [{"entry": {"speaker_id": "spk_1"}}],
            ["spk_1", "spk_2", "spk_3"],
            {0: "spk_1", 1: "spk_2", 2: "spk_3"}, self.policy)
        self.assertTrue(result["accepted"])

    def test_policy_requires_inherited_query_bounds(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "config.toml"
            text = (ROOT / "specs" / "013-industrial-closing-validation" /
                    "speaker-v21-bounded-local-run-voiceprint.toml").read_text(
                        encoding="ascii")
            path.write_text(text.replace(
                "maximum_query_sec = 3.0", "maximum_query_sec = 2.9"),
                encoding="ascii")
            with self.assertRaisesRegex(ValueError, "maximum differs"):
                MODULE.load_policy(path)

    def test_production_window_policy_preserves_exact_toml_parity(self):
        policy = MODULE.load_policy(
            ROOT / "specs" / "013-industrial-closing-validation" /
            "speaker-v21-production-window-local-run.toml")
        self.assertEqual(policy["maximum_phrase_sec"], 10.0)
        self.assertEqual(policy["maximum_query_sec"], 10.0)


if __name__ == "__main__":
    unittest.main()
