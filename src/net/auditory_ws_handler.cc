#include "net/auditory_ws_handler.h"

#include <cstdint>
#include <vector>

namespace orator {
namespace net {

AuditoryWsHandler::AuditoryWsHandler(
    std::shared_ptr<pipeline::AuditoryStream> stream,
    std::shared_ptr<SessionEmit> emit_target)
    : stream_(std::move(stream)), emit_target_(std::move(emit_target)) {}

void AuditoryWsHandler::OnOpen(WebSocketConnection& conn) {
  conn_ = &conn;
  float_format_ = false;

  // Route emitted events to this connection. Previous connection has already
  // cleared the pointer in OnClose(); we take ownership here under the lock.
  {
    std::lock_guard<std::mutex> lk(emit_target_->mu);
    emit_target_->conn = &conn;
  }

  // Reset the shared stream for a fresh session (models stay loaded).
  stream_->Reset();

  std::string ready = "{\"type\":\"ready\",\"sample_rate\":" +
                      std::to_string(16000) +  // config is embedded in stream
                      ",\"asr\":" + (stream_->asr_enabled() ? "true" : "false") +
                      ",\"time_base\":\"absolute_samples\",\"origin_sample\":0}";
  conn.SendText(ready);
}

void AuditoryWsHandler::OnBinary(WebSocketConnection& conn, const uint8_t* data,
                                 size_t n) {
  (void)conn;
  std::vector<float> in;
  if (float_format_) {
    size_t count = n / sizeof(float);
    const float* f = reinterpret_cast<const float*>(data);
    in.assign(f, f + count);
  } else {
    size_t count = n / sizeof(int16_t);
    in.reserve(count);
    const uint8_t* p = data;
    for (size_t i = 0; i < count; ++i) {
      int16_t s = int16_t(uint16_t(p[0]) | (uint16_t(p[1]) << 8));
      in.push_back(float(s) / 32768.0f);
      p += 2;
    }
  }
  if (!in.empty()) stream_->PushAudio(in.data(), static_cast<int>(in.size()));
}

void AuditoryWsHandler::OnText(WebSocketConnection& conn, const std::string& text) {
  if (text.find("\"f32\"") != std::string::npos ||
      text.find("float32") != std::string::npos) {
    float_format_ = true;
  }
  if (text.find("reset") != std::string::npos) {
    stream_->Reset();
    conn.SendText("{\"type\":\"reset_ok\"}");
    return;
  }
  if (text.find("end") != std::string::npos) {
    stream_->EmitTimeline(/*finalize=*/true);
    stream_->Reset();
    return;
  }
  if (text.find("flush") != std::string::npos) {
    stream_->EmitTimeline(/*finalize=*/false);
  }
}

void AuditoryWsHandler::OnClose() {
  // Deregister this connection as the emit target so in-flight worker events
  // are silently dropped (not sent to a dead socket).
  {
    std::lock_guard<std::mutex> lk(emit_target_->mu);
    if (emit_target_->conn == conn_) emit_target_->conn = nullptr;
  }
  conn_ = nullptr;
  float_format_ = false;
  // stream_ is shared; do NOT reset or destroy it here.
}

}  // namespace net
}  // namespace orator
