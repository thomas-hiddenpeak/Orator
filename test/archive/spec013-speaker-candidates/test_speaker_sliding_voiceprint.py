#!/usr/bin/env python3
"""Unit tests for reference-free multi-scale voiceprint evidence."""

import csv
import importlib.util
import json
import pathlib
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[3]
MODULE_PATH = ROOT / "tools/verify/py/speaker_sliding_voiceprint.py"
SPEC = importlib.util.spec_from_file_location(
    "speaker_sliding_voiceprint", MODULE_PATH)
speaker_sliding_voiceprint = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(speaker_sliding_voiceprint)


def timeline_package():
    return {
        "timeline": {
            "audio_sec": 8.0,
            "sample_rate": 16000,
            "resolved_config": {
                "diarizer": {"max_speakers": 2},
                "speaker": {
                    "local_drift_competing_candidate_threshold": 0.58,
                    "local_drift_competing_candidate_margin": 0.04,
                    "local_drift_competing_threshold": 0.72,
                    "local_drift_competing_margin": 0.08,
                },
            },
            "tracks": [{
                "kind": "diarization",
                "entries": [
                    {"start": 0.0, "end": 4.0,
                     "speaker_id": "spk_1"},
                    {"start": 4.0, "end": 8.0,
                     "speaker_id": "spk_2"},
                ],
            }],
        },
    }


CONFIG = """
[sliding_voiceprint]
window_sec = [3.0, 5.0]
step_sec = 1.0
round_digits = 3
min_scale_agreement = 2
max_center_gap_sec = 2.0
min_run_sec = 2.0

[sliding_voiceprint.projection]
unknown_fill_enabled = true
known_override_enabled = true
known_override_min_rolling_sec = 2.0
known_override_min_rolling_ratio = 0.25
known_override_max_competing_rolling_sec = 0.5
candidate_override_clip_to_rolling = true
align_pause_min_sec = 0.12
boundary_snap_tolerance_sec = 1.0
boundary_snap_inward_tolerance_sec = 0.08
reject_selected_neighbor_sandwich = true
neighbor_gap_sec = 2.0
"""


class SpeakerSlidingVoiceprintTest(unittest.TestCase):
    def setUp(self):
        self.tempdir = tempfile.TemporaryDirectory()
        root = pathlib.Path(self.tempdir.name)
        self.timeline = root / "timeline.json"
        self.config = root / "candidate.toml"
        self.titanet = root / "titanet.tsv"
        self.timeline.write_text(
            json.dumps(timeline_package()), encoding="utf-8")
        self.config.write_text(CONFIG, encoding="utf-8")

    def tearDown(self):
        self.tempdir.cleanup()

    def write_titanet(self, rows):
        fields = [
            "evidence_id", "start_sec", "end_sec", "status",
            "score_spk_1", "score_spk_2", "score_spk_9",
        ]
        with self.titanet.open("w", encoding="utf-8", newline="") as output:
            writer = csv.DictWriter(output, fieldnames=fields, delimiter="\t")
            writer.writeheader()
            writer.writerows(rows)

    def test_span_generation_covers_only_full_windows(self):
        timeline = speaker_sliding_voiceprint.timeline_from(
            timeline_package())
        policy = speaker_sliding_voiceprint.read_policy(
            speaker_sliding_voiceprint.load_toml(self.config), timeline)
        spans = speaker_sliding_voiceprint.generate_spans(8.0, policy)
        self.assertEqual(len(spans), 10)
        self.assertEqual(spans[0], {
            "evidence_id": "multiscale:3.000:0",
            "start_sec": 0.0,
            "end_sec": 3.0,
        })
        self.assertEqual(spans[-1], {
            "evidence_id": "multiscale:5.000:3",
            "start_sec": 3.0,
            "end_sec": 8.0,
        })

    def test_build_uses_active_ids_and_requires_scale_agreement(self):
        rows = [
            # Centre 2.5: stale spk_9 is highest, but both active rankings
            # independently select spk_1 at the strong gate.
            {"evidence_id": "3:a", "start_sec": 1, "end_sec": 4,
             "status": "ok", "score_spk_1": .82,
             "score_spk_2": .31, "score_spk_9": .99},
            {"evidence_id": "5:a", "start_sec": 0, "end_sec": 5,
             "status": "ok", "score_spk_1": .80,
             "score_spk_2": .30, "score_spk_9": .98},
            # Centre 3.5: candidate agreement extends the same run.
            {"evidence_id": "3:b", "start_sec": 2, "end_sec": 5,
             "status": "ok", "score_spk_1": .66,
             "score_spk_2": .55, "score_spk_9": .97},
            {"evidence_id": "5:b", "start_sec": 1, "end_sec": 6,
             "status": "ok", "score_spk_1": .64,
             "score_spk_2": .55, "score_spk_9": .96},
            # Centre 4.5: each scale passes, but identities disagree.
            {"evidence_id": "3:c", "start_sec": 3, "end_sec": 6,
             "status": "ok", "score_spk_1": .80,
             "score_spk_2": .30, "score_spk_9": .95},
            {"evidence_id": "5:c", "start_sec": 2, "end_sec": 7,
             "status": "ok", "score_spk_1": .30,
             "score_spk_2": .80, "score_spk_9": .95},
        ]
        self.write_titanet(rows)

        result = speaker_sliding_voiceprint.build_evidence(
            self.timeline, self.config, self.titanet)

        self.assertEqual(result["active_speaker_ids"], ["spk_1", "spk_2"])
        self.assertEqual(result["selected_point_count"], 2)
        self.assertEqual(result["points"][0]["speaker_id"], "spk_1")
        self.assertEqual(result["points"][0]["strength"], "strong")
        self.assertEqual(result["points"][1]["strength"], "candidate")
        self.assertEqual(result["retained_run_count"], 1)
        self.assertEqual(result["runs"][0]["start_sec"], 2.0)
        self.assertEqual(result["runs"][0]["end_sec"], 4.0)
        self.assertEqual(
            result["rejected_point_counts"]["candidate_gate_or_agreement"],
            1)

    def test_missing_active_score_is_rejected(self):
        self.titanet.write_text(
            "evidence_id\tstart_sec\tend_sec\tstatus\tscore_spk_1\n"
            "3:a\t1\t4\tok\t0.8\n"
            "5:a\t0\t5\tok\t0.8\n",
            encoding="utf-8",
        )
        with self.assertRaisesRegex(ValueError, "missing active identities"):
            speaker_sliding_voiceprint.build_evidence(
                self.timeline, self.config, self.titanet)

    def test_projection_requires_direct_and_rolling_for_known_override(self):
        policy = speaker_sliding_voiceprint.read_policy(
            speaker_sliding_voiceprint.load_toml(self.config),
            speaker_sliding_voiceprint.timeline_from(timeline_package()))
        entry = {
            "start": 1.0,
            "end": 5.0,
            "speaker_id": "spk_1",
        }
        direct = {
            "status": "ok",
            "candidate": True,
            "speaker_id": "spk_2",
            "score": 0.8,
            "margin": 0.3,
        }
        accepted_runs = [{
            "start_sec": 2.0,
            "end_sec": 5.0,
            "speaker_id": "spk_2",
            "point_count": 3,
            "strong_point_count": 1,
            "score_floor": 0.7,
            "margin_floor": 0.1,
            "evidence_ids": ["a", "b"],
        }]
        speaker_id, reason, audit = (
            speaker_sliding_voiceprint.project_decision(
                entry, direct, accepted_runs, policy))
        self.assertEqual(speaker_id, "spk_2")
        self.assertEqual(reason, "candidate_direct_and_rolling_voiceprint")
        self.assertEqual(audit["selected_rolling_sec"], 3.0)

        conflicting_runs = accepted_runs + [{
            "start_sec": 1.0,
            "end_sec": 2.0,
            "speaker_id": "spk_1",
            "point_count": 1,
            "strong_point_count": 0,
            "score_floor": 0.6,
            "margin_floor": 0.1,
            "evidence_ids": ["c"],
        }]
        speaker_id, reason, _ = (
            speaker_sliding_voiceprint.project_decision(
                entry, direct, conflicting_runs, policy))
        self.assertEqual(speaker_id, "spk_1")
        self.assertEqual(reason, "baseline_competing_rolling_identity")

    def test_projection_can_fill_unknown_from_direct_voiceprint(self):
        policy = speaker_sliding_voiceprint.read_policy(
            speaker_sliding_voiceprint.load_toml(self.config),
            speaker_sliding_voiceprint.timeline_from(timeline_package()))
        entry = {"start": 0.0, "end": 4.0, "speaker_id": None}
        direct = {
            "status": "ok",
            "candidate": True,
            "speaker_id": "spk_2",
            "score": 0.7,
            "margin": 0.1,
        }
        speaker_id, reason, _ = (
            speaker_sliding_voiceprint.project_decision(
                entry, direct, [], policy))
        self.assertEqual(speaker_id, "spk_2")
        self.assertEqual(reason, "candidate_direct_voiceprint_fill")

    def test_projection_preserves_selected_neighbor_sandwich(self):
        policy = speaker_sliding_voiceprint.read_policy(
            speaker_sliding_voiceprint.load_toml(self.config),
            speaker_sliding_voiceprint.timeline_from(timeline_package()))
        entry = {"start": 2.0, "end": 4.0, "speaker_id": "spk_1"}
        direct = {
            "status": "ok",
            "candidate": True,
            "strong": True,
            "speaker_id": "spk_2",
            "score": 0.9,
            "margin": 0.4,
        }
        runs = [{
            "start_sec": 2.0,
            "end_sec": 4.0,
            "speaker_id": "spk_2",
            "point_count": 2,
            "strong_point_count": 2,
            "score_floor": 0.8,
            "margin_floor": 0.2,
            "evidence_ids": ["a", "b"],
        }]
        previous_entry = {
            "start": 0.0, "end": 2.0, "speaker_id": "spk_2"}
        next_entry = {
            "start": 4.0, "end": 6.0, "speaker_id": "spk_2"}

        speaker_id, reason, _ = speaker_sliding_voiceprint.project_decision(
            entry, direct, runs, policy,
            previous_entry=previous_entry, next_entry=next_entry)

        self.assertEqual(speaker_id, "spk_1")
        self.assertEqual(reason, "baseline_selected_neighbor_sandwich")

    def test_candidate_override_splits_only_at_alignment_pauses(self):
        policy = speaker_sliding_voiceprint.read_policy(
            speaker_sliding_voiceprint.load_toml(self.config),
            speaker_sliding_voiceprint.timeline_from(timeline_package()))
        entry = {
            "start": 0.0,
            "end": 6.0,
            "text_id": 7,
            "text": "ABC",
            "speaker": 0,
            "speaker_id": "spk_1",
            "speaker_uncertain": False,
            "confidence": 0.5,
        }
        direct = {
            "status": "ok",
            "candidate": True,
            "strong": False,
            "speaker_id": "spk_2",
            "score": 0.7,
            "margin": 0.1,
        }
        runs = [{
            "start_sec": 2.0,
            "end_sec": 4.0,
            "speaker_id": "spk_2",
            "point_count": 2,
            "strong_point_count": 0,
            "score_floor": 0.65,
            "margin_floor": 0.08,
            "evidence_ids": ["a", "b"],
        }]
        align_by_id = {7: {"units": [
            {"start": 0.0, "end": 1.8, "text": "A"},
            {"start": 2.2, "end": 3.8, "text": "B"},
            {"start": 4.2, "end": 6.0, "text": "C"},
        ]}}

        speaker_id, reason, audit = (
            speaker_sliding_voiceprint.project_decision(
                entry, direct, runs, policy, align_by_id))
        pieces = speaker_sliding_voiceprint.split_projected_entry(
            entry, speaker_id, reason, audit, direct, align_by_id)

        self.assertEqual(speaker_id, "spk_2")
        self.assertEqual(audit["override_intervals"], [[2.0, 4.0]])
        self.assertEqual(
            [(piece["start"], piece["end"], piece["speaker_id"])
             for piece in pieces],
            [(0.0, 2.0, "spk_1"),
             (2.0, 4.0, "spk_2"),
             (4.0, 6.0, "spk_1")])
        self.assertEqual([piece["text"] for piece in pieces], ["A", "B", "C"])


if __name__ == "__main__":
    unittest.main()
