#include "net/auditory_ws_handler.h"

#include "core/log.h"
#include "protocol/protocol_timeline.h"
#include "protocol/session_store.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace orator {
namespace net {

namespace {

// Escape a raw JSON string so it can be safely embedded as a value inside
// another JSON object. Replaces " with \", \ with \\, newlines, etc.
std::string EscapeJsonString(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 16);
  for (char c : s) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char buf[8];
          snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
          out += buf;
        } else {
          out += c;
        }
    }
  }
  return out;
}

// Check if the JSON message already contains a "topic" key (protocol envelope).
bool HasTopicKey(const std::string& json) {
  // Look for "topic" as a JSON key (preceded by { or , and whitespace).
  const std::string needle = "\"topic\"";
  size_t pos = 0;
  while ((pos = json.find(needle, pos)) != std::string::npos) {
    // Verify it's a key: check character before is { or , (with optional
    // whitespace).
    if (pos == 0) {
      pos += needle.size();
      continue;
    }
    char prev = json[pos - 1];
    if (prev == '{' || prev == ',' || prev == ' ' || prev == '\t' ||
        prev == '\n' || prev == '\r') {
      // Also verify there's a : after the key name
      size_t afterKey = pos + needle.size();
      while (afterKey < json.size() &&
             (json[afterKey] == ' ' || json[afterKey] == '\t'))
        ++afterKey;
      if (afterKey < json.size() && json[afterKey] == ':') return true;
    }
    pos += needle.size();
  }
  return false;
}

// Extract the "type" value from a legacy JSON message. Returns empty string if
// not found.
std::string ExtractType(const std::string& json) {
  const std::string needle = "\"type\"";
  size_t pos = json.find(needle);
  if (pos == std::string::npos) return "";

  size_t afterKey = pos + needle.size();
  while (afterKey < json.size() &&
         (json[afterKey] == ' ' || json[afterKey] == '\t'))
    ++afterKey;
  if (afterKey >= json.size() || json[afterKey] != ':') return "";
  ++afterKey;  // skip ':'
  while (afterKey < json.size() &&
         (json[afterKey] == ' ' || json[afterKey] == '\t'))
    ++afterKey;
  if (afterKey >= json.size() || json[afterKey] != '"') return "";
  ++afterKey;  // skip opening quote

  size_t end = json.find('"', afterKey);
  if (end == std::string::npos) return "";
  return json.substr(afterKey, end - afterKey);
}

// Wrap a legacy message in the new topic-based envelope format.
// If the message already has a "topic" key, return it unchanged.
std::string WrapMessageInEnvelope(const std::string& json, int64_t msg_id) {
  if (HasTopicKey(json)) {
    return json;
  }

  std::string type = ExtractType(json);
  if (type.empty()) type = "unknown";

  std::string topic = type + "/event";

  // Escape the original JSON for embedding in the "data" field.
  std::string escapedData = EscapeJsonString(json);

  // Build envelope. Known limitation: ts is always 0 because the emit
  // callback only passes raw JSON strings, not timestamps. Consumers
  // should read the actual timestamp from envelope.data.start or
  // envelope.data.time_sec.
  std::string envelope = "{\"topic\":\"" + topic + "\",\"pipeline\":\"" + type +
                         "\",\"pipeline_version\":\"1.0.0\"" +
                         ",\"msg_id\":" + std::to_string(msg_id) + ",\"ts\":0" +
                         ",\"qos\":0" + ",\"schema_version\":1" +
                         ",\"data\":\"" + escapedData + "\"}";
  return envelope;
}

std::string BuildReadyMessage(
    const std::shared_ptr<pipeline::AuditoryStream>& stream) {
  const auto now = std::chrono::system_clock::now();
  const double wall_start =
      std::chrono::duration<double>(now.time_since_epoch()).count();
  return "{\"type\":\"ready\",\"sample_rate\":16000,\"asr\":" +
         std::string(stream->asr_enabled() ? "true" : "false") +
         ",\"time_base\":\"absolute_samples\",\"origin_sample\":0," +
         "\"session_start_wall_sec\":" + std::to_string(wall_start) +
         ",\"protocol_version\":2,\"envelope_format\":true}";
}

}  // namespace

void SessionEmit::Register(WebSocketConnection* conn) {
  if (conn == nullptr) return;
  std::lock_guard<std::mutex> lk(mu);
  if (std::find(connections.begin(), connections.end(), conn) ==
      connections.end()) {
    connections.push_back(conn);
  }
}

void SessionEmit::Unregister(WebSocketConnection* conn) {
  std::lock_guard<std::mutex> lk(mu);
  connections.erase(std::remove(connections.begin(), connections.end(), conn),
                    connections.end());
  if (producer == conn) producer = nullptr;
}

SessionEmit::ProducerClaim SessionEmit::ClaimProducer(
    WebSocketConnection* conn) {
  std::lock_guard<std::mutex> lk(mu);
  if (producer == conn) return ProducerClaim::kOwned;
  if (producer != nullptr) return ProducerClaim::kBusy;
  producer = conn;
  return ProducerClaim::kClaimed;
}

bool SessionEmit::IsProducer(WebSocketConnection* conn) {
  std::lock_guard<std::mutex> lk(mu);
  return producer == conn;
}

bool SessionEmit::CanReset(WebSocketConnection* conn) {
  std::lock_guard<std::mutex> lk(mu);
  return producer == nullptr || producer == conn;
}

void SessionEmit::ReleaseProducer(WebSocketConnection* conn) {
  std::lock_guard<std::mutex> lk(mu);
  if (producer == conn) producer = nullptr;
}

void SessionEmit::Send(const std::string& json) {
  std::lock_guard<std::mutex> lk(mu);
  const std::string envelope = WrapMessageInEnvelope(json, ++msg_id);
  for (WebSocketConnection* conn : connections) {
    if (conn != nullptr && !conn->closed()) conn->SendText(envelope);
  }
}

AuditoryWsHandler::AuditoryWsHandler(
    std::shared_ptr<pipeline::AuditoryStream> stream,
    std::shared_ptr<SessionEmit> emit_target)
    : stream_(std::move(stream)), emit_target_(std::move(emit_target)) {}

void AuditoryWsHandler::OnOpen(WebSocketConnection& conn) {
  conn_ = &conn;
  float_format_ = false;
  producer_conflict_reported_ = false;
  emit_target_->Register(&conn);
  conn.SendText(BuildReadyMessage(stream_));
}

void AuditoryWsHandler::OnBinary(WebSocketConnection& conn, const uint8_t* data,
                                 size_t n) {
  const SessionEmit::ProducerClaim claim = emit_target_->ClaimProducer(&conn);
  if (claim == SessionEmit::ProducerClaim::kBusy) {
    if (!producer_conflict_reported_) {
      conn.SendText(
          "{\"type\":\"error\",\"error\":\"audio producer already active\"}");
      producer_conflict_reported_ = true;
    }
    return;
  }
  if (claim == SessionEmit::ProducerClaim::kClaimed) {
    // Producer ownership begins a new common-time-base session. Observer
    // connections remain registered and receive the matching ready event.
    stream_->Reset();
    emit_target_->Send(BuildReadyMessage(stream_));
  }
  LOG_DEBUG("OnBinary: received %zu bytes\n", n);
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
  LOG_DEBUG("OnBinary: converted to %zu float samples\n", in.size());
  if (!in.empty()) {
    LOG_DEBUG("OnBinary: calling stream_->PushAudio\n");
    stream_->PushAudio(in.data(), static_cast<int>(in.size()));
    LOG_DEBUG("OnBinary: PushAudio returned\n");
  }
}

void AuditoryWsHandler::OnText(WebSocketConnection& conn,
                               const std::string& text) {
  // Match known commands by JSON key (not substring) to avoid false positives.
  if (text.find("\"f32\"") != std::string::npos) {
    float_format_ = true;
  }
  if (text.find("\"describe\"") != std::string::npos) {
    const auto* pt = stream_->protocol_timeline();
    if (pt) {
      conn.SendText(pt->Describe());
    } else {
      conn.SendText("{\"error\":\"protocol_timeline not initialized\"}");
    }
    return;
  }
  if (text.find("\"reset\"") != std::string::npos) {
    if (!emit_target_->CanReset(&conn)) {
      conn.SendText(
          "{\"type\":\"error\",\"error\":\"only the audio producer may "
          "reset\"}");
      return;
    }
    stream_->Reset();
    emit_target_->Send(BuildReadyMessage(stream_));
    conn.SendText("{\"type\":\"reset_ok\"}");
    return;
  }
  if (text.find("\"end\"") != std::string::npos) {
    if (!emit_target_->IsProducer(&conn)) {
      conn.SendText(
          "{\"type\":\"error\",\"error\":\"only the audio producer may end\"}");
      return;
    }
    stream_->EmitTimeline(/*finalize=*/true);
    stream_->Reset();
    emit_target_->ReleaseProducer(&conn);
    return;
  }
  if (text.find("\"flush\"") != std::string::npos) {
    stream_->EmitTimeline(/*finalize=*/false);
    return;
  }
  if (text.find("\"sessions\"") != std::string::npos) {
    auto* store = stream_->session_store();
    if (!store || !store->enabled()) {
      conn.SendText("{\"type\":\"sessions\",\"sessions\":[]}");
      return;
    }
    auto list = store->List();
    std::string resp = "{\"type\":\"sessions\",\"sessions\":[";
    for (size_t i = 0; i < list.size(); ++i) {
      if (i > 0) resp += ",";
      resp += "{\"id\":\"" + list[i].session_id +
              "\",\"time\":" + std::to_string(list[i].wall_clock_sec) +
              ",\"audio_sec\":" + std::to_string(list[i].audio_sec) +
              ",\"file_size\":" + std::to_string(list[i].file_size) + "}";
    }
    resp += "]}";
    conn.SendText(resp);
    return;
  }
  if (text.find("\"rename_speaker\"") != std::string::npos) {
    // {"rename_speaker":{"id":"spk_0","name":"..."}} — set a display name for a
    // global voiceprint identity (Spec 006 / Spec 010 naming hook).
    auto extract = [&text](const char* key) -> std::string {
      const std::string k = std::string("\"") + key + "\"";
      size_t p = text.find(k);
      if (p == std::string::npos) return std::string();
      size_t vs = text.find('"', p + k.size() + 1);  // value opening quote
      if (vs == std::string::npos) return std::string();
      ++vs;
      size_t ve = text.find('"', vs);
      if (ve == std::string::npos) return std::string();
      return text.substr(vs, ve - vs);
    };
    const std::string id = extract("id");
    const std::string name = extract("name");
    if (id.empty()) {
      conn.SendText("{\"error\":\"missing speaker id\"}");
      return;
    }
    const bool ok = stream_->RenameSpeaker(id, name);
    if (!ok) {
      conn.SendText("{\"error\":\"speaker registry not available\"}");
      return;
    }
    conn.SendText(stream_->SerializeSpeakers());  // echo the updated list
    return;
  }
  if (text.find("\"speakers\"") != std::string::npos) {
    conn.SendText(stream_->SerializeSpeakers());
    return;
  }
  if (text.find("\"load_session\"") != std::string::npos) {
    auto* store = stream_->session_store();
    if (!store || !store->enabled()) {
      conn.SendText("{\"error\":\"session store not available\"}");
      return;
    }
    // Extract session_id value from JSON (simple substring parse).
    std::string sid_key = "\"session_id\"";
    size_t pos = text.find(sid_key);
    if (pos == std::string::npos) {
      conn.SendText("{\"error\":\"missing session_id\"}");
      return;
    }
    size_t val_start = text.find('"', pos + sid_key.size() + 1);
    if (val_start == std::string::npos) {
      conn.SendText("{\"error\":\"invalid session_id format\"}");
      return;
    }
    ++val_start;
    size_t val_end = text.find('"', val_start);
    if (val_end == std::string::npos) {
      conn.SendText("{\"error\":\"invalid session_id format\"}");
      return;
    }
    std::string session_id = text.substr(val_start, val_end - val_start);
    std::string timeline = store->Load(session_id);
    if (timeline.empty()) {
      conn.SendText("{\"error\":\"session not found\",\"session_id\":\"" +
                    session_id + "\"}");
      return;
    }
    conn.SendText(timeline);
    return;
  }
}

void AuditoryWsHandler::OnClose() {
  emit_target_->Unregister(conn_);
  conn_ = nullptr;
  float_format_ = false;
  // stream_ is shared; do NOT reset or destroy it here.
}

}  // namespace net
}  // namespace orator
