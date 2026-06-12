#include "net/auditory_ws_handler.h"

#include <cstdint>
#include <vector>

namespace orator {
namespace net {

AuditoryWsHandler::AuditoryWsHandler(const pipeline::AuditoryStream::Config& config)
    : config_(config) {}

void AuditoryWsHandler::OnOpen(WebSocketConnection& conn) {
  conn_ = &conn;
  // The stream emits JSON result events; forward each straight to the client.
  stream_ = std::make_unique<pipeline::AuditoryStream>(
      config_, [this](const std::string& json) {
        if (conn_) conn_->SendText(json);
      });
  stream_->Start();

  std::string ready = "{\"type\":\"ready\",\"sample_rate\":" +
                      std::to_string(config_.sample_rate) +
                      ",\"asr\":" + (stream_->asr_enabled() ? "true" : "false") +
                      "}";
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
  if (!in.empty() && stream_)
    stream_->PushAudio(in.data(), static_cast<int>(in.size()));
}

void AuditoryWsHandler::OnText(WebSocketConnection& conn, const std::string& text) {
  if (text.find("\"f32\"") != std::string::npos ||
      text.find("float32") != std::string::npos) {
    float_format_ = true;
  }
  if (text.find("reset") != std::string::npos) {
    if (stream_) stream_->Reset();
    conn.SendText("{\"type\":\"reset_ok\"}");
    return;
  }
  if (text.find("end") != std::string::npos) {
    if (stream_) {
      stream_->EmitTimeline(/*finalize=*/true);
      stream_->Reset();
    }
    return;
  }
  if (text.find("flush") != std::string::npos) {
    if (stream_) stream_->EmitTimeline(/*finalize=*/false);
  }
}

void AuditoryWsHandler::OnClose() { conn_ = nullptr; }

}  // namespace net
}  // namespace orator
