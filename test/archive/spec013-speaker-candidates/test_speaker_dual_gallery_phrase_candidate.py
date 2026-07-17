#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[3]
TOOLS = ROOT / "tools" / "verify" / "py"
sys.path.insert(0, str(TOOLS))
MODULE_PATH = TOOLS / "speaker_dual_gallery_phrase_candidate.py"
SPEC = importlib.util.spec_from_file_location("dual_gallery_phrase", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class SpeakerDualGalleryPhraseCandidateTest(unittest.TestCase):
    def setUp(self):
        self.policy = {
            "enabled": True,
            "require_baseline_disagreement": True,
            "veto_competing_direct_anchor": True,
            "require_target_channel_top1": True,
            "reject_competing_sustained_top1": True,
            "require_known_baseline_conflict": True,
            "reject_short_only_direct_support": True,
        }
        self.voiceprint = {
            "short_max_sec": 1.5,
            "short_min_score": 0.0,
            "short_min_margin": 0.04,
            "regular_min_score": 0.55,
            "regular_min_margin": 0.04,
            "frame_activity_threshold": 0.5,
            "minimum_sustained_run_sec": 0.4,
        }
        self.phrase = {
            "evidence_id": "punctuation_phrase:0",
            "text_id": 0,
            "source_start": 0,
            "source_end": 2,
            "start": 1.0,
            "end": 2.0,
            "speaker_id": "spk_1",
            "reason": "phrase_short_voiceprint",
        }
        self.fragments = [{
            "source_start": 0,
            "source_end": 2,
            "entry": {
                "speaker_id": "spk_2",
                "decision_reason": "baseline_regular_score_below_gate",
            },
        }]
        self.id_to_local = {"spk_1": 0, "spk_2": 1}
        self.frames = [
            {"time": 1.0, "top1": 1, "channels": [0.4, 0.7]},
            {"time": 1.1, "top1": 0, "channels": [0.49, 0.4]},
            {"time": 1.2, "top1": 0, "channels": [0.7, 0.2]},
        ]
        self.frame_sec = 0.1

    def evidence(self, first="spk_1", first_score=0.7,
                 second="spk_2", second_score=0.3):
        return {
            "punctuation_phrase:0": {
                "status": "ok",
                "duration_sec": 1.0,
                "scores": {first: first_score, second: second_score},
            }
        }

    def test_accepts_exact_dual_gallery_consensus(self):
        result = MODULE.decide_phrase(
            self.phrase, self.evidence(), self.fragments,
            ["spk_1", "spk_2"], self.id_to_local, self.frames,
            self.frame_sec, self.voiceprint, self.policy)
        self.assertTrue(result["accepted"])
        self.assertEqual(result["selected_speaker_id"], "spk_1")
        self.assertEqual(result["reason"],
                         "dual_gallery_exact_phrase_consensus")

    def test_rejects_gallery_disagreement(self):
        result = MODULE.decide_phrase(
            self.phrase, self.evidence(first="spk_2", second="spk_1"),
            self.fragments, ["spk_1", "spk_2"], self.id_to_local,
            self.frames, self.frame_sec, self.voiceprint, self.policy)
        self.assertFalse(result["accepted"])
        self.assertEqual(result["reason"],
                         "dual_gallery_phrase_identity_disagreement")

    def test_rejects_clean_gallery_abstention(self):
        result = MODULE.decide_phrase(
            self.phrase,
            self.evidence(first_score=0.51, second_score=0.49),
            self.fragments, ["spk_1", "spk_2"], self.id_to_local,
            self.frames, self.frame_sec, self.voiceprint, self.policy)
        self.assertFalse(result["accepted"])
        self.assertEqual(result["reason"],
                         "dual_gallery_phrase_clean_gallery_abstention")

    def test_rejects_competing_direct_anchor(self):
        self.fragments[0]["entry"]["decision_reason"] = (
            "baseline_confirmed_regular_direct_voiceprint")
        result = MODULE.decide_phrase(
            self.phrase, self.evidence(), self.fragments,
            ["spk_1", "spk_2"], self.id_to_local, self.frames,
            self.frame_sec, self.voiceprint, self.policy)
        self.assertFalse(result["accepted"])
        self.assertEqual(result["direct_anchor_ids"], ["spk_2"])
        self.assertEqual(result["reason"],
                         "dual_gallery_phrase_competing_direct_anchor")

    def test_policy_requires_all_safety_switches(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "policy.toml"
            path.write_text(
                "[dual_gallery_phrase]\n"
                "enabled = true\n"
                "require_baseline_disagreement = false\n"
                "veto_competing_direct_anchor = true\n"
                "require_target_channel_top1 = true\n"
                "reject_competing_sustained_top1 = true\n"
                "require_known_baseline_conflict = true\n"
                "reject_short_only_direct_support = true\n",
                encoding="ascii")
            with self.assertRaisesRegex(ValueError, "safety contracts"):
                MODULE.load_policy(path)

    def test_rejects_target_channel_without_top1_frame(self):
        frames = [
            {"time": 1.0, "top1": 1, "channels": [0.4, 0.7]},
            {"time": 1.1, "top1": 1, "channels": [0.49, 0.6]},
        ]
        result = MODULE.decide_phrase(
            self.phrase, self.evidence(), self.fragments,
            ["spk_1", "spk_2"], self.id_to_local, frames, self.frame_sec,
            self.voiceprint, self.policy)
        self.assertFalse(result["accepted"])
        self.assertEqual(result["reason"],
                         "dual_gallery_phrase_target_channel_never_top1")

    def test_rejects_competing_sustained_top1_run(self):
        frames = [
            {"time": 1.0 + index * 0.1,
             "top1": 1 if index < 4 else 0,
             "channels": [0.7 if index == 4 else 0.2,
                          0.8 if index < 4 else 0.1]}
            for index in range(5)
        ]
        result = MODULE.decide_phrase(
            self.phrase, self.evidence(), self.fragments,
            ["spk_1", "spk_2"], self.id_to_local, frames, self.frame_sec,
            self.voiceprint, self.policy)
        self.assertFalse(result["accepted"])
        self.assertEqual(result["reason"],
                         "dual_gallery_phrase_competing_sustained_top1")

    def test_rejects_pure_unknown_baseline(self):
        self.fragments[0]["entry"]["speaker_id"] = None
        result = MODULE.decide_phrase(
            self.phrase, self.evidence(), self.fragments,
            ["spk_1", "spk_2"], self.id_to_local, self.frames,
            self.frame_sec, self.voiceprint, self.policy)
        self.assertFalse(result["accepted"])
        self.assertEqual(result["reason"],
                         "dual_gallery_phrase_known_conflict_missing")

    def test_rejects_short_only_direct_support(self):
        self.fragments.append({
            "source_start": 0,
            "source_end": 1,
            "entry": {
                "speaker_id": "spk_1",
                "decision_reason":
                    "baseline_confirmed_short_direct_voiceprint",
            },
        })
        result = MODULE.decide_phrase(
            self.phrase, self.evidence(), self.fragments,
            ["spk_1", "spk_2"], self.id_to_local, self.frames,
            self.frame_sec, self.voiceprint, self.policy)
        self.assertFalse(result["accepted"])
        self.assertEqual(result["reason"],
                         "dual_gallery_phrase_short_only_direct_support")

    def test_accepts_agreeing_regular_direct_support(self):
        self.fragments.append({
            "source_start": 0,
            "source_end": 1,
            "entry": {
                "speaker_id": "spk_1",
                "decision_reason":
                    "baseline_confirmed_regular_direct_voiceprint",
            },
        })
        result = MODULE.decide_phrase(
            self.phrase, self.evidence(), self.fragments,
            ["spk_1", "spk_2"], self.id_to_local, self.frames,
            self.frame_sec, self.voiceprint, self.policy)
        self.assertTrue(result["accepted"])


if __name__ == "__main__":
    unittest.main()
