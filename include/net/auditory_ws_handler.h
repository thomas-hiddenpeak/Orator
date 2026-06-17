#pragma once

// WebSocket handler that runs the AuditoryStream (independent diarization + ASR
// pipelines) over a real-time PCM stream. It is a thin transport adapter: it
// decodes incoming PCM frames and forwards them to AuditoryStream, and wires the
// stream's JSON result callback to the client connection.
//
// Architecture: the AuditoryStream (and its loaded GPU models) is created ONCE
// and shared across all connections via a shared_ptr. Each new client connection
// calls Reset() on the stream and re-routes the emit callback to the new socket.
// This avoids the GPU OOM crash caused by loading the ASR model once per client.
//
// Wire protocol (client -> server):
//   * Binary frames: raw mono PCM at the configured sample rate. int16
//     little-endian by default; send text {"format":"f32"} first to switch to
//     float32. Each frame feeds BOTH pipelines independently.
//   * Text control (substring-matched, JSON-tolerant):
//       "reset" -> drop all state, start a fresh session
//       "flush" -> emit the unified timeline so far; streaming continues
//       "end"   -> finalize: drain tails, emit the unified timeline, reset
//
// Server -> client (text JSON):
//   {"type":"ready",...}                                    on open
//   {"type":"asr","start":..,"end":..,"text":..}            per completed utterance
//   {"type":"timeline",...}                                  on flush/end

#include <memory>
#include <mutex>
#include <string>

#include "net/websocket_server.h"
#include "pipeline/auditory_stream.h"

namespace orator {
namespace net {

// Thread-safe emit target shared between the AuditoryStream and all connections.
// The active connection registers itself; previous connection clears on close.
struct SessionEmit {
  std::mutex mu;
  WebSocketConnection* conn = nullptr;  // guarded by mu

  void Send(const std::string& json) {
    std::lock_guard<std::mutex> lk(mu);
    if (conn) conn->SendText(json);
  }
};

class AuditoryWsHandler final : public WebSocketHandler {
 public:
  // Accepts a pre-started shared stream and its associated emit target.
  AuditoryWsHandler(std::shared_ptr<pipeline::AuditoryStream> stream,
                    std::shared_ptr<SessionEmit> emit_target);

  void OnOpen(WebSocketConnection& conn) override;
  void OnBinary(WebSocketConnection& conn, const uint8_t* data, size_t n) override;
  void OnText(WebSocketConnection& conn, const std::string& text) override;
  void OnClose() override;

 private:
  std::shared_ptr<pipeline::AuditoryStream> stream_;
  std::shared_ptr<SessionEmit> emit_target_;
  WebSocketConnection* conn_ = nullptr;
  bool float_format_ = false;
};

}  // namespace net
}  // namespace orator
