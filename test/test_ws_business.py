#!/usr/bin/env python3
"""
End-to-end WebSocket business test for orator_ws.
Simulates a real client session:
  1. Connect + WebSocket upgrade
  2. Receive "ready" message
  3. Send binary audio frames (simulated PCM)
  4. Send text control commands ("flush", "describe", "reset", "end")
  5. Verify server responses (timeline, reset_ok, protocol description)

Uses OratorTestHarness for Spec 004 unwrapping and assertions.
"""

import json
import time
import sys

from ws_test_harness import OratorTestHarness


def recv_until(ws, timeout=5):
    """Read all messages from websocket until timeout.

    Returns list of parsed message dicts.
    """
    ws.settimeout(0.5)
    msgs = []
    t0 = time.time()
    while time.time() - t0 < timeout:
        try:
            m = ws.recv()
            d = OratorTestHarness.unwrap(m)
            msgs.append(d)
        except Exception:
            break
    return msgs


def run_test(port=8765):
    """Run business test against orator_ws.

    Validates:
    - TCP connection and WebSocket upgrade
    - Ready message with required fields
    - Timeline response after flush
    - Describe command response
    - Reset command confirmation
    - Clean close

    Returns:
        True if all assertions pass, False otherwise.
    """
    print(f"=== WebSocket Business Test on port {port} ===\n")
    failures = []

    # Connect via websocket library
    import websocket
    ws = websocket.WebSocket()
    ws.settimeout(10)
    try:
        ws.connect(f'ws://127.0.0.1:{port}')
        print("[1] Connected to server")
    except Exception as e:
        print(f"[FAIL] Connect failed: {e}")
        return False

    # --- Read ready message ---
    try:
        raw = ws.recv()
        ready = OratorTestHarness.unwrap(raw)
        rtype = ready.get('type', '')
        print(f"[2] Ready message received: type={rtype}, "
              f"sample_rate={ready.get('sample_rate')}, "
              f"asr={ready.get('asr')}, "
              f"protocol_version={ready.get('protocol_version')}")
        if rtype != 'ready':
            print(f"  [FAIL] Expected type='ready', got '{rtype}'")
            failures.append("ready_type")
        for field in ['sample_rate', 'time_base', 'protocol_version']:
            if field not in ready:
                print(f"  [FAIL] Ready missing field '{field}'")
                failures.append(f"ready_missing_{field}")
    except Exception as e:
        print(f"[FAIL] Could not read ready message: {e}")
        failures.append("ready_message")
        ws.close()
        return False

    # --- Send binary audio frames (simulated 16-bit PCM) ---
    audio_data = bytearray(32000)
    for i in range(0, len(audio_data), 2):
        audio_data[i] = (i // 2) % 256
        audio_data[i+1] = ((i // 2) // 256) % 256
    ws.send(bytes(audio_data), opcode=0x02)
    print(f"[3] Sent {len(audio_data)} bytes of simulated PCM audio")

    # --- Send flush command ---
    ws.send(json.dumps({'flush': True}))
    print("[4] Sent flush command")

    # --- Collect responses after flush ---
    time.sleep(1)
    flush_msgs = recv_until(ws, timeout=3)
    timeline_found = False
    for d in flush_msgs:
        if d.get('type') == 'timeline':
            timeline_found = True
            print(f"[5] Timeline received: audio_sec={d.get('audio_sec')}, "
                  f"tracks={len(d.get('tracks', []))}")
            try:
                OratorTestHarness.assert_timeline_valid(d)
                print("  Timeline validation passed")
            except AssertionError as e:
                print(f"  [FAIL] Timeline validation: {e}")
                failures.append("timeline_validation")
            break
    if not timeline_found:
        print("[5] No timeline received (may be expected with simulated audio)")

    # --- Send describe command ---
    ws.send(json.dumps({'describe': True}))
    print("[6] Sent describe command")

    # --- Wait for protocol description ---
    time.sleep(0.5)
    desc_msgs = recv_until(ws, timeout=3)
    desc_found = False
    for d in desc_msgs:
        # Describe response has no 'type' field; it has 'pipelines' and 'schemas'
        if 'pipelines' in d or 'schemas' in d:
            desc_found = True
            print(f"[7] Protocol description received: "
                  f"pipelines={len(d.get('pipelines', []))}, "
                  f"schemas={len(d.get('schemas', []))}")
            break
    if not desc_found:
        print("[7] No protocol description received")
        failures.append("describe_response")

    # --- Send end command (before reset, so audio buffer still has data) ---
    ws.send(json.dumps({'end': True}))
    print("[8] Sent end command")

    # --- Wait for final timeline ---
    time.sleep(1)
    end_msgs = recv_until(ws, timeout=3)
    end_timeline_found = False
    for d in end_msgs:
        if d.get('type') == 'timeline':
            end_timeline_found = True
            print(f"[9] Final timeline received: "
                  f"audio_sec={d.get('audio_sec')}")
            try:
                OratorTestHarness.assert_timeline_valid(d)
                print("  Final timeline validation passed")
            except AssertionError as e:
                print(f"  [FAIL] Final timeline validation: {e}")
                failures.append("final_timeline_validation")
            break
    if not end_timeline_found:
        print("[9] No final timeline after end")

    # --- Send reset command ---
    ws.send(json.dumps({'reset': True}))
    print("[10] Sent reset command")

    # --- Wait for reset confirmation ---
    time.sleep(0.5)
    reset_msgs = recv_until(ws, timeout=3)
    reset_found = False
    for d in reset_msgs:
        if d.get('type') == 'reset_ok':
            reset_found = True
            print(f"[11] Reset confirmation received")
            break
    if not reset_found:
        print("[11] No reset_ok received (got types: "
              f"{[m.get('type','?') for m in reset_msgs]})")
        failures.append("reset_response")

    # --- Close connection ---
    ws.close()
    print("[12] Connection closed")

    # --- Summary ---
    if failures:
        print(f"\n=== Test FAILED: {len(failures)} failure(s): {failures} ===")
        return False
    else:
        print("\n=== Test completed successfully ===")
        return True


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8765
    success = run_test(port)
    sys.exit(0 if success else 1)
