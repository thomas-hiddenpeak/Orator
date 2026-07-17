#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[3]
TOOLS = ROOT / "tools" / "verify" / "py"
sys.path.insert(0, str(TOOLS))
MODULE_PATH = TOOLS / "speaker_complete_edge_contribution_candidate.py"
SPEC = importlib.util.spec_from_file_location("complete_edge", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class CompleteEdgeContributionCandidateTest(unittest.TestCase):
    def setUp(self):
        self.source = "旧句，啊，新句。"
        self.run = {
            "start": 1.0, "end": 2.0, "local_speaker": 2,
            "edge": "end", "frame_start": 10, "frame_end": 20,
        }
        self.metadata = {
            "kind": "orator_vad_active_edge_spans",
            "asr": {"0": {"start": 0.0, "end": 3.0,
                           "text": self.source}},
            "align": {"0": [
                {"start": 0.1, "end": 0.2, "text": "旧"},
                {"start": 0.2, "end": 0.3, "text": "句"},
                {"start": 1.1, "end": 1.2, "text": "啊"},
                {"start": 1.3, "end": 1.4, "text": "新"},
                {"start": 1.4, "end": 1.5, "text": "句"},
            ]},
            "active_edge_runs": [self.run],
            "pieces": [{
                "active_edge_run_id": "vad_active_edge:0",
                "evidence_id": "vad_active_edge_piece:9",
                **self.run,
            }],
        }
        self.baseline = {"track": [{
            "text_id": 0, "text": self.source, "speaker_id": "spk_3",
        }]}
        self.policy = {"punctuation": "，。？！；：、,.?!;:"}

    def test_merges_adjacent_complete_clauses_inside_edge_run(self):
        pieces = MODULE.enumerate_pieces(
            self.metadata, self.baseline, self.policy)
        self.assertEqual(len(pieces), 1)
        self.assertEqual(pieces[0]["text"], "啊，新句。")
        self.assertEqual(pieces[0]["complete_clause_count"], 2)
        self.assertEqual(
            pieces[0]["source_evidence_id"], "vad_active_edge_piece:9")

    def test_missing_character_prevents_partial_clause_projection(self):
        self.metadata["align"]["0"].pop()
        pieces = MODULE.enumerate_pieces(
            self.metadata, self.baseline, self.policy)
        self.assertEqual(len(pieces), 1)
        self.assertEqual(pieces[0]["text"], "啊，")

    def test_out_of_run_character_prevents_clause_projection(self):
        self.metadata["align"]["0"][-1]["end"] = 2.1
        pieces = MODULE.enumerate_pieces(
            self.metadata, self.baseline, self.policy)
        self.assertEqual(len(pieces), 1)
        self.assertEqual(pieces[0]["text"], "啊，")

    def test_rejects_initial_edge_run(self):
        self.metadata["active_edge_runs"][0]["edge"] = "start"
        self.assertEqual(MODULE.enumerate_pieces(
            self.metadata, self.baseline, self.policy), [])

    def test_rejects_mixed_baseline_identity_group(self):
        self.baseline["track"] = [
            {"text_id": 0, "text": "旧句，啊，", "speaker_id": "spk_3"},
            {"text_id": 0, "text": "新句。", "speaker_id": "spk_1"},
        ]
        pieces = MODULE.enumerate_pieces(
            self.metadata, self.baseline, self.policy)
        self.assertEqual(pieces, [])

    def test_remaps_only_exact_source_duration(self):
        metadata = {"pieces": [{
            "evidence_id": "complete:0",
            "source_evidence_id": "source:0",
            "start": 1.0, "end": 2.0,
        }]}
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "evidence.tsv"
            path.write_text(
                "evidence_id\tstart_sec\tend_sec\tduration_sec\tstatus\t"
                "score_spk_1\tscore_spk_2\n"
                "source:0\t1.0\t2.0\t1.0\tok\t0.7\t0.2\n",
                encoding="ascii")
            evidence = MODULE.remap_evidence(path, metadata)
            self.assertIn("complete:0", evidence)
            metadata["pieces"][0]["end"] = 2.1
            with self.assertRaisesRegex(ValueError, "duration differs"):
                MODULE.remap_evidence(path, metadata)

    def test_policy_keeps_inherited_active_edge_contracts(self):
        policy = MODULE.load_policy(
            ROOT / "specs" / "013-industrial-closing-validation" /
            "speaker-v21-complete-edge-contribution.toml")
        self.assertEqual(policy["minimum_run_sec"], 0.4)
        self.assertEqual(policy["frame_activity_threshold"], 0.5)
        self.assertTrue(policy["complete_edge_contribution"][
            "require_terminal_edge_run"])


if __name__ == "__main__":
    unittest.main()
