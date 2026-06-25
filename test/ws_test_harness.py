#!/usr/bin/env python3
"""Reusable test harness for orator_ws integration tests.

Provides server management, Spec 004 envelope unwrapping, paced push, and
assertion helpers.

Usage:
    with OratorTestHarness(port=18765,
                           diarizer="models/sortformer_4spk_v2.safetensors",
                           asr="models/asr/Qwen/Qwen3-ASR-1.7B") as h:
        ws = h.connect()
        h.push_audio(ws, pcm_data, pace=1.0)
        ws.send(json.dumps({'end': True}))
        timeline = h.collect_timeline(ws, timeout=60)
        h.assert_timeline_valid(timeline)
        h.assert_diarization(timeline, min_segments=1)
"""

import atexit
import json
import os
import signal
import socket
import subprocess
import time
from typing import Optional

DEFAULT_PORT = 18765
SERVER_BIN = "./build/orator_ws"


class OratorTestHarness:
    """Manages orator_ws server lifecycle for testing.

    Attributes:
        port: Server port
        server_pid: PID of the managed server process
        sid: Server identity string (PID + start time)
    """

    def __init__(self, port: int = DEFAULT_PORT, diarizer: str = "",
                 asr: str = "", vad: str = "models/vad/silero_vad.safetensors",
                 ui_port: Optional[int] = None,
                 config_overrides: Optional[dict] = None):
        self.port = port
        self.ui_port = ui_port or port + 1
        self.diarizer = diarizer
        self.asr = asr
        self.vad = vad
        self.config_overrides = config_overrides or {}
        self._config_file: Optional[str] = None
        self.proc: Optional[subprocess.Popen] = None
        self.server_pid: Optional[int] = None
        self._cleanup_done = False

    def _ensure_port_free(self):
        """Kill any process on our port to avoid stale server connections."""
        try:
            import subprocess as sp
            sp.run(['fuser', '-k', f'{self.port}/tcp'],
                   capture_output=True, timeout=3)
            time.sleep(1)
        except:
            pass
    
    def start(self, wait_sec: float = 30.0) -> 'OratorTestHarness':
        """Start server, wait for TCP readiness, return self for 'with' usage."""
        self._ensure_port_free()
        env = os.environ.copy()
        env['ORATOR_UI_PORT'] = str(self.ui_port)
        env['ORATOR_GPU_TELEMETRY_SEC'] = '0'
        if self.vad:
            env['ORATOR_VAD_MODEL'] = self.vad
        if self.config_overrides:
            # Map Config struct field names to TOML key names.
            FIELD_TO_TOML = {
                'diar_threshold': ('diarizer', 'threshold'),
                'diar_merge_gap_sec': ('diarizer', 'merge_gap_sec'),
                'diar_deliver_interval_sec': ('diarizer', 'deliver_interval_sec'),
                'diar_spkcache_len': ('diarizer', 'spkcache_len'),
                'diar_chunk_len': ('diarizer', 'chunk_len'),
                'diar_spkcache_update_period': ('diarizer', 'spkcache_update_period'),
                'diar_chunk_left_context': ('diarizer', 'chunk_left_context'),
                'diar_chunk_right_context': ('diarizer', 'chunk_right_context'),
                'diar_spkcache_sil_frames': ('diarizer', 'spkcache_sil_frames'),
                'diar_onset': ('diarizer', 'onset'),
                'diar_offset': ('diarizer', 'offset'),
                'diar_pad_onset': ('diarizer', 'pad_onset'),
                'diar_pad_offset': ('diarizer', 'pad_offset'),
                'diar_min_dur_on': ('diarizer', 'min_dur_on'),
                'diar_min_dur_off': ('diarizer', 'min_dur_off'),
            }
            # Group overrides by TOML section
            sections = {}
            for field, v in self.config_overrides.items():
                if field in FIELD_TO_TOML:
                    sec, key = FIELD_TO_TOML[field]
                    sections.setdefault(sec, {})[key] = v
            # Write temp config with proper TOML keys
            if sections:
                import tempfile
                self._config_file = os.path.join(tempfile.gettempdir(), f'orator_test_{os.getpid()}.toml')
                lines = ['[server]', f'port = {self.port}']
                # Diarizer section: model path + overrides
                if self.diarizer:
                    lines.append('[diarizer]')
                    lines.append(f'model = "{self.diarizer}"')
                    # Add diarizer overrides in same section (no duplicate header)
                    if 'diarizer' in sections:
                        for k, v in sections['diarizer'].items():
                            if isinstance(v, str): lines.append(f'{k} = "{v}"')
                            elif isinstance(v, bool): lines.append(f'{k} = {"true" if v else "false"}')
                            else: lines.append(f'{k} = {v}')
                if 'asr' in sections:
                    lines.append('[asr]')
                    for k, v in sections['asr'].items():
                        if isinstance(v, str): lines.append(f'{k} = "{v}"')
                        else: lines.append(f'{k} = {v}')
                if self.asr:
                    if 'asr' not in sections:
                        lines.append('[asr]')
                    lines.append(f'model_dir = "{self.asr}"')
                if self.vad:
                    lines.append('[vad]')
                    lines.append(f'model = "{self.vad}"')
                    lines.append('stream = true')
                with open(self._config_file, 'w') as f:
                    f.write('\n'.join(lines) + '\n')
                env['ORATOR_CONFIG'] = self._config_file
            with open(self._config_file, 'w') as f:
                f.write('\n'.join(lines) + '\n')
            env['ORATOR_CONFIG'] = self._config_file

        cmd = [SERVER_BIN, str(self.port)]
        if self.diarizer:
            cmd.append(self.diarizer)
        else:
            cmd.append("")
        if self.asr:
            cmd.append(self.asr)
        else:
            cmd.append("")

        self.proc = subprocess.Popen(
            cmd,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            env=env,
        )
        self.server_pid = self.proc.pid
        atexit.register(self.cleanup)

        # Wait for TCP readiness
        deadline = time.time() + wait_sec
        while time.time() < deadline:
            try:
                with socket.create_connection(('127.0.0.1', self.port), timeout=1):
                    return self
            except (ConnectionRefusedError, OSError):
                if self.proc.poll() is not None:
                    raise RuntimeError(
                        f'Server died on startup (PID={self.server_pid})')
                time.sleep(1)
        raise TimeoutError(f'Server not ready within {wait_sec}s')

    def cleanup(self):
        """Terminate the server process if still running."""
        if self._cleanup_done or self.proc is None:
            return
        self._cleanup_done = True
        if self.proc.poll() is None:
            self.proc.terminate()
            try:
                self.proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait()
        atexit.unregister(self.cleanup)

    def __enter__(self):
        return self.start()

    def __exit__(self, *args):
        self.cleanup()

    def connect(self, timeout: float = 10.0):
        """Create a WebSocket connection to the server.

        Returns a websocket.WebSocket instance connected and ready.
        """
        import websocket
        ws = websocket.WebSocket()
        ws.settimeout(timeout)
        ws.connect(f'ws://127.0.0.1:{self.port}')
        return ws

    @staticmethod
    def unwrap(msg: str) -> dict:
        """Unwrap Spec 004 protocol envelope.

        Server wraps pipeline messages as:
          {"topic":"X","pipeline":"Y","data":"<json_escaped>"}
        Returns the inner data dict with '_topic' key added.
        """
        d = json.loads(msg)
        if 'data' in d and isinstance(d['data'], str):
            try:
                inner = json.loads(d['data'])
                inner['_topic'] = d.get('topic', '')
                return inner
            except (json.JSONDecodeError, TypeError):
                pass
        return d

    @staticmethod
    def push_audio(ws, pcm_data: bytes, pace: float = 1.0,
                   chunk_ms: int = 1000):
        """Push PCM audio at specified pace.

        Args:
            ws: websocket.WebSocket connection
            pcm_data: int16 mono 16kHz PCM bytes
            pace: 1.0 = real-time, 2.0 = 2x, 0 = max (no pacing)
            chunk_ms: milliseconds of audio per chunk
        """
        chunk_bytes = chunk_ms * 16  # 16 bytes per ms at 16kHz 16bit
        t0 = time.time()
        sent = 0
        while sent < len(pcm_data):
            chunk = pcm_data[sent:sent + chunk_bytes]
            ws.send(chunk, opcode=0x02)
            sent += len(chunk)
            if pace > 0:
                elapsed = time.time() - t0
                target_wall = (sent / 32000) / pace
                if elapsed < target_wall:
                    time.sleep(target_wall - elapsed)

    @staticmethod
    def collect_timeline(ws, timeout: float = 60.0) -> Optional[dict]:
        """Collect messages until timeline received or timeout.

        Returns the timeline dict, or None if timeout.
        """
        ws.settimeout(0.5)
        t0 = time.time()
        while time.time() - t0 < timeout:
            try:
                m = ws.recv()
                d = OratorTestHarness.unwrap(m)
                if d.get('type') == 'timeline':
                    return d
            except Exception:
                continue
        return None

    @staticmethod
    def assert_timeline_valid(tl: dict, check_wall_clock: bool = True):
        """Assert timeline has required structure.

        Args:
            tl: Timeline dict (raw or unwrapped).
            check_wall_clock: If True, assert wall_clock_ok is True.
                              Pass False for end timelines.
        """
        assert tl is not None, 'No timeline received'
        t = tl.get('timeline', tl)
        assert 'tracks' in t, f'Timeline missing tracks: {list(t.keys())}'
        assert t.get('audio_sec', 0) > 0, 'audio_sec is 0'
        assert 'wall_clock_ok' in t, 'Timeline missing wall_clock_ok'
        if check_wall_clock:
            assert t.get('wall_clock_ok', False) is True, 'wall_clock_ok is False'

    @staticmethod
    def get_track(tl: dict, kind: str) -> dict:
        """Get a track by kind from timeline."""
        t = tl.get('timeline', tl)
        for trk in t.get('tracks', []):
            if trk.get('kind') == kind:
                return trk
        return {}

    @staticmethod
    def assert_diarization(tl: dict, min_segments: int = 1):
        """Assert diarization produced at least min_segments."""
        trk = OratorTestHarness.get_track(tl, 'diarization')
        entries = trk.get('entries', [])
        assert len(entries) >= min_segments, \
            (f'Diarization: expected >= {min_segments} segments, '
             f'got {len(entries)}')

    @staticmethod
    def assert_asr(tl: dict, min_utterances: int = 1):
        """Assert ASR produced at least min_utterances."""
        trk = OratorTestHarness.get_track(tl, 'asr')
        entries = trk.get('entries', [])
        assert len(entries) >= min_utterances, \
            (f'ASR: expected >= {min_utterances} utterances, '
             f'got {len(entries)}')

    @staticmethod
    def assert_speakers(tl: dict, min_speakers: int = 1):
        """Assert at least min_speakers identified."""
        t = tl.get('timeline', tl)
        comp = t.get('comprehensive', [])
        speakers = set()
        for c in comp:
            s = c.get('speaker', '?')
            if isinstance(s, int) and s >= 0:
                speakers.add(s)
        assert len(speakers) >= min_speakers, \
            (f'Speakers: expected >= {min_speakers}, '
             f'got {len(speakers)} ({speakers})')
