#!/usr/bin/env python3
# Minimal WS client: stream a PCM window, flush, and dump the returned segments
# as plain text (start end speaker conf) for manual inspection. stdlib only.
import base64, hashlib, json, os, socket, struct, sys

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8765
PCM = sys.argv[2] if len(sys.argv) > 2 else "/tmp/base.pcm"
SECONDS = float(sys.argv[3]) if len(sys.argv) > 3 else 3615.0
CHUNK_MS = 200
SR = 16000


def mask(op, payload):
    h = bytes([0x80 | op])
    n = len(payload)
    if n < 126:
        h += bytes([0x80 | n])
    elif n < 65536:
        h += bytes([0x80 | 126]) + struct.pack(">H", n)
    else:
        h += bytes([0x80 | 127]) + struct.pack(">Q", n)
    m = os.urandom(4)
    return h + m + bytes(b ^ m[i % 4] for i, b in enumerate(payload))


def recvn(s, n):
    b = b""
    while len(b) < n:
        c = s.recv(n - len(b))
        if not c:
            raise ConnectionError("closed")
        b += c
    return b


def readf(s):
    h = recvn(s, 2)
    op = h[0] & 0x0F
    ln = h[1] & 0x7F
    if ln == 126:
        ln = struct.unpack(">H", recvn(s, 2))[0]
    elif ln == 127:
        ln = struct.unpack(">Q", recvn(s, 8))[0]
    return op, recvn(s, ln)


s = socket.create_connection(("127.0.0.1", PORT))
k = base64.b64encode(os.urandom(16)).decode()
s.sendall(("GET / HTTP/1.1\r\nHost: x\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
           "Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n\r\n" % k).encode())
buf = b""
while b"\r\n\r\n" not in buf:
    buf += s.recv(4096)
readf(s)  # ready

total = int(SECONDS * SR)
step = int(CHUNK_MS / 1000 * SR) * 2
with open(PCM, "rb") as f:
    sent = 0
    want = total * 2
    while sent < want:
        d = f.read(min(step, want - sent))
        if not d:
            break
        s.sendall(mask(0x2, d))
        sent += len(d)
s.sendall(mask(0x1, b'{"flush"}'))
op, pl = readf(s)
o = json.loads(pl.decode())
print("audio_sec=%.2f compute_sec=%.2f rt=%.2f segments=%d" %
      (o["audio_sec"], o["compute_sec"], o["rt_factor"], len(o["segments"])))
for seg in o["segments"]:
    print("%.2f %.2f spk%d %.2f" %
          (seg["start"], seg["end"], seg["speaker"], seg["confidence"]))
s.close()
