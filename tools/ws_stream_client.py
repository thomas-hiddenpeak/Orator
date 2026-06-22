#!/usr/bin/env python3
"""Real streaming WebSocket client for orator_ws (stdlib only, no deps).

This is the AUTHORITATIVE streaming validation (Spec 001, T040-T042): it pushes
PCM audio THROUGH the WebSocket transport as an incremental stream, optionally
faster than real time, while a dedicated reader thread captures EVERY server
frame (incremental {"type":"asr"} events and the final {"type":"timeline"}
document). It then writes the complete event log + timeline + metrics to a JSON
file for inspection and later context comparison.

It deliberately does NOT hand a whole clip to one offline call -- the server's
two pipelines (diarization, ASR) consume the streamed buffer independently, each
at its own maximum rate.

Usage:
  python3 ws_stream_client.py --pcm /tmp/base.pcm [--port 8765]
      [--rate 0] [--frame-ms 100] [--out /tmp/orator_stream.json]

  --rate R   push at R x real-time (e.g. 8 = 8x faster). 0 = max rate
             (frames back-to-back, no pacing). The server still works at its
             own speed; this only bounds how fast the producer feeds it.
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
        self.timeline_event = threading.Event()

    def run(self):
        while True:
            op, pl = read_frame(self.sock)
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
            elif kind == "timeline":
                self.timeline = raw
                self.timeline_event.set()
            elif kind == "ready":
                self.ready = raw


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pcm", required=True, help="int16 mono 16k PCM file")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=8765)
    ap.add_argument("--rate", type=float, default=0.0,
                    help="push speed x real-time; 0 = max (no pacing)")
    ap.add_argument("--frame-ms", type=int, default=100)
    ap.add_argument("--out", default="/tmp/orator_stream.json")
    ap.add_argument("--timeline-timeout", type=float, default=600.0,
                    help="seconds to wait for final timeline after sending end")
    args = ap.parse_args()

    pcm = open(args.pcm, "rb").read()
    total_samples = len(pcm) // BYTES_PER_SAMPLE
    audio_sec = total_samples / SAMPLE_RATE
    frame_bytes = int(SAMPLE_RATE * args.frame_ms / 1000) * BYTES_PER_SAMPLE

    sock = socket.create_connection((args.host, args.port))
    handshake(sock, args.host, args.port)
    reader = Reader(sock)
    reader.start()
    # Give the server a moment to send "ready" (weights load on connect).
    time.sleep(0.2)
    print(f"connected; streaming {audio_sec:.2f}s of audio "
          f"({'max rate' if args.rate == 0 else str(args.rate) + 'x'}, "
          f"{args.frame_ms}ms frames)")

    # Producer: push PCM frames through the socket. At rate R, frame_ms of audio
    # should take frame_ms/R of wall time; rate 0 means no pacing (max rate).
    t0 = time.monotonic()
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

    # Flush: send flush command and wait for the timeline document.
    sock.sendall(mask_frame(0x1, b'{"flush":true}'))
    print("  [flush] waiting for timeline...")
    got_flush = reader.timeline_event.wait(timeout=args.timeline_timeout)
    if got_flush and reader.timeline is not None:
        print(f"  [flush] timeline received ({reader.timeline.get('audio_sec', '?')}s)")
    else:
        print("  [flush] no timeline (continuing to end)")

    # End: send end command and wait for the final timeline document.
    reader.timeline_event.clear()
    sock.sendall(mask_frame(0x1, b'{"end":true}'))
    print("  [end] waiting for final timeline...")
    got_end = reader.timeline_event.wait(timeout=args.timeline_timeout)
    total_wall = time.monotonic() - t0
    sock.sendall(mask_frame(0x8, b"\x03\xe8"))  # CLOSE
    sock.close()
    # Join the reader thread so it stops reading before the interpreter exits.
    reader.join(timeout=2.0)

    if not got_end or reader.timeline is None:
        print(f"ERROR: no final timeline received within {args.timeline_timeout:.1f}s",
              file=sys.stderr)
        return 1

    tl = reader.timeline
    # The comprehensive timeline carries one track per pipeline; index them by
    # kind so the metrics summary does not depend on track ordering.
    tracks = {t.get("kind"): t for t in tl.get("tracks", [])}
    diar = tracks.get("diarization", {})
    asr = tracks.get("asr", {})
    diar_compute = diar.get("compute_sec")
    asr_compute = asr.get("compute_sec")
    out = {
        "meta": {
            "pcm": args.pcm,
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
        "events": reader.events,
        "timeline": tl,
    }
    with open(args.out, "w", encoding="utf-8") as f:
        json.dump(out, f, ensure_ascii=False, indent=2)

    m = out["meta"]
    n_diar = len(diar.get("entries", []))
    n_asr = len(asr.get("entries", []))
    comprehensive = tl.get("comprehensive", [])
    print(f"\nwrote {args.out}")
    print(f"  audio={audio_sec:.2f}s  total_wall={total_wall:.2f}s  "
          f"stream_rt={m['stream_rt_factor']}x")
    print(f"  diar: {n_diar} segments, rt={m['diar_rt_factor']}x   "
          f"asr: {n_asr} utterances, rt={m['asr_rt_factor']}x")
    if comprehensive:
        print(f"  comprehensive: {len(comprehensive)} speaker turns")
        for turn in comprehensive:
            mm = int(turn["start"]) // 60
            ss = int(turn["start"]) % 60
            print(f"    [{mm:02d}:{ss:02d}] {turn['speaker']}: {turn['text']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
