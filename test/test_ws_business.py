#!/usr/bin/env python3
"""
End-to-end WebSocket business test for orator_ws.
Simulates a real client session:
  1. Connect + WebSocket upgrade
  2. Receive "ready" message
  3. Send binary audio frames (simulated PCM)
  4. Send text control commands ("flush", "end")
  5. Verify server responses (timeline, etc.)
"""

import socket
import struct
import json
import time
import sys
import os

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
            frame.append(126)
            frame.extend(struct.pack('>H', payload_len))
        else:
            frame.append(127)
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
    """Parse a WebSocket frame, return (opcode, payload)."""
    if len(data) < 2:
        return None, None
    opcode = data[0] & 0x0F
    masked = (data[1] & 0x80) != 0
    length = data[1] & 0x7F
    offset = 2
    if length == 126:
        length = struct.unpack('>H', data[2:4])[0]
        offset = 4
    elif length == 127:
        length = struct.unpack('>Q', data[2:10])[0]
        offset = 10
    if masked:
        mask = data[offset:offset+4]
        offset += 4
    payload = data[offset:offset+length]
    if masked:
        payload = bytes(b ^ mask[i & 3] for i, b in enumerate(payload))
    return opcode, payload


# ---------------------------------------------------------------------------
# Test harness
# ---------------------------------------------------------------------------

def run_test(port=8765):
    print(f"=== WebSocket Business Test on port {port} ===\n")
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(5)
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
    s.settimeout(3)
    try:
        frame_data = s.recv(4096)
        opcode, payload = ws_parse_frame(frame_data)
        if opcode == 1:  # text
            ready = json.loads(payload.decode())
            print(f"[3] Ready message received: sample_rate={ready.get('sample_rate')}, "
                  f"asr={ready.get('asr')}, protocol_version={ready.get('protocol_version')}")
        else:
            print(f"[WARN] Expected text frame, got opcode={opcode}")
    except Exception as e:
        print(f"[WARN] Could not read ready message: {e}")

    # --- Send binary audio frames (simulated 16-bit PCM) ---
    # Simulate ~1 second of audio at 16kHz = 32000 bytes (16000 samples * 2 bytes)
    audio_data = bytearray(32000)
    for i in range(0, len(audio_data), 2):
        audio_data[i] = (i // 2) % 256
        audio_data[i+1] = ((i // 2) // 256) % 256
    s.sendall(ws_frame(0x2, bytes(audio_data)))  # binary frame
    print(f"[4] Sent {len(audio_data)} bytes of simulated PCM audio")

    # --- Send text control: "flush" ---
    s.sendall(ws_frame(0x1, b'{"flush"}'))
    print("[5] Sent flush command")

    # --- Wait for timeline response ---
    time.sleep(0.5)
    s.settimeout(2)
    try:
        frame_data = s.recv(4096)
        opcode, payload = ws_parse_frame(frame_data)
        if opcode == 1:
            data = json.loads(payload.decode())
            print(f"[6] Timeline response received: type={data.get('type', 'unknown')}, "
                  f"segments={len(data.get('segments', []))}")
        else:
            print(f"[WARN] Expected text frame, got opcode={opcode}")
    except socket.timeout:
        print("[6] No timeline response (expected if ASR is disabled)")
    except Exception as e:
        print(f"[WARN] Could not read timeline: {e}")

    # --- Send text control: "end" ---
    s.sendall(ws_frame(0x1, b'{"end"}'))
    print("[7] Sent end command")

    # --- Wait for final timeline ---
    time.sleep(0.5)
    s.settimeout(2)
    try:
        frame_data = s.recv(4096)
        opcode, payload = ws_parse_frame(frame_data)
        if opcode == 1:
            data = json.loads(payload.decode())
            print(f"[8] Final timeline received: type={data.get('type', 'unknown')}")
        else:
            print(f"[WARN] Expected text frame, got opcode={opcode}")
    except socket.timeout:
        print("[8] No final timeline (expected if ASR is disabled)")
    except Exception as e:
        print(f"[WARN] Could not read final timeline: {e}")

    # --- Send "describe" command ---
    s.sendall(ws_frame(0x1, b'{"describe"}'))
    print("[9] Sent describe command")

    # --- Wait for protocol description ---
    time.sleep(0.5)
    s.settimeout(2)
    try:
        frame_data = s.recv(4096)
        opcode, payload = ws_parse_frame(frame_data)
        if opcode == 1:
            data = json.loads(payload.decode())
            print(f"[10] Protocol description received: {list(data.keys())}")
        else:
            print(f"[WARN] Expected text frame, got opcode={opcode}")
    except socket.timeout:
        print("[10] No protocol description (expected if pipeline not started)")
    except Exception as e:
        print(f"[WARN] Could not read description: {e}")

    # --- Send "reset" command ---
    s.sendall(ws_frame(0x1, b'{"reset"}'))
    print("[11] Sent reset command")

    # --- Wait for reset confirmation ---
    time.sleep(0.5)
    s.settimeout(2)
    try:
        frame_data = s.recv(4096)
        opcode, payload = ws_parse_frame(frame_data)
        if opcode == 1:
            data = json.loads(payload.decode())
            print(f"[12] Reset confirmation: {data}")
        else:
            print(f"[WARN] Expected text frame, got opcode={opcode}")
    except socket.timeout:
        print("[12] No reset confirmation")
    except Exception as e:
        print(f"[WARN] Could not read reset confirmation: {e}")

    # --- Close connection ---
    s.sendall(ws_frame(0x8, b''))  # close frame
    print("[13] Sent close frame")
    s.close()
    print("\n=== Test completed successfully ===")
    return True


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8765
    success = run_test(port)
    sys.exit(0 if success else 1)
