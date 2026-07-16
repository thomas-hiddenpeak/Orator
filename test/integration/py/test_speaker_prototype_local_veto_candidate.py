#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[3]
TOOLS = ROOT / "tools" / "verify" / "py"
sys.path.insert(0, str(TOOLS))
MODULE_PATH = TOOLS / "speaker_prototype_local_veto_candidate.py"
SPEC = importlib.util.spec_from_file_location("prototype_local_veto", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


POLICY = {name: True for name in MODULE.REQUIRED_POLICY}
POLICY.update({
    "active_channel_threshold": 0.5,
    "minimum_active_channel_run_sec": 0.08,
    "minimum_raw_local_run_sec": 0.4,
})


def fragment(speaker_id, anchor=True):
    return {
        "anchor": anchor,
        "decision": {
            "speaker_id": speaker_id,
            "evidence_id": "direct:0",
        },
    }


def evidence(mapped="spk_1", selected=None):
    return {
        "mapped_speaker_id": mapped,
        "speaker_id": selected,
    }


class SpeakerPrototypeLocalVetoCandidateTest(unittest.TestCase):
    def test_abstaining_current_audio_allows_initial_local_consensus(self):
        accepted, reason = MODULE.challenge_decision(
            fragment("spk_1"), fragment("spk_2"), evidence(),
            {"speaker_id": None}, POLICY)
        self.assertTrue(accepted)
        self.assertEqual(reason, "initial_prototype_local_consensus")

    def test_eligible_piece_identity_vetoes_challenge(self):
        accepted, reason = MODULE.challenge_decision(
            fragment("spk_1"), fragment("spk_2"),
            evidence(selected="spk_3"), {"speaker_id": None}, POLICY)
        self.assertFalse(accepted)
        self.assertEqual(reason, "eligible_piece_identity_veto")

    def test_eligible_phrase_identity_vetoes_challenge(self):
        accepted, reason = MODULE.challenge_decision(
            fragment("spk_1"), fragment("spk_2"), evidence(),
            {"speaker_id": "spk_3"}, POLICY)
        self.assertFalse(accepted)
        self.assertEqual(reason, "eligible_phrase_identity_veto")

    def test_raw_local_disagreement_rejects_challenge(self):
        accepted, reason = MODULE.challenge_decision(
            fragment("spk_1"), fragment("spk_2"),
            evidence(mapped="spk_4"), {"speaker_id": None}, POLICY)
        self.assertFalse(accepted)
        self.assertEqual(reason, "raw_local_identity_disagreement")

    def test_clean_gallery_and_current_piece_can_override_raw_local(self):
        initial = fragment("spk_1")
        initial["decision"]["ranked"] = [{
            "speaker_id": "spk_1",
            "score": 0.7,
            "prototype_evidence_id": "diarization:7",
        }]
        accepted, reason = MODULE.challenge_decision(
            initial, fragment("spk_2"),
            evidence(mapped="spk_4", selected="spk_1"),
            {"speaker_id": None}, POLICY)
        self.assertTrue(accepted)
        self.assertEqual(reason, "clean_gallery_current_voiceprint_consensus")

    def test_clean_gallery_current_conflict_preserves_baseline(self):
        initial = fragment("spk_1")
        initial["decision"]["ranked"] = [{
            "speaker_id": "spk_1",
            "score": 0.7,
            "prototype_evidence_id": "diarization:7",
        }]
        accepted, reason = MODULE.challenge_decision(
            initial, fragment("spk_2"),
            evidence(mapped="spk_4", selected="spk_1"),
            {"speaker_id": "spk_3"}, POLICY)
        self.assertFalse(accepted)
        self.assertEqual(reason, "eligible_phrase_identity_veto")

    def test_clean_gallery_multiscale_channel_consensus_can_use_non_top(self):
        initial = fragment("spk_1")
        initial["decision"]["ranked"] = [{
            "speaker_id": "spk_1",
            "score": 0.7,
            "prototype_evidence_id": "diarization:7",
        }]
        accepted, reason = MODULE.challenge_decision(
            initial, fragment("spk_2"), evidence(mapped="spk_4"),
            {"speaker_id": None}, POLICY,
            gallery_piece_id="spk_1", gallery_phrase_id="spk_1",
            non_top_channel_active=True)
        self.assertTrue(accepted)
        self.assertEqual(reason, "clean_gallery_non_top_channel_consensus")

    def test_non_top_consensus_requires_active_sortformer_channel(self):
        initial = fragment("spk_1")
        initial["decision"]["ranked"] = [{
            "speaker_id": "spk_1",
            "score": 0.7,
            "prototype_evidence_id": "diarization:7",
        }]
        accepted, reason = MODULE.challenge_decision(
            initial, fragment("spk_2"), evidence(mapped="spk_4"),
            {"speaker_id": None}, POLICY,
            gallery_piece_id="spk_1", gallery_phrase_id="spk_1",
            non_top_channel_active=False)
        self.assertFalse(accepted)
        self.assertEqual(reason, "raw_local_identity_disagreement")

    def test_gallery_multiscale_channel_consensus_needs_no_direct_anchor(self):
        accepted, selected, reason = MODULE.gallery_channel_decision(
            evidence(mapped="spk_1"), {"speaker_id": None},
            {"speaker_id": "spk_1"}, {"speaker_id": "spk_1"},
            {"speaker_id": "spk_1", "qualified": True}, True, POLICY)
        self.assertTrue(accepted)
        self.assertEqual(selected, "spk_1")
        self.assertEqual(reason, "gallery_multiscale_channel_consensus")

    def test_raw_local_conflict_vetoes_gallery_multiscale(self):
        accepted, selected, reason = MODULE.gallery_channel_decision(
            evidence(mapped="spk_4"), {"speaker_id": None},
            {"speaker_id": "spk_1"}, {"speaker_id": "spk_1"},
            {"speaker_id": "spk_1", "qualified": True}, True, POLICY)
        self.assertFalse(accepted)
        self.assertIsNone(selected)
        self.assertEqual(reason, "raw_local_identity_disagreement")

    def test_single_gallery_scale_may_consensus_with_local_channel(self):
        accepted, selected, reason = MODULE.gallery_channel_decision(
            evidence(mapped="spk_1"), {"speaker_id": None},
            {"speaker_id": "spk_1"}, {"speaker_id": None},
            {"speaker_id": "spk_1", "qualified": True}, True, POLICY)
        self.assertTrue(accepted)
        self.assertEqual(selected, "spk_1")
        self.assertEqual(reason, "gallery_single_scale_channel_consensus")

    def test_two_eligible_gallery_scales_must_not_disagree(self):
        accepted, selected, reason = MODULE.gallery_channel_decision(
            evidence(mapped="spk_1"), {"speaker_id": None},
            {"speaker_id": "spk_1"}, {"speaker_id": "spk_2"},
            {"speaker_id": "spk_1", "qualified": True}, True, POLICY)
        self.assertFalse(accepted)
        self.assertIsNone(selected)
        self.assertEqual(reason, "gallery_scale_identity_disagreement")

    def test_current_multiscale_local_channel_consensus(self):
        accepted, selected, reason = MODULE.current_channel_decision(
            evidence(mapped="spk_1", selected="spk_1"),
            {"speaker_id": "spk_1"},
            {"speaker_id": None}, {"speaker_id": "spk_1"},
            {"speaker_id": "spk_1", "qualified": True}, True, POLICY)
        self.assertTrue(accepted)
        self.assertEqual(selected, "spk_1")
        self.assertEqual(reason, "current_multiscale_channel_consensus")

    def test_clean_gallery_conflict_vetoes_current_multiscale(self):
        accepted, selected, reason = MODULE.current_channel_decision(
            evidence(mapped="spk_1", selected="spk_1"),
            {"speaker_id": "spk_1"},
            {"speaker_id": "spk_2"}, {"speaker_id": None},
            {"speaker_id": "spk_1", "qualified": True}, True, POLICY)
        self.assertFalse(accepted)
        self.assertIsNone(selected)
        self.assertEqual(reason, "clean_gallery_identity_veto")

    def test_current_single_scale_local_channel_consensus(self):
        accepted, selected, reason = MODULE.current_channel_decision(
            evidence(mapped="spk_1", selected=None),
            {"speaker_id": "spk_1"},
            {"speaker_id": None}, {"speaker_id": "spk_1"},
            {"speaker_id": "spk_1", "qualified": True}, True, POLICY)
        self.assertTrue(accepted)
        self.assertEqual(selected, "spk_1")
        self.assertEqual(reason, "current_single_scale_channel_consensus")

    def test_current_single_scale_requires_clean_gallery_support(self):
        accepted, selected, reason = MODULE.current_channel_decision(
            evidence(mapped="spk_1", selected=None),
            {"speaker_id": "spk_1"},
            {"speaker_id": None}, {"speaker_id": None},
            {"speaker_id": "spk_1", "qualified": True}, True, POLICY)
        self.assertFalse(accepted)
        self.assertIsNone(selected)
        self.assertEqual(reason, "clean_gallery_identity_missing")

    def test_two_current_scales_must_not_disagree(self):
        accepted, selected, reason = MODULE.current_channel_decision(
            evidence(mapped="spk_1", selected="spk_1"),
            {"speaker_id": "spk_2"},
            {"speaker_id": None}, {"speaker_id": None},
            {"speaker_id": "spk_1", "qualified": True}, True, POLICY)
        self.assertFalse(accepted)
        self.assertIsNone(selected)
        self.assertEqual(reason, "current_scale_identity_disagreement")

    def test_sustained_raw_local_can_restore_voiceprint_abstention(self):
        accepted, selected, reason = MODULE.raw_local_decision(
            evidence(mapped="spk_1", selected=None),
            {"speaker_id": None}, {"speaker_id": None},
            {"speaker_id": None},
            {"speaker_id": "spk_1", "longest_run_sec": 0.4},
            True, POLICY)
        self.assertTrue(accepted)
        self.assertEqual(selected, "spk_1")
        self.assertEqual(reason, "sustained_raw_local_consensus")

    def test_voiceprint_conflict_vetoes_sustained_raw_local(self):
        accepted, selected, reason = MODULE.raw_local_decision(
            evidence(mapped="spk_1", selected="spk_2"),
            {"speaker_id": None}, {"speaker_id": None},
            {"speaker_id": None},
            {"speaker_id": "spk_1", "longest_run_sec": 0.8},
            True, POLICY)
        self.assertFalse(accepted)
        self.assertIsNone(selected)
        self.assertEqual(reason, "eligible_voiceprint_identity_veto")

    def test_dual_registry_multiscale_consensus(self):
        accepted, selected, reason = MODULE.dual_registry_decision(
            evidence(mapped="spk_4", selected="spk_1"),
            {"speaker_id": "spk_1"},
            {"speaker_id": "spk_1"}, {"speaker_id": "spk_1"},
            True, POLICY)
        self.assertTrue(accepted)
        self.assertEqual(selected, "spk_1")
        self.assertEqual(reason, "dual_registry_multiscale_consensus")

    def test_dual_registry_requires_all_four_voiceprint_views(self):
        accepted, selected, reason = MODULE.dual_registry_decision(
            evidence(mapped="spk_4", selected="spk_1"),
            {"speaker_id": None},
            {"speaker_id": "spk_1"}, {"speaker_id": "spk_1"},
            True, POLICY)
        self.assertFalse(accepted)
        self.assertIsNone(selected)
        self.assertEqual(reason, "dual_registry_voiceprint_abstention")

    def test_optional_candidate_switch_may_be_disabled(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "policy.toml"
            lines = ["[prototype_local_veto]"]
            for name in MODULE.REQUIRED_POLICY:
                value = "false" if name.startswith("allow_") else "true"
                lines.append(f"{name} = {value}")
            lines.extend([
                "active_channel_threshold = 0.5",
                "minimum_active_channel_run_sec = 0.08",
                "minimum_raw_local_run_sec = 0.4",
            ])
            path.write_text("\n".join(lines) + "\n", encoding="ascii")
            loaded = MODULE.load_policy(path)
            self.assertFalse(loaded[
                "allow_dual_registry_multiscale_consensus"])

    def test_dominant_micro_requires_every_native_frame(self):
        value = {
            "mapped_speaker_id": "spk_1",
            "local_speaker": 0,
            "frame_start": 1,
            "frame_end": 3,
            "frame_count": 2,
        }
        frames = [
            {"frame": 0, "top1": 1, "active_count": 1,
             "channels": [0.1, 0.8]},
            {"frame": 1, "top1": 0, "active_count": 1,
             "channels": [0.9, 0.1]},
            {"frame": 2, "top1": 0, "active_count": 1,
             "channels": [0.8, 0.1]},
        ]
        accepted, selected, reason = MODULE.dominant_micro_decision(
            value, frames, {"spk_1": 0}, True, POLICY)
        self.assertTrue(accepted)
        self.assertEqual(selected, "spk_1")
        self.assertEqual(reason, "dominant_raw_micro_consensus")

    def test_multichannel_native_frame_vetoes_dominant_micro(self):
        value = {
            "mapped_speaker_id": "spk_1",
            "local_speaker": 0,
            "frame_start": 0,
            "frame_end": 1,
            "frame_count": 1,
        }
        frames = [{"frame": 0, "top1": 0, "active_count": 2,
                   "channels": [0.9, 0.7]}]
        accepted, selected, reason = MODULE.dominant_micro_decision(
            value, frames, {"spk_1": 0}, True, POLICY)
        self.assertFalse(accepted)
        self.assertIsNone(selected)
        self.assertEqual(reason, "micro_native_frame_not_dominant")

    def test_current_identity_conflict_vetoes_gallery_multiscale(self):
        accepted, selected, reason = MODULE.gallery_channel_decision(
            evidence(mapped="spk_1", selected="spk_2"),
            {"speaker_id": None},
            {"speaker_id": "spk_1"}, {"speaker_id": "spk_1"},
            {"speaker_id": "spk_1", "qualified": True}, True, POLICY)
        self.assertFalse(accepted)
        self.assertIsNone(selected)
        self.assertEqual(reason, "eligible_piece_identity_veto")

    def test_ineligible_initial_direct_identity_is_not_evidence(self):
        accepted, reason = MODULE.challenge_decision(
            fragment("spk_1", anchor=False), fragment("spk_2"), evidence(),
            {"speaker_id": None}, POLICY)
        self.assertFalse(accepted)
        self.assertEqual(reason, "initial_direct_ineligible")

    def test_cross_prototype_margin_conflict_withdraws_terminal_override(self):
        reduced = fragment("spk_1", anchor=False)
        reduced["decision"].update({
            "reason": "baseline_regular_margin_below_gate",
            "ranked": [
                {"speaker_id": "spk_2", "score": 0.58},
                {"speaker_id": "spk_1", "score": 0.55},
            ],
        })
        terminal = fragment("spk_2")
        terminal["decision"].update({
            "baseline_speaker_id": "spk_1",
            "changed": True,
        })
        accepted, reason = MODULE.margin_veto_decision(
            reduced, terminal, evidence(mapped="spk_1"),
            {"speaker_id": None}, POLICY)
        self.assertTrue(accepted)
        self.assertEqual(reason, "cross_prototype_margin_veto")

    def test_current_phrase_vetoes_cross_prototype_restoration(self):
        reduced = fragment("spk_1", anchor=False)
        reduced["decision"].update({
            "reason": "baseline_short_margin_below_gate",
            "ranked": [
                {"speaker_id": "spk_2", "score": 0.40},
                {"speaker_id": "spk_1", "score": 0.38},
            ],
        })
        terminal = fragment("spk_2")
        terminal["decision"].update({
            "baseline_speaker_id": "spk_1",
            "changed": True,
        })
        accepted, reason = MODULE.margin_veto_decision(
            reduced, terminal, evidence(mapped="spk_1"),
            {"speaker_id": "spk_3"}, POLICY)
        self.assertFalse(accepted)
        self.assertEqual(reason, "eligible_phrase_identity_veto")

    def test_policy_requires_every_frozen_contract(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "policy.toml"
            lines = ["[prototype_local_veto]"]
            lines.extend(f"{name} = true" for name in MODULE.REQUIRED_POLICY)
            lines.extend([
                "active_channel_threshold = 0.5",
                "minimum_active_channel_run_sec = 0.08",
                "minimum_raw_local_run_sec = 0.4",
            ])
            path.write_text("\n".join(lines) + "\n", encoding="ascii")
            self.assertEqual(MODULE.load_policy(path), POLICY)
            path.write_text(
                "[prototype_local_veto]\n"
                "require_initial_direct_anchor = false\n",
                encoding="ascii")
            with self.assertRaisesRegex(ValueError, "policy missing"):
                MODULE.load_policy(path)


if __name__ == "__main__":
    unittest.main()
