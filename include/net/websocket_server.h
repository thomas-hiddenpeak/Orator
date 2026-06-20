#pragma once

// WebSocket server using libwebsockets (MIT license).
// Replaces hand-rolled implementation with industrial-grade WS support:
// - Multi-client concurrent connections
// - Proper EINTR handling, timeouts, backpressure
// - RFC 6455/7692 compliant

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <libwebsockets.h>

namespace orator {
namespace net {

// Forward declaration for the per-session linked-list node.
struct per_session_data;

// ---------------------------------------------------------------------------
// Per-connection handle the handler uses to push messages back to the client.
// ---------------------------------------------------------------------------
class WebSocketConnection {
  friend int ws_callback(struct lws *, enum lws_callback_reasons,
                         void *, void *, size_t);
 public:
  WebSocketConnection(struct lws *wsi, struct lws_context *ctx)
      : wsi_(wsi), context_(ctx) {}

  bool SendText(const std::string& text);
  bool SendBinary(const void* data, size_t n);
  void Close(uint16_t code = 1000);

  bool closed() const { return closed_; }

  private:
   struct lws *wsi_;
   struct lws_context *context_;
   bool closed_ = false;
   bool wakeup_pending_ = false;
   std::vector<std::string> pending_text_;
   std::shared_ptr<std::mutex> mu_ = std::make_shared<std::mutex>();
};

// Application callback surface. Override the events of interest.
struct WebSocketHandler {
  virtual ~WebSocketHandler() = default;
  virtual void OnOpen(WebSocketConnection&) {}
  virtual void OnBinary(WebSocketConnection&, const uint8_t*, size_t) {}
  virtual void OnText(WebSocketConnection&, const std::string&) {}
  virtual void OnClose() {}
};

// ---------------------------------------------------------------------------
// detail: SHA-1 + Base64 utilities (retained for test compatibility with
// the RFC 6455 handshake reference vectors).
// ---------------------------------------------------------------------------
namespace detail {

std::string Sha1Base64(const std::string& key, const std::string& magic);
std::string Sha1Base64(const std::string& combined);
std::string Base64Encode(const uint8_t* data, size_t len);

}  // namespace detail

class WebSocketServer {
  friend int ws_callback(struct lws *, enum lws_callback_reasons,
                         void *, void *, size_t);
 public:
  explicit WebSocketServer(int port);
  ~WebSocketServer();

  // Bind + listen. Returns false on error (port in use, etc.).
  bool Start();

  // Accept connections forever; each gets a fresh handler from the factory.
  // Blocks; intended to run on a dedicated thread or as the app main loop.
  void Serve(const std::function<std::unique_ptr<WebSocketHandler>()>& factory);

  // Accept exactly one connection, serve it with the given handler, then
  // return.  Used by unit tests that need a single loopback.
  void ServeOnce(WebSocketHandler& handler);

  void Stop();
  int port() const { return port_; }

 private:
  int port_;
  struct lws_context *context_ = nullptr;
  bool running_ = false;

  // Per-instance serve-once state (replaces file-scope statics).
  bool serve_once_done_ = false;
  WebSocketHandler* serve_once_handler_ = nullptr;

  // Per-instance serve-loop state.
  std::function<std::unique_ptr<WebSocketHandler>()> serve_factory_;

  // Linked-list head for all active per_session_data nodes.
  struct per_session_data *pss_list_head_ = nullptr;
};

}  // namespace net
}  // namespace orator
