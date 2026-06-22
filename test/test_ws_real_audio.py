#!/usr/bin/env python3
"""
Real audio business test for orator_ws using test.mp3.
Converts MP3 to PCM and sends it via WebSocket to get ASR timeline.

Uses OratorTestHarness for Spec 004 unwrapping and assertions.
"""

import socket
import struct
import json
import time
import sys
import os
import subprocess

from ws_test_harness import OratorTestHarness

# ---------------------------------------------------------------------------
# WebSocket frame helpers (RFC 6455)
# ---------------------------------------------------------------------------

def ws_frame(opcode, payload, mask=True):
    """Build a WebSocket frame."""
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


def ws_parse_frame(data):
    """Parse a WebSocket frame, return (opcode, payload) or (None, None).

    Returns None if the frame is incomplete (not enough data).
    """
    if len(data) < 2:
        return None, None
    opcode = data[0] & 0x0F
    masked = (data[1] & 0x80) != 0
    length = data[1] & 0x7F
    offset = 2
    if length == 126:
        if len(data) < 4:
            return None, None
        length = struct.unpack('>H', data[2:4])[0]
        offset = 4
    elif length == 127:
        if len(data) < 10:
            return None, None
        length = struct.unpack('>Q', data[2:10])[0]
        offset = 10
    if masked:
        if len(data) < offset + 4:
            return None, None
        mask = data[offset:offset+4]
        offset += 4
    if len(data) < offset + length:
        return None, None  # Incomplete frame
    payload = data[offset:offset+length]
    if masked:
        payload = bytes(b ^ mask[i & 3] for i, b in enumerate(payload))
    return opcode, payload


# ---------------------------------------------------------------------------
# Test harness
# ---------------------------------------------------------------------------

def run_test(port=8765, audio_file="test.mp3"):
    """Run real audio test against orator_ws.

    Args:
        port: Server port
        audio_file: Path to audio file (MP3 or WAV)

    Returns:
        True if timeline received and valid, False otherwise.
    """
    print(f"=== Real Audio Business Test on port {port} ===\n")
    print(f"Audio file: {audio_file}")

    # Convert MP3 to PCM using ffmpeg (use first 30 seconds for testing)
    pcm_file = audio_file.replace('.mp3', '.pcm').replace('.wav', '.pcm')
    print(f"[0] Converting {audio_file} to PCM (first 30 seconds)...")
    result = subprocess.run([
        'ffmpeg', '-i', audio_file, '-t', '30',
        '-f', 's16le', '-ar', '16000', '-ac', '1',
        '-acodec', 'pcm_s16le', pcm_file, '-y'
    ], capture_output=True, text=True)

    if result.returncode != 0:
        print(f"[FAIL] ffmpeg conversion failed: {result.stderr}")
        return False
    print(f"[0] PCM file created: {pcm_file}")

    # Read PCM data
    with open(pcm_file, 'rb') as f:
        pcm_data = f.read()
    print(f"[0] PCM data size: {len(pcm_data)} bytes "
          f"({len(pcm_data)/32000:.2f} seconds)")

    # Clean up PCM file
    os.remove(pcm_file)

    # Connect to server
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(10)
    try:
        s.connect(('127.0.0.1', port))
        print("[1] TCP connected")
    except Exception as e:
        print(f"[FAIL] TCP connect failed: {e}")
        return False

    # --- HTTP upgrade ---
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
            if not c:
                break
            resp += c
        except socket.timeout:
            break
    if b"101" not in resp:
        print(f"[FAIL] Upgrade failed: {resp.decode('latin-1')[:200]}")
        return False
    print("[2] WebSocket upgrade OK (101 Switching Protocols)")

    # --- Read ready message ---
    s.settimeout(5)
    try:
        frame_data = s.recv(4096)
        opcode, payload = ws_parse_frame(frame_data)
        if opcode == 1:  # text
            ready = OratorTestHarness.unwrap(payload.decode())
            print(f"[3] Ready message received: "
                  f"sample_rate={ready.get('sample_rate')}, "
                  f"asr={ready.get('asr')}, "
                  f"protocol_version={ready.get('protocol_version')}")
        else:
            print(f"[FAIL] Expected text frame, got opcode={opcode}")
            return False
    except Exception as e:
        print(f"[FAIL] Could not read ready message: {e}")
        return False

    # --- Send PCM audio in chunks ---
    chunk_size = 32000  # 1 second of audio per chunk
    total_chunks = ((len(pcm_data) + chunk_size - 1) // chunk_size)
    print(f"[4] Sending {len(pcm_data)} bytes of PCM audio "
          f"in {total_chunks} chunks (chunk_size={chunk_size})...")

    sent_chunks = 0
    s.setblocking(False)  # Non-blocking to drain responses without waiting
    for i in range(0, len(pcm_data), chunk_size):
        chunk = pcm_data[i:i+chunk_size]
        try:
            frame = ws_frame(0x2, bytes(chunk))  # binary frame
            s.sendall(frame)
            sent_chunks += 1
            # Drain any server responses to prevent TCP buffer issues
            try:
                while True:
                    resp = s.recv(4096)
                    if not resp:
                        break
            except BlockingIOError:
                pass
            if sent_chunks % 20 == 0 or sent_chunks == total_chunks:
                print(f"  Sent {sent_chunks}/{total_chunks} chunks")
            time.sleep(0.01)  # Small delay to avoid overwhelming the server
        except BrokenPipeError:
            print(f"[FAIL] Connection closed after sending "
                  f"{sent_chunks}/{total_chunks} chunks")
            try:
                s.settimeout(1)
                error_data = s.recv(4096)
                if error_data:
                    opcode, payload = ws_parse_frame(error_data)
                    if opcode == 8:
                        print(f"  Server sent close frame (opcode=8)")
                    elif opcode == 1:
                        print(f"  Server sent message: {payload.decode()}")
            except Exception:
                pass
            return False

    print(f"[4] All {total_chunks} chunks sent")

    # --- Send flush command to get timeline ---
    s.setblocking(True)
    s.sendall(ws_frame(0x1, b'{"flush"}'))
    print("[5] Sent flush command")

    # --- Wait for timeline response (with frame buffering) ---
    time.sleep(2)  # Give server time to process
    s.settimeout(5)
    timeline_received = False
    timeline_data = None
    buf = b""
    deadline = time.time() + 10
    try:
        while time.time() < deadline and not timeline_received:
            try:
                chunk = s.recv(4096)
                if not chunk:
                    break
                buf += chunk
            except socket.timeout:
                pass  # Continue trying until deadline
            # Parse all complete frames from buffer
            while True:
                opcode, payload = ws_parse_frame(buf)
                if opcode is None:
                    break  # Incomplete frame
                # Compute frame size to advance buffer
                if len(buf) >= 2:
                    hdr_len = 2
                    n = buf[1] & 0x7F
                    if n == 126:
                        hdr_len += 2
                    elif n == 127:
                        hdr_len += 8
                    if buf[1] & 0x80:
                        hdr_len += 4
                    frame_size = hdr_len + len(payload)
                    buf = buf[frame_size:]
                else:
                    break
                if opcode == 1:  # text
                    data = OratorTestHarness.unwrap(payload.decode())
                    print(f"[6] Response received: "
                          f"type={data.get('type', 'unknown')}")
                    if data.get('type') == 'timeline':
                        timeline_data = data
                        timeline_received = True
                        try:
                            OratorTestHarness.assert_timeline_valid(data)
                            print("  Timeline structure validation passed")
                        except AssertionError as e:
                            print(f"  Timeline validation failed: {e}")
                            timeline_received = False
                        break
                elif opcode == 8:  # close
                    print(f"[6] Server closed connection (opcode=8)")
                    break
    except Exception as e:
        print(f"[6] Error reading timeline: {e}")

    if not timeline_received:
        print("[6] No timeline response (timeout)")

    # --- Send end command ---
    s.settimeout(3)
    try:
        s.sendall(ws_frame(0x1, b'{"end"}'))
        print("[7] Sent end command")
    except Exception as e:
        print(f"[7] Failed to send end: {e}")

    # --- Wait for final timeline (with frame buffering) ---
    time.sleep(2)
    s.settimeout(5)
    buf = b""
    deadline = time.time() + 10
    try:
        while time.time() < deadline and not timeline_received:
            try:
                chunk = s.recv(4096)
                if not chunk:
                    break
                buf += chunk
            except socket.timeout:
                pass
            while True:
                opcode, payload = ws_parse_frame(buf)
                if opcode is None:
                    break
                if len(buf) >= 2:
                    hdr_len = 2
                    n = buf[1] & 0x7F
                    if n == 126:
                        hdr_len += 2
                    elif n == 127:
                        hdr_len += 8
                    if buf[1] & 0x80:
                        hdr_len += 4
                    frame_size = hdr_len + len(payload)
                    buf = buf[frame_size:]
                else:
                    break
                if opcode == 1:
                    data = OratorTestHarness.unwrap(payload.decode())
                    print(f"[8] Final response: type={data.get('type', 'unknown')}")
                    if data.get('type') == 'timeline':
                        timeline_data = data
                        timeline_received = True
                        break
                elif opcode == 8:
                    print("[8] Server closed connection after end")
                    break
    except Exception as e:
        print(f"[8] Error reading final timeline: {e}")

    if not timeline_received:
        print("[8] No final timeline (timeout)")

    # --- Close connection ---
    try:
        s.sendall(ws_frame(0x8, b''))  # close frame
        print("[9] Sent close frame")
    except Exception:
        pass
    s.close()

    if timeline_received and timeline_data:
        print("\n=== Test completed successfully ===")
        return True
    else:
        print("\n=== Test FAILED: no timeline received ===")
        return False


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8765
    audio_file = sys.argv[2] if len(sys.argv) > 2 else "test.mp3"
    success = run_test(port, audio_file)
    sys.exit(0 if success else 1)
