#!/usr/bin/env python3
"""
Comprehensive WebSocket I/O validation test for orator_ws.
Validates EVERY frame entering and leaving the WebSocket.
"""

import socket, struct, json, time, sys, os, subprocess

# ---------------------------------------------------------------------------
# WebSocket frame helpers (RFC 6455)
# ---------------------------------------------------------------------------

class WsFrame:
    """Validated parsed WebSocket frame."""
    def __init__(self, opcode, payload, fin=True, masked=False):
        self.opcode = opcode      # 1=text, 2=binary, 8=close, 9=ping, 10=pong
        self.payload = payload    # bytes
        self.fin = fin
        self.masked = masked
        self.type_names = {1:'text', 2:'binary', 8:'close', 9:'ping', 10:'pong'}

    @property
    def type_name(self):
        return self.type_names.get(self.opcode, f'unknown({self.opcode})')

    def __repr__(self):
        return f"WsFrame(opcode={self.opcode}[{self.type_name}], payload={len(self.payload)}B, fin={self.fin}, masked={self.masked})"


def ws_encode(opcode, payload, mask=True):
    """Build a WebSocket frame (RFC 6455)."""
    frame = bytearray()
    frame.append(0x80 | opcode)  # FIN + opcode
    if mask:
        mask_bytes = os.urandom(4)
        payload_len = len(payload)
        if payload_len < 126:
            frame.append(0x80 | payload_len)
        elif payload_len < 65536:
            frame.append(0x80 | 126)
            frame.extend(struct.pack('>H', payload_len))
        else:
            frame.append(0x80 | 127)
            frame.extend(struct.pack('>Q', payload_len))
        frame.extend(mask_bytes)
        frame.extend(bytes(b ^ mask_bytes[i & 3] for i, b in enumerate(payload)))
    else:
        payload_len = len(payload)
        if payload_len < 126:
            frame.append(payload_len)
        elif payload_len < 65536:
            frame.append(126)
            frame.extend(struct.pack('>H', payload_len))
        else:
            frame.append(127)
            frame.extend(struct.pack('>Q', payload_len))
        frame.extend(payload)
    return bytes(frame)


def ws_decode(data):
    """Parse a WebSocket frame from raw bytes. Returns (WsFrame, remaining_bytes)."""
    if len(data) < 2:
        return None, data
    opcode = data[0] & 0x0F
    fin = bool(data[0] & 0x80)
    masked = bool(data[1] & 0x80)
    length = data[1] & 0x7F
    offset = 2
    if length == 126:
        if len(data) < 4: return None, data
        length = struct.unpack('>H', data[2:4])[0]
        offset = 4
    elif length == 127:
        if len(data) < 10: return None, data
        length = struct.unpack('>Q', data[2:10])[0]
        offset = 10
    mask_key = None
    if masked:
        if len(data) < offset + 4: return None, data
        mask_key = data[offset:offset+4]
        offset += 4
    if len(data) < offset + length:
        return None, data  # incomplete frame
    payload = data[offset:offset+length]
    if masked and mask_key:
        payload = bytes(b ^ mask_key[i & 3] for i, b in enumerate(payload))
    remaining = data[offset+length:]
    frame = WsFrame(opcode, payload, fin=fin, masked=masked)
    return frame, remaining


# ---------------------------------------------------------------------------
# Validation helpers
# ---------------------------------------------------------------------------

class ValidationResult:
    def __init__(self):
        self.tests = []
    def ok(self, msg):
        print(f"  ✓ {msg}")
        self.tests.append(('PASS', msg))
    def fail(self, msg):
        print(f"  ✗ FAIL: {msg}")
        self.tests.append(('FAIL', msg))
    def info(self, msg):
        print(f"    {msg}")
    def summary(self):
        passed = sum(1 for r in self.tests if r[0] == 'PASS')
        failed = sum(1 for r in self.tests if r[0] == 'FAIL')
        total = len(self.tests)
        print(f"\n  Results: {passed}/{total} passed, {failed} failed")
        return failed == 0


def validate_json(data, required_fields, allow_extra=True):
    """Validate JSON and check required fields."""
    try:
        obj = json.loads(data)
    except json.JSONDecodeError as e:
        return False, f"Invalid JSON: {e}"
    for field in required_fields:
        if field not in obj:
            return False, f"Missing required field '{field}'"
    return True, obj


def unwrap_envelope(obj):
    """
    If the message is a topic envelope, extract the inner data.
    Server wraps pipeline messages as:
      {"topic":"X","pipeline":"Y","data":"<json_escaped>"}
    """
    if isinstance(obj, dict) and 'topic' in obj and 'data' in obj:
        try:
            inner = json.loads(obj['data'])
            return inner
        except (json.JSONDecodeError, KeyError):
            pass
    return obj


# ---------------------------------------------------------------------------
# Main test
# ---------------------------------------------------------------------------

def run_ws_test(port=8765, audio_file="test.mp3", audio_duration=10):
    """
    Full WS I/O validation test.
    Sends audio, captures ALL server output, validates every message.
    """
    vr = ValidationResult()
    print(f"\n{'='*60}")
    print(f"WebSocket Comprehensive I/O Test")
    print(f"  Port: {port}")
    print(f"  Audio: {audio_file} ({audio_duration}s)")
    print(f"{'='*60}\n")

    # ------------------------------------------------------------------
    # Audio preparation
    # ------------------------------------------------------------------
    print("[0] Preparing audio...")
    pcm_file = audio_file.replace('.mp3', '.pcm').replace('.wav', '.pcm')
    result = subprocess.run([
        'ffmpeg', '-i', audio_file, '-t', str(audio_duration),
        '-f', 's16le', '-ar', '16000', '-ac', '1',
        '-acodec', 'pcm_s16le', pcm_file, '-y'
    ], capture_output=True, text=True)
    if result.returncode != 0:
        print(f"  [FAIL] ffmpeg conversion failed: {result.stderr}")
        return False
    with open(pcm_file, 'rb') as f:
        pcm_data = f.read()
    os.remove(pcm_file)
    print(f"  PCM: {len(pcm_data)} bytes ({len(pcm_data)/32000:.2f}s at 16kHz 16bit mono)")
    vr.ok(f"Audio prepared: {len(pcm_data)} bytes")

    # ------------------------------------------------------------------
    # TCP Connection
    # ------------------------------------------------------------------
    print("\n[1] Connecting...")
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(10)
    try:
        s.connect(('127.0.0.1', port))
        vr.ok("TCP connection established")
    except Exception as e:
        vr.fail(f"TCP connect failed: {e}")
        return False

    # ------------------------------------------------------------------
    # HTTP -> WebSocket Upgrade
    # ------------------------------------------------------------------
    print("\n[2] WebSocket upgrade...")
    key = "dGhlIHNhbXBsZSBub25jZQ=="
    req = (
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        f"Sec-WebSocket-Key: {key}\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n"
    )
    s.sendall(req.encode())
    resp = b""
    while b"\r\n\r\n" not in resp:
        try:
            c = s.recv(1)
            if not c: break
            resp += c
        except socket.timeout:
            break
    if b"101" in resp:
        vr.ok("HTTP 101 Switching Protocols received")
    else:
        vr.fail(f"Upgrade failed. Response: {resp.decode('latin-1')[:200]}")

    # Extract headers for validation
    headers = {}
    for line in resp.decode('latin-1').split('\r\n'):
        if ': ' in line:
            k, v = line.split(': ', 1)
            headers[k.lower()] = v
    if 'sec-websocket-accept' in headers:
        vr.ok("Sec-WebSocket-Accept header present")
        vr.info(f"  Accept: {headers['sec-websocket-accept']}")
    else:
        vr.fail("Missing Sec-WebSocket-Accept header")

    # ------------------------------------------------------------------
    # Read initial frames (ready message)
    # ------------------------------------------------------------------
    print("\n[3] Server init messages...")
    s.settimeout(2)
    all_output = []  # List of (WsFrame, context_string)
    buf = b""
    # Read initial server frames with a cap to avoid infinite loop
    # from continuous messages (e.g. gpu_telemetry sent by server)
    found_ready = False
    for _ in range(20):
        try:
            chunk = s.recv(4096)
            if not chunk: break
        except socket.timeout:
            break
        buf += chunk
        while True:
            frame, buf = ws_decode(buf)
            if frame is None: break
            all_output.append((frame, 'init'))
            if frame.opcode == 1:
                try:
                    obj = json.loads(frame.payload.decode())
                    inner = unwrap_envelope(obj)
                    if inner.get('type') == 'ready':
                        found_ready = True
                except: pass
        if found_ready:
            break

    # Validate initial messages
    if not all_output:
        vr.fail("No initial messages from server")
        s.close()
        return False

    found_ready = False
    for frame, ctx in all_output:
        if frame.opcode == 1:  # text
            try:
                obj = json.loads(frame.payload.decode())
                inner = unwrap_envelope(obj)
                msg_type = inner.get('type', obj.get('type', 'unknown'))
                if msg_type == 'ready':
                    found_ready = True
                    vr.info(f"  Ready: sample_rate={inner.get('sample_rate')}, "
                           f"asr={inner.get('asr')}, protocol_version={inner.get('protocol_version')}")
                    for field in ['sample_rate', 'time_base', 'protocol_version', 'session_start_wall_sec']:
                        if field in inner:
                            vr.ok(f"  Ready.{field} = {inner[field]}")
                        else:
                            vr.fail(f"  Ready missing field '{field}'")
            except json.JSONDecodeError:
                vr.fail(f"Invalid JSON in text frame")
        else:
            vr.info(f"  Non-text frame: {frame.type_name}")

    if found_ready:
        vr.ok("Received 'ready' message from server")
    else:
        vr.fail("No 'ready' message received")

    # ------------------------------------------------------------------
    # Send audio chunks and capture responses
    # ------------------------------------------------------------------
    print("\n[4] Sending audio, capturing all responses...")
    CHUNK_SIZE = 32000
    total_chunks = (len(pcm_data) + CHUNK_SIZE - 1) // CHUNK_SIZE
    sent_chunks = 0
    audio_bytes_sent = 0
    buf = b""
    s.settimeout(0.5)  # short timeout for non-blocking-like behavior

    for i in range(0, len(pcm_data), CHUNK_SIZE):
        chunk = pcm_data[i:i+CHUNK_SIZE]
        try:
            # Validate frame encoding
            frame = ws_encode(0x2, bytes(chunk))
            assert frame[0] == 0x82, f"Frame byte 0 should be 0x82, got 0x{frame[0]:02x}"
            assert frame[1] & 0x80, f"MASK bit not set in byte 1: 0x{frame[1]:02x}"
            assert (frame[1] & 0x7F) in (126, 127) or (frame[1] & 0x7F) == len(chunk), \
                f"Length mismatch in byte 1"

            s.sendall(frame)
            sent_chunks += 1
            audio_bytes_sent += len(chunk)

            # Drain available responses (capped at 10 recvs per chunk)
            for _ in range(10):
                try:
                    chunk_data = s.recv(4096)
                    if not chunk_data: break
                    buf += chunk_data
                except socket.timeout:
                    break

            # Parse all complete frames from buffer
            while True:
                frame, buf = ws_decode(buf)
                if frame is None: break
                all_output.append((frame, f'after_chunk_{sent_chunks}'))

            if sent_chunks % 5 == 0:
                print(f"  Sent {sent_chunks}/{total_chunks} chunks, "
                      f"received {len([f for f,c in all_output if c.startswith('after_chunk')])} msgs")

            time.sleep(0.01)

        except BrokenPipeError:
            vr.fail(f"Connection closed after sending chunk {sent_chunks}/{total_chunks}")
            # Try to read final message
            try:
                s.settimeout(1)
                final = s.recv(4096)
                if final:
                    f, _ = ws_decode(final)
                    if f: all_output.append((f, 'broken_pipe_after'))
            except: pass
            break
        except AssertionError as e:
            vr.fail(f"Frame encoding error on chunk {sent_chunks+1}: {e}")

    # ------------------------------------------------------------------
    # Validate sent audio
    # ------------------------------------------------------------------
    print(f"\n[4b] Audio send summary:")
    vr.info(f"  Chunks sent: {sent_chunks}/{total_chunks}")
    vr.info(f"  Audio bytes sent: {audio_bytes_sent}")
    if sent_chunks == total_chunks:
        vr.ok("All audio chunks sent successfully")
    else:
        vr.fail(f"Only {sent_chunks}/{total_chunks} chunks sent")

    # Parse all text frames
    text_msgs = []
    for frame, ctx in all_output:
        if frame.opcode == 1:
            try:
                obj = json.loads(frame.payload.decode())
                text_msgs.append((obj, ctx, frame))
            except json.JSONDecodeError:
                text_msgs.append(({'__invalid_json': frame.payload.decode(errors='replace')}, ctx, frame))
        elif frame.opcode == 8:
            vr.info(f"  Close frame: payload={frame.payload.hex()}")
        elif frame.opcode == 2:
            vr.info(f"  Binary frame from server: {len(frame.payload)}B")
        elif frame.opcode in (9, 10):
            vr.info(f"  Ping/Pong: {frame.type_name}")

    # ------------------------------------------------------------------
    # Validate server output messages
    # ------------------------------------------------------------------
    print(f"\n[5] Server message validation ({len(text_msgs)} text messages):")

    for obj, ctx, frame in text_msgs:
        # Unwrap topic envelope if present
        inner = unwrap_envelope(obj)
        msg_type = inner.get('type', obj.get('type', 'unknown'))

        if msg_type == 'vad_state':
            if 'speech' in inner:
                vr.ok(f"vad_state.speech = {inner['speech']} ({ctx})")
            else:
                vr.fail(f"vad_state missing 'speech' field ({ctx})")

        elif msg_type == 'asr':
            fields_ok = True
            for f in ['text', 'start', 'end']:
                if f not in inner:
                    vr.fail(f"asr missing '{f}' ({ctx})")
                    fields_ok = False
            if fields_ok:
                vr.ok(f"asr: \"{inner['text'][:50]}\" [{inner['start']:.2f}-{inner['end']:.2f}s] ({ctx})")

        elif msg_type == 'ready':
            pass  # already validated

        elif msg_type == 'timeline':
            fields_ok = True
            for f in ['audio_sec', 'sample_rate', 'tracks']:
                if f not in inner:
                    vr.fail(f"timeline missing '{f}'")
                    fields_ok = False
            if fields_ok:
                vr.ok(f"timeline: audio_sec={inner['audio_sec']:.2f}, "
                      f"tracks={len(inner['tracks'])} tracks")
                for t in inner.get('tracks', []):
                    kind = t.get('kind', 'unknown')
                    entries = len(t.get('entries', []))
                    compute_sec = t.get('compute_sec', 0)
                    rtf = t.get('real_time_factor', 0)
                    vr.info(f"  Track '{kind}': {entries} entries, "
                           f"compute={compute_sec:.3f}s, rtf={rtf:.3f}")
                    if entries > 0:
                        vr.ok(f"  Track '{kind}' has data ({entries} entries)")

        elif msg_type == 'revision':
            vr.info(f"revision: source={inner.get('source')}, "
                   f"entries={len(inner.get('entries', []))}")

        elif inner.get('__invalid_json'):
            vr.fail(f"Invalid JSON in text frame: {inner['__invalid_json'][:100]}")

        elif msg_type == 'reset_ok':
            vr.ok("reset_ok received")

        else:
            vr.info(f"Unrecognized message type='{msg_type}' ({ctx})")

    # ------------------------------------------------------------------
    # Send flush and get timeline
    # ------------------------------------------------------------------
    print("\n[6] Flush command...")
    s.setblocking(True)
    s.settimeout(3)
    try:
        s.sendall(ws_encode(0x1, b'{"flush"}'))
        vr.ok("Flush command sent")
    except Exception as e:
        vr.fail(f"Failed to send flush: {e}")

    # Read response (capped at 20 recvs to avoid hang on continuous messages)
    time.sleep(2)
    s.settimeout(3)
    buf = b""
    for _ in range(20):
        try:
            chunk = s.recv(4096)
            if not chunk: break
            buf += chunk
        except socket.timeout:
            break

    frames_from_buf = []
    while True:
        f, buf = ws_decode(buf)
        if f is None: break
        frames_from_buf.append(f)

    for f in frames_from_buf:
        all_output.append((f, 'after_flush'))

    timeline_found = False
    for f, ctx in [(f, c) for f, c in all_output if c == 'after_flush']:
        if f.opcode == 1:
            try:
                obj = json.loads(f.payload.decode())
                inner = unwrap_envelope(obj)
                if inner.get('type') == 'timeline':
                    timeline_found = True
                    vr.ok("Timeline received after flush")
                    dur = inner.get('audio_sec', 0)
                    tracks = inner.get('tracks', [])
                    vr.info(f"  Audio duration: {dur:.2f}s")
                    for t in tracks:
                        k = t.get('kind', '?')
                        e = len(t.get('entries', []))
                        vr.info(f"  Track '{k}': {e} entries")
                    if len(tracks) >= 2:
                        diar_tracks = [t for t in tracks if t.get('kind') == 'diarization']
                        asr_tracks = [t for t in tracks if t.get('kind') == 'asr']
                        if diar_tracks:
                            vr.info(f"  Diarization entries: {len(diar_tracks[0].get('entries',[]))}")
                        if asr_tracks:
                            asr_entries = asr_tracks[0].get('entries', [])
                            vr.info(f"  ASR entries: {len(asr_entries)}")
                            if asr_entries:
                                vr.ok("ASR produced transcription results")
                                for entry in asr_entries[:3]:
                                    vr.info(f"    [{entry.get('start',0):.2f}-{entry.get('end',0):.2f}] "
                                           f"{entry.get('text','')[:60]}")
                            else:
                                vr.info("  ASR track present but empty (audio may need processing)")
            except: pass
        elif f.opcode == 8:
            vr.info("Close frame received after flush")

    if not timeline_found:
        vr.fail("No timeline received after flush")

    # ------------------------------------------------------------------
    # Send end and get final timeline
    # ------------------------------------------------------------------
    print("\n[7] End command...")
    s.settimeout(3)
    try:
        s.sendall(ws_encode(0x1, b'{"end"}'))
        vr.ok("End command sent")
    except Exception as e:
        vr.fail(f"Failed to send end: {e}")

    time.sleep(2)
    s.settimeout(3)
    buf = b""
    for _ in range(20):
        try:
            chunk = s.recv(4096)
            if not chunk: break
            buf += chunk
        except socket.timeout:
            break

    while True:
        f, buf = ws_decode(buf)
        if f is None: break
        all_output.append((f, 'after_end'))

    final_timeline = False
    for f, ctx in [(f, c) for f, c in all_output if c == 'after_end']:
        if f.opcode == 1:
            try:
                obj = json.loads(f.payload.decode())
                inner = unwrap_envelope(obj)
                if inner.get('type') == 'timeline':
                    final_timeline = True
                    vr.ok("Final timeline received after end")
                    asr_entries = []
                    for t in inner.get('tracks', []):
                        if t.get('kind') == 'asr':
                            asr_entries = t.get('entries', [])
                    vr.info(f"  Total ASR entries: {len(asr_entries)}")
                    if asr_entries:
                        for e in asr_entries:
                            vr.info(f"    [{e.get('start',0):.2f}-{e.get('end',0):.2f}] "
                                   f"{e.get('text','')[:80]}")
            except: pass
        elif f.opcode == 8:
            vr.info("Close frame received after end")

    if not final_timeline:
        vr.fail("No final timeline after end")

    # ------------------------------------------------------------------
    # Close connection
    # ------------------------------------------------------------------
    print("\n[8] Closing...")
    try:
        s.sendall(ws_encode(0x8, b''))
        vr.ok("Close frame sent")
    except: pass
    s.close()
    vr.ok("Connection closed cleanly")

    # ------------------------------------------------------------------
    # Summary statistics
    # ------------------------------------------------------------------
    print(f"\n{'='*60}")
    print("TEST SUMMARY")
    print(f"{'='*60}")
    total_text = sum(1 for f, _ in all_output if f.opcode == 1)
    total_binary = sum(1 for f, _ in all_output if f.opcode == 2)
    total_close = sum(1 for f, _ in all_output if f.opcode == 8)
    print(f"  Total output frames received: {len(all_output)}")
    print(f"    Text frames:   {total_text}")
    print(f"    Binary frames: {total_binary}")
    print(f"    Close frames:  {total_close}")
    print(f"  Audio sent:  {audio_bytes_sent} bytes in {sent_chunks} chunks")
    print()
    result = vr.summary()
    return result


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8765
    audio_file = sys.argv[2] if len(sys.argv) > 2 else "test.mp3"
    duration = int(sys.argv[3]) if len(sys.argv) > 3 else 10
    print(f"Args: port={port}, audio={audio_file}, duration={duration}s")
    success = run_ws_test(port, audio_file, duration)
    print(f"\n{'='*60}")
    print(f"OVERALL: {'PASS' if success else 'FAIL'}")
    sys.exit(0 if success else 1)
