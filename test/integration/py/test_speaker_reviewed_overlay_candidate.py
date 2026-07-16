#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import tempfile
import unittest


REPO = pathlib.Path(__file__).resolve().parents[3]
TOOLS = REPO / "tools" / "verify" / "py"
sys.path.insert(0, str(TOOLS))
MODULE_PATH = TOOLS / "speaker_reviewed_overlay_candidate.py"
SPEC = importlib.util.spec_from_file_location(
    "speaker_reviewed_overlay_candidate", MODULE_PATH)
overlay = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = overlay
SPEC.loader.exec_module(overlay)


class SpeakerReviewedOverlayCandidateTest(unittest.TestCase):
    def candidate(self, track, active=False):
        value = {
            "kind": "orator_frozen_speaker_candidate",
            "audio_sec": 4.0,
            "sample_rate": 16000,
            "track": track,
        }
        if active:
            value["active_speaker_ids"] = ["spk_1", "spk_2"]
        return value

    def entry(self, text, speaker, speaker_id, reason, start, end):
        return {
            "text_id": 0,
            "text": text,
            "speaker": speaker,
            "speaker_id": speaker_id,
            "speaker_uncertain": speaker_id is None,
            "decision_reason": reason,
            "start": start,
            "end": end,
        }

    def test_applies_only_allowlisted_exact_source_range(self):
        base = self.candidate([
            self.entry("甲乙", 0, "spk_1", "sole_diar_support", 0.0, 2.0),
            self.entry("丙丁", -1, None, "no_diar_support", 2.0, 4.0),
        ])
        retained = self.candidate([
            self.entry("甲", 1, "spk_2", "not_allowed", 0.0, 1.0),
            self.entry("乙丙", 1, "spk_2", "reviewed_reason", 1.0, 3.0),
            self.entry("丁", 0, "spk_1", "not_allowed", 3.0, 4.0),
        ], active=True)
        metadata = {
            "asr": {"0": {"text": "甲乙丙丁"}},
            "align": {"0": [
                {"text": value, "start": float(index),
                 "end": float(index + 1)}
                for index, value in enumerate("甲乙丙丁")
            ]},
        }
        result = overlay.build_candidate(
            base, retained, metadata,
            {"mapping": {"0": "spk_1", "1": "spk_2"}},
            {"enabled": True, "allowed_reasons": ["reviewed_reason"]})
        self.assertEqual("".join(item["text"] for item in result["track"]),
                         "甲乙丙丁")
        self.assertEqual(
            [(item["text"], item.get("speaker_id"),
              item["decision_reason"]) for item in result["track"]],
            [
                ("甲", "spk_1", "sole_diar_support"),
                ("乙", "spk_2", "reviewed_reason"),
                ("丙", "spk_2", "reviewed_reason"),
                ("丁", None, "no_diar_support"),
            ])
        self.assertEqual(result["overlay_count"], 1)

    def test_rejects_conflicting_overlay_ranges(self):
        with self.assertRaises(ValueError):
            overlay.validate_overlays({0: [
                {"source_start": 0, "source_end": 2},
                {"source_start": 1, "source_end": 3},
            ]})

    def test_rejects_generic_allowlist_reason(self):
        with tempfile.TemporaryDirectory() as temp:
            config = pathlib.Path(temp) / "config.toml"
            config.write_text(
                "[reviewed_speaker_overlay]\n"
                "enabled = true\n"
                "allowed_reasons = [\"baseline_direct_voiceprint\"]\n",
                encoding="utf-8")
            with self.assertRaises(ValueError):
                overlay.load_policy(str(config))


if __name__ == "__main__":
    unittest.main()
