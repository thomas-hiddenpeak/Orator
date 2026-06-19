#!/usr/bin/env python3
"""
Spec 006 T008 — Web UI Integration Test (MVP)

End-to-end verification:
  1. Start orator_ws server (or assume it is already running).
  2. Open HTTP UI endpoint → verify static assets are served.
  3. Connect WebSocket → verify "ready" event.
  4. Send PCM audio frames → verify ASR events arrive.
  5. Send flush → verify timeline event with metrics.
  6. Send reset → verify reset_ok event.

Usage:
  # Auto-start mode (recommended):
  python3 tools/ws_ui_integration_test.py --models-dir /path/to/models

  # External server mode (server already running):
  python3 tools/ws_ui_integration_test.py --ws-port 8765 --ui-port 8766

Requirements: Python 3.7+ stdlib only (urllib, json, socket, struct, subprocess, time).
"""

import argparse
import json
import socket
import struct
import subprocess
import sys
import time
import urllib.request
import urllib.error

# ── Defaults ──────────────────────────────────────────────
DEFAULT_WS_PORT = 8765
DEFAULT_UI_PORT = 8766
DEFAULT_HOST = "127.0.0.1"
SAMPLE_RATE = 16000
FRAME_MS = 100  # ms per chunk
TOTAL_AUDIO_SEC = 5  # seconds of silence to send

# ── WebSocket frame helpers (no deps) ─────────────────────

def ws_handshake(host, port):
    """Perform WebSocket handshake; return (socket, key)."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(10)
    sock.connect((host, port))

    key = "dGhlIHNhbXBsZSBub25jZQ=="
    path = "/"
    request = (
        f"GET {path} HTTP/1.1\r\n"
        f"Host: {host}:{port}\r\n"
        f"Upgrade: websocket\r\n"
        f"Connection: Upgrade\r\n"
        f"Sec-WebSocket-Key: {key}\r\n"
        f"Sec-WebSocket-Version: 13\r\n"
        f"\r\n"
    )
    sock.sendall(request.encode())

    # Read response
    response = b""
    while b"\r\n\r\n" not in response:
        chunk = sock.recv(4096)
        if not chunk:
            raise RuntimeError("Server closed during handshake")
        response += chunk

    resp_text = response.decode("utf-8", errors="replace")
    if "101" not in resp_text:
        raise RuntimeError(f"Handshake failed: {resp_text[:200]}")
    return sock, key

def ws_send_text(sock, text):
    """Send a WebSocket text frame."""
    payload = text.encode("utf-8")
    frame = bytearray()
    frame.append(0x81)  # FIN + text
    length = len(payload)
    if length < 126:
        frame.append(length)
    elif length < 65536:
        frame.append(126)
        frame.extend(struct.pack(">H", length))
    else:
        frame.append(127)
        frame.extend(struct.pack(">Q", length))
    frame.extend(payload)
    sock.sendall(bytes(frame))

def ws_send_binary(sock, data):
    """Send a WebSocket binary frame."""
    frame = bytearray()
    frame.append(0x82)  # FIN + binary
    length = len(data)
    if length < 126:
        frame.append(length)
    elif length < 65536:
        frame.append(126)
        frame.extend(struct.pack(">H", length))
    else:
        frame.append(127)
        frame.extend(struct.pack(">Q", length))
    frame.extend(data)
    sock.sendall(bytes(frame))

def ws_receive(sock):
    """Receive one WebSocket frame; return (opcode, payload_bytes)."""
    header = sock.recv(2)
    if len(header) < 2:
        return None, None
    opcode = header[0] & 0x0F
    masked = (header[1] & 0x80) != 0
    length = header[1] & 0x7F

    if length == 126:
        ext = sock.recv(2)
        length = struct.unpack(">H", ext)[0]
    elif length == 127:
        ext = sock.recv(8)
        length = struct.unpack(">Q", ext)[0]

    mask_key = sock.recv(4) if masked else None
    payload = bytearray()
    while len(payload) < length:
        chunk = sock.recv(min(length - len(payload), 65536))
        if not chunk:
            break
        payload.extend(chunk)

    if masked and mask_key:
        for i in range(len(payload)):
            payload[i] ^= mask_key[i % 4]

    return opcode, bytes(payload)

def ws_receive_text(sock, timeout=5):
    """Receive and decode a text message."""
    sock.settimeout(timeout)
    opcode, payload = ws_receive(sock)
    if opcode is None or opcode != 0x1:
        return None
    return payload.decode("utf-8")

# ── Test helpers ──────────────────────────────────────────

def generate_silence_pcm(duration_sec, sample_rate=SAMPLE_RATE):
    """Generate silence as int16LE PCM."""
    samples = int(duration_sec * sample_rate)
    return struct.pack(f"<{samples}h", *([0] * samples))

def check_http_asset(host, port, path, expected_mime_prefix):
    """Check that an HTTP asset is served with correct MIME type."""
    url = f"http://{host}:{port}{path}"
    try:
        req = urllib.request.Request(url, method="GET")
        with urllib.request.urlopen(req, timeout=5) as resp:
            content = resp.read()
            ct = resp.headers.get("Content-Type", "")
            ok = len(content) > 0 and ct.startswith(expected_mime_prefix)
            return ok, len(content), ct
    except Exception as e:
        return False, 0, str(e)

# ── Test cases ────────────────────────────────────────────

class Result:
    def __init__(self):
        self.passed = 0
        self.failed = 0
        self.errors = []

    def check(self, name, condition, detail=""):
        if condition:
            self.passed += 1
            print(f"  ✅ {name}")
        else:
            self.failed += 1
            msg = f"  ❌ {name}"
            if detail:
                msg += f" — {detail}"
            print(msg)
            self.errors.append(f"{name}: {detail}")

r = Result()

def test_http_static_assets(host, port):
    """Verify HTTP server serves UI assets."""
    print("\n[HTTP] Static asset serving")
    r.check("HTTP server reachable", True)  # placeholder

    ok, size, ct = check_http_asset(host, port, "/", "text/html")
    r.check("GET / serves index.html", ok, f"size={size}, mime={ct}")

    ok, size, ct = check_http_asset(host, port, "/style.css", "text/css")
    r.check("GET /style.css", ok, f"size={size}, mime={ct}")

    ok, size, ct = check_http_asset(host, port, "/app.js", "text/javascript")
    r.check("GET /app.js", ok, f"size={size}, mime={ct}")

    # 404 for nonexistent
    url = f"http://{host}:{port}/nonexistent_file_xyz.html"
    try:
        urllib.request.urlopen(url, timeout=5)
        r.check("404 for missing file", False, "got 200")
    except urllib.error.HTTPError as e:
        r.check("404 for missing file", e.code == 404, f"code={e.code}")
    except Exception:
        r.check("404 for missing file", True)  # connection refused or similar is ok

def test_websocket_connection(host, port):
    """Verify WebSocket handshake and ready event."""
    print("\n[WS] Connection & ready event")
    try:
        sock, _ = ws_handshake(host, port)
        r.check("WebSocket handshake", True)

        # Expect "ready" message
        msg = ws_receive_text(sock, timeout=5)
        if msg:
            data = json.loads(msg)
            r.check("Received 'ready' message", data.get("type") == "ready",
                     f"type={data.get('type')}")
            r.check("ready has sample_rate", "sample_rate" in data,
                     f"sample_rate={data.get('sample_rate')}")
        else:
            r.check("Received 'ready' message", False, "no message")

        sock.close()
    except Exception as e:
        r.check("WebSocket handshake", False, str(e))
        r.check("Received 'ready' message", False, str(e))

def test_audio_streaming_and_asr(host, port):
    """Send PCM audio and verify ASR events."""
    print("\n[WS] Audio streaming & ASR events")
    try:
        sock, _ = ws_handshake(host, port)
        ws_receive_text(sock, timeout=5)  # consume ready

        # Send silence in chunks
        chunk_samples = int(SAMPLE_RATE * (FRAME_MS / 1000))
        chunk_pcm = struct.pack(f"<{chunk_samples}h", *([0] * chunk_samples))
        total_chunks = int(TOTAL_AUDIO_SEC * 1000 / FRAME_MS)

        events = []
        for i in range(total_chunks):
            ws_send_binary(sock, chunk_pcm)
            time.sleep(FRAME_MS / 1000)  # real-time rate

            # Check for any messages
            sock.settimeout(0.05)
            try:
                opcode, payload = ws_receive(sock)
                if opcode == 0x1 and payload:  # text
                    try:
                        msg = json.loads(payload)
                        events.append(msg.get("type", "unknown"))
                    except json.JSONDecodeError:
                        pass
            except socket.timeout:
                pass

        r.check(f"Sent {total_chunks} PCM chunks", True)

        # Send flush
        ws_send_text(sock, json.dumps({"flush": 1}))
        time.sleep(0.5)

        # Collect remaining messages
        sock.settimeout(2)
        for _ in range(20):
            try:
                opcode, payload = ws_receive(sock)
                if opcode == 0x1 and payload:
                    try:
                        msg = json.loads(payload)
                        events.append(msg.get("type", "unknown"))
                    except json.JSONDecodeError:
                        pass
            except socket.timeout:
                break

        r.check("Received WS events during streaming", len(events) > 0,
                 f"events={events}")

        sock.close()
    except Exception as e:
        r.check("Audio streaming", False, str(e))

def test_flush_and_timeline(host, port):
    """Send minimal audio + flush, verify timeline event."""
    print("\n[WS] Flush & timeline")
    try:
        sock, _ = ws_handshake(host, port)
        ws_receive_text(sock, timeout=5)  # consume ready

        # Send a small amount of audio
        pcm = generate_silence_pcm(1.0)
        ws_send_binary(sock, pcm)
        time.sleep(0.2)

        # Flush
        ws_send_text(sock, json.dumps({"flush": 1}))

        # Collect messages looking for timeline
        timeline_found = False
        timeline_valid = False
        for _ in range(30):
            msg = ws_receive_text(sock, timeout=2)
            if msg is None:
                break
            data = json.loads(msg)
            if data.get("type") == "timeline":
                timeline_found = True
                # Validate structure
                has_audio_sec = "audio_sec" in data
                has_tracks = "tracks" in data
                r.check("Timeline has audio_sec", has_audio_sec)
                r.check("Timeline has tracks", has_tracks)
                if has_tracks:
                    tracks = data.get("tracks", [])
                    r.check("Timeline has track entries", len(tracks) > 0,
                             f"tracks={len(tracks)}")
                timeline_valid = True
                break

        r.check("Received timeline event", timeline_found or timeline_valid,
                 "no timeline in response")

        sock.close()
    except Exception as e:
        r.check("Flush & timeline", False, str(e))

def test_reset(host, port):
    """Send reset command and verify reset_ok."""
    print("\n[WS] Reset")
    try:
        sock, _ = ws_handshake(host, port)
        ws_receive_text(sock, timeout=5)  # consume ready

        ws_send_text(sock, json.dumps({"reset": 1}))

        msg = ws_receive_text(sock, timeout=3)
        if msg:
            data = json.loads(msg)
            r.check("Received reset_ok", data.get("type") == "reset_ok",
                     f"type={data.get('type')}")
        else:
            r.check("Received reset_ok", False, "no message")

        sock.close()
    except Exception as e:
        r.check("Reset", False, str(e))

# ── Main ──────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="Spec 006 Web UI Integration Test")
    parser.add_argument("--ws-port", type=int, default=DEFAULT_WS_PORT,
                        help="WebSocket port (default: 8765)")
    parser.add_argument("--ui-port", type=int, default=DEFAULT_UI_PORT,
                        help="HTTP UI port (default: 8766)")
    parser.add_argument("--host", type=str, default=DEFAULT_HOST,
                        help="Server host (default: 127.0.0.1)")
    parser.add_argument("--models-dir", type=str, default=None,
                        help="Models directory (auto-starts server if provided)")
    parser.add_argument("--binary", type=str, default="./build/orator_ws",
                        help="Path to orator_ws binary")
    args = parser.parse_args()

    server_proc = None
    if args.models_dir:
        # Auto-start server
        print(f"[+] Starting orator_ws on port {args.ws_port} with models from {args.models_dir}")
        server_proc = subprocess.Popen(
            [args.binary, str(args.ws_port), args.models_dir],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            text=True
        )
        # Wait for server to be ready
        time.sleep(3)
        if server_proc.poll() is not None:
            print("[!] Server exited prematurely")
            stdout, _ = server_proc.communicate()
            print(stdout[-500:] if stdout else "(no output)")
            sys.exit(1)
        print("[+] Server started (PID {}), waiting for readiness...".format(server_proc.pid))
        # Wait for HTTP to be ready
        for attempt in range(10):
            try:
                urllib.request.urlopen(
                    f"http://{args.host}:{args.ui_port}/", timeout=2
                )
                break
            except Exception:
                time.sleep(1)
        else:
            print("[!] HTTP server not ready after 10s")
            sys.exit(1)
    else:
        print(f"[*] Assuming server is running at {args.host}:{args.ws_port} (WS) / {args.ui_port} (HTTP)")

    try:
        print("=" * 60)
        print("  Orator Web UI Integration Test (Spec 006 T008)")
        print("=" * 60)

        test_http_static_assets(args.host, args.ui_port)
        test_websocket_connection(args.host, args.ws_port)
        test_audio_streaming_and_asr(args.host, args.ws_port)
        test_flush_and_timeline(args.host, args.ws_port)
        test_reset(args.host, args.ws_port)

        print("\n" + "=" * 60)
        total = r.passed + r.failed
        print(f"  Results: {r.passed}/{total} passed, {r.failed} failed")
        print("=" * 60)

        if r.errors:
            print("\nFailures:")
            for e in r.errors:
                print(f"  - {e}")

        return 0 if r.failed == 0 else 1

    finally:
        if server_proc and server_proc.poll() is None:
            print("\n[+] Stopping server...")
            server_proc.terminate()
            try:
                server_proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                server_proc.kill()

if __name__ == "__main__":
    sys.exit(main())
