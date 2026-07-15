#!/usr/bin/env python3
"""Start Orator and run the unified WebSocket client contract scenarios."""

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import signal
import socket
import subprocess
import sys
import time
import urllib.error
import urllib.request


def find_port_pair():
    for _ in range(100):
        first = socket.socket()
        first.bind(("127.0.0.1", 0))
        port = first.getsockname()[1]
        if port >= 65535:
            first.close()
            continue
        second = socket.socket()
        try:
            second.bind(("127.0.0.1", port + 1))
        except OSError:
            first.close()
            second.close()
            continue
        first.close()
        second.close()
        return port
    raise RuntimeError("could not reserve adjacent WebSocket and HTTP ports")


def replace_toml_value(text, key, value):
    pattern = rf"(?m)^{re.escape(key)}\s*=.*$"
    replacement = f'{key} = "{value}"' if isinstance(value, str) else f"{key} = {value}"
    updated, count = re.subn(pattern, replacement, text, count=1)
    if count != 1:
        raise RuntimeError(f"missing TOML key: {key}")
    return updated


def write_isolated_config(source, destination, root, port):
    text = source.read_text(encoding="utf-8")
    text = replace_toml_value(text, "port", port)
    text = replace_toml_value(text, "ui_port", port + 1)
    text = replace_toml_value(
        text, "registry_path", str(root / "speakers.json"))
    text = replace_toml_value(
        text, "disk_path", str(root / "storage") + "/")
    text = replace_toml_value(
        text, "session_dir", str(root / "sessions") + "/")
    destination.write_text(text, encoding="utf-8")


def wait_http(url, process, timeout_sec):
    deadline = time.monotonic() + timeout_sec
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError(f"orator_ws exited with code {process.returncode}")
        try:
            with urllib.request.urlopen(url, timeout=1) as response:
                if response.status == 200:
                    return
        except (urllib.error.URLError, TimeoutError):
            pass
        time.sleep(0.2)
    raise RuntimeError("orator_ws startup timed out")


def stop_server(process):
    if process is None or process.poll() is not None:
        return
    os.killpg(process.pid, signal.SIGINT)
    try:
        process.wait(timeout=10)
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGKILL)
        process.wait(timeout=5)


def run_client(args, port, pcm, duration, output):
    command = [
        sys.executable,
        str(args.client),
        "--pcm", str(pcm),
        "--duration", str(duration),
        "--rate", "0",
        "--port", str(port),
        "--out", str(output),
        "--config-path", str(args.artifacts / "orator.generated.toml"),
        "--server-binary", str(args.server),
        "--timeline-timeout", "120",
        "--max-total-time", "180",
    ]
    result = subprocess.run(
        command, cwd=args.repo, text=True, capture_output=True, timeout=190)
    if result.stdout:
        print(result.stdout, end="")
    if result.stderr:
        print(result.stderr, end="", file=sys.stderr)
    if result.returncode != 0:
        raise RuntimeError(
            f"unified WebSocket client failed with code {result.returncode}")


def assert_silence_contract(output):
    package = json.loads(output.read_text(encoding="utf-8"))
    tracks = {
        track.get("kind"): track.get("entries", [])
        for track in package.get("timeline", {}).get("tracks", [])
    }
    if tracks.get("asr"):
        raise AssertionError("confirmed silence produced terminal ASR entries")
    if tracks.get("business_speaker"):
        raise AssertionError(
            "confirmed silence produced terminal business-speaker entries")
    substantive_live = [
        event for event in package.get("events", [])
        if event.get("type") in ("asr_partial", "asr")
        and str(event.get("text", "")).strip()
    ]
    if substantive_live:
        raise AssertionError("confirmed silence exposed substantive live ASR text")


def has_cuda_device():
    return Path("/dev/nvidia0").exists() or Path("/dev/nvhost-gpu").exists()


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--server", type=Path, required=True)
    parser.add_argument("--client", type=Path, required=True)
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--audio", type=Path, required=True)
    parser.add_argument("--artifacts", type=Path, required=True)
    return parser.parse_args()


def main():
    args = parse_args()
    args.repo = args.repo.resolve()
    args.server = args.server.resolve()
    args.client = args.client.resolve()
    args.config = args.config.resolve()
    args.audio = args.audio.resolve()
    args.artifacts = args.artifacts.resolve()

    if not has_cuda_device():
        print("SKIP: no CUDA device node available")
        return 77
    if shutil.which("ffmpeg") is None:
        print("SKIP: ffmpeg is required to decode canonical test.mp3")
        return 77

    if args.artifacts.exists():
        shutil.rmtree(args.artifacts)
    args.artifacts.mkdir(parents=True)
    port = find_port_pair()
    generated_config = args.artifacts / "orator.generated.toml"
    write_isolated_config(args.config, generated_config, args.artifacts, port)
    silence = args.artifacts / "silence_30s.pcm"
    silence.write_bytes(bytes(30 * 16000 * 2))
    server_log_path = args.artifacts / "server.log"

    process = None
    with server_log_path.open("w", encoding="utf-8") as server_log:
        try:
            env = os.environ.copy()
            env["ORATOR_CONFIG"] = str(generated_config)
            process = subprocess.Popen(
                [str(args.server)], cwd=args.repo, env=env,
                stdout=server_log, stderr=subprocess.STDOUT,
                start_new_session=True)
            wait_http(f"http://127.0.0.1:{port + 1}/", process, 90)
            run_client(
                args, port, args.audio, 12, args.artifacts / "speech_12s.json")
            silence_output = args.artifacts / "silence_30s.json"
            run_client(args, port, silence, 30, silence_output)
            assert_silence_contract(silence_output)
        except Exception:
            server_log.flush()
            if server_log_path.exists():
                print("\n--- orator_ws log ---", file=sys.stderr)
                print(server_log_path.read_text(encoding="utf-8")[-12000:],
                      file=sys.stderr)
            raise
        finally:
            stop_server(process)

    print(f"WebSocket integration artifacts: {args.artifacts}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(1)
