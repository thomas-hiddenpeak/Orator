#!/usr/bin/env python3

import importlib.util
import pathlib
import tempfile
import unittest


REPO = pathlib.Path(__file__).resolve().parents[3]
MODULE_PATH = REPO / "tools" / "verify" / "py" / "spec015_freeze_guard.py"
SPEC = importlib.util.spec_from_file_location(
    "spec015_freeze_guard", MODULE_PATH)
freeze_guard = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(freeze_guard)


class Spec015FreezeGuardTest(unittest.TestCase):
    def make_repo(self, root):
        (root / "frozen").mkdir()
        (root / "models").mkdir()
        (root / "frozen" / "source.cc").write_text(
            "int frozen = 1;\n", encoding="utf-8")
        (root / "models" / "weight.bin").write_bytes(b"weights")
        (root / "orator.toml").write_text(
            "[speaker]\nenable = true\n\n[vad]\nthreshold = 0.5\n",
            encoding="utf-8")

    def build(self, root):
        return freeze_guard.build_manifest(
            root,
            "baseline",
            source_paths=("frozen/source.cc",),
            model_paths=("models/weight.bin",),
            config_sections=("speaker", "vad"))

    def test_unchanged_control_passes(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = pathlib.Path(temp_dir)
            self.make_repo(root)
            manifest = self.build(root)
            self.assertEqual(
                freeze_guard.verify_manifest(
                    manifest, root, include_models=True),
                [])

    def test_source_change_fails(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = pathlib.Path(temp_dir)
            self.make_repo(root)
            manifest = self.build(root)
            (root / "frozen" / "source.cc").write_text(
                "int frozen = 2;\n", encoding="utf-8")
            errors = freeze_guard.verify_manifest(manifest, root)
            self.assertTrue(any("source" in error for error in errors))

    def test_config_change_fails(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = pathlib.Path(temp_dir)
            self.make_repo(root)
            manifest = self.build(root)
            (root / "orator.toml").write_text(
                "[speaker]\nenable = false\n\n[vad]\nthreshold = 0.5\n",
                encoding="utf-8")
            errors = freeze_guard.verify_manifest(manifest, root)
            self.assertIn("one or more frozen TOML sections changed", errors)

    def test_model_hash_is_optional_for_fast_guard(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = pathlib.Path(temp_dir)
            self.make_repo(root)
            manifest = self.build(root)
            (root / "models" / "weight.bin").write_bytes(b"changed")
            self.assertEqual(freeze_guard.verify_manifest(manifest, root), [])
            errors = freeze_guard.verify_manifest(
                manifest, root, include_models=True)
            self.assertTrue(any("model" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
