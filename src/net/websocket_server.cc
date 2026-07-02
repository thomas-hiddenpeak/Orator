#include "net/websocket_server.h"

#include "core/log.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <sys/syscall.h>
#include <unistd.h>
#include <vector>

namespace orator {
namespace net {
namespace detail {

namespace {
static const char b64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
}

std::string Base64Encode(const uint8_t* data, size_t len) {
  std::string out;
  out.reserve(((len + 2) / 3) * 4);
  size_t i = 0;
  while (i + 2 < len) {
    out += b64_table[data[i] >> 2];
    out += b64_table[((data[i] & 0x03) << 4) | (data[i + 1] >> 4)];
    out += b64_table[((data[i + 1] & 0x0f) << 2) | (data[i + 2] >> 6)];
    out += b64_table[data[i + 2] & 0x3f];
    i += 3;
  }
  if (i < len) {
    out += b64_table[data[i] >> 2];
    if (i + 1 < len) {
      out += b64_table[((data[i] & 0x03) << 4) | (data[i + 1] >> 4)];
      out += b64_table[(data[i + 1] & 0x0f) << 2];
    } else {
      out += b64_table[(data[i] & 0x03) << 4];
      out += '=';
    }
    out += '=';
  }
  return out;
}

namespace {

struct Sha1 {
  std::array<uint8_t, 20> digest;

  void Update(const uint8_t* data, size_t len);
  void Finalize();

 private:
  std::vector<uint8_t> buffer_;
  uint64_t count_ = 0;
  std::array<uint32_t, 5> h{};

  void Init();
  void Transform(const uint8_t block[64]);
};

void Sha1::Init() {
  h[0] = 0x67452301;
  h[1] = 0xEFCDAB89;
  h[2] = 0x98BADCFE;
  h[3] = 0x10325476;
  h[4] = 0xC3D2E1F0;
}

static uint32_t Rotl(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

void Sha1::Transform(const uint8_t block[64]) {
  std::array<uint32_t, 80> w;
  for (int i = 0; i < 16; ++i) {
    w[i] = (uint32_t(block[4 * i]) << 24) | (uint32_t(block[4 * i + 1]) << 16) |
           (uint32_t(block[4 * i + 2]) << 8) | uint32_t(block[4 * i + 3]);
  }
  for (int i = 16; i < 80; ++i) {
    w[i] = Rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
  }

  uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];

  for (int i = 0; i < 80; ++i) {
    uint32_t f, k;
    if (i < 20) {
      f = (b & c) | (~b & d);
      k = 0x5A827999;
    } else if (i < 40) {
      f = b ^ c ^ d;
      k = 0x6ED9EBA1;
    } else if (i < 60) {
      f = (b & c) | (b & d) | (c & d);
      k = 0x8F1BBCDC;
    } else {
      f = b ^ c ^ d;
      k = 0xCA62C1D6;
    }
    uint32_t t = Rotl(a, 5) + f + e + k + w[i];
    e = d;
    d = c;
    c = Rotl(b, 30);
    b = a;
    a = t;
  }

  h[0] += a;
  h[1] += b;
  h[2] += c;
  h[3] += d;
  h[4] += e;
}

void Sha1::Update(const uint8_t* data, size_t len) {
  if (h[0] == 0 && h[1] == 0 && h[2] == 0 && h[3] == 0 && h[4] == 0) Init();

  for (size_t i = 0; i < len; ++i) {
    buffer_.push_back(data[i]);
    if (buffer_.size() == 64) {
      Transform(buffer_.data());
      buffer_.clear();
    }
  }
  count_ += len;
}

void Sha1::Finalize() {
  if (h[0] == 0 && h[1] == 0 && h[2] == 0 && h[3] == 0 && h[4] == 0) Init();

  uint64_t bits = count_ * 8;
  buffer_.push_back(0x80);
  while ((buffer_.size() % 64) != 56) buffer_.push_back(0);
  for (int i = 7; i >= 0; --i)
    buffer_.push_back(uint8_t((bits >> (i * 8)) & 0xff));

  for (size_t off = 0; off < buffer_.size(); off += 64)
    Transform(buffer_.data() + off);
  buffer_.clear();

  for (int i = 0; i < 5; ++i) {
    digest[4 * i] = uint8_t((h[i] >> 24) & 0xff);
    digest[4 * i + 1] = uint8_t((h[i] >> 16) & 0xff);
    digest[4 * i + 2] = uint8_t((h[i] >> 8) & 0xff);
    digest[4 * i + 3] = uint8_t(h[i] & 0xff);
  }
}

}  // namespace

std::string Sha1Base64(const std::string& key, const std::string& magic) {
  Sha1 sha;
  sha.Update(reinterpret_cast<const uint8_t*>(key.data()), key.size());
  sha.Update(reinterpret_cast<const uint8_t*>(magic.data()), magic.size());
  sha.Finalize();
  return Base64Encode(sha.digest.data(), sha.digest.size());
}

std::string Sha1Base64(const std::string& combined) {
  Sha1 sha;
  sha.Update(reinterpret_cast<const uint8_t*>(combined.data()),
             combined.size());
  sha.Finalize();
  return Base64Encode(sha.digest.data(), sha.digest.size());
}

}  // namespace detail

// Transitional diagnostic logger for WebSocket text frames.
// Enable only during local experiments with ORATOR_WS_LOG=/path/file.jsonl.
// It records full protocol payloads, including transcript text, so production
// runs should leave the variable unset.
namespace {
FILE* GetWsLogFile() {
  static FILE* log_fp = nullptr;
  if (!log_fp) {
    const char* path = std::getenv("ORATOR_WS_LOG");
    if (path) log_fp = std::fopen(path, "a");
  }
  return log_fp;
}

void LogWsMessage(const char* direction, const char* text) {
  FILE* fp = GetWsLogFile();
  if (!fp) return;
  std::fprintf(fp, "%s %s\n", direction, text);
  std::fflush(fp);
}
}  // namespace

struct per_session_data {
  struct per_session_data* pss_list = nullptr;
  struct lws* wsi_ = nullptr;
  struct lws_context* context_ = nullptr;
  WebSocketHandler* handler = nullptr;
  WebSocketConnection conn;
  std::vector<uint8_t> fragment_buf;
  bool is_binary = false;
  bool pending_open_send = false;
  bool message_processed = false;
  bool pending_writable = false;
};

// Retrieve the WebSocketServer instance stored in the lws context user data.
inline WebSocketServer* GetServer(struct lws* wsi) {
  return static_cast<WebSocketServer*>(lws_context_user(lws_get_context(wsi)));
}

int ws_callback(struct lws* wsi, enum lws_callback_reasons reason, void* user,
                void* in, size_t len) {
  struct per_session_data* pss = static_cast<struct per_session_data*>(user);
  WebSocketServer* server = GetServer(wsi);

  switch (reason) {
    case LWS_CALLBACK_ESTABLISHED: {
      if (pss && server) {
        pss->context_ = lws_get_context(wsi);
        pss->wsi_ = wsi;
        pss->conn = WebSocketConnection(wsi, pss->context_);
        if (server->serve_once_handler_) {
          pss->handler = server->serve_once_handler_;
        } else if (server->serve_factory_) {
          // Ownership crosses RAII -> C here: the factory's unique_ptr is
          // release()d into libwebsockets' C per-session storage (pss is
          // lws-allocated and memset, so it cannot hold a C++ smart pointer).
          // The matching delete is the CLOSED branch below.
          pss->handler = server->serve_factory_().release();
        }
        lws_ll_fwd_insert(pss, pss_list, server->pss_list_head_);
        if (pss->handler) {
          pss->pending_open_send = true;
          lws_callback_on_writable(wsi);
        }
      }
      break;
    }

    case LWS_CALLBACK_GET_THREAD_ID:
      if (in) *(uint64_t*)in = (uint64_t)gettid();
      break;

    case LWS_CALLBACK_LOCK_POLL:
    case LWS_CALLBACK_UNLOCK_POLL:
      // Distinct lws poll-lock callbacks, both intentionally no-ops here.
      break;

    case LWS_CALLBACK_EVENT_WAIT_CANCELLED: {
      lws_start_foreach_llp(struct per_session_data**, ppss,
                            server->pss_list_head_) {
        std::lock_guard<std::mutex> lk(*(*ppss)->conn.mu_);
        if (!(*ppss)->conn.pending_text_.empty() &&
            !(*ppss)->pending_writable && !(*ppss)->conn.closed_) {
          (*ppss)->pending_writable = true;
          lws_callback_on_writable((*ppss)->wsi_);
        }
      }
      lws_end_foreach_llp(ppss, pss_list);
      break;
    }

    case LWS_CALLBACK_RECEIVE: {
      if (!pss || !pss->handler) return 0;

      // Confirm validity to reset the keepalive/validity timer
      lws_validity_confirmed(wsi);

      if (lws_is_first_fragment(wsi)) {
        pss->is_binary = lws_frame_is_binary(wsi);
        pss->fragment_buf.clear();
      }

      pss->fragment_buf.insert(pss->fragment_buf.end(),
                               static_cast<const uint8_t*>(in),
                               static_cast<const uint8_t*>(in) + len);

      if (lws_is_final_fragment(wsi)) {
        LOG_DEBUG("RECEIVE: final fragment, is_binary=%d, len=%zu\n",
                  pss->is_binary, pss->fragment_buf.size());
        if (pss->is_binary) {
          // Copy data to a local buffer before calling OnBinary to avoid memory
          // issues
          std::vector<uint8_t> data_copy(pss->fragment_buf.begin(),
                                         pss->fragment_buf.end());
          pss->fragment_buf.clear();  // Clear before calling handler
          pss->handler->OnBinary(pss->conn, data_copy.data(), data_copy.size());
        } else {
          std::string text(pss->fragment_buf.begin(), pss->fragment_buf.end());
          pss->fragment_buf.clear();  // Clear before calling handler
          LogWsMessage("<--", text.c_str());
          pss->handler->OnText(pss->conn, text);
        }
        // Flush any pending text messages (VAD/ASR responses) that worker
        // threads queued via SendText.  This is a safety net: it ensures
        // messages are sent even if the EVENT_WAIT_CANCELLED→writable
        // pipeline stalls (e.g. after flags were stuck *before* the fix in
        // SERVER_WRITEABLE, or if lws_cancel_service from a worker thread
        // races with the service thread).
        {
          std::lock_guard<std::mutex> lk(*pss->conn.mu_);
          if (!pss->conn.pending_text_.empty() && !pss->conn.closed_) {
            lws_callback_on_writable(wsi);
          }
        }
        // Schedule completion in SERVER_WRITEABLE after response is flushed
        if (server && server->serve_once_handler_) {
          pss->message_processed = true;
          lws_callback_on_writable(wsi);
        }
        break;
      }
      break;
    }

    case LWS_CALLBACK_SERVER_WRITEABLE: {
      if (pss && pss->pending_open_send && pss->handler) {
        pss->pending_open_send = false;
        pss->handler->OnOpen(pss->conn);
      }
      if (pss) {
        std::vector<std::string> to_send;
        {
          std::lock_guard<std::mutex> lk(*pss->conn.mu_);
          to_send.swap(pss->conn.pending_text_);
        }
        size_t sent = 0;
        for (const auto& text : to_send) {
          // Backpressure guard: if the outbound socket is choked, stop feeding
          // lws.  lws_write() always accepts the whole payload into its
          // internal buflist, so draining the entire queue every callback lets
          // that buflist grow without bound when the client reads slowly —
          // eventually lws force-closes the connection (observed as a client
          // "Broken pipe" mid-stream).  Leaving the remaining messages queued
          // (and re-applying the bound below) keeps the pipe healthy.
          if (lws_send_pipe_choked(pss->conn.wsi_)) {
            break;
          }
          std::vector<uint8_t> buf(LWS_PRE + text.size());
          std::memcpy(buf.data() + LWS_PRE, text.data(), text.size());
          int ret = lws_write(pss->conn.wsi_, buf.data() + LWS_PRE, text.size(),
                              LWS_WRITE_TEXT);
          LogWsMessage("-->", text.c_str());
          if (ret < 0) {
            LOG_DEBUG("SERVER_WRITEABLE: lws_write failed (ret=%d), closing\n",
                      ret);
            pss->conn.closed_ = true;
            lws_close_reason(pss->conn.wsi_, LWS_CLOSE_STATUS_NOSTATUS, nullptr,
                             0);
            break;
          }
          ++sent;
        }
        // Re-queue whatever we could not send (pipe choked) at the front of the
        // pending queue so ordering is preserved, then re-apply the outbound
        // bound: a persistently slow client drops oldest intermediate messages
        // instead of forcing an lws-side disconnect.
        if (sent < to_send.size() && !pss->conn.closed()) {
          std::lock_guard<std::mutex> lk(*pss->conn.mu_);
          pss->conn.pending_text_.insert(
              pss->conn.pending_text_.begin(),
              std::make_move_iterator(to_send.begin() + sent),
              std::make_move_iterator(to_send.end()));
          static constexpr size_t kMaxPendingText = 2048;
          if (pss->conn.pending_text_.size() > kMaxPendingText) {
            pss->conn.pending_text_.erase(
                pss->conn.pending_text_.begin(),
                pss->conn.pending_text_.begin() +
                    (pss->conn.pending_text_.size() - kMaxPendingText));
          }
        }
        // Reset flow-control flags so the next SendText/lws_cancel_service
        // cycle can schedule a fresh writable callback.  This must happen
        // *after* the send loop: when to_send was non-empty on entry the
        // old code kept the flags set even after draining everything, which
        // permanently blocked subsequent worker-thread messages.
        {
          std::lock_guard<std::mutex> lk(*pss->conn.mu_);
          pss->pending_writable = false;
          pss->conn.wakeup_pending_ = false;
        }
        if (sent < to_send.size() && !pss->conn.closed()) {
          lws_callback_on_writable(wsi);
        }
      }
      if (pss && pss->message_processed && server &&
          server->serve_once_handler_) {
        server->serve_once_done_ = true;
      }
      break;
    }

    case LWS_CALLBACK_CLOSED:
    case LWS_CALLBACK_WS_PEER_INITIATED_CLOSE: {
      LOG_DEBUG("CLOSED: reason=%d, pss=%p, handler=%p, closed_=%d\n", reason,
                (void*)pss, pss ? (void*)pss->handler : (void*)nullptr,
                pss ? pss->conn.closed_ : -1);
      if (pss) {
        if (server)
          lws_ll_fwd_remove(struct per_session_data, pss_list, pss,
                            server->pss_list_head_);
        pss->conn.closed_ = true;
      }
      if (pss && pss->handler) {
        pss->handler->OnClose();
        if (!server || !server->serve_once_handler_) {
          // Raw delete (justified C-interop): reclaims the handler whose
          // ownership was release()d into lws' per-session pointer at
          // ESTABLISHED. The serve_once handler is owned elsewhere and is left
          // untouched.
          delete pss->handler;
        }
        pss->handler = nullptr;
      }
      if (server && server->serve_once_handler_) {
        server->serve_once_done_ = true;
      }
      break;
    }

    default:
      break;
  }
  return 0;
}

bool WebSocketConnection::SendText(const std::string& text) {
  if (closed_ || !wsi_) return false;
  // Bound the outbound backlog. Under a slow/congested client the queue would
  // otherwise grow without limit until libwebsockets buffers megabytes and
  // drops the connection (the observed "WS drops after a while" that was not a
  // server crash). At this cap the oldest queued messages are discarded,
  // keeping memory bounded and the connection alive; the periodic telemetry and
  // the next full timeline resynchronise the client after any congestion.
  static constexpr size_t kMaxPendingText = 2048;
  bool should_wakeup = false;
  {
    std::lock_guard<std::mutex> lk(*mu_);
    pending_text_.push_back(text);
    if (pending_text_.size() > kMaxPendingText) {
      pending_text_.erase(
          pending_text_.begin(),
          pending_text_.begin() + (pending_text_.size() - kMaxPendingText));
    }
    if (!wakeup_pending_) {
      wakeup_pending_ = true;
      should_wakeup = true;
    }
  }
  if (should_wakeup) lws_cancel_service(context_);
  return true;
}

bool WebSocketConnection::SendBinary(const void* data, size_t n) {
  if (closed_ || !wsi_) return false;
  // For binary data, send directly (assumes caller is on lws thread)
  std::vector<uint8_t> buf(LWS_PRE + n);
  std::memcpy(buf.data() + LWS_PRE, data, n);
  int ret = lws_write(wsi_, buf.data() + LWS_PRE, n, LWS_WRITE_BINARY);
  return ret == static_cast<ssize_t>(n);
}

void WebSocketConnection::Close(uint16_t code) {
  if (closed_ || !wsi_) return;
  lws_close_reason(wsi_, static_cast<enum lws_close_status>(code), nullptr, 0);
  closed_ = true;
}

WebSocketServer::WebSocketServer(int port) : port_(port) {}

WebSocketServer::~WebSocketServer() { Stop(); }

bool WebSocketServer::Start() {
  static struct lws_protocols protocols[] = {
      {"ws", ws_callback, sizeof(struct per_session_data), 0, 0, nullptr, 0},
      {NULL, NULL, 0, 0, 0, nullptr, 0}};

  struct lws_context_creation_info info;
  std::memset(&info, 0, sizeof(info));

  info.port = port_;
  info.protocols = protocols;
  info.options = LWS_SERVER_OPTION_DISABLE_IPV6;
  info.user = this;  // ws_callback retrieves server via lws_context_user()
  // Only errors and warnings. LLL_DEBUG logged every pollfd/partial-write event
  // (hundreds of lines/second, ~640k lines over a 24-min run) -- pure I/O and
  // formatting overhead that also buried real diagnostics.
  lws_set_log_level(LLL_ERR | LLL_WARN, nullptr);

  context_ = lws_create_context(&info);
  if (!context_) {
    LOG_ERROR("lws_create_context failed\n");
    return false;
  }

  struct lws_vhost* vhost = lws_get_vhost_by_name(context_, "default");
  if (vhost) {
    int actual_port = lws_get_vhost_port(vhost);
    if (actual_port > 0) {
      port_ = actual_port;
    }
  }

  running_ = true;
  return true;
}

void WebSocketServer::Serve(
    const std::function<std::unique_ptr<WebSocketHandler>()>& factory) {
  if (!context_ && !Start()) return;

  serve_factory_ = factory;

  while (running_) {
    lws_service(context_, 100);
  }

  serve_factory_ = nullptr;
}

void WebSocketServer::ServeOnce(WebSocketHandler& handler) {
  // For ServeOnce, we reuse the existing context if it exists.
  // lws v4.3.3 supports multi-threading with proper callback handling.
  if (!context_ && !Start()) {
    return;
  }
  running_ = true;

  serve_once_handler_ = &handler;
  serve_once_done_ = false;

  // Process events until serve_once_done_ is set (after message is processed).
  // lws_service(context_, 0) blocks for LWS_POLL_WAIT_LIMIT (~23 days).
  // Use small poll intervals to allow checking serve_once_done_.
  const int timeout_ms = 5000;
  const int poll_ms = 50;
  int elapsed = 0;
  while (elapsed < timeout_ms && !serve_once_done_ && running_) {
    int ret = lws_service(context_, poll_ms);
    if (ret < 0) break;
    elapsed += poll_ms;
  }
  // Run additional cycles to ensure the response is fully flushed.
  for (int i = 0; i < 10 && running_; ++i) {
    lws_service(context_, 100);
  }

  serve_once_handler_ = nullptr;
}

void WebSocketServer::Stop() {
  running_ = false;
  if (context_) {
    lws_context_destroy(context_);
    context_ = nullptr;
  }
}

}  // namespace net
}  // namespace orator
