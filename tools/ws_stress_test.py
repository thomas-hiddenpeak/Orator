#!/usr/bin/env python3
# Stdlib-only WebSocket stress / injection simulator for orator_ws.
#
# Streams raw int16le mono-16k PCM to the diarization server as fast as the
# socket will accept it (maximum injection rate -> very high "x real-time"),
# then flushes and validates the returned timeline. Repeats for N cycles with a
# reset in between to surface buffer growth, integer overflow, frame-reassembly
# bugs and state leaks under sustained high-rate load.
#
# Usage: ws_stress_test.py [port] [pcm_path] [seconds_per_cycle] [cycles] [chunk_ms]
import base64, hashlib, json, os, socket, struct, sys, time

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8765
PCM_PATH = sys.argv[2] if len(sys.argv) > 2 else "/tmp/base.pcm"
SECONDS = float(sys.argv[3]) if len(sys.argv) > 3 else 120.0
CYCLES = int(sys.argv[4]) if len(sys.argv) > 4 else 3
CHUNK_MS = int(sys.argv[5]) if len(sys.argv) > 5 else 100
HOST = "127.0.0.1"
SR = 16000
MAX_SPK = 4


def mask_frame(opcode, payload: bytes) -> bytes:
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


def recvn(sock, n) -> bytes:
    buf = b""
    while len(buf) < n:
        r = sock.recv(n - len(buf))
        if not r:
            raise ConnectionError("server closed mid-frame")
        buf += r
    return buf


def read_frame(sock):
    h = recvn(sock, 2)
    opcode = h[0] & 0x0F
    n = h[1] & 0x7F
    if n == 126:
        n = struct.unpack(">H", recvn(sock, 2))[0]
    elif n == 127:
        n = struct.unpack(">Q", recvn(sock, 8))[0]
    payload = recvn(sock, n) if n else b""
    return opcode, payload


def handshake(s):
    key = base64.b64encode(os.urandom(16)).decode()
    req = (f"GET / HTTP/1.1\r\nHost: {HOST}:{PORT}\r\nUpgrade: websocket\r\n"
           f"Connection: Upgrade\r\nSec-WebSocket-Key: {key}\r\n"
           f"Sec-WebSocket-Version: 13\r\n\r\n")
    s.sendall(req.encode())
    resp = b""
    while b"\r\n\r\n" not in resp:
        resp += s.recv(1)
    accept = base64.b64encode(
        hashlib.sha1((key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode())
        .digest()).decode()
    assert accept.encode() in resp, "handshake accept mismatch"


def validate(obj, expect_audio_sec, label):
    errs = []
    if obj.get("type") != "diarization":
        errs.append(f"bad type {obj.get('type')!r}")
    a = obj.get("audio_sec", -1)
    if abs(a - expect_audio_sec) > 0.5:
        errs.append(f"audio_sec {a:.3f} != expected ~{expect_audio_sec:.3f}")
    if obj.get("compute_sec", 0) <= 0:
        errs.append("compute_sec <= 0")
    if obj.get("rt_factor", 0) <= 0:
        errs.append("rt_factor <= 0")
    segs = obj.get("segments", [])
    per_spk_last = {}
    for i, sg in enumerate(segs):
        st, en = sg["start"], sg["end"]
        sp, cf = sg["speaker"], sg["confidence"]
        if not (0.0 <= st <= en <= a + 0.05):
            errs.append(f"seg{i} bounds bad: start={st} end={en} audio={a}")
        if not (0 <= sp < MAX_SPK):
            errs.append(f"seg{i} speaker out of range: {sp}")
        if not (0.0 <= cf <= 1.0 + 1e-6):
            errs.append(f"seg{i} confidence out of [0,1]: {cf}")
        # Overlapping speech across DIFFERENT speakers is valid in diarization;
        # only segments of the SAME speaker must be ordered & non-overlapping.
        if sp in per_spk_last and st < per_spk_last[sp] - 1e-6:
            errs.append(f"seg{i} speaker{sp} overlaps prev same-speaker end "
                        f"{per_spk_last[sp]} (start={st})")
        per_spk_last[sp] = en
    return errs, segs


def run_cycle(s, pcm, label):
    n_bytes = len(pcm)
    audio_sec = (n_bytes // 2) / SR
    step = int(SR * CHUNK_MS / 1000) * 2  # bytes per chunk
    t0 = time.time()
    for i in range(0, n_bytes, step):
        s.sendall(mask_frame(0x2, pcm[i:i + step]))
    inject_s = time.time() - t0
    inject_x = audio_sec / inject_s if inject_s > 0 else float("inf")
    s.sendall(mask_frame(0x1, b'{"flush":true}'))
    op, pl = read_frame(s)
    obj = json.loads(pl.decode())
    errs, segs = validate(obj, audio_sec, label)
    print(f"  [{label}] injected {audio_sec:.1f}s in {inject_s:.3f}s wall "
          f"(={inject_x:.1f}x inject-rate); server rt_factor={obj.get('rt_factor'):.2f}, "
          f"compute={obj.get('compute_sec'):.3f}s, segments={len(segs)}")
    if errs:
        for e in errs:
            print(f"     !! {e}")
    return errs, obj


def main():
    full = open(PCM_PATH, "rb").read()
    want_bytes = int(SECONDS) * SR * 2
    pcm = full[:want_bytes]
    if len(pcm) < want_bytes:
        # tile to reach requested duration
        reps = (want_bytes + len(full) - 1) // len(full)
        pcm = (full * reps)[:want_bytes]
    print(f"stress: port={PORT} dur={SECONDS}s cycles={CYCLES} chunk={CHUNK_MS}ms "
          f"pcm={len(pcm)} bytes")

    s = socket.create_connection((HOST, PORT))
    handshake(s)
    op, pl = read_frame(s)  # ready
    print("  ready:", pl.decode())

    all_errs = 0
    first_obj = None
    for c in range(CYCLES):
        errs, obj = run_cycle(s, pcm, f"cycle{c+1}/{CYCLES}")
        all_errs += len(errs)
        if first_obj is None:
            first_obj = obj
        else:
            # determinism across cycles (reset must fully clear state)
            if obj.get("segments") != first_obj.get("segments"):
                print(f"     !! cycle{c+1} segments differ from cycle1 "
                      f"({len(obj.get('segments'))} vs {len(first_obj.get('segments'))}) "
                      f"-> possible state leak after reset")
                all_errs += 1
        s.sendall(mask_frame(0x1, b'{"reset":true}'))
        op, pl = read_frame(s)
        assert b"reset_ok" in pl, f"reset failed: {pl}"

    s.sendall(mask_frame(0x8, b"\x03\xe8"))
    s.close()
    print("RESULT:", "PASS" if all_errs == 0 else f"FAIL ({all_errs} issues)")
    sys.exit(0 if all_errs == 0 else 1)


if __name__ == "__main__":
    main()
