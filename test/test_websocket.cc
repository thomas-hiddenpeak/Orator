// WebSocket server unit test (dependency-free):
//   1) SHA-1 + base64 handshake primitives vs the RFC6455 reference vector.
//   2) base64 known-answer vectors.
//   3) A real loopback: server ServeOnce on a thread, a raw client performs the
//      RFC6455 upgrade, sends a masked text frame, and gets an echo back.

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "net/websocket_server.h"

using namespace orator;
using namespace orator::net;

// Echo handler: bounces any text it receives back to the client.
struct EchoHandler : WebSocketHandler {
  void OnText(WebSocketConnection& c, const std::string& t) override {
    c.SendText("echo:" + t);
  }
};

static std::string Recv(int fd, size_t n) {
  std::string s;
  s.resize(n);
  size_t got = 0;
  while (got < n) {
    ssize_t r = ::recv(fd, &s[got], n - got, 0);
    if (r <= 0) break;
    got += size_t(r);
  }
  s.resize(got);
  return s;
}

int main() {
  // 1) Handshake accept vector from RFC6455 section 1.3.
  std::string accept =
      detail::Sha1Base64("dGhlIHNhbXBsZSBub25jZQ=="
                         "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
  assert(accept == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");

  // 2) base64 known answers.
  assert(detail::Base64Encode(reinterpret_cast<const uint8_t*>("Man"), 3) ==
         "TWFu");
  assert(detail::Base64Encode(reinterpret_cast<const uint8_t*>("Ma"), 2) ==
         "TWE=");
  assert(detail::Base64Encode(reinterpret_cast<const uint8_t*>("M"), 1) ==
         "TQ==");

  // 3) Loopback echo over a real socket.
  WebSocketServer server(0);  // OS-assigned port
  bool ok = server.Start();
  assert(ok);
  int port = server.port();

  EchoHandler handler;
  std::thread srv([&]() { server.ServeOnce(handler); });

  // ---- raw client ----
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  assert(fd >= 0);
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(uint16_t(port));
  inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
  int cr = ::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
  assert(cr == 0);

  std::string key = "dGhlIHNhbXBsZSBub25jZQ==";
  std::string req =
      "GET / HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\n"
      "Connection: Upgrade\r\nSec-WebSocket-Key: " +
      key + "\r\nSec-WebSocket-Version: 13\r\n\r\n";
  ::send(fd, req.data(), req.size(), 0);

  // Read HTTP response headers up to blank line.
  std::string resp;
  char c;
  while (resp.find("\r\n\r\n") == std::string::npos) {
    if (::recv(fd, &c, 1, 0) <= 0) break;
    resp.push_back(c);
  }
  assert(resp.find("101") != std::string::npos);
  assert(resp.find("s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") != std::string::npos);

  // Send a masked text frame "hi".
  const char* msg = "hi";
  uint8_t mask[4] = {0x12, 0x34, 0x56, 0x78};
  std::vector<uint8_t> frame;
  frame.push_back(0x81);              // FIN + text
  frame.push_back(0x80 | 2);          // masked, len 2
  frame.insert(frame.end(), mask, mask + 4);
  for (int i = 0; i < 2; ++i) frame.push_back(uint8_t(msg[i]) ^ mask[i & 3]);
  ::send(fd, frame.data(), frame.size(), 0);

  // Read echoed frame: expect "echo:hi" (7 bytes, unmasked from server).
  std::string hdr = Recv(fd, 2);
  assert(hdr.size() == 2);
  assert((uint8_t(hdr[0]) & 0x0F) == 0x1);  // text opcode
  size_t len = uint8_t(hdr[1]) & 0x7F;
  assert(len == 7);
  std::string payload = Recv(fd, len);
  assert(payload == "echo:hi");

  ::close(fd);
  srv.join();
  server.Stop();

  std::cout << "test_websocket OK" << std::endl;
  return 0;
}
