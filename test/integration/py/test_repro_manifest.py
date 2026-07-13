#!/usr/bin/env python3

import importlib.util
import json
import os
import pathlib
import tempfile
import unittest


REPO = pathlib.Path(__file__).resolve().parents[3]
MODULE_PATH = REPO / "tools" / "verify" / "py" / "repro_manifest.py"
SPEC = importlib.util.spec_from_file_location("repro_manifest", MODULE_PATH)
repro_manifest = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(repro_manifest)


class ReproManifestTest(unittest.TestCase):
    def test_directory_hash_is_stable_and_content_sensitive(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = pathlib.Path(temp_dir)
            (root / "b.bin").write_bytes(b"second")
            (root / "a.bin").write_bytes(b"first")

            first = repro_manifest.hash_path(root)
            second = repro_manifest.hash_path(root)
            self.assertEqual(first["sha256"], second["sha256"])
            self.assertEqual(
                [item["path"] for item in first["files"]],
                ["a.bin", "b.bin"])

            (root / "a.bin").write_bytes(b"changed")
            changed = repro_manifest.hash_path(root)
            self.assertNotEqual(first["sha256"], changed["sha256"])

    def test_missing_asset_is_explicit(self):
        missing = repro_manifest.hash_path("/tmp/orator-manifest-no-such-file")
        self.assertFalse(missing["exists"])

    def test_directory_digest_matches_serialized_file_ledger(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = pathlib.Path(temp_dir) / "value.json"
            path.write_text(json.dumps({"value": 3}), encoding="utf-8")
            result = repro_manifest.hash_path(temp_dir)
            self.assertEqual(result["file_count"], 1)
            self.assertEqual(result["size"], os.path.getsize(path))


if __name__ == "__main__":
    unittest.main()
