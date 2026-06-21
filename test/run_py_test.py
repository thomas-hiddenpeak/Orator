#!/usr/bin/env python3
"""Run a Python integration test with an auto-managed orator_ws server.

Usage:
    python3 run_py_test.py test_script.py

The server is started on port 18765 (non-standard) before the test and
killed unconditionally after, regardless of pass/fail.
"""

import atexit
import os
import signal
import subprocess
import sys
import time

SERVER_BIN = "./build/orator_ws"
PORT = 18765
UI_PORT = 18766
SERVER_READY_TIMEOUT = 30  # seconds


def start_server() -> subprocess.Popen:
    env = os.environ.copy()
    env["ORATOR_UI_PORT"] = str(UI_PORT)
    env["ORATOR_GPU_TELEMETRY_SEC"] = "0"
    env["ORATOR_VAD_STREAM"] = "0"

    proc = subprocess.Popen(
        [SERVER_BIN, str(PORT), "", ""],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        env=env,
    )

    def cleanup():
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()

    atexit.register(cleanup)
    return proc


def wait_for_server(timeout: int = SERVER_READY_TIMEOUT) -> bool:
    import socket

    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            with socket.create_connection(("127.0.0.1", PORT), timeout=1):
                return True
        except (ConnectionRefusedError, OSError):
            time.sleep(1)
    return False


def main() -> None:
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <test_script.py>", file=sys.stderr)
        sys.exit(1)

    test_script = sys.argv[1]
    script_dir = os.path.dirname(os.path.abspath(__file__))
    server_bin = os.path.join(os.path.dirname(script_dir), SERVER_BIN)
    test_path = os.path.join(script_dir, test_script)

    if not os.path.isfile(server_bin):
        print(f"[py-test] server not found: {server_bin}", file=sys.stderr)
        sys.exit(1)
    if not os.path.isfile(test_path):
        print(f"[py-test] test not found: {test_path}", file=sys.stderr)
        sys.exit(1)

    print(f"[py-test] starting orator_ws on port {PORT} ...")
    proc = start_server()

    if not wait_for_server():
        proc.kill()
        print(f"[py-test] server failed to start within {SERVER_READY_TIMEOUT}s", file=sys.stderr)
        sys.exit(1)

    print(f"[py-test] server ready. running {test_script} ...")
    env = os.environ.copy()
    result = subprocess.run(
        ["python3", test_path, str(PORT)],
        capture_output=True,
        text=True,
        env=env,
        cwd=os.path.dirname(script_dir),
    )

    print(result.stdout, end="")
    if result.stderr:
        print(result.stderr, file=sys.stderr, end="")

    if result.returncode != 0:
        print(f"[py-test] FAILED (exit code {result.returncode})", file=sys.stderr)
        sys.exit(result.returncode)
    else:
        print("[py-test] PASSED")


if __name__ == "__main__":
    main()
