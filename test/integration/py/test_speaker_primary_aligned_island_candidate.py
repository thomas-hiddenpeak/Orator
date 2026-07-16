#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import tempfile
import unittest


REPO = pathlib.Path(__file__).resolve().parents[3]
TOOLS = REPO / "tools" / "verify" / "py"
sys.path.insert(0, str(TOOLS))
MODULE_PATH = TOOLS / "speaker_primary_aligned_island_candidate.py"
SPEC = importlib.util.spec_from_file_location(
    "speaker_primary_aligned_island_candidate", MODULE_PATH)
island = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = island
SPEC.loader.exec_module(island)


class SpeakerPrimaryAlignedIslandCandidateTest(unittest.TestCase):
    def policy(self):
        return {
            "minimum_probability": 0.5,
            "minimum_run_sec": 0.4,
            "minimum_activity_support_sec": 0.4,
            "protected_decision_reasons": ["reviewed_repair"],
            "short_max_sec": 1.5,
            "short_min_score": 0.0,
            "short_min_margin": 0.04,
            "regular_min_score": 0.55,
            "regular_min_margin": 0.04,
        }

    def fixture(self, reason="baseline", baseline_id="spk_1"):
        baseline = {
            "kind": "orator_frozen_speaker_candidate",
            "audio_sec": 2.0,
            "sample_rate": 16000,
            "active_speaker_ids": ["spk_1", "spk_2"],
            "track": [{
                "text_id": 0,
                "text": "甲乙丙丁",
                "start": 0.0,
                "end": 1.6,
                "speaker": 0 if baseline_id == "spk_1" else 1,
                "speaker_id": baseline_id,
                "speaker_uncertain": False,
                "decision_reason": reason,
            }],
        }
        metadata = {
            "kind": "orator_punctuation_phrase_spans",
            "asr": {"0": {"text": "甲乙丙丁"}},
            "align": {"0": [
                {"text": "甲", "start": 0.0, "end": 0.4},
                {"text": "乙", "start": 0.4, "end": 0.8},
                {"text": "丙", "start": 0.8, "end": 0.8},
                {"text": "丁", "start": 1.2, "end": 1.6},
            ]},
        }
        primary = [{
            "evidence_id": "primary_top1:0",
            "start": 0.4,
            "end": 0.9,
            "local": 1,
            "confidence": 0.8,
            "speaker_id": "spk_2",
        }]
        activity = [{
            "evidence_id": None,
            "start": 0.4,
            "end": 0.9,
            "local": 1,
            "confidence": 0.7,
            "speaker_id": "spk_2",
        }]
        voiceprint = {"primary_top1:0": {
            "start": 0.4,
            "end": 0.9,
            "duration_sec": 0.5,
            "status": "ok",
            "scores": {"spk_1": 0.1, "spk_2": 0.8},
        }}
        mapping = {"mapping": {"0": "spk_1", "1": "spk_2"}}
        return baseline, metadata, primary, activity, voiceprint, mapping

    def build(self, reason="baseline", baseline_id="spk_1"):
        values = self.fixture(reason, baseline_id)
        return island.build_candidate(*values, self.policy())

    def test_projects_complete_positive_and_zero_duration_units(self):
        result = self.build()
        self.assertEqual(result["island_count"], 1)
        self.assertEqual(result["query_spans"], [{
            "evidence_id": "primary_aligned_island_query:0",
            "primary_evidence_id": "primary_top1:0",
            "start": 0.4,
            "end": 0.8,
        }])
        self.assertEqual("".join(item["text"] for item in result["track"]),
                         "甲乙丙丁")
        self.assertEqual(
            [(item["text"], item["speaker_id"], item["decision_reason"])
             for item in result["track"]],
            [
                ("甲", "spk_1", "baseline"),
                ("乙丙", "spk_2", island.DECISION_REASON),
                ("丁", "spk_1", "baseline"),
            ])

    def test_abstains_when_voiceprint_disagrees(self):
        values = list(self.fixture())
        values[4]["primary_top1:0"]["scores"] = {
            "spk_1": 0.8, "spk_2": 0.1,
        }
        result = island.build_candidate(*values, self.policy())
        self.assertEqual(result["island_count"], 0)

    def test_abstains_below_activity_floor(self):
        values = list(self.fixture())
        values[3][0]["end"] = 0.79
        result = island.build_candidate(*values, self.policy())
        self.assertEqual(result["island_count"], 0)

    def test_preserves_reviewed_overlay_and_baseline_agreement(self):
        self.assertEqual(self.build(reason="reviewed_repair")["island_count"], 0)
        self.assertEqual(self.build(baseline_id="spk_2")["island_count"], 0)

    def test_detects_overlapping_proposals_symmetrically(self):
        proposals = [
            {"text_id": 0, "source_start": 0, "source_end": 2},
            {"text_id": 0, "source_start": 1, "source_end": 3},
            {"text_id": 0, "source_start": 3, "source_end": 4},
        ]
        self.assertEqual(island.proposal_conflicts(proposals), {0, 1})

    def test_rejects_configuration_threshold_drift(self):
        with tempfile.TemporaryDirectory() as temp:
            path = pathlib.Path(temp) / "config.toml"
            path.write_text(
                "[speaker_primary_top1]\n"
                "enabled=true\nrequire_vad_support=true\n"
                "minimum_probability=0.5\nminimum_run_sec=0.4\n"
                "[speaker_fusion]\n"
                "short_max_sec=1.5\nshort_min_score=0.0\n"
                "short_min_margin=0.04\nregular_min_score=0.55\n"
                "regular_min_margin=0.04\n"
                "[primary_aligned_island]\n"
                "enabled=true\nrequire_vad_bounded_primary_run=true\n"
                "require_same_identity_activity_support=true\n"
                "require_robust_gallery_agreement=true\n"
                "require_complete_alignment_units=true\n"
                "require_uniform_known_baseline_conflict=true\n"
                "reject_conflicting_overlays=true\n"
                "minimum_activity_support_sec=0.3\n"
                "protected_decision_reasons=[\"reviewed_repair\"]\n",
                encoding="utf-8")
            with self.assertRaises(ValueError):
                island.load_policy(str(path))


if __name__ == "__main__":
    unittest.main()
