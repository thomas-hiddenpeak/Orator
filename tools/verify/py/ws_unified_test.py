#!/usr/bin/env python3
"""Unified WebSocket streaming test client for Orator project.

This is the AUTHORITATIVE and ONLY WebSocket test client for pipeline testing.
It streams PCM audio through the WebSocket transport and captures all server
responses for comparison with reference files.

Testing Principles:
1. All tests except unit tests MUST use test.mp3 as the audio source and test.txt as reference
2. Actual pipeline testing MAY use different speeds for testing (120s, 360s, 600s, full-length)
3. Test results MUST be compared item by item with the reference file by placing them in context
4. Device metrics (power, CPU, GPU, RAM) MUST be observed using tegrastats on Jetson devices
5. ASR accuracy comparison MUST use semantic comparison, not character-level comparison
6. Speaker diarization accuracy MUST provide total comparison of speaker segmentation time blocks

Supported WebSocket Interfaces:
- Binary data: PCM audio (int16 and float32 formats)
- Text commands: f32, describe, reset, flush, end, sessions, load_session
- Server responses: ready, asr, timeline, reset_ok, sessions, error responses

Usage:
  # 120s test:
  python3 tools/verify/py/ws_unified_test.py --duration 120 --port 8765 --out test_120s.json

  # 360s test:
  python3 tools/verify/py/ws_unified_test.py --duration 360 --port 8765 --out test_360s.json

  # 600s test:
  python3 tools/verify/py/ws_unified_test.py --duration 600 --port 8765 --out test_600s.json

  # Full-length test:
  python3 tools/verify/py/ws_unified_test.py --duration 3615 --port 8765 --out test_full.json

  # Different speed test (2x speed):
  python3 tools/verify/py/ws_unified_test.py --duration 3615 --port 8765 --rate 2.0 --out test_2x.json

  # Test all WebSocket interfaces:
  python3 tools/verify/py/ws_unified_test.py --duration 120 --port 8765 --out test.json \
    --test-describe --test-reset --test-sessions --test-load-session session_id

  # Parameter matrix evaluation (120s, all configs):
  python3 tools/verify/py/ws_unified_test.py --matrix-eval 120 --port 8765 --out matrix_results.json
"""

import argparse
import base64
import hashlib
import json
import os
import socket
import struct
import sys
import threading
import time

GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
SAMPLE_RATE = 16000
BYTES_PER_SAMPLE = 2  # int16 mono

# Full 3x3x3 parameter evaluation matrix for diarization
PARAM_MATRIX = [
    # (threshold, merge_gap, sil_frames)
    # --- baseline region ---
    (0.50, 0.5, 3),
    (0.50, 0.6, 3),  (0.50, 0.8, 3),  (0.50, 1.0, 3),
    (0.50, 0.6, 5),  (0.50, 0.8, 5),  (0.50, 1.0, 5),
    (0.50, 0.6, 7),  (0.50, 0.8, 7),  (0.50, 1.0, 7),
    # --- optimal region ---
    (0.40, 0.5, 3),
    (0.40, 0.6, 3),  (0.40, 0.8, 3),  (0.40, 1.0, 3),
    (0.40, 0.6, 5),  (0.40, 0.8, 5),  (0.40, 1.0, 5),
    (0.40, 0.6, 7),  (0.40, 0.8, 7),  (0.40, 1.0, 7),
    # --- aggressive region ---
    (0.35, 0.5, 3),
    (0.35, 0.6, 3),  (0.35, 0.8, 3),  (0.35, 1.0, 3),
    (0.35, 0.6, 5),  (0.35, 0.8, 5),  (0.35, 1.0, 5),
    (0.35, 0.6, 7),  (0.35, 0.8, 7),  (0.35, 1.0, 7),
]


def mask_frame(opcode: int, payload: bytes) -> bytes:
    """Create WebSocket masked frame."""
    b = bytearray()
    b.append(0x80 | opcode)
    n = len(payload)
    mask = os.urandom(4)
    if n < 126:
        b.append(0x80 | n)
    elif n <= 0xFFFF:
        b.append(0x80 | 126)
        b += struct.pack(">H", n)
    else:
        b.append(0x80 | 127)
        b += struct.pack(">Q", n)
    b += mask
    b += bytes(payload[i] ^ mask[i & 3] for i in range(n))
    return bytes(b)


def recvn(sock, n: int) -> bytes:
    """Receive exactly n bytes from socket."""
    buf = b""
    while len(buf) < n:
        r = sock.recv(n - len(buf))
        if not r:
            break
        buf += r
    return buf


def read_frame(sock):
    """Return (opcode, payload) or (None, None) on close/EOF/short read."""
    h = recvn(sock, 2)
    if len(h) < 2:
        return None, None
    opcode = h[0] & 0x0F
    n = h[1] & 0x7F
    if n == 126:
        ext = recvn(sock, 2)
        if len(ext) < 2:
            return None, None
        n = struct.unpack(">H", ext)[0]
    elif n == 127:
        ext = recvn(sock, 8)
        if len(ext) < 8:
            return None, None
        n = struct.unpack(">Q", ext)[0]
    payload = recvn(sock, n) if n else b""
    if len(payload) < n:
        return None, None
    return opcode, payload


def handshake(sock, host, port):
    """Perform WebSocket handshake."""
    key = base64.b64encode(os.urandom(16)).decode()
    req = (
        f"GET / HTTP/1.1\r\nHost: {host}:{port}\r\nUpgrade: websocket\r\n"
        f"Connection: Upgrade\r\nSec-WebSocket-Key: {key}\r\n"
        f"Sec-WebSocket-Version: 13\r\n\r\n"
    )
    sock.sendall(req.encode())
    resp = b""
    while b"\r\n\r\n" not in resp:
        chunk = sock.recv(1)
        if not chunk:
            raise RuntimeError("connection closed during handshake")
        resp += chunk
    accept = base64.b64encode(hashlib.sha1((key + GUID).encode()).digest()).decode()
    if accept.encode() not in resp:
        raise RuntimeError("handshake accept mismatch")


class Reader(threading.Thread):
    """Captures every server text frame; signals when the timeline arrives."""

    def __init__(self, sock):
        super().__init__(daemon=True)
        self.sock = sock
        self.events = []           # incremental {"type":"asr"} events
        self.timeline = None       # final {"type":"timeline"} document
        self.ready = None          # {"type":"ready"} info
        self.telemetry = []        # periodic {"type":"gpu_telemetry"/"cursor_progress"}
        self.timeline_event = threading.Event()

    def run(self):
        while True:
            try:
                op, pl = read_frame(self.sock)
            except (ConnectionResetError, BrokenPipeError, OSError):
                break
            if op is None or op == 0x8:  # EOF or CLOSE
                break
            if op != 0x1:                # only text frames carry JSON
                continue
            try:
                raw = json.loads(pl.decode("utf-8"))
                # Spec 004 envelope unwrapping
                if 'data' in raw and isinstance(raw['data'], str):
                    raw = json.loads(raw['data'])
            except (ValueError, UnicodeDecodeError, json.JSONDecodeError):
                continue
            kind = raw.get("type")
            if kind == "asr":
                self.events.append(raw)
                t = raw.get("text", "")
                print(f"  [stream] asr [{raw.get('start'):.2f}-{raw.get('end'):.2f}] {t}")
            elif kind == "align":
                self.events.append(raw)
                us = raw.get("units", [])
                preview = " ".join(
                    f"{u.get('text')}[{u.get('start'):.2f}-{u.get('end'):.2f}]"
                    for u in us[:8])
                print(f"  [stream] align [{raw.get('start'):.2f}-{raw.get('end'):.2f}] "
                      f"({len(us)} units) {preview}")
            elif kind == "timeline":
                self.timeline = raw
                self.timeline_event.set()
            elif kind == "ready":
                self.ready = raw
            elif kind in ("gpu_telemetry", "cursor_progress"):
                # Spec 011: capture the runtime's periodic telemetry samples so
                # the offline rerun exporter has the per-pipeline RTF / cursor
                # time series without a live connection.
                self.telemetry.append(raw)


def parse_audio_file(audio_path: str, duration_sec: float = None) -> bytes:
    """Read audio file, convert to PCM if needed, optionally truncate to duration."""
    import subprocess
    import os
    import tempfile
    
    # Check if audio file exists
    if not os.path.exists(audio_path):
        raise FileNotFoundError(f"Audio file not found: {audio_path}")
    
    # Check if file is MP3 or other non-PCM format
    if audio_path.endswith('.mp3') or audio_path.endswith('.wav') or audio_path.endswith('.flac'):
        # Convert to PCM using the project's dump_pcm tool or ffmpeg
        pcm_file = tempfile.NamedTemporaryFile(suffix='.pcm', delete=False).name
        try:
            # Try to use ffmpeg to convert to 16kHz mono int16 PCM
            result = subprocess.run([
                'ffmpeg', '-y', '-i', audio_path, '-ar', '16000', '-ac', '1', '-f', 's16le', pcm_file
            ], capture_output=True, text=True)
            
            if result.returncode != 0:
                print(f"Warning: ffmpeg conversion failed, trying alternative method")
                # Fallback: try to read as PCM anyway
                with open(audio_path, "rb") as f:
                    pcm = f.read()
            else:
                # Read the converted PCM file
                with open(pcm_file, "rb") as f:
                    pcm = f.read()
        finally:
            # Clean up temp file if it exists and is not the original
            if os.path.exists(pcm_file) and pcm_file != audio_path:
                try:
                    os.unlink(pcm_file)
                except:
                    pass
    else:
        # Assume it's already PCM format
        with open(audio_path, "rb") as f:
            pcm = f.read()
    
    if duration_sec is not None:
        total_samples = int(duration_sec * SAMPLE_RATE)
        max_bytes = total_samples * BYTES_PER_SAMPLE
        pcm = pcm[:max_bytes]
    
    return pcm


def collect_tegrastats_snapshot():
    """Collect Jetson device metrics using tegrastats if available."""
    import subprocess
    import shutil
    
    metrics = {}
    if shutil.which('tegrastats') is None:
        return metrics
        
    try:
        # Run tegrastats in the background, capture output, then kill it after 2.5 seconds
        proc = subprocess.Popen(
            ['tegrastats', '--interval', '1000'],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        time.sleep(2.5)  # Collect ~2-3 lines of metrics
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=1)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
        
        stdout, stderr = proc.communicate()
        if stdout:
            metrics['tegrastats_snapshot'] = stdout.strip()
    except Exception:
        pass
    return metrics


class TegraSampler(threading.Thread):
    """Continuous device telemetry (Spec 011 Phase 2, FR5).

    Spawns `tegrastats` for the streamed duration and records each raw line
    tagged with elapsed seconds from a shared `t0` (the audio producer's t0), so
    device samples align to the audio timeline at rate=1. Raw lines are stored
    and parsed offline by the exporter (single parsing source of truth). This is
    tools-side only; the C++/CUDA runtime gains no dependency (Constitution
    Art. I). On Jetson the only continuous device source is `tegrastats`
    (`nvidia-smi` is incomplete — Constitution Art. note).
    """

    def __init__(self, t0: float, interval_ms: int = 1000):
        super().__init__(daemon=True)
        self.t0 = t0
        self.interval_ms = interval_ms
        self.samples = []          # [{"t_sec": float, "line": str}]
        self._proc = None
        self._stop_event = threading.Event()

    def run(self):
        import subprocess
        import shutil
        if shutil.which("tegrastats") is None:
            return
        try:
            self._proc = subprocess.Popen(
                ["tegrastats", "--interval", str(self.interval_ms)],
                stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True)
        except (OSError, ValueError):
            return
        for line in self._proc.stdout:
            if self._stop_event.is_set():
                break
            line = line.strip()
            if line:
                self.samples.append({"t_sec": round(time.monotonic() - self.t0, 3),
                                     "line": line})

    def stop(self):
        self._stop_event.set()
        if self._proc and self._proc.poll() is None:
            self._proc.terminate()
            try:
                self._proc.wait(timeout=1)
            except Exception:
                try:
                    self._proc.kill()
                except Exception:
                    pass
        self.join(timeout=2.0)


def main(args):
    # Parse audio file (handles MP3, WAV, FLAC, and PCM formats)
    pcm = parse_audio_file(args.pcm, args.duration)
    total_samples = len(pcm) // BYTES_PER_SAMPLE
    audio_sec = total_samples / SAMPLE_RATE
    frame_bytes = int(SAMPLE_RATE * args.frame_ms / 1000) * BYTES_PER_SAMPLE

    sock = socket.create_connection((args.host, args.port))
    handshake(sock, args.host, args.port)
    reader = Reader(sock)
    reader.start()
    
    # Give the server a moment to send "ready"
    time.sleep(0.5)
    print(f"connected; streaming {audio_sec:.2f}s of audio "
          f"({'max rate' if args.rate == 0 else str(args.rate) + 'x'}, "
          f"{args.frame_ms}ms frames)")
    
    # Test describe command if requested
    if args.test_describe:
        print("  [test] sending describe command...")
        sock.sendall(mask_frame(0x1, b'{"describe":true}'))
        # Read response
        op, pl = read_frame(sock)
        if op == 0x1 and pl:
            try:
                raw = json.loads(pl.decode("utf-8"))
                # Spec 004 envelope unwrapping
                if 'data' in raw and isinstance(raw['data'], str):
                    raw = json.loads(raw['data'])
                print(f"  [test] describe response: {json.dumps(raw, indent=2)}")
            except (ValueError, UnicodeDecodeError, json.JSONDecodeError):
                print(f"  [test] describe response (raw): {pl.decode('utf-8', errors='replace')}")
        else:
            print("  [test] no describe response received")
    
    # Test reset command if requested
    if args.test_reset:
        print("  [test] sending reset command...")
        sock.sendall(mask_frame(0x1, b'{"reset":true}'))
        # Read response
        op, pl = read_frame(sock)
        if op == 0x1 and pl:
            try:
                raw = json.loads(pl.decode("utf-8"))
                # Spec 004 envelope unwrapping
                if 'data' in raw and isinstance(raw['data'], str):
                    raw = json.loads(raw['data'])
                print(f"  [test] reset response: {json.dumps(raw, indent=2)}")
            except (ValueError, UnicodeDecodeError, json.JSONDecodeError):
                print(f"  [test] reset response (raw): {pl.decode('utf-8', errors='replace')}")
        else:
            print("  [test] no reset response received")
    
    # Test sessions command if requested
    if args.test_sessions:
        print("  [test] sending sessions command...")
        sock.sendall(mask_frame(0x1, b'{"sessions":true}'))
        # Read response
        op, pl = read_frame(sock)
        if op == 0x1 and pl:
            try:
                raw = json.loads(pl.decode("utf-8"))
                # Spec 004 envelope unwrapping
                if 'data' in raw and isinstance(raw['data'], str):
                    raw = json.loads(raw['data'])
                print(f"  [test] sessions response: {json.dumps(raw, indent=2)}")
            except (ValueError, UnicodeDecodeError, json.JSONDecodeError):
                print(f"  [test] sessions response (raw): {pl.decode('utf-8', errors='replace')}")
        else:
            print("  [test] no sessions response received")
    
    # Test load_session command if requested
    if args.test_load_session:
        session_id = args.test_load_session
        print(f"  [test] sending load_session command for session: {session_id}...")
        load_cmd = f'{{"load_session":"{session_id}"}}'
        sock.sendall(mask_frame(0x1, load_cmd.encode('utf-8')))
        # Read response
        op, pl = read_frame(sock)
        if op == 0x1 and pl:
            try:
                raw = json.loads(pl.decode("utf-8"))
                # Spec 004 envelope unwrapping
                if 'data' in raw and isinstance(raw['data'], str):
                    raw = json.loads(raw['data'])
                print(f"  [test] load_session response: {json.dumps(raw, indent=2)}")
            except (ValueError, UnicodeDecodeError, json.JSONDecodeError):
                print(f"  [test] load_session response (raw): {pl.decode('utf-8', errors='replace')}")
        else:
            print("  [test] no load_session response received")

    # Collect initial tegrastats snapshot before streaming
    tegrastats_before = collect_tegrastats_snapshot()
    
    # Producer: push PCM frames through the socket
    t0 = time.monotonic()
    # Continuous device telemetry for the streamed duration (Spec 011 Phase 2).
    tegra_sampler = TegraSampler(t0, interval_ms=1000)
    tegra_sampler.start()
    sent = 0
    for off in range(0, len(pcm), frame_bytes):
        sock.sendall(mask_frame(0x2, pcm[off:off + frame_bytes]))
        sent += 1
        if args.rate > 0:
            target_wall = (sent * args.frame_ms / 1000.0) / args.rate
            slack = target_wall - (time.monotonic() - t0)
            if slack > 0:
                time.sleep(slack)
    push_wall = time.monotonic() - t0
    
    # Collect tegrastats snapshot after streaming
    tegrastats_after = collect_tegrastats_snapshot()

    # Flush: send flush command and wait for the timeline document
    sock.sendall(mask_frame(0x1, b'{"flush":true}'))
    print("  [flush] waiting for timeline...")
    got_flush = reader.timeline_event.wait(timeout=args.timeline_timeout)
    if got_flush and reader.timeline is not None:
        print(f"  [flush] timeline received ({reader.timeline.get('audio_sec', '?')}s)")
    else:
        print("  [flush] no timeline (continuing to end)")

    # End: send end command and wait for the final timeline document
    reader.timeline_event.clear()
    sock.sendall(mask_frame(0x1, b'{"end":true}'))
    print("  [end] waiting for final timeline...")
    got_end = reader.timeline_event.wait(timeout=args.timeline_timeout)
    total_wall = time.monotonic() - t0
    sock.sendall(mask_frame(0x8, b"\x03\xe8"))  # CLOSE
    sock.close()
    
    # Join the reader thread
    reader.join(timeout=2.0)

    if not got_end or reader.timeline is None:
        print(f"ERROR: no final timeline received within {args.timeline_timeout:.1f}s",
              file=sys.stderr)
        print("\n=== TEST_SCRIPT_COMPLETED_WITH_ERRORS ===")
        return 1

    tl = reader.timeline
    tracks = {t.get("kind"): t for t in tl.get("tracks", [])}
    diar = tracks.get("diarization", {})
    asr = tracks.get("asr", {})
    diar_compute = diar.get("compute_sec")
    asr_compute = asr.get("compute_sec")
    
    # Collect final tegrastats snapshot after timeline
    tegrastats_final = collect_tegrastats_snapshot()
    # Stop the continuous device sampler and harvest its time series.
    tegra_sampler.stop()
    device_series = tegra_sampler.samples
    
    out = {
        "meta": {
            "pcm": args.pcm,
            "duration_sec": args.duration,
            "audio_sec": audio_sec,
            "rate_requested": ("max" if args.rate == 0 else args.rate),
            "frame_ms": args.frame_ms,
            "push_wall_sec": round(push_wall, 3),
            "total_wall_sec": round(total_wall, 3),
            "stream_rt_factor": round(audio_sec / total_wall, 3) if total_wall else None,
            "diar_compute_sec": diar_compute,
            "asr_compute_sec": asr_compute,
            "diar_rt_factor": diar.get("real_time_factor"),
            "asr_rt_factor": asr.get("real_time_factor"),
        },
        "tegrastats": {
            "before": tegrastats_before.get('tegrastats_snapshot') if tegrastats_before else None,
            "after": tegrastats_after.get('tegrastats_snapshot') if tegrastats_after else None,
            "final": tegrastats_final.get('tegrastats_snapshot') if tegrastats_final else None,
        },
        "device_series": device_series,
        "events": reader.events,
        "telemetry": reader.telemetry,
        "timeline": tl,
    }
    
    with open(args.out, "w", encoding="utf-8") as f:
        json.dump(out, f, ensure_ascii=False, indent=2)

    if getattr(args, "rrd", None):
        import os
        import subprocess
        script = os.path.join(os.path.dirname(__file__), "..", "..",
                              "observability", "timeline_to_rerun.py")
        try:
            subprocess.run([sys.executable, script, "--in", args.out,
                            "--out", args.rrd], check=True)
        except (subprocess.CalledProcessError, FileNotFoundError, OSError) as e:
            print(f"  [rrd] export skipped: {e}")

    m = out["meta"]
    n_diar = len(diar.get("entries", []))
    n_asr = len(asr.get("entries", []))
    comprehensive = tl.get("comprehensive", [])
    
    print(f"\nwrote {args.out}")
    print(f"  audio={audio_sec:.2f}s  total_wall={total_wall:.2f}s  "
          f"stream_rt={m['stream_rt_factor']}x")
    print(f"  diar: {n_diar} segments, rt={m['diar_rt_factor']}x   "
          f"asr: {n_asr} utterances, rt={m['asr_rt_factor']}x")
    
    # Send completion signal to terminal/agent
    print("\n=== TEST_SCRIPT_COMPLETED_SUCCESSFULLY ===")
    return 0


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description="Unified WebSocket streaming test client")
    ap.add_argument("--pcm", default="test/data/audio/test.mp3", 
                    help="Audio file to stream (mp3, wav, flac, or pcm)")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=8765)
    ap.add_argument("--duration", type=float, default=3615.0,
                    help="Duration in seconds to test (120, 360, 600, or full-length)")
    ap.add_argument("--rate", type=float, default=1.0,
                    help="Push speed x real-time; 1.0 = real-time, 0 = max (no pacing)")
    ap.add_argument("--frame-ms", type=int, default=100)
    ap.add_argument("--out", help="Output JSON file path")
    ap.add_argument("--rrd", help="Also export the timeline to a rerun .rrd "
                    "recording (offline observability; needs rerun-sdk)")
    ap.add_argument("--timeline-timeout", type=float, default=600.0,
                    help="Seconds to wait for final timeline after sending end")
    ap.add_argument("--test-describe", action="store_true", help="Test describe command")
    ap.add_argument("--test-reset", action="store_true", help="Test reset command")
    ap.add_argument("--test-sessions", action="store_true", help="Test sessions command")
    ap.add_argument("--test-load-session", type=str, help="Test load_session command with session ID")
    ap.add_argument("--matrix-eval", type=int, metavar='SEC', help='Run parameter matrix evaluation (duration in seconds)')
    ap.add_argument("--max-total-time", type=float, default=7200.0,
                    help="Maximum total execution time in seconds (default: 7200)")
    args = ap.parse_args()

    import signal

    # Timeout handler
    def timeout_handler(signum, frame):
        print(f"\nERROR: Execution timeout after {args.max_total_time} seconds", file=sys.stderr)
        print("\n=== TEST_SCRIPT_COMPLETED_WITH_ERRORS ===")
        sys.exit(1)

    # Set timeout signal handler
    signal.signal(signal.SIGALRM, timeout_handler)
    signal.alarm(int(args.max_total_time))

    try:
        # Handle parameter matrix evaluation mode
        if args.matrix_eval:
            # Import websocket module for parameter evaluation
            import websocket
            
            # Full 3x3x3 parameter evaluation matrix
            PARAM_MATRIX = [
                # (threshold, merge_gap, sil_frames)
                # --- baseline region ---
                (0.50, 0.5, 3),
                (0.50, 0.6, 3),  (0.50, 0.8, 3),  (0.50, 1.0, 3),
                (0.50, 0.6, 5),  (0.50, 0.8, 5),  (0.50, 1.0, 5),
                (0.50, 0.6, 7),  (0.50, 0.8, 7),  (0.50, 1.0, 7),
                # --- optimal region ---
                (0.40, 0.5, 3),
                (0.40, 0.6, 3),  (0.40, 0.8, 3),  (0.40, 1.0, 3),
                (0.40, 0.6, 5),  (0.40, 0.8, 5),  (0.40, 1.0, 5),
                (0.40, 0.6, 7),  (0.40, 0.8, 7),  (0.40, 1.0, 7),
                # --- aggressive region ---
                (0.35, 0.5, 3),
                (0.35, 0.6, 3),  (0.35, 0.8, 3),  (0.35, 1.0, 3),
                (0.35, 0.6, 5),  (0.35, 0.8, 5),  (0.35, 1.0, 5),
                (0.35, 0.6, 7),  (0.35, 0.8, 7),  (0.35, 1.0, 7),
            ]

            def prepare_audio(duration_sec: int, audio_path: str) -> bytes:
                """Convert first N seconds of test.mp3 to PCM bytes."""
                if not os.path.exists(audio_path):
                    raise FileNotFoundError(f"Audio file not found: {audio_path}")
                return subprocess.run(
                    ['ffmpeg', '-y', '-i', audio_path, '-f', 's16le', '-ar', '16000', '-ac', '1',
                     '-t', str(duration_sec), '-'],
                    capture_output=True).stdout

            def collect_timeline(ws, timeout: float) -> dict:
                import json
                ws.settimeout(1.0)
                t0 = time.time()
                while time.time() - t0 < timeout:
                    try:
                        m = ws.recv()
                        j = json.loads(m)
                        if 'data' in j and isinstance(j['data'], str):
                            j = json.loads(j['data'])
                        if j.get('type') == 'timeline':
                            return j
                    except:
                        continue
                return None

            def eval_single(duration_sec: int, port: int, overrides: dict = None, audio_path: str = "test/data/audio/test.mp3") -> dict:
                """Run a single evaluation and return standardized metrics."""
                cfg = {}
                for k, v in (overrides or {}).items():
                    if k.startswith('diar_'):
                        cfg[k] = v

                audio = prepare_audio(duration_sec, audio_path)

                # Connect to existing server instead of starting a new one
                ws = websocket.create_connection(f'ws://127.0.0.1:{port}', timeout=10)
                ws.recv()  # ready

                # Push at 1x, drain server responses to avoid buffer fill
                t0 = time.time()
                sent = 0
                ws.settimeout(0.01)
                while sent < len(audio):
                    ws.send(audio[sent:sent + 16000], opcode=0x02)
                    sent += 16000
                    elapsed = time.time() - t0
                    if elapsed < sent / 32000:
                        time.sleep(sent / 32000 - elapsed)
                    # Drain server responses to prevent WS buffer blocking
                    while True:
                        try:
                            ws.recv()
                        except:
                            break
                    ws.settimeout(0.01)

                push_sec = time.time() - t0
                ws.send(json.dumps({'end': True}))
                tl = collect_timeline(ws, 120)
                wall_sec = time.time() - t0
                ws.close()

                if not tl:
                    print("ERROR: no timeline received")
                    print("\n=== TEST_SCRIPT_COMPLETED_WITH_ERRORS ===")
                    return {'error': 'no_timeline', 'wall_sec': wall_sec}

                t = tl.get('timeline', tl)
                tracks = {k.get('kind'): k for k in t.get('tracks', [])}
                comp = t.get('comprehensive', [])
                diar = tracks.get('diarization', {})
                asr = tracks.get('asr', {})
                vad = tracks.get('vad', {})

                # Core pipeline metrics
                metrics = {
                    'audio_sec': t.get('audio_sec', duration_sec),
                    'wall_sec': round(wall_sec, 1),
                    'push_sec': round(push_sec, 1),
                    'rtf': round(duration_sec / wall_sec, 2) if wall_sec > 0 else 0,
                    'wall_clock_ok': t.get('wall_clock_ok', False),
                    'pipelines': {
                        'diarization': {'segments': len(diar.get('entries', [])),
                                        'compute_sec': round(diar.get('compute_sec', 0), 2)},
                        'asr': {'utterances': len(asr.get('entries', [])),
                                'compute_sec': round(asr.get('compute_sec', 0), 2)},
                        'vad': {'segments': len(vad.get('entries', [])),
                                'compute_sec': round(vad.get('compute_sec', 0), 2)},
                    },
                }

                # Speaker metrics
                speakers = set()
                total_unknown = 0.0
                total_duration = 0.0
                transitions = 0
                prev_spk = None
                total_segments = 0

                for c in comp:
                    spk = c.get('speaker', '?')
                    span = c['end'] - c['start']
                    if isinstance(spk, int) and spk >= 0:
                        speakers.add(spk)
                    if spk == -1:
                        total_unknown += span
                    total_duration += span
                    if spk != prev_spk:
                        transitions += 1
                        prev_spk = spk
                    total_segments += 1

                metrics['speakers'] = {
                    'count': len(speakers),
                    'ids': sorted(speakers),
                    'transitions': transitions,
                    'comprehensive_turns': total_segments,
                    'unknown_pct': round(total_unknown / total_duration * 100, 1) if total_duration > 0 else 0,
                }

                # Config params
                metrics['config'] = {
                    'diar_threshold': overrides.get('diar_threshold', 0.4) if overrides else 0.4,
                    'diar_merge_gap_sec': overrides.get('diar_merge_gap_sec', 0.8) if overrides else 0.8,
                    'diar_spkcache_sil_frames': overrides.get('diar_spkcache_sil_frames', 5) if overrides else 5,
                }

                # ASR transcript sample
                asr_entries = asr.get('entries', [])
                metrics['asr_sample'] = [{'start': e['start'], 'end': e['end'],
                                           'text': e['text'][:100]}
                                          for e in asr_entries]

                return metrics

            def eval_matrix(duration_sec: int, port: int, audio_path: str = "test/data/audio/test.mp3"):
                """Run the full parameter matrix and output consolidated results."""
                results = []
                n = len(PARAM_MATRIX)

                for i, (thr, mg, sil) in enumerate(PARAM_MATRIX):
                    overrides = {
                        'diar_threshold': thr,
                        'diar_merge_gap_sec': mg,
                        'diar_spkcache_sil_frames': sil,
                    }
                    print(f'  [{i+1:2d}/{n}] thr={thr:.2f} mg={mg:.1f} sil={sil} ... ', end='', flush=True)
                    r = eval_single(duration_sec, port, overrides, audio_path)
                    if 'error' in r:
                        print(f'FAILED: {r["error"]}')
                        continue

                    spk = r['speakers']
                    pip = r['pipelines']
                    print(f'segs={pip["diarization"]["segments"]:3d} '
                          f'unk={spk["unknown_pct"]:5.1f}% '
                          f'turns={spk["comprehensive_turns"]:3d} '
                          f'trans={spk["transitions"]:3d} '
                          f'spk={spk["count"]}')
                    results.append(r)
                    time.sleep(1)

                # Rank by unknown_pct ascending
                results.sort(key=lambda x: x['speakers']['unknown_pct'])

                output = {
                    'duration_sec': duration_sec,
                    'total_configs': n,
                    'completed': len(results),
                    'ranking': [{
                        'rank': i + 1,
                        'config': r['config'],
                        'segs': r['pipelines']['diarization']['segments'],
                        'unknown_pct': r['speakers']['unknown_pct'],
                        'turns': r['speakers']['comprehensive_turns'],
                        'transitions': r['speakers']['transitions'],
                        'speakers': r['speakers']['count'],
                    } for i, r in enumerate(results)],
                }

                # Print summary
                print(f'\n{"─"*70}')
                print(f'MATRIX RESULTS ({duration_sec}s)')
                print(f'{"─"*70}')
                print(f'{"Rank":>4s} {"thr":>5s} {"mg":>5s} {"sil":>4s} {"Segs":>5s} {"Unk%":>6s} {"Turns":>6s} {"Trans":>6s} {"Spk":>4s}')
                print(f'{"─"*70}')
                for r in output['ranking']:
                    cfg = r['config']
                    print(f'{r["rank"]:4d} {cfg["diar_threshold"]:5.2f} {cfg["diar_merge_gap_sec"]:5.1f} '
                          f'{cfg["diar_spkcache_sil_frames"]:4d} {r["segs"]:5d} {r["unknown_pct"]:6.1f}% '
                          f'{r["turns"]:6d} {r["transitions"]:6d} {r["speakers"]:4d}')

                # Find best
                best = results[0]
                print(f'\nBest: thr={best["config"]["diar_threshold"]} '
                      f'mg={best["config"]["diar_merge_gap_sec"]} '
                      f'sil={best["config"]["diar_spkcache_sil_frames"]} '
                      f'→ unk={best["speakers"]["unknown_pct"]}%')

                return output

            # Run matrix evaluation
            print(f'Matrix: {args.matrix_eval}s x {len(PARAM_MATRIX)} configs (port {args.port})')
            output_file = args.out if args.out else f"matrix_results_{args.matrix_eval}s.json"
            r = eval_matrix(args.matrix_eval, args.port, args.pcm)
            if args.out:
                with open(args.out, 'w') as f:
                    json.dump(r, f, indent=2, ensure_ascii=False)
                print(f'Output: {args.out}')
            
            # Send completion signal to terminal/agent
            print("\n=== TEST_SCRIPT_COMPLETED_SUCCESSFULLY ===")
            signal.alarm(0)  # Cancel timeout
            sys.exit(0)

        # Standard streaming test mode
        if not args.out:
            print("ERROR: --out is required for standard streaming test mode", file=sys.stderr)
            print("\n=== TEST_SCRIPT_COMPLETED_WITH_ERRORS ===")
            signal.alarm(0)  # Cancel timeout
            sys.exit(1)

        # Standard streaming test mode execution
        try:
            result = main(args)
            signal.alarm(0)  # Cancel timeout
            sys.exit(result)
        except Exception as e:
            print(f"\nERROR: Unexpected exception: {e}", file=sys.stderr)
            print("\n=== TEST_SCRIPT_COMPLETED_WITH_ERRORS ===")
            signal.alarm(0)  # Cancel timeout
            sys.exit(1)
            
    except Exception as e:
        print(f"\nERROR: Unexpected exception in main block: {e}", file=sys.stderr)
        print("\n=== TEST_SCRIPT_COMPLETED_WITH_ERRORS ===")
        signal.alarm(0)  # Cancel timeout
        sys.exit(1)
