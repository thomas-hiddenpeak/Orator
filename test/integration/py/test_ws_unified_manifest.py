#!/usr/bin/env python3

import importlib.util
import os
import pathlib
import socket
import subprocess
import sys
import tempfile
import unittest


REPO = pathlib.Path(__file__).resolve().parents[3]
MODULE_PATH = REPO / "tools" / "verify" / "py" / "ws_unified_test.py"
SPEC = importlib.util.spec_from_file_location(
    "ws_unified_test", MODULE_PATH)
ws_unified_test = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = ws_unified_test
SPEC.loader.exec_module(ws_unified_test)


class UnifiedManifestTest(unittest.TestCase):
    def test_command_help_is_renderable(self):
        result = subprocess.run(
            [sys.executable, str(MODULE_PATH), "--help"],
            cwd=REPO, text=True, capture_output=True, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("--require-telemetry", result.stdout)
        self.assertIn("--test-flush", result.stdout)

    def test_direct_end_timing_is_eligible_for_latency_gate(self):
        timing = ws_unified_test.terminal_timing_report(
            push_complete_at=100.0,
            flush_requested_at=None,
            flush_received_at=None,
            end_requested_at=100.1,
            end_received_at=129.9,
        )
        self.assertEqual(timing["mode"], "direct_end")
        self.assertFalse(timing["flush"]["requested"])
        self.assertEqual(timing["end"]["request_to_timeline_sec"], 29.8)
        self.assertEqual(timing["final_frame_to_terminal_sec"], 29.9)
        self.assertTrue(timing["eligible_for_terminal_latency_gate"])
        self.assertTrue(timing["within_terminal_latency_limit"])

        late = ws_unified_test.terminal_timing_report(
            push_complete_at=100.0,
            flush_requested_at=None,
            flush_received_at=None,
            end_requested_at=100.1,
            end_received_at=130.1,
        )
        self.assertTrue(late["eligible_for_terminal_latency_gate"])
        self.assertFalse(late["within_terminal_latency_limit"])

    def test_flush_primed_timing_is_not_latency_gate_evidence(self):
        timing = ws_unified_test.terminal_timing_report(
            push_complete_at=100.0,
            flush_requested_at=100.1,
            flush_received_at=125.1,
            end_requested_at=125.2,
            end_received_at=130.2,
        )
        self.assertEqual(timing["mode"], "flush_then_end")
        self.assertEqual(
            timing["flush"]["request_to_timeline_sec"], 25.0)
        self.assertEqual(timing["end"]["request_to_timeline_sec"], 5.0)
        self.assertEqual(timing["final_frame_to_terminal_sec"], 30.2)
        self.assertFalse(timing["eligible_for_terminal_latency_gate"])
        self.assertIsNone(timing["within_terminal_latency_limit"])

    def test_reader_replies_to_ping_with_masked_same_payload_pong(self):
        server_sock, client_sock = socket.socketpair()
        server_sock.settimeout(2.0)
        reader = ws_unified_test.Reader(client_sock, verbose=False)
        reader.start()
        payload = b"orator-validity"
        try:
            server_sock.sendall(bytes([0x89, len(payload)]) + payload)
            header = ws_unified_test.recvn(server_sock, 2)
            self.assertEqual(len(header), 2)
            self.assertEqual(header[0] & 0x0F, 0x0A)
            self.assertNotEqual(header[1] & 0x80, 0)
            self.assertEqual(header[1] & 0x7F, len(payload))
            mask = ws_unified_test.recvn(server_sock, 4)
            encoded = ws_unified_test.recvn(server_sock, len(payload))
            decoded = bytes(
                encoded[index] ^ mask[index & 3]
                for index in range(len(encoded)))
            self.assertEqual(decoded, payload)
            self.assertEqual(reader.messages, [])
            self.assertEqual(reader.events, [])
        finally:
            server_sock.close()
            client_sock.close()
            reader.join(timeout=2.0)

    def test_git_workspace_digest_detects_content_change_at_same_path(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            subprocess.run(["git", "init", "-q"], cwd=root, check=True)
            tracked = root / "tracked.txt"
            tracked.write_text("base\n", encoding="utf-8")
            subprocess.run(["git", "add", "tracked.txt"], cwd=root,
                           check=True)
            subprocess.run([
                "git", "-c", "user.name=Orator Test", "-c",
                "user.email=test@orator.invalid", "commit", "-qm", "base",
            ], cwd=root, check=True)
            tracked.write_text("first dirty content\n", encoding="utf-8")

            previous_cwd = os.getcwd()
            try:
                os.chdir(root)
                first = ws_unified_test.git_metadata()
                tracked.write_text("second dirty content\n", encoding="utf-8")
                second = ws_unified_test.git_metadata()
            finally:
                os.chdir(previous_cwd)

            self.assertEqual(first["status"], second["status"])
            self.assertNotEqual(
                first["workspace_sha256"], second["workspace_sha256"])

    def test_telemetry_summary_reports_coverage_and_statistics(self):
        runtime = [{
            "type": "gpu_telemetry",
            "device": {
                "gpu_utilization_pct": 75.0,
                "gpu_mem_used_mb": 2048.0,
                "gpu_mem_used_pct": 25.0,
                "gpu_freq_mhz": 900.0,
                "system_power_w": 20.0,
            },
        }]
        line = (
            "RAM 1024/8192MB CPU [10%@1000,30%@1000] "
            "GR3D_FREQ 75% cpu@42.5C gpu@45.0C "
            "VDD_GPU 5000mW VIN 20000mW")
        result = ws_unified_test.telemetry_summary(
            runtime, [{"t_sec": 1.0, "line": line}],
            duration_sec=1.0, gpu_interval_sec=1.0)
        self.assertTrue(result["required_fields_at_least_95_percent"])
        self.assertEqual(
            result["runtime"]["gpu_utilization_pct"]["mean"], 75.0)
        self.assertEqual(
            result["tegrastats"]["cpu_pct"]["mean"], 20.0)
        self.assertEqual(
            result["tegrastats"]["max_temp_c"]["max"], 45.0)

    def test_telemetry_summary_rejects_sparse_sample_cadence(self):
        runtime = [{
            "type": "gpu_telemetry",
            "device": {
                "gpu_utilization_pct": 75.0,
                "gpu_mem_used_mb": 2048.0,
                "system_power_w": 20.0,
            },
        }]
        line = "RAM 1024/8192MB CPU [10%@1000] gpu@45.0C VIN 20000mW"
        result = ws_unified_test.telemetry_summary(
            runtime, [{"line": line}], duration_sec=10.0,
            gpu_interval_sec=1.0, device_interval_sec=1.0)
        self.assertEqual(
            result["required_field_coverage"]["runtime_sample_cadence"],
            0.1)
        self.assertEqual(
            result["required_field_coverage"]["tegrastats_sample_cadence"],
            0.1)
        self.assertFalse(result["required_fields_at_least_95_percent"])

    def test_config_provenance_requires_same_path_and_content(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "orator.toml"
            path.write_text("[server]\nport = 8765\n", encoding="utf-8")
            start = ws_unified_test.config_snapshot(path)
            timeline = {
                "resolved_config": {"config_source_path": str(path)}}
            stable = ws_unified_test.config_provenance(start, timeline)
            self.assertTrue(stable["acceptance_consistent"])

            path.write_text("[server]\nport = 9000\n", encoding="utf-8")
            changed = ws_unified_test.config_provenance(start, timeline)
            self.assertFalse(changed["acceptance_consistent"])
            self.assertFalse(changed["unchanged_during_client_run"])

    def test_config_provenance_rejects_different_resolved_path(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            first = root / "first.toml"
            second = root / "second.toml"
            first.write_text("same", encoding="utf-8")
            second.write_text("same", encoding="utf-8")
            result = ws_unified_test.config_provenance(
                ws_unified_test.config_snapshot(first),
                {"resolved_config": {"config_source_path": str(second)}},
            )
            self.assertFalse(result["path_matches"])
            self.assertFalse(result["acceptance_consistent"])


if __name__ == "__main__":
    unittest.main()
