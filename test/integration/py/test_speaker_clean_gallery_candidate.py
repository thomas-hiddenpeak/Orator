#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[3]
TOOLS = ROOT / "tools" / "verify" / "py"
sys.path.insert(0, str(TOOLS))
MODULE_PATH = TOOLS / "speaker_clean_gallery_candidate.py"
SPEC = importlib.util.spec_from_file_location("speaker_clean_gallery", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


POLICY = {
    "minimum_prototype_duration_sec": 3.0,
    "minimum_diar_confidence": 0.5,
    "overlap_epsilon_sec": 0.1,
    "prototype_min_score": 0.55,
    "prototype_min_margin": 0.04,
    "max_prototypes_per_identity": 2,
    "aggregation": "maximum",
    "require_initial_terminal_agreement": True,
    "require_raw_local_identity_agreement": True,
    "require_complete_gallery": True,
    "short_max_sec": 1.5,
    "short_min_score": 0.0,
    "short_min_margin": 0.04,
    "regular_min_score": 0.55,
    "regular_min_margin": 0.04,
}


def segment(index=0, identity="spk_1", confidence=0.9, start=0.0):
    return {
        "evidence_id": f"diarization:{index}",
        "start": start,
        "end": start + 4.0,
        "local_speaker": index,
        "confidence": confidence,
        "speaker_id": identity,
    }


def scores(start=0.0, best="spk_1", second="spk_2"):
    values = {"spk_1": 0.2, "spk_2": 0.2}
    values[best] = 0.7
    values[second] = 0.3
    return {
        "start": start,
        "end": start + 4.0,
        "status": "ok",
        "embed_start": start + 0.3,
        "embed_end": start + 3.7,
        "scores": values,
    }


class SpeakerCleanGalleryCandidateTest(unittest.TestCase):
    def test_three_way_identity_agreement_accepts_clean_prototype(self):
        value = segment()
        record, reason = MODULE.prototype_record(
            value, scores(), scores(), ["spk_1", "spk_2"], POLICY, [value])
        self.assertEqual(reason, "eligible")
        self.assertEqual(record["identity"], "spk_1")

    def test_registry_disagreement_rejects_prototype(self):
        value = segment()
        record, reason = MODULE.prototype_record(
            value, scores(), scores(best="spk_2", second="spk_1"),
            ["spk_1", "spk_2"], POLICY, [value])
        self.assertIsNone(record)
        self.assertEqual(reason, "registry_identity_disagreement")

    def test_cross_local_overlap_rejects_prototype(self):
        value = segment()
        other = segment(index=1, identity="spk_2", start=2.0)
        record, reason = MODULE.prototype_record(
            value, scores(), scores(), ["spk_1", "spk_2"], POLICY,
            [value, other])
        self.assertIsNone(record)
        self.assertEqual(reason, "overlap_contaminated")

    def test_existing_quality_order_retains_highest_clean_spans(self):
        values = [
            segment(index=0, confidence=0.6, start=0.0),
            segment(index=1, confidence=0.9, start=10.0),
            segment(index=2, confidence=0.8, start=20.0),
            segment(index=3, identity="spk_2", confidence=0.9, start=30.0),
            segment(index=4, identity="spk_2", confidence=0.8, start=40.0),
        ]
        initial = {}
        terminal = {}
        for value in values:
            top = value["speaker_id"]
            other = "spk_2" if top == "spk_1" else "spk_1"
            initial[value["evidence_id"]] = scores(
                start=value["start"], best=top, second=other)
            terminal[value["evidence_id"]] = scores(
                start=value["start"], best=top, second=other)
        selected, _ = MODULE.select_prototypes(
            values, initial, terminal, ["spk_1", "spk_2"], POLICY)
        kept = [value["evidence_id"] for value in selected
                if value["identity"] == "spk_1"]
        self.assertEqual(kept, ["diarization:1", "diarization:2"])

    def test_gallery_reduces_only_within_identity(self):
        gallery = {
            "spk_1": [
                {"evidence_id": "a", "embedding": [1.0, 0.0]},
                {"evidence_id": "b", "embedding": [0.8, 0.6]},
            ],
            "spk_2": [
                {"evidence_id": "c", "embedding": [0.0, 1.0]},
            ],
        }
        scores_by_id, support = MODULE.gallery_scores([1.0, 0.0], gallery)
        self.assertAlmostEqual(scores_by_id["spk_1"], 1.0)
        self.assertAlmostEqual(scores_by_id["spk_2"], 0.0)
        self.assertEqual(support["spk_1"], "a")


if __name__ == "__main__":
    unittest.main()
