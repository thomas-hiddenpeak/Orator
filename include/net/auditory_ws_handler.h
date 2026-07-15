#pragma once

// WebSocket handler that runs the AuditoryStream (independent diarization + ASR
// pipelines) over a real-time PCM stream. It is a thin transport adapter: it
// decodes incoming PCM frames and forwards them to AuditoryStream, and wires
// the stream's JSON result callback to the client connection.
//
// Architecture: the AuditoryStream (and its loaded GPU models) is created ONCE
// and shared across all connections via a shared_ptr. The first connection to
// send audio owns production for that session; other connections observe the
// same emitted events without resetting the stream. This avoids loading the
// ASR model once per client and allows the Web UI to observe an external test
// client safely.
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
//   {"type":"asr","start":..,"end":..,"text":..}            per completed
//   utterance
//   {"type":"timeline",...}                                  on flush/end

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "net/websocket_server.h"
#include "pipeline/auditory_stream.h"

namespace orator {
namespace net {

// Thread-safe session hub shared between AuditoryStream and all connections.
// Exactly one connection may produce audio; every registered connection
// receives the same stream-generated events.
struct SessionEmit {
  enum class ProducerClaim { kClaimed, kOwned, kBusy };

  std::mutex mu;
  std::vector<WebSocketConnection*> connections;  // guarded by mu
  WebSocketConnection* producer = nullptr;        // guarded by mu
  int64_t msg_id = 0;                             // guarded by mu

  void Register(WebSocketConnection* conn);
  void Unregister(WebSocketConnection* conn);
  ProducerClaim ClaimProducer(WebSocketConnection* conn);
  bool IsProducer(WebSocketConnection* conn);
  bool CanReset(WebSocketConnection* conn);
  void ReleaseProducer(WebSocketConnection* conn);

  // Transform a legacy message to the topic envelope once, then broadcast it.
  void Send(const std::string& json);
};

class AuditoryWsHandler final : public WebSocketHandler {
 public:
  // Accepts a pre-started shared stream and its associated emit target.
  AuditoryWsHandler(std::shared_ptr<pipeline::AuditoryStream> stream,
                    std::shared_ptr<SessionEmit> emit_target);

  void OnOpen(WebSocketConnection& conn) override;
  void OnBinary(WebSocketConnection& conn, const uint8_t* data,
                size_t n) override;
  void OnText(WebSocketConnection& conn, const std::string& text) override;
  void OnClose() override;

 private:
  std::shared_ptr<pipeline::AuditoryStream> stream_;
  std::shared_ptr<SessionEmit> emit_target_;
  WebSocketConnection* conn_ = nullptr;
  bool float_format_ = false;
  bool producer_conflict_reported_ = false;
};

}  // namespace net
}  // namespace orator
