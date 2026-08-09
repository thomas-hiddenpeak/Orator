#!/usr/bin/env python3

import importlib.util
import pathlib
import tempfile
import unittest
from unittest import mock


REPO = pathlib.Path(__file__).resolve().parents[3]
MODULE_PATH = REPO / "tools/reference/asr_oracle.py"
SPEC = importlib.util.spec_from_file_location("asr_oracle", MODULE_PATH)
asr_oracle = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(asr_oracle)


class AsrOracleContractTest(unittest.TestCase):
    def test_project_defaults_resolve_to_existing_inputs(self):
        self.assertEqual(asr_oracle.PROJECT_ROOT, REPO)
        self.assertTrue(asr_oracle.DEFAULT_MODEL_DIR.is_dir())
        self.assertTrue(asr_oracle.DEFAULT_AUDIO.is_file())
        self.assertTrue(asr_oracle.DEFAULT_CONFIG.is_file())
        config = asr_oracle.load_asr_config(asr_oracle.DEFAULT_CONFIG)
        self.assertEqual(config["language"], "Chinese")
        self.assertGreater(config["max_new_tokens"], 0)

    def test_reference_backend_requires_all_official_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            backend = root / "qwen_asr/core/transformers_backend"
            backend.mkdir(parents=True)
            for filename in asr_oracle.REFERENCE_FILES:
                (backend / filename).write_text("# fixture\n", encoding="utf-8")
            self.assertEqual(asr_oracle.reference_backend(root), backend)
            (backend / asr_oracle.REFERENCE_FILES[0]).unlink()
            with self.assertRaisesRegex(RuntimeError, "incomplete Qwen3-ASR"):
                asr_oracle.reference_backend(root)

    def test_reference_revision_is_pinned(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            backend = root / "qwen_asr/core/transformers_backend"
            backend.mkdir(parents=True)
            for filename in asr_oracle.REFERENCE_FILES:
                (backend / filename).write_text("# fixture\n", encoding="utf-8")
            with mock.patch.object(
                    asr_oracle, "git_revision",
                    return_value=asr_oracle.EXPECTED_REFERENCE_REVISION), \
                    mock.patch.object(
                        asr_oracle, "git_worktree_status", return_value=""):
                checked, revision = asr_oracle.verify_reference(root)
            self.assertEqual(checked, backend)
            self.assertEqual(
                revision, asr_oracle.EXPECTED_REFERENCE_REVISION)
            with mock.patch.object(
                    asr_oracle, "git_revision", return_value="wrong"), \
                    mock.patch.object(
                        asr_oracle, "git_worktree_status", return_value=""):
                with self.assertRaisesRegex(RuntimeError, "revision mismatch"):
                    asr_oracle.verify_reference(root)
            with mock.patch.object(
                    asr_oracle, "git_revision",
                    return_value=asr_oracle.EXPECTED_REFERENCE_REVISION), \
                    mock.patch.object(
                        asr_oracle, "git_worktree_status",
                        return_value=" M modeling_qwen3_asr.py"):
                with self.assertRaisesRegex(RuntimeError, "worktree is not clean"):
                    asr_oracle.verify_reference(root)


if __name__ == "__main__":
    unittest.main()
