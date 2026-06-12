#pragma once

// WebSocket handler that runs the AuditoryStream (independent diarization + ASR
// pipelines) over a real-time PCM stream. It is a thin transport adapter: it
// decodes incoming PCM frames and forwards them to AuditoryStream, and wires the
// stream's JSON result callback to the client connection.
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
//   {"type":"timeline","audio_sec":..,"diar_compute_sec":..,
//    "asr_compute_sec":..,"diarization":[..],"transcript":[..]}  on flush/end

#include <memory>
#include <string>

#include "net/websocket_server.h"
#include "pipeline/auditory_stream.h"

namespace orator {
namespace net {

class AuditoryWsHandler final : public WebSocketHandler {
 public:
  explicit AuditoryWsHandler(const pipeline::AuditoryStream::Config& config);

  void OnOpen(WebSocketConnection& conn) override;
  void OnBinary(WebSocketConnection& conn, const uint8_t* data, size_t n) override;
  void OnText(WebSocketConnection& conn, const std::string& text) override;
  void OnClose() override;

 private:
  pipeline::AuditoryStream::Config config_;
  std::unique_ptr<pipeline::AuditoryStream> stream_;
  WebSocketConnection* conn_ = nullptr;  // valid for the connection lifetime
  bool float_format_ = false;
};

}  // namespace net
}  // namespace orator
