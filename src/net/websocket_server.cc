#include "net/websocket_server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cctype>
#include <cstring>
#include <vector>

namespace orator {
namespace net {
namespace {

constexpr const char* kMagicGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

// ---------------------------------------------------------------------------
// SHA-1 (RFC 3174), self-contained.
// ---------------------------------------------------------------------------
struct Sha1 {
  uint32_t h[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u,
                   0xC3D2E1F0u};
  uint64_t bitlen = 0;
  uint8_t buf[64];
  size_t buflen = 0;

  static uint32_t Rol(uint32_t v, int s) { return (v << s) | (v >> (32 - s)); }

  void Block(const uint8_t* p) {
    uint32_t w[80];
    for (int i = 0; i < 16; ++i)
      w[i] = (uint32_t(p[i * 4]) << 24) | (uint32_t(p[i * 4 + 1]) << 16) |
             (uint32_t(p[i * 4 + 2]) << 8) | uint32_t(p[i * 4 + 3]);
    for (int i = 16; i < 80; ++i)
      w[i] = Rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (int i = 0; i < 80; ++i) {
      uint32_t f, k;
      if (i < 20) {
        f = (b & c) | ((~b) & d);
        k = 0x5A827999u;
      } else if (i < 40) {
        f = b ^ c ^ d;
        k = 0x6ED9EBA1u;
      } else if (i < 60) {
        f = (b & c) | (b & d) | (c & d);
        k = 0x8F1BBCDCu;
      } else {
        f = b ^ c ^ d;
        k = 0xCA62C1D6u;
      }
      uint32_t t = Rol(a, 5) + f + e + k + w[i];
      e = d;
      d = c;
      c = Rol(b, 30);
      b = a;
      a = t;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
  }

  void Update(const uint8_t* data, size_t n) {
    bitlen += uint64_t(n) * 8;
    while (n > 0) {
      size_t take = 64 - buflen;
      if (take > n) take = n;
      std::memcpy(buf + buflen, data, take);
      buflen += take;
      data += take;
      n -= take;
      if (buflen == 64) {
        Block(buf);
        buflen = 0;
      }
    }
  }

  void Final(uint8_t out[20]) {
    uint8_t pad = 0x80;
    uint64_t bl = bitlen;
    Update(&pad, 1);
    uint8_t zero = 0;
    while (buflen != 56) Update(&zero, 1);
    uint8_t len[8];
    for (int i = 0; i < 8; ++i) len[i] = uint8_t(bl >> (56 - i * 8));
    Update(len, 8);
    for (int i = 0; i < 5; ++i) {
      out[i * 4] = uint8_t(h[i] >> 24);
      out[i * 4 + 1] = uint8_t(h[i] >> 16);
      out[i * 4 + 2] = uint8_t(h[i] >> 8);
      out[i * 4 + 3] = uint8_t(h[i]);
    }
  }
};

ssize_t ReadAll(int fd, uint8_t* dst, size_t n) {
  size_t got = 0;
  while (got < n) {
    ssize_t r = ::recv(fd, dst + got, n - got, 0);
    if (r <= 0) return r;  // 0 = peer closed, <0 = error
    got += size_t(r);
  }
  return ssize_t(got);
}

bool WriteAll(int fd, const uint8_t* src, size_t n) {
  size_t sent = 0;
  while (sent < n) {
    ssize_t w = ::send(fd, src + sent, n - sent, MSG_NOSIGNAL);
    if (w <= 0) return false;
    sent += size_t(w);
  }
  return true;
}

}  // namespace

namespace detail {

std::string Base64Encode(const uint8_t* data, size_t n) {
  static const char* tbl =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  out.reserve(((n + 2) / 3) * 4);
  size_t i = 0;
  for (; i + 3 <= n; i += 3) {
    uint32_t v = (uint32_t(data[i]) << 16) | (uint32_t(data[i + 1]) << 8) |
                 uint32_t(data[i + 2]);
    out.push_back(tbl[(v >> 18) & 0x3F]);
    out.push_back(tbl[(v >> 12) & 0x3F]);
    out.push_back(tbl[(v >> 6) & 0x3F]);
    out.push_back(tbl[v & 0x3F]);
  }
  if (n - i == 1) {
    uint32_t v = uint32_t(data[i]) << 16;
    out.push_back(tbl[(v >> 18) & 0x3F]);
    out.push_back(tbl[(v >> 12) & 0x3F]);
    out.push_back('=');
    out.push_back('=');
  } else if (n - i == 2) {
    uint32_t v = (uint32_t(data[i]) << 16) | (uint32_t(data[i + 1]) << 8);
    out.push_back(tbl[(v >> 18) & 0x3F]);
    out.push_back(tbl[(v >> 12) & 0x3F]);
    out.push_back(tbl[(v >> 6) & 0x3F]);
    out.push_back('=');
  }
  return out;
}

std::string Sha1Base64(const std::string& input) {
  Sha1 s;
  s.Update(reinterpret_cast<const uint8_t*>(input.data()), input.size());
  uint8_t digest[20];
  s.Final(digest);
  return Base64Encode(digest, 20);
}

}  // namespace detail

// ---------------------------------------------------------------------------
// WebSocketConnection
// ---------------------------------------------------------------------------
bool WebSocketConnection::SendFrame(uint8_t opcode, const void* data,
                                    size_t n) {
  std::vector<uint8_t> frame;
  frame.push_back(uint8_t(0x80 | opcode));  // FIN + opcode
  if (n < 126) {
    frame.push_back(uint8_t(n));
  } else if (n <= 0xFFFF) {
    frame.push_back(126);
    frame.push_back(uint8_t(n >> 8));
    frame.push_back(uint8_t(n));
  } else {
    frame.push_back(127);
    for (int i = 7; i >= 0; --i) frame.push_back(uint8_t(uint64_t(n) >> (i * 8)));
  }
  // Server->client frames are NOT masked.
  size_t hdr = frame.size();
  frame.resize(hdr + n);
  if (n) std::memcpy(frame.data() + hdr, data, n);
  return WriteAll(fd_, frame.data(), frame.size());
}

bool WebSocketConnection::SendText(const std::string& text) {
  return SendFrame(0x1, text.data(), text.size());
}

bool WebSocketConnection::SendBinary(const void* data, size_t n) {
  return SendFrame(0x2, data, n);
}

void WebSocketConnection::Close(uint16_t code) {
  if (closed_) return;
  uint8_t payload[2] = {uint8_t(code >> 8), uint8_t(code)};
  SendFrame(0x8, payload, 2);
  closed_ = true;
}

// ---------------------------------------------------------------------------
// WebSocketServer
// ---------------------------------------------------------------------------
WebSocketServer::WebSocketServer(int port) : port_(port) {}

WebSocketServer::~WebSocketServer() { Stop(); }

bool WebSocketServer::Start() {
  listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd_ < 0) return false;
  int yes = 1;
  ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(uint16_t(port_));
  if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    ::close(listen_fd_);
    listen_fd_ = -1;
    return false;
  }
  // If port 0 was requested, learn the OS-assigned port.
  socklen_t alen = sizeof(addr);
  if (::getsockname(listen_fd_, reinterpret_cast<sockaddr*>(&addr), &alen) == 0)
    port_ = ntohs(addr.sin_port);
  if (::listen(listen_fd_, 4) < 0) {
    ::close(listen_fd_);
    listen_fd_ = -1;
    return false;
  }
  running_ = true;
  return true;
}

void WebSocketServer::Stop() {
  running_ = false;
  if (listen_fd_ >= 0) {
    ::close(listen_fd_);
    listen_fd_ = -1;
  }
}

bool WebSocketServer::Handshake(int fd) {
  // Read the HTTP request headers up to the blank line.
  std::string req;
  uint8_t c;
  while (req.find("\r\n\r\n") == std::string::npos) {
    ssize_t r = ::recv(fd, &c, 1, 0);
    if (r <= 0) return false;
    req.push_back(char(c));
    if (req.size() > 16384) return false;  // guard
  }
  // Extract Sec-WebSocket-Key (case-insensitive header name).
  std::string lower = req;
  for (char& ch : lower) ch = char(::tolower(ch));
  size_t kpos = lower.find("sec-websocket-key:");
  if (kpos == std::string::npos) return false;
  size_t vstart = req.find_first_not_of(" \t", kpos + 18);
  size_t vend = req.find("\r\n", vstart);
  if (vstart == std::string::npos || vend == std::string::npos) return false;
  std::string key = req.substr(vstart, vend - vstart);

  std::string accept = detail::Sha1Base64(key + kMagicGuid);
  std::string resp =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept: " +
      accept + "\r\n\r\n";
  return WriteAll(fd, reinterpret_cast<const uint8_t*>(resp.data()),
                  resp.size());
}

void WebSocketServer::RunConnection(int fd, WebSocketHandler& handler) {
  int one = 1;
  ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
  // Keepalive: detect dead peers in ~30 s (10 idle + 3 probes × 5 s).
  ::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one));
  int idle = 10;  ::setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE,  &idle,  sizeof(idle));
  int intvl = 5;  ::setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl));
  int cnt = 3;    ::setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT,   &cnt,   sizeof(cnt));
  WebSocketConnection conn(fd);
  handler.OnOpen(conn);

  std::string frag;       // reassembly buffer for fragmented messages
  uint8_t frag_opcode = 0;

  while (running_ && !conn.closed()) {
    uint8_t hdr[2];
    if (ReadAll(fd, hdr, 2) <= 0) break;
    bool fin = (hdr[0] & 0x80) != 0;
    uint8_t opcode = hdr[0] & 0x0F;
    bool masked = (hdr[1] & 0x80) != 0;
    uint64_t len = hdr[1] & 0x7F;
    if (len == 126) {
      uint8_t ext[2];
      if (ReadAll(fd, ext, 2) <= 0) break;
      len = (uint64_t(ext[0]) << 8) | ext[1];
    } else if (len == 127) {
      uint8_t ext[8];
      if (ReadAll(fd, ext, 8) <= 0) break;
      len = 0;
      for (int i = 0; i < 8; ++i) len = (len << 8) | ext[i];
    }
    uint8_t mask[4] = {0, 0, 0, 0};
    if (masked && ReadAll(fd, mask, 4) <= 0) break;
    if (len > (64ull << 20)) break;  // 64 MiB frame guard

    std::string payload;
    payload.resize(size_t(len));
    if (len && ReadAll(fd, reinterpret_cast<uint8_t*>(&payload[0]),
                       size_t(len)) <= 0)
      break;
    if (masked)
      for (size_t i = 0; i < payload.size(); ++i)
        payload[i] = char(uint8_t(payload[i]) ^ mask[i & 3]);

    if (opcode == 0x8) {  // CLOSE
      conn.Close();
      break;
    } else if (opcode == 0x9) {  // PING -> PONG (opcode 0xA), echo payload
      std::vector<uint8_t> pong;
      pong.push_back(0x8A);
      pong.push_back(uint8_t(payload.size()));
      pong.insert(pong.end(), payload.begin(), payload.end());
      WriteAll(fd, pong.data(), pong.size());
      continue;
    } else if (opcode == 0xA) {  // PONG
      continue;
    }

    // Data frames: 0x0 continuation, 0x1 text, 0x2 binary.
    if (opcode == 0x1 || opcode == 0x2) {
      frag_opcode = opcode;
      frag = payload;
    } else if (opcode == 0x0) {
      frag += payload;
    }
    if (fin) {
      if (frag_opcode == 0x1)
        handler.OnText(conn, frag);
      else if (frag_opcode == 0x2)
        handler.OnBinary(conn, reinterpret_cast<const uint8_t*>(frag.data()),
                         frag.size());
      frag.clear();
      frag_opcode = 0;
    }
  }
  handler.OnClose();
  ::close(fd);
}

bool WebSocketServer::ServeOnce(WebSocketHandler& handler) {
  if (listen_fd_ < 0 && !Start()) return false;
  int fd = ::accept(listen_fd_, nullptr, nullptr);
  if (fd < 0) return false;
  if (!Handshake(fd)) {
    ::close(fd);
    return false;
  }
  RunConnection(fd, handler);
  return true;
}

void WebSocketServer::Serve(
    const std::function<std::unique_ptr<WebSocketHandler>()>& factory) {
  if (listen_fd_ < 0 && !Start()) return;
  while (running_) {
    int fd = ::accept(listen_fd_, nullptr, nullptr);
    if (fd < 0) {
      if (!running_) break;
      continue;
    }
    if (!Handshake(fd)) {
      ::close(fd);
      continue;
    }
    auto handler = factory();
    RunConnection(fd, *handler);
  }
}

}  // namespace net
}  // namespace orator
