#!/usr/bin/env python3
"""Minimal raw WS frame capture: records EVERY server text frame to prove the
real-path message set (ready/time_base, diar, asr, endpoint, revision, timeline).
Reuses the handshake/frame helpers from ws_stream_client.py.
"""
import argparse
import json
import os
import socket
import struct
import sys
import threading
import time
from collections import Counter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ws_stream_client import handshake, read_frame, mask_frame  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pcm", required=True)
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=8765)
    ap.add_argument("--rate", type=float, default=8.0)
    ap.add_argument("--frame-ms", type=int, default=100)
    ap.add_argument("--out", default="/tmp/ws_raw_frames.jsonl")
    args = ap.parse_args()

    sock = socket.create_connection((args.host, args.port))
    handshake(sock, args.host, args.port)

    frames = []
    counter = Counter()
    done = threading.Event()

    def reader():
        while True:
            op, pl = read_frame(sock)
            if op is None or op == 0x8:
                break
            if op != 0x1:
                continue
            try:
                msg = json.loads(pl.decode("utf-8"))
            except Exception:
                continue
            frames.append(msg)
            counter[msg.get("type", "?")] += 1
            if msg.get("type") == "timeline":
                done.set()

    rt = threading.Thread(target=reader, daemon=True)
    rt.start()

    with open(args.pcm, "rb") as f:
        pcm = f.read()
    bytes_per_frame = int(16000 * 2 * args.frame_ms / 1000)
    frame_interval = (args.frame_ms / 1000.0) / args.rate if args.rate > 0 else 0
    for off in range(0, len(pcm), bytes_per_frame):
        sock.sendall(mask_frame(0x2, pcm[off:off + bytes_per_frame]))
        if frame_interval:
            time.sleep(frame_interval)
    sock.sendall(mask_frame(0x1, json.dumps({"end": True}).encode()))

    done.wait(timeout=300)
    time.sleep(0.5)

    with open(args.out, "w") as f:
        for m in frames:
            f.write(json.dumps(m, ensure_ascii=False) + "\n")

    print("=== real-path WS frame histogram ===")
    for k, v in sorted(counter.items()):
        print(f"  {k:12s} {v}")
    ready = next((m for m in frames if m.get("type") == "ready"), None)
    print("=== ready meta ===")
    print("  " + json.dumps(ready, ensure_ascii=False) if ready else "  (none)")
    eps = [m for m in frames if m.get("type") == "endpoint"]
    print(f"=== endpoints: {len(eps)} ===")
    for m in eps[:8]:
        print("  " + json.dumps(m, ensure_ascii=False))
    revs = [m for m in frames if m.get("type") == "revision"]
    print(f"=== revisions: {len(revs)} (first 3) ===")
    for m in revs[:3]:
        print("  " + json.dumps(m, ensure_ascii=False)[:160])
    print(f"wrote {len(frames)} frames -> {args.out}")


if __name__ == "__main__":
    main()
