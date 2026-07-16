#!/usr/bin/env python3

import csv
import importlib.util
import json
import pathlib
import sys
import tempfile
import unittest


REPO = pathlib.Path(__file__).resolve().parents[3]
MODULE_PATH = (
    REPO / "tools" / "verify" / "py" /
    "speaker_business_replay_inputs.py")
SPEC = importlib.util.spec_from_file_location(
    "speaker_business_replay_inputs", MODULE_PATH)
speaker_business_replay_inputs = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = speaker_business_replay_inputs
SPEC.loader.exec_module(speaker_business_replay_inputs)


class SpeakerBusinessReplayInputsTest(unittest.TestCase):
    def test_slot_mapping_rejects_conflict(self):
        candidate = {"sessions": {
            "0": {"slots": [{"local_speaker": 0, "speaker_id": "a"}]},
            "1": {"slots": [{"local_speaker": 0, "speaker_id": "b"}]},
        }}
        with self.assertRaises(ValueError):
            speaker_business_replay_inputs.slot_mapping(candidate)

    def test_export_preserves_utf8_and_mapping(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            timeline = root / "timeline.json"
            diar = root / "diar.csv"
            mapping = root / "mapping.json"
            timeline.write_text(json.dumps({"timeline": {
                "audio_sec": 2.0,
                "sample_rate": 16000,
                "tracks": [
                    {"kind": "asr", "entries": [{
                        "text_id": 7, "start": 0.0, "end": 1.0,
                        "text": "你好"}]},
                    {"kind": "align", "entries": [{
                        "text_id": 7, "start": 0.0, "end": 1.0,
                        "units": [{"start": 0.0, "end": 0.5,
                                   "text": "你"}]}]},
                ],
            }}, ensure_ascii=False), encoding="utf-8")
            with diar.open("w", encoding="utf-8", newline="") as output:
                writer = csv.writer(output)
                writer.writerow([
                    "start_sec", "end_sec", "duration_sec", "session",
                    "local_speaker", "confidence", "mean_margin"])
                writer.writerow([0.0, 1.0, 1.0, 0, 0, 0.9, 0.5])
            mapping.write_text(json.dumps({"sessions": {"0": {"slots": [{
                "local_speaker": 0, "speaker_id": "spk_4"}]}}}),
                               encoding="utf-8")
            manifest_path, manifest = (
                speaker_business_replay_inputs.export_inputs(
                    str(timeline), str(diar), str(mapping),
                    str(root / "replay")))
            self.assertTrue(pathlib.Path(manifest_path).exists())
            self.assertEqual(manifest["mapping"], {"0": "spk_4"})
            asr_row = pathlib.Path(
                manifest["outputs"]["asr"]["path"]).read_text(
                    encoding="ascii").splitlines()[1].split("\t")
            self.assertEqual(bytes.fromhex(asr_row[3]).decode(), "你好")


if __name__ == "__main__":
    unittest.main()
