#include "net/diarization_ws_handler.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "core/types.h"
#include "pipeline/diar_postprocess.h"

namespace orator {
namespace net {

std::string DiarizationSegmentsToJson(const std::vector<core::DiarSegment>& segs,
                                      double audio_sec, double compute_sec) {
  char buf[256];
  std::string out = "{\"type\":\"diarization\",";
  std::snprintf(buf, sizeof(buf),
                "\"audio_sec\":%.3f,\"compute_sec\":%.3f,\"rt_factor\":%.3f,",
                audio_sec, compute_sec,
                compute_sec > 0 ? audio_sec / compute_sec : 0.0);
  out += buf;
  out += "\"segments\":[";
  for (size_t i = 0; i < segs.size(); ++i) {
    const auto& s = segs[i];
    std::snprintf(buf, sizeof(buf),
                  "{\"start\":%.3f,\"end\":%.3f,\"speaker\":%d,"
                  "\"confidence\":%.3f}",
                  s.start_sec, s.end_sec, s.local_speaker, s.confidence);
    out += buf;
    if (i + 1 < segs.size()) out += ",";
  }
  out += "]}";
  return out;
}

DiarizationWsHandler::DiarizationWsHandler(const DiarizationWsConfig& config)
    : config_(config) {
  diarizer_ = std::make_unique<model::SortformerDiarizer>();
  core::DiarizationConfig dc;
  dc.sample_rate = config_.sample_rate;
  dc.max_speakers = config_.max_speakers;
  dc.activity_threshold = config_.activity_threshold;
  diarizer_->Initialize(dc);
  diarizer_->LoadWeights(config_.weights);
}

void DiarizationWsHandler::OnOpen(WebSocketConnection& conn) {
  conn.SendText("{\"type\":\"ready\",\"sample_rate\":" +
                std::to_string(config_.sample_rate) + "}");
}

void DiarizationWsHandler::OnBinary(WebSocketConnection& conn,
                                    const uint8_t* data, size_t n) {
  // Decode the incoming PCM into a temporary, then append under the buffer cap.
  std::vector<float> in;
  if (float_format_) {
    size_t count = n / sizeof(float);
    const float* f = reinterpret_cast<const float*>(data);
    in.assign(f, f + count);
  } else {
    size_t count = n / sizeof(int16_t);
    const uint8_t* p = data;
    in.reserve(count);
    for (size_t i = 0; i < count; ++i) {
      int16_t s = int16_t(uint16_t(p[0]) | (uint16_t(p[1]) << 8));
      in.push_back(float(s) / 32768.0f);
      p += 2;
    }
  }

  // Bound the accumulated buffer to prevent unbounded memory growth and the
  // sample-count integer overflow on very long un-flushed sessions.
  size_t cap = 0;
  if (config_.max_buffer_sec > 0)
    cap = static_cast<size_t>(config_.max_buffer_sec * config_.sample_rate);
  if (cap && pcm_.size() + in.size() > cap) {
    size_t room = pcm_.size() < cap ? cap - pcm_.size() : 0;
    if (room < in.size()) in.resize(room);
    if (!buffer_capped_) {
      buffer_capped_ = true;
      conn.SendText(
          "{\"type\":\"warning\",\"code\":\"buffer_cap_reached\","
          "\"max_buffer_sec\":" +
          std::to_string(config_.max_buffer_sec) +
          ",\"detail\":\"flush+reset to continue; excess audio dropped\"}");
    }
  }
  pcm_.insert(pcm_.end(), in.begin(), in.end());
}

void DiarizationWsHandler::OnText(WebSocketConnection& conn,
                                  const std::string& text) {
  if (text.find("\"f32\"") != std::string::npos ||
      text.find("float32") != std::string::npos) {
    float_format_ = true;
  }
  if (text.find("reset") != std::string::npos) {
    pcm_.clear();
    buffer_capped_ = false;
    diarizer_->Reset();
    conn.SendText("{\"type\":\"reset_ok\"}");
    return;
  }
  if (text.find("flush") != std::string::npos ||
      text.find("end") != std::string::npos) {
    Flush(conn);
  }
}

void DiarizationWsHandler::Flush(WebSocketConnection& conn) {
  if (pcm_.empty()) {
    conn.SendText(DiarizationSegmentsToJson({}, 0.0, 0.0));
    return;
  }
  core::AudioChunk chunk;
  chunk.samples = pcm_.data();
  // pcm_ is bounded by max_buffer_sec, so this fits in int; clamp defensively.
  chunk.num_samples = pcm_.size() > size_t(INT32_MAX)
                          ? INT32_MAX
                          : static_cast<int>(pcm_.size());
  chunk.sample_rate = config_.sample_rate;
  chunk.t_start_sec = 0.0;

  auto t0 = std::chrono::steady_clock::now();
  core::DiarizationFrames frames = diarizer_->ProcessChunk(chunk);
  auto t1 = std::chrono::steady_clock::now();
  double compute_s = std::chrono::duration<double>(t1 - t0).count();
  double audio_s = double(pcm_.size()) / config_.sample_rate;

  auto segs = pipeline::FramesToSegments(frames, config_.activity_threshold,
                                         config_.merge_gap_sec);
  segs = pipeline::CoalesceSegments(std::move(segs), config_.merge_gap_sec);

  conn.SendText(DiarizationSegmentsToJson(segs, audio_s, compute_s));
}

void DiarizationWsHandler::OnClose() {}

}  // namespace net
}  // namespace orator
