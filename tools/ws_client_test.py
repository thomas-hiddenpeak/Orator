#!/usr/bin/env python3
# Stdlib-only WebSocket client: streams /tmp/test.pcm to the orator_ws server
# and prints the diarization JSON it returns. No third-party deps.
import socket, base64, os, struct, sys, hashlib

HOST, PORT = "127.0.0.1", int(sys.argv[1]) if len(sys.argv) > 1 else 8765

def mask_frame(opcode, payload: bytes) -> bytes:
    b = bytearray()
    b.append(0x80 | opcode)
    n = len(payload)
    mask = os.urandom(4)
    if n < 126:
        b.append(0x80 | n)
    elif n <= 0xFFFF:
        b.append(0x80 | 126); b += struct.pack(">H", n)
    else:
        b.append(0x80 | 127); b += struct.pack(">Q", n)
    b += mask
    b += bytes(payload[i] ^ mask[i & 3] for i in range(n))
    return bytes(b)

def read_frame(sock) -> tuple:
    h = recvn(sock, 2)
    opcode = h[0] & 0x0F
    n = h[1] & 0x7F
    if n == 126:
        n = struct.unpack(">H", recvn(sock, 2))[0]
    elif n == 127:
        n = struct.unpack(">Q", recvn(sock, 8))[0]
    payload = recvn(sock, n) if n else b""
    return opcode, payload

def recvn(sock, n) -> bytes:
    buf = b""
    while len(buf) < n:
        r = sock.recv(n - len(buf))
        if not r:
            break
        buf += r
    return buf

s = socket.create_connection((HOST, PORT))
key = base64.b64encode(os.urandom(16)).decode()
req = (f"GET / HTTP/1.1\r\nHost: {HOST}:{PORT}\r\nUpgrade: websocket\r\n"
       f"Connection: Upgrade\r\nSec-WebSocket-Key: {key}\r\n"
       f"Sec-WebSocket-Version: 13\r\n\r\n")
s.sendall(req.encode())
resp = b""
while b"\r\n\r\n" not in resp:
    resp += s.recv(1)
# verify accept
accept = base64.b64encode(hashlib.sha1((key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode()).digest()).decode()
assert accept.encode() in resp, "handshake accept mismatch"
print("handshake OK")

# server 'ready'
op, pl = read_frame(s)
print("server:", pl.decode())

pcm = open("/tmp/test.pcm", "rb").read()
# stream in 3200-byte (100ms) binary chunks
step = 3200
for i in range(0, len(pcm), step):
    s.sendall(mask_frame(0x2, pcm[i:i+step]))
print(f"streamed {len(pcm)} bytes")

s.sendall(mask_frame(0x1, b'{"flush":true}'))
op, pl = read_frame(s)
print("diarization:", pl.decode())
s.sendall(mask_frame(0x8, b"\x03\xe8"))  # close
s.close()
