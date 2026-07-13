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
    """The single socket reader for live events, commands, and timelines."""

    def __init__(self, sock):
        super().__init__(daemon=True)
        self.sock = sock
        self.events = []
        self.timelines = []
        self.ready = None
        self.telemetry = []
        self.messages = []
        self._condition = threading.Condition()

    def message_index(self):
        with self._condition:
            return len(self.messages)

    def wait_for(self, predicate, after_index=0, timeout=30.0):
        """Wait for the first parsed message after an absolute list index."""
        deadline = time.monotonic() + timeout
        with self._condition:
            while True:
                for message in self.messages[after_index:]:
                    if predicate(message):
                        return message
                remaining = deadline - time.monotonic()
                if remaining <= 0.0:
                    return None
                self._condition.wait(remaining)

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
            if not isinstance(raw, dict):
                continue
            kind = raw.get("type")
            with self._condition:
                self.messages.append(raw)
                if kind in ("asr_partial", "asr_retract", "asr", "revision",
                            "align", "diar", "vad", "vad_state"):
                    self.events.append(raw)
                elif kind == "timeline":
                    self.timelines.append(raw)
                elif kind == "ready":
                    self.ready = raw
                elif kind in ("gpu_telemetry", "cursor_progress"):
                    self.telemetry.append(raw)
                self._condition.notify_all()

            if kind == "asr":
                t = raw.get("text", "")
                print(f"  [stream] asr [{raw.get('start'):.2f}-{raw.get('end'):.2f}] {t}")
            elif kind == "align":
                units = raw.get("units", [])
                preview = " ".join(
                    f"{unit.get('text')}[{unit.get('start'):.2f}-{unit.get('end'):.2f}]"
                    for unit in units[:8])
                print(f"  [stream] align [{raw.get('start'):.2f}-{raw.get('end'):.2f}] "
                      f"({len(units)} units) {preview}")


def parse_audio_file(audio_path: str, duration_sec: float = None) -> bytes:
    """Read audio, convert supported containers, and optionally truncate it."""
    import subprocess
    import tempfile

    if not os.path.exists(audio_path):
        raise FileNotFoundError(f"Audio file not found: {audio_path}")

    if audio_path.lower().endswith((".mp3", ".wav", ".flac")):
        pcm_file = tempfile.NamedTemporaryFile(
            suffix=".pcm", delete=False).name
        try:
            result = subprocess.run(
                ["ffmpeg", "-y", "-i", audio_path, "-ar", "16000",
                 "-ac", "1", "-f", "s16le", pcm_file],
                capture_output=True, text=True, check=False)
            if result.returncode != 0:
                detail = result.stderr.strip().splitlines()
                reason = detail[-1] if detail else "unknown ffmpeg error"
                raise RuntimeError(f"audio conversion failed: {reason}")
            with open(pcm_file, "rb") as pcm_input:
                pcm = pcm_input.read()
        finally:
            try:
                os.unlink(pcm_file)
            except FileNotFoundError:
                pass
    else:
        with open(audio_path, "rb") as pcm_input:
            pcm = pcm_input.read()

    if duration_sec is not None:
        total_samples = int(duration_sec * SAMPLE_RATE)
        max_bytes = total_samples * BYTES_PER_SAMPLE
        pcm = pcm[:max_bytes]

    return pcm


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


def validate_terminal_contract(events, timeline):
    """Return mechanical live/final and typed-track contract violations."""
    issues = []
    track_list = timeline.get("tracks", [])
    tracks = {}
    for track in track_list:
        kind = track.get("kind")
        if not kind:
            issues.append("terminal track without kind")
        elif kind in tracks:
            issues.append(f"duplicate terminal track kind: {kind}")
        else:
            tracks[kind] = track

    asr_entries = tracks.get("asr", {}).get("entries", [])
    asr_by_id = {}
    for entry in asr_entries:
        text_id = entry.get("text_id")
        if text_id in asr_by_id:
            issues.append(f"duplicate terminal ASR text_id: {text_id}")
        asr_by_id[text_id] = entry

    live_asr = {}
    active_partials = {}
    for event in events:
        if event.get("type") == "asr_partial":
            active_partials[event.get("text_id")] = event
            continue
        if event.get("type") == "asr_retract":
            active_partials.pop(event.get("text_id"), None)
            continue
        if event.get("type") != "asr":
            continue
        text_id = event.get("text_id")
        active_partials.pop(text_id, None)
        if text_id in live_asr:
            issues.append(f"duplicate live final ASR text_id: {text_id}")
        live_asr[text_id] = event
    if active_partials:
        issues.append(
            "unresolved live ASR partial IDs: "
            f"{sorted(active_partials)}")
    if set(live_asr) != set(asr_by_id):
        issues.append(
            "live/terminal ASR ID mismatch: "
            f"live={sorted(live_asr)} terminal={sorted(asr_by_id)}")
    for text_id in set(live_asr) & set(asr_by_id):
        live = live_asr[text_id]
        final = asr_by_id[text_id]
        if live.get("text") != final.get("text"):
            issues.append(f"live/terminal ASR text mismatch: {text_id}")
        for field in ("start", "end"):
            if abs(float(live.get(field, 0.0)) -
                   float(final.get(field, 0.0))) > 0.0015:
                issues.append(
                    f"live/terminal ASR {field} mismatch: {text_id}")

    align_track = tracks.get("align")
    if align_track is not None:
        align_entries = align_track.get("entries", [])
        align_by_id = {
            entry.get("text_id"): entry for entry in align_entries}
        align_ids = [entry.get("text_id") for entry in align_entries]
        if len(align_ids) != len(set(align_ids)):
            issues.append("duplicate alignment text_id")
        if set(align_ids) != set(asr_by_id):
            issues.append(
                "ASR/alignment ID mismatch: "
                f"asr={sorted(asr_by_id)} align={sorted(align_ids)}")
        for group in align_entries:
            group_start = float(group.get("start", 0.0))
            group_end = float(group.get("end", 0.0))
            previous_start = group_start
            previous_end = group_start
            for unit in group.get("units", []):
                start = float(unit.get("start", 0.0))
                end = float(unit.get("end", 0.0))
                if (start < group_start - 0.0015 or
                        end > group_end + 0.0015 or end < start - 0.0015):
                    issues.append(
                        f"out-of-bounds alignment unit: {group.get('text_id')}")
                    break
                if (start < previous_start - 0.0015 or
                        end < previous_end - 0.0015):
                    issues.append(
                        f"non-monotonic alignment unit: {group.get('text_id')}")
                    break
                previous_start = start
                previous_end = end

        live_align = {}
        for event in events:
            if event.get("type") != "align":
                continue
            text_id = event.get("text_id", event.get("id"))
            if text_id in live_align:
                issues.append(f"duplicate live alignment text_id: {text_id}")
            live_align[text_id] = event
        if set(live_align) != set(align_by_id):
            issues.append(
                "live/terminal alignment ID mismatch: "
                f"live={sorted(live_align)} terminal={sorted(align_by_id)}")
        for text_id in set(live_align) & set(align_by_id):
            live = live_align[text_id]
            final = align_by_id[text_id]
            if live.get("units") != final.get("units"):
                issues.append(
                    f"live/terminal alignment units mismatch: {text_id}")

    diar_entries = tracks.get("diarization", {}).get("entries", [])
    diar_events = [
        event for event in events if event.get("type") == "diar"]
    if diar_entries or diar_events:
        if not diar_events:
            issues.append("terminal diarization has no live snapshot")
        else:
            def normalize_diar(entry):
                speaker = entry.get("speaker")
                if isinstance(speaker, str) and speaker.startswith("speaker_"):
                    try:
                        speaker = int(speaker[8:])
                    except ValueError:
                        speaker = -1
                return {
                    "start": entry.get("start"),
                    "end": entry.get("end"),
                    "speaker": speaker,
                    "speaker_id": entry.get("speaker_id"),
                    "confidence": entry.get("confidence"),
                }

            live_diar = [
                normalize_diar(entry)
                for entry in diar_events[-1].get("segments", [])]
            final_diar = [normalize_diar(entry) for entry in diar_entries]
            if live_diar != final_diar:
                issues.append("latest live diarization differs from terminal track")

    vad_entries = tracks.get("vad", {}).get("entries", [])
    live_vad = [
        {"start": event.get("start"), "end": event.get("end")}
        for event in events if event.get("type") == "vad"]
    if live_vad != vad_entries:
        issues.append("live VAD segments differ from terminal track")

    business = tracks.get("business_speaker", {}).get("entries", [])
    comprehensive = timeline.get("comprehensive", [])
    if business != comprehensive:
        issues.append("business_speaker track differs from comprehensive alias")
    business_by_id = {}
    for entry in business:
        text_id = entry.get("text_id")
        business_by_id.setdefault(text_id, []).append(entry)
        source = asr_by_id.get(text_id)
        if source is None:
            issues.append(f"business entry without ASR source: {text_id}")
            continue
        if (float(entry.get("start", 0.0)) <
                float(source.get("start", 0.0)) - 0.0015 or
                float(entry.get("end", 0.0)) >
                float(source.get("end", 0.0)) + 0.0015):
            issues.append(f"business entry outside ASR span: {text_id}")
    for text_id, source in asr_by_id.items():
        pieces = business_by_id.get(text_id, [])
        reconstructed = "".join(piece.get("text", "") for piece in pieces)
        if reconstructed != source.get("text", ""):
            issues.append(f"business text reconstruction mismatch: {text_id}")

    def projection(entries):
        fields = ("start", "end", "text_id", "speaker", "speaker_id",
                  "text", "speaker_support", "speaker_uncertain")
        return [{field: entry.get(field) for field in fields}
                for entry in entries]

    latest_revision = {}
    for event in events:
        if (event.get("type") != "revision" or
                event.get("source") != "business_speaker"):
            continue
        entries = event.get("entries", [])
        text_ids = {entry.get("text_id") for entry in entries}
        for text_id in text_ids:
            latest_revision[text_id] = [
                entry for entry in entries if entry.get("text_id") == text_id]
    if set(latest_revision) != set(asr_by_id):
        issues.append(
            "business revision/ASR ID mismatch: "
            f"revision={sorted(latest_revision)} asr={sorted(asr_by_id)}")
    for text_id in set(latest_revision) & set(business_by_id):
        if projection(latest_revision[text_id]) != projection(
                business_by_id[text_id]):
            issues.append(f"revision/terminal business mismatch: {text_id}")

    if not timeline.get("timebase_reconciled", False):
        issues.append("terminal time-base reconciliation not completed")
    if not timeline.get("timebase_ok", False):
        issues.append("terminal time-base reconciliation failed")
    for extent in timeline.get("track_extents", []):
        if extent.get("gap_samples") != 0:
            issues.append(
                f"nonzero extent gap for {extent.get('pipeline')}: "
                f"{extent.get('gap_samples')}")
    return issues


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

    ready = reader.wait_for(
        lambda message: message.get("type") == "ready",
        timeout=args.command_timeout)
    if ready is None:
        raise RuntimeError("no ready response from WebSocket server")
    print(f"connected; streaming {audio_sec:.2f}s of audio "
          f"({'max rate' if args.rate == 0 else str(args.rate) + 'x'}, "
          f"{args.frame_ms}ms frames)")

    command_responses = {}

    def run_command(name, payload, predicate):
        start_index = reader.message_index()
        print(f"  [test] sending {name} command...")
        sock.sendall(mask_frame(0x1, json.dumps(payload).encode("utf-8")))
        response = reader.wait_for(
            predicate, after_index=start_index, timeout=args.command_timeout)
        if response is None:
            raise RuntimeError(
                f"no {name} response within {args.command_timeout:.1f}s")
        command_responses[name] = response
        if name == "describe":
            pipeline_names = [
                item.get("name") for item in response.get("pipelines", [])]
            summary = {"type": response.get("type"),
                       "pipelines": pipeline_names,
                       "schema_count": len(response.get("schemas", []))}
        elif name == "sessions":
            summary = {"type": response.get("type"),
                       "session_count": len(response.get("sessions", []))}
        elif name == "load_session" and response.get("type") == "timeline":
            summary = {"type": "timeline",
                       "audio_sec": response.get("audio_sec")}
        else:
            summary = response
        print(f"  [test] {name} response: "
              f"{json.dumps(summary, ensure_ascii=False)}")

    if args.test_describe:
        run_command(
            "describe", {"describe": True},
            lambda message: message.get("type") == "describe" or
            ("pipelines" in message and "schemas" in message))

    if args.test_reset:
        run_command(
            "reset", {"reset": True},
            lambda message: message.get("type") == "reset_ok")

    if args.test_sessions:
        run_command(
            "sessions", {"sessions": True},
            lambda message: message.get("type") == "sessions")

    if args.test_load_session:
        session_id = args.test_load_session
        run_command(
            "load_session",
            {"load_session": True, "session_id": session_id},
            lambda message: message.get("type") == "timeline" or
            "error" in message)

    # Producer: push PCM frames through the socket.
    t0 = time.monotonic()
    tegra_sampler = TegraSampler(t0, interval_ms=1000)
    tegra_sampler.start()
    try:
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

        flush_index = reader.message_index()
        sock.sendall(mask_frame(0x1, b'{"flush":true}'))
        print("  [flush] waiting for timeline...")
        flush_timeline = reader.wait_for(
            lambda message: message.get("type") == "timeline",
            after_index=flush_index, timeout=args.timeline_timeout)
        if flush_timeline is not None:
            print("  [flush] timeline received "
                  f"({flush_timeline.get('audio_sec', '?')}s)")
        else:
            print("  [flush] no timeline (continuing to end)")

        end_index = reader.message_index()
        sock.sendall(mask_frame(0x1, b'{"end":true}'))
        print("  [end] waiting for final timeline...")
        final_timeline = reader.wait_for(
            lambda message: message.get("type") == "timeline",
            after_index=end_index, timeout=args.timeline_timeout)
        total_wall = time.monotonic() - t0
    finally:
        tegra_sampler.stop()
        try:
            sock.sendall(mask_frame(0x8, b"\x03\xe8"))
        except (BrokenPipeError, ConnectionResetError, OSError):
            pass
        sock.close()
        reader.join(timeout=2.0)

    device_series = tegra_sampler.samples

    if final_timeline is None:
        print(f"ERROR: no final timeline received within {args.timeline_timeout:.1f}s",
              file=sys.stderr)
        print("\n=== TEST_SCRIPT_COMPLETED_WITH_ERRORS ===")
        return 1

    tl = final_timeline
    tracks = {t.get("kind"): t for t in tl.get("tracks", [])}
    diar = tracks.get("diarization", {})
    asr = tracks.get("asr", {})
    diar_compute = diar.get("compute_sec")
    asr_compute = asr.get("compute_sec")
    contract_issues = validate_terminal_contract(reader.events, tl)
    
    first_device_line = device_series[0]["line"] if device_series else None
    last_device_line = device_series[-1]["line"] if device_series else None
    
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
            "contract_issues": contract_issues,
        },
        "tegrastats": {
            "before": first_device_line,
            "after": last_device_line,
            "final": last_device_line,
        },
        "device_series": device_series,
        "command_responses": command_responses,
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
    print(f"\nwrote {args.out}")
    print(f"  audio={audio_sec:.2f}s  total_wall={total_wall:.2f}s  "
          f"stream_rt={m['stream_rt_factor']}x")
    print(f"  diar: {n_diar} segments, rt={m['diar_rt_factor']}x   "
          f"asr: {n_asr} utterances, rt={m['asr_rt_factor']}x")

    if contract_issues:
        for issue in contract_issues:
            print(f"  CONTRACT ERROR: {issue}", file=sys.stderr)
        print("\n=== TEST_SCRIPT_COMPLETED_WITH_ERRORS ===")
        return 1

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
    ap.add_argument("--command-timeout", type=float, default=30.0,
                    help="Seconds to wait for ready and optional command responses")
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

    if not args.out:
        print("ERROR: --out is required", file=sys.stderr)
        print("\n=== TEST_SCRIPT_COMPLETED_WITH_ERRORS ===")
        signal.alarm(0)
        sys.exit(1)

    try:
        result = main(args)
    except Exception as error:
        print(f"\nERROR: Unexpected exception: {error}", file=sys.stderr)
        print("\n=== TEST_SCRIPT_COMPLETED_WITH_ERRORS ===")
        result = 1
    finally:
        signal.alarm(0)
    sys.exit(result)
