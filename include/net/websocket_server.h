#pragma once

// Dependency-free WebSocket server (RFC 6455) over raw POSIX sockets.
//
// No third-party libraries: the HTTP upgrade handshake (SHA-1 + base64 of the
// Sec-WebSocket-Key) and the frame codec are implemented from scratch using
// only libc / OS sockets, consistent with the project's "pure C++, only CUDA"
// constraint (networking is OS/libc, not a third-party dependency).
//
// The server accepts connections sequentially: one active connection at a time.
// Each connection gets a fresh WebSocketHandler from the factory. The server
// returns to accept() immediately after a handler completes, so short-lived
// clients (browser refresh) see the server available again within milliseconds.
// TCP keepalive is enabled per connection to detect dead peers quickly.

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace orator {
namespace net {

// Per-connection handle the handler uses to push messages back to the client.
class WebSocketConnection {
 public:
  explicit WebSocketConnection(int fd) : fd_(fd) {}

  bool SendText(const std::string& text);
  bool SendBinary(const void* data, size_t n);
  // Sends a CLOSE frame; the accept loop tears the socket down afterwards.
  void Close(uint16_t code = 1000);

  bool closed() const { return closed_; }
  int fd() const { return fd_; }

 private:
  bool SendFrame(uint8_t opcode, const void* data, size_t n);
  int fd_;
  bool closed_ = false;
};

// Application callback surface. Override the events of interest.
struct WebSocketHandler {
  virtual ~WebSocketHandler() = default;
  virtual void OnOpen(WebSocketConnection&) {}
  virtual void OnBinary(WebSocketConnection&, const uint8_t*, size_t) {}
  virtual void OnText(WebSocketConnection&, const std::string&) {}
  virtual void OnClose() {}
};

class WebSocketServer {
 public:
  explicit WebSocketServer(int port);
  ~WebSocketServer();

  // Bind + listen. Returns false on error (port in use, etc.).
  bool Start();

  // Accept connections forever; each gets a fresh handler from the factory.
  // Blocks; intended to run on a dedicated thread or as the app main loop.
  void Serve(const std::function<std::unique_ptr<WebSocketHandler>()>& factory);

  // Accept and fully serve exactly one connection with the given handler.
  // Returns true if a client connected and the session ran. Used by tests and
  // single-shot servers.
  bool ServeOnce(WebSocketHandler& handler);

  void Stop();
  int port() const { return port_; }

 private:
  // Performs the RFC6455 upgrade handshake on an accepted fd. Returns true on
  // success (101 sent).
  bool Handshake(int fd);
  // Runs the read loop for one upgraded connection, dispatching to handler.
  void RunConnection(int fd, WebSocketHandler& handler);

  int port_;
  int listen_fd_ = -1;
  bool running_ = false;
};

// Exposed for unit testing the handshake primitives.
namespace detail {
std::string Sha1Base64(const std::string& input);
std::string Base64Encode(const uint8_t* data, size_t n);
}  // namespace detail

}  // namespace net
}  // namespace orator
