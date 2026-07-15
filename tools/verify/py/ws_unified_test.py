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

  # Verify one producer with observers connected before and during streaming:
  python3 tools/verify/py/ws_unified_test.py --duration 120 --port 8765 \
    --out test_observer.json --test-observer
"""

import argparse
import base64
import datetime
import hashlib
import json
import math
import os
import platform
import re
import socket
import statistics
import struct
import subprocess
import sys
import threading
import time
from collections import Counter

GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
SAMPLE_RATE = 16000
BYTES_PER_SAMPLE = 2  # int16 mono


def canonical_json_sha256(value) -> str:
    payload = json.dumps(
        value, ensure_ascii=False, sort_keys=True,
        separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(payload).hexdigest()


def sha256_file(path: str):
    if not path or not os.path.isfile(path):
        return None
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def config_snapshot(path):
    resolved_path = os.path.realpath(os.path.abspath(path)) if path else None
    return {
        "path": resolved_path,
        "sha256": sha256_file(resolved_path),
        "bytes": (
            os.path.getsize(resolved_path)
            if resolved_path and os.path.isfile(resolved_path) else None),
    }


def config_provenance(start_snapshot, timeline):
    resolved = timeline.get("resolved_config")
    source_path = (
        resolved.get("config_source_path")
        if isinstance(resolved, dict) else None)
    end_snapshot = config_snapshot(source_path)
    path_matches = (
        bool(start_snapshot.get("path")) and
        start_snapshot.get("path") == end_snapshot.get("path"))
    unchanged = (
        bool(start_snapshot.get("sha256")) and
        start_snapshot.get("sha256") == end_snapshot.get("sha256"))
    return {
        "client_pre_stream": start_snapshot,
        "client_post_stream": end_snapshot,
        "server_resolved_path": source_path,
        "path_matches": path_matches,
        "unchanged_during_client_run": unchanged,
        "acceptance_consistent": path_matches and unchanged,
    }


def command_output(command):
    try:
        result = subprocess.run(
            command, cwd=os.getcwd(), capture_output=True, text=True,
            check=False, timeout=10)
    except (OSError, subprocess.TimeoutExpired):
        return None
    if result.returncode != 0:
        return None
    return result.stdout.strip()


def git_workspace_sha256():
    root = command_output(["git", "rev-parse", "--show-toplevel"])
    if not root:
        return None
    try:
        diff = subprocess.run(
            ["git", "diff", "--binary", "--no-ext-diff", "HEAD", "--"],
            cwd=root, capture_output=True, check=False, timeout=30)
        untracked = subprocess.run(
            ["git", "ls-files", "--others", "--exclude-standard", "-z"],
            cwd=root, capture_output=True, check=False, timeout=10)
    except (OSError, subprocess.TimeoutExpired):
        return None
    if diff.returncode != 0 or untracked.returncode != 0:
        return None

    digest = hashlib.sha256()
    digest.update(b"tracked\0")
    digest.update(diff.stdout)
    digest.update(b"untracked\0")
    for relative_bytes in sorted(filter(None, untracked.stdout.split(b"\0"))):
        relative = os.fsdecode(relative_bytes)
        path = os.path.join(root, relative)
        digest.update(relative_bytes)
        digest.update(b"\0")
        try:
            if os.path.islink(path):
                digest.update(b"symlink\0")
                digest.update(os.fsencode(os.readlink(path)))
            else:
                digest.update(b"file\0")
                with open(path, "rb") as source:
                    for chunk in iter(lambda: source.read(1024 * 1024), b""):
                        digest.update(chunk)
        except OSError:
            return None
        digest.update(b"\0")
    return digest.hexdigest()


def numeric_summary(values):
    finite = [float(value) for value in values
              if isinstance(value, (int, float)) and
              not isinstance(value, bool) and math.isfinite(value)]
    if not finite:
        return None
    ordered = sorted(finite)
    p95_index = max(0, math.ceil(0.95 * len(ordered)) - 1)
    return {
        "count": len(ordered),
        "min": ordered[0],
        "mean": statistics.fmean(ordered),
        "p95": ordered[p95_index],
        "max": ordered[-1],
    }


def parse_tegrastats_line(line):
    values = {}
    match = re.search(r"RAM (\d+)/(\d+)MB", line)
    if match:
        values["ram_used_mb"] = float(match.group(1))
    match = re.search(r"CPU \[([^\]]*)\]", line)
    if match:
        cores = [int(value) for value in re.findall(
            r"(\d+)%@", match.group(1))]
        if cores:
            values["cpu_pct"] = statistics.fmean(cores)
    match = re.search(r"GR3D_FREQ (\d+)%", line)
    if match:
        values["tegrastats_gpu_pct"] = float(match.group(1))
    temperatures = [float(value) for value in re.findall(
        r"@[\s]*([\d.]+)C", line)]
    if temperatures:
        values["max_temp_c"] = max(temperatures)
    for pattern, key in (
            (r"VDD_GPU\S* (\d+)mW", "gpu_power_w"),
            (r"(?:^| )VIN (\d+)mW", "system_power_w")):
        match = re.search(pattern, line)
        if match:
            values[key] = float(match.group(1)) / 1000.0
    return values


def telemetry_summary(messages, device_series, duration_sec=None,
                      gpu_interval_sec=None, device_interval_sec=1.0):
    runtime_devices = [
        item["device"] for item in messages
        if isinstance(item, dict) and item.get("type") == "gpu_telemetry" and
        isinstance(item.get("device"), dict)
    ]
    tegra_lines = [
        item.get("line", "") for item in device_series
        if isinstance(item, dict) and item.get("line")
    ]
    tegra_values = [parse_tegrastats_line(line) for line in tegra_lines]

    runtime_fields = (
        "gpu_utilization_pct", "gpu_mem_used_mb", "gpu_mem_used_pct",
        "gpu_freq_mhz", "system_power_w")
    tegra_fields = (
        "cpu_pct", "ram_used_mb", "max_temp_c", "tegrastats_gpu_pct",
        "gpu_power_w", "system_power_w")
    runtime_metrics = {
        field: numeric_summary([item.get(field) for item in runtime_devices])
        for field in runtime_fields
    }
    tegra_metrics = {
        field: numeric_summary([item.get(field) for item in tegra_values])
        for field in tegra_fields
    }

    def coverage(metrics, field, total):
        summary = metrics.get(field)
        return (summary["count"] / total
                if summary is not None and total else 0.0)

    def cadence_coverage(sample_count, interval_sec):
        if (not isinstance(duration_sec, (int, float)) or
                duration_sec <= 0.0 or
                not isinstance(interval_sec, (int, float)) or
                interval_sec <= 0.0):
            return None
        expected = max(1, math.floor(duration_sec / interval_sec))
        return min(1.0, sample_count / expected)

    required_coverage = {
        "gpu_utilization_pct": coverage(
            runtime_metrics, "gpu_utilization_pct", len(runtime_devices)),
        "gpu_mem_used_mb": coverage(
            runtime_metrics, "gpu_mem_used_mb", len(runtime_devices)),
        "system_power_w": coverage(
            runtime_metrics, "system_power_w", len(runtime_devices)),
        "cpu_pct": coverage(tegra_metrics, "cpu_pct", len(tegra_values)),
        "ram_used_mb": coverage(
            tegra_metrics, "ram_used_mb", len(tegra_values)),
        "max_temp_c": coverage(
            tegra_metrics, "max_temp_c", len(tegra_values)),
    }
    runtime_cadence = cadence_coverage(
        len(runtime_devices), gpu_interval_sec)
    tegrastats_cadence = cadence_coverage(
        len(tegra_lines), device_interval_sec)
    if runtime_cadence is not None:
        required_coverage["runtime_sample_cadence"] = runtime_cadence
    if tegrastats_cadence is not None:
        required_coverage["tegrastats_sample_cadence"] = tegrastats_cadence
    return {
        "runtime_sample_count": len(runtime_devices),
        "tegrastats_sample_count": len(tegra_lines),
        "gpu_utilization_sources": dict(Counter(
            str(item.get("gpu_utilization_source"))
            for item in runtime_devices
            if item.get("gpu_utilization_source") is not None)),
        "runtime": runtime_metrics,
        "tegrastats": tegra_metrics,
        "required_field_coverage": required_coverage,
        "required_fields_at_least_95_percent": (
            bool(runtime_devices) and bool(tegra_values) and
            all(value >= 0.95 for value in required_coverage.values())),
    }


def git_metadata():
    commit = command_output(["git", "rev-parse", "HEAD"])
    status = command_output(
        ["git", "status", "--porcelain", "--untracked-files=all"])
    return {
        "commit": commit,
        "dirty": bool(status),
        "status": status.splitlines() if status else [],
        "workspace_sha256": git_workspace_sha256(),
    }


def write_run_manifest(args, timeline, pcm, artifact_path,
                       source_evidence):
    resolved = timeline.get("resolved_config")
    generated = datetime.datetime.now(datetime.timezone.utc)
    start_git = source_evidence["git"]["client_pre_stream"]
    short_commit = (start_git.get("commit") or "unknown")[:12]
    duration_label = f"{len(pcm) // BYTES_PER_SAMPLE / SAMPLE_RATE:.3f}s"
    artifact_id = (
        f"orator-{generated.strftime('%Y%m%dT%H%M%SZ')}-"
        f"{short_commit}-{duration_label}")

    jetson_release = None
    try:
        with open("/etc/nv_tegra_release", encoding="utf-8") as release_file:
            jetson_release = release_file.read().strip()
    except OSError:
        pass

    manifest = {
        "schema_version": 1,
        "artifact_id": artifact_id,
        "generated_utc": generated.isoformat(),
        "artifact": {
            "path": os.path.abspath(artifact_path),
            "sha256": sha256_file(artifact_path),
        },
        "source_audio": {
            "path": os.path.abspath(args.pcm),
            "container_sha256": sha256_file(args.pcm),
            "stream_pcm_s16le_sha256": hashlib.sha256(pcm).hexdigest(),
            "samples": len(pcm) // BYTES_PER_SAMPLE,
            "sample_rate": SAMPLE_RATE,
        },
        "resolved_config": resolved,
        "resolved_config_sha256": (
            canonical_json_sha256(resolved)
            if isinstance(resolved, dict) else None),
        "config_source": source_evidence["config"]["client_pre_stream"],
        "config_source_end": source_evidence["config"]["client_post_stream"],
        "config_source_provenance": source_evidence["config"],
        "git": start_git,
        "git_end": source_evidence["git"]["client_post_stream"],
        "git_provenance": source_evidence["git"],
        "server_binary": source_evidence["server_binary"],
        "source_stable_during_run": source_evidence[
            "acceptance_consistent"],
        "client_environment_overrides": {
            key: value for key, value in sorted(os.environ.items())
            if key.startswith("ORATOR_")
        },
        "host": {
            "platform": platform.platform(),
            "machine": platform.machine(),
            "python": platform.python_version(),
            "jetson_release": jetson_release,
            "nvpmodel": command_output(["nvpmodel", "-q"]),
            "jetson_clocks": command_output(["jetson_clocks", "--show"]),
        },
        "invocation": {
            "host": args.host,
            "port": args.port,
            "duration_sec": args.duration,
            "rate": args.rate,
            "frame_ms": args.frame_ms,
        },
    }
    manifest_path = artifact_path + ".manifest.json"
    with open(manifest_path, "w", encoding="utf-8") as output:
        json.dump(manifest, output, ensure_ascii=False, indent=2)
    return manifest_path, manifest


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

    def __init__(self, sock, verbose=True):
        super().__init__(daemon=True)
        self.sock = sock
        self.verbose = verbose
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

            if self.verbose and kind == "asr":
                t = raw.get("text", "")
                print(f"  [stream] asr [{raw.get('start'):.2f}-{raw.get('end'):.2f}] {t}")
            elif self.verbose and kind == "align":
                units = raw.get("units", [])
                preview = " ".join(
                    f"{unit.get('text')}[{unit.get('start'):.2f}-{unit.get('end'):.2f}]"
                    for unit in units[:8])
                print(f"  [stream] align [{raw.get('start'):.2f}-{raw.get('end'):.2f}] "
                      f"({len(units)} units) {preview}")


def open_reader(host, port, verbose=True):
    sock = socket.create_connection((host, port))
    handshake(sock, host, port)
    reader = Reader(sock, verbose=verbose)
    reader.start()
    return sock, reader


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
    if not isinstance(timeline.get("resolved_config"), dict):
        issues.append("terminal timeline has no resolved_config object")
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
    configured_path = (
        args.config_path or os.environ.get("ORATOR_CONFIG") or
        "orator.toml")
    start_config_snapshot = config_snapshot(configured_path)
    start_git_snapshot = git_metadata()
    start_binary_snapshot = config_snapshot(args.server_binary)
    # Parse audio file (handles MP3, WAV, FLAC, and PCM formats)
    pcm = parse_audio_file(args.pcm, args.duration)
    total_samples = len(pcm) // BYTES_PER_SAMPLE
    audio_sec = total_samples / SAMPLE_RATE
    frame_bytes = int(SAMPLE_RATE * args.frame_ms / 1000) * BYTES_PER_SAMPLE

    observer_sock = None
    observer_reader = None
    late_observer_sock = None
    late_observer_reader = None
    if args.test_observer:
        observer_sock, observer_reader = open_reader(
            args.host, args.port, verbose=False)
        observer_ready = observer_reader.wait_for(
            lambda message: message.get("type") == "ready",
            timeout=args.command_timeout)
        if observer_ready is None:
            raise RuntimeError("no ready response for observer connection")

    sock, reader = open_reader(args.host, args.port)

    ready = reader.wait_for(
        lambda message: message.get("type") == "ready",
        timeout=args.command_timeout)
    if ready is None:
        raise RuntimeError("no ready response from WebSocket server")
    print(f"connected; streaming {audio_sec:.2f}s of audio "
          f"({'max rate' if args.rate == 0 else str(args.rate) + 'x'}, "
          f"{args.frame_ms}ms frames)")
    if observer_reader is not None:
        print("  [observer] connected before audio producer")

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
    final_timeline = None
    observer_final_timeline = None
    late_observer_final_timeline = None
    contender_error = None
    try:
        sent = 0
        for off in range(0, len(pcm), frame_bytes):
            sock.sendall(mask_frame(0x2, pcm[off:off + frame_bytes]))
            sent += 1
            if args.test_observer and sent == 10:
                transient_sock, transient_reader = open_reader(
                    args.host, args.port, verbose=False)
                transient_ready = transient_reader.wait_for(
                    lambda message: message.get("type") == "ready",
                    timeout=args.command_timeout)
                if transient_ready is None:
                    raise RuntimeError(
                        "no ready response for transient observer connection")
                conflict_index = transient_reader.message_index()
                transient_sock.sendall(mask_frame(0x2, pcm[:frame_bytes]))
                conflict = transient_reader.wait_for(
                    lambda message: message.get("type") == "error",
                    after_index=conflict_index,
                    timeout=args.command_timeout)
                contender_error = (
                    conflict.get("error") if conflict is not None else None)
                if contender_error != "audio producer already active":
                    raise RuntimeError(
                        "second audio producer was not explicitly rejected")
                transient_sock.sendall(mask_frame(0x8, b"\x03\xe8"))
                transient_sock.close()
                transient_reader.join(timeout=2.0)
                print("  [observer] transient connection closed during stream")
            if args.test_observer and sent == 20:
                late_observer_sock, late_observer_reader = open_reader(
                    args.host, args.port, verbose=False)
                late_ready = late_observer_reader.wait_for(
                    lambda message: message.get("type") == "ready",
                    timeout=args.command_timeout)
                if late_ready is None:
                    raise RuntimeError(
                        "no ready response for late observer connection")
                print("  [observer] late connection joined after audio start")
            if args.rate > 0:
                target_wall = (sent * args.frame_ms / 1000.0) / args.rate
                slack = target_wall - (time.monotonic() - t0)
                if slack > 0:
                    time.sleep(slack)
        push_wall = time.monotonic() - t0

        flush_index = reader.message_index()
        observer_flush_index = (
            observer_reader.message_index()
            if observer_reader is not None else 0)
        late_observer_flush_index = (
            late_observer_reader.message_index()
            if late_observer_reader is not None else 0)
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
        if observer_reader is not None:
            observer_flush_timeline = observer_reader.wait_for(
                lambda message: message.get("type") == "timeline",
                after_index=observer_flush_index,
                timeout=args.timeline_timeout)
            if observer_flush_timeline is None:
                print("  [observer] no flush timeline", file=sys.stderr)
        if late_observer_reader is not None:
            late_observer_flush_timeline = late_observer_reader.wait_for(
                lambda message: message.get("type") == "timeline",
                after_index=late_observer_flush_index,
                timeout=args.timeline_timeout)
            if late_observer_flush_timeline is None:
                print("  [observer] late observer has no flush timeline",
                      file=sys.stderr)

        end_index = reader.message_index()
        observer_end_index = (
            observer_reader.message_index()
            if observer_reader is not None else 0)
        late_observer_end_index = (
            late_observer_reader.message_index()
            if late_observer_reader is not None else 0)
        sock.sendall(mask_frame(0x1, b'{"end":true}'))
        print("  [end] waiting for final timeline...")
        final_timeline = reader.wait_for(
            lambda message: message.get("type") == "timeline",
            after_index=end_index, timeout=args.timeline_timeout)
        if observer_reader is not None:
            observer_final_timeline = observer_reader.wait_for(
                lambda message: message.get("type") == "timeline",
                after_index=observer_end_index,
                timeout=args.timeline_timeout)
        if late_observer_reader is not None:
            late_observer_final_timeline = late_observer_reader.wait_for(
                lambda message: message.get("type") == "timeline",
                after_index=late_observer_end_index,
                timeout=args.timeline_timeout)
        total_wall = time.monotonic() - t0
    finally:
        tegra_sampler.stop()
        try:
            sock.sendall(mask_frame(0x8, b"\x03\xe8"))
        except (BrokenPipeError, ConnectionResetError, OSError):
            pass
        sock.close()
        reader.join(timeout=2.0)
        if observer_sock is not None:
            try:
                observer_sock.sendall(mask_frame(0x8, b"\x03\xe8"))
            except (BrokenPipeError, ConnectionResetError, OSError):
                pass
            observer_sock.close()
            observer_reader.join(timeout=2.0)
        if late_observer_sock is not None:
            try:
                late_observer_sock.sendall(mask_frame(0x8, b"\x03\xe8"))
            except (BrokenPipeError, ConnectionResetError, OSError):
                pass
            late_observer_sock.close()
            late_observer_reader.join(timeout=2.0)

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
    if abs(float(tl.get("audio_sec", -1.0)) - audio_sec) > 0.0015:
        contract_issues.append(
            "terminal audio extent differs from streamed PCM duration")

    observer_report = {"enabled": bool(args.test_observer)}
    if args.test_observer:
        early_timeline_match = observer_final_timeline == tl
        late_timeline_match = late_observer_final_timeline == tl
        early_events_match = observer_reader.events == reader.events
        early_telemetry_match = observer_reader.telemetry == reader.telemetry
        early_ready_count = sum(
            message.get("type") == "ready"
            for message in observer_reader.messages)
        early_observer_errors = [
            message.get("error") for message in observer_reader.messages
            if message.get("type") == "error"]
        late_observer_errors = [
            message.get("error") for message in late_observer_reader.messages
            if message.get("type") == "error"]
        observer_report.update({
            "early_ready_count": early_ready_count,
            "early_event_count": len(observer_reader.events),
            "late_event_count": len(late_observer_reader.events),
            "early_telemetry_count": len(observer_reader.telemetry),
            "late_telemetry_count": len(late_observer_reader.telemetry),
            "early_events_match": early_events_match,
            "early_telemetry_match": early_telemetry_match,
            "early_terminal_match": early_timeline_match,
            "late_terminal_match": late_timeline_match,
            "producer_terminal_sha256": canonical_json_sha256(tl),
            "early_terminal_sha256": (
                canonical_json_sha256(observer_final_timeline)
                if observer_final_timeline is not None else None),
            "late_terminal_sha256": (
                canonical_json_sha256(late_observer_final_timeline)
                if late_observer_final_timeline is not None else None),
            "contender_error": contender_error,
            "unexpected_errors": early_observer_errors + late_observer_errors,
        })
        if early_ready_count < 2:
            contract_issues.append(
                "observer did not receive the producer session-start event")
        if not early_events_match:
            contract_issues.append(
                "early observer live events differ from producer events")
        if not early_telemetry_match:
            contract_issues.append(
                "early observer telemetry differs from producer telemetry")
        if not early_timeline_match:
            contract_issues.append(
                "early observer terminal timeline differs from producer")
        if not late_timeline_match:
            contract_issues.append(
                "late observer terminal timeline differs from producer")
        unexpected_errors = observer_report["unexpected_errors"]
        if unexpected_errors:
            contract_issues.append(
                "observer received unexpected server errors: " +
                ", ".join(unexpected_errors))
    config_evidence = config_provenance(start_config_snapshot, tl)
    if not config_evidence["acceptance_consistent"]:
        contract_issues.append(
            "configuration source path/hash changed or did not match "
            "the server resolved path")
    end_git_snapshot = git_metadata()
    git_stable = start_git_snapshot == end_git_snapshot
    end_binary_snapshot = config_snapshot(args.server_binary)
    binary_stable = (
        bool(start_binary_snapshot.get("sha256")) and
        start_binary_snapshot.get("path") == end_binary_snapshot.get("path") and
        start_binary_snapshot.get("sha256") ==
        end_binary_snapshot.get("sha256"))
    if not git_stable:
        contract_issues.append("Git commit or worktree changed during run")
    if not binary_stable:
        contract_issues.append("server binary path/hash changed during run")
    source_evidence = {
        "config": config_evidence,
        "git": {
            "client_pre_stream": start_git_snapshot,
            "client_post_stream": end_git_snapshot,
            "unchanged_during_client_run": git_stable,
        },
        "server_binary": {
            "client_pre_stream": start_binary_snapshot,
            "client_post_stream": end_binary_snapshot,
            "unchanged_during_client_run": binary_stable,
        },
        "acceptance_consistent": (
            config_evidence["acceptance_consistent"] and
            git_stable and binary_stable),
    }
    resolved_telemetry = (
        tl.get("resolved_config", {}).get("telemetry", {})
        if isinstance(tl.get("resolved_config"), dict) else {})
    gpu_interval_sec = resolved_telemetry.get("gpu_interval_sec")
    telemetry_report = telemetry_summary(
        reader.telemetry, device_series, audio_sec, gpu_interval_sec, 1.0)
    telemetry_required = (
        args.require_telemetry or
        (args.rate == 1.0 and audio_sec >= 120.0))
    telemetry_report["required_for_contract"] = telemetry_required
    if (telemetry_required and
            not telemetry_report["required_fields_at_least_95_percent"]):
        contract_issues.append(
            "required GPU/memory/power/CPU/RAM/temperature telemetry fields "
            "are below 95% coverage")
    
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
            "resolved_config_sha256": (
                canonical_json_sha256(tl.get("resolved_config"))
                if isinstance(tl.get("resolved_config"), dict) else None),
            "source_provenance": source_evidence,
            "telemetry_summary": telemetry_report,
        },
        "tegrastats": {
            "before": first_device_line,
            "after": last_device_line,
            "final": last_device_line,
        },
        "device_series": device_series,
        "telemetry_summary": telemetry_report,
        "command_responses": command_responses,
        "observer": observer_report,
        "events": reader.events,
        "telemetry": reader.telemetry,
        "timeline": tl,
    }
    
    with open(args.out, "w", encoding="utf-8") as f:
        json.dump(out, f, ensure_ascii=False, indent=2)
    manifest_path, _ = write_run_manifest(
        args, tl, pcm, args.out, source_evidence)

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
    print(f"  manifest={manifest_path}")
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
    ap.add_argument(
        "--config-path",
        help="server TOML path for pre/post-run provenance capture; defaults "
             "to ORATOR_CONFIG or orator.toml",
    )
    ap.add_argument(
        "--server-binary", default="build/orator_ws",
        help="server executable path for pre/post-run hash capture",
    )
    ap.add_argument("--rrd", help="Also export the timeline to a rerun .rrd "
                    "recording (offline observability; needs rerun-sdk)")
    ap.add_argument(
        "--require-telemetry", action="store_true",
        help="fail unless required telemetry fields cover at least 95%% of "
             "captured samples",
    )
    ap.add_argument("--timeline-timeout", type=float, default=600.0,
                    help="Seconds to wait for final timeline after sending end")
    ap.add_argument("--test-describe", action="store_true", help="Test describe command")
    ap.add_argument("--test-reset", action="store_true", help="Test reset command")
    ap.add_argument("--test-sessions", action="store_true", help="Test sessions command")
    ap.add_argument("--test-load-session", type=str, help="Test load_session command with session ID")
    ap.add_argument(
        "--test-observer", action="store_true",
        help="verify early and late observer connections against the producer")
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
