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
  // Decode the incoming PCM frame.
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
  if (in.empty()) return;

  // True incremental streaming: feed this frame straight into the diarizer,
  // which keeps persistent state across the session. Newly stabilized frames
  // are appended to the full-session timeline. Compute is O(n) and the
  // diarizer's internal buffers stay bounded regardless of session length.
  auto t0 = std::chrono::steady_clock::now();
  core::DiarizationFrames part =
      diarizer_->StreamAudio(in.data(), static_cast<int>(in.size()), false);
  auto t1 = std::chrono::steady_clock::now();
  compute_sec_ += std::chrono::duration<double>(t1 - t0).count();
  total_samples_ += static_cast<long>(in.size());

  if (part.num_frames > 0) {
    accum_probs_.insert(accum_probs_.end(), part.probs.begin(),
                        part.probs.end());
    total_frames_ += part.num_frames;
  }
  (void)conn;
}

void DiarizationWsHandler::OnText(WebSocketConnection& conn,
                                  const std::string& text) {
  if (text.find("\"f32\"") != std::string::npos ||
      text.find("float32") != std::string::npos) {
    float_format_ = true;
  }
  if (text.find("reset") != std::string::npos) {
    ResetState();
    conn.SendText("{\"type\":\"reset_ok\"}");
    return;
  }
  if (text.find("end") != std::string::npos) {
    Flush(conn, /*finalize=*/true);
    return;
  }
  if (text.find("flush") != std::string::npos) {
    Flush(conn, /*finalize=*/false);
  }
}

void DiarizationWsHandler::ResetState() {
  accum_probs_.clear();
  total_frames_ = 0;
  total_samples_ = 0;
  compute_sec_ = 0.0;
  diarizer_->Reset();
}

void DiarizationWsHandler::Flush(WebSocketConnection& conn, bool finalize) {
  if (finalize) {
    // Drain any buffered tail frames (partial final chunk) out of the diarizer.
    auto t0 = std::chrono::steady_clock::now();
    core::DiarizationFrames tail = diarizer_->StreamAudio(nullptr, 0, true);
    auto t1 = std::chrono::steady_clock::now();
    compute_sec_ += std::chrono::duration<double>(t1 - t0).count();
    if (tail.num_frames > 0) {
      accum_probs_.insert(accum_probs_.end(), tail.probs.begin(),
                          tail.probs.end());
      total_frames_ += tail.num_frames;
    }
  }

  if (total_frames_ == 0) {
    conn.SendText(DiarizationSegmentsToJson({}, 0.0, 0.0));
    return;
  }

  core::DiarizationFrames frames;
  frames.num_frames = static_cast<int>(total_frames_);
  frames.num_speakers = config_.max_speakers;
  frames.frame_period_sec = diarizer_->frame_period_sec();
  frames.t_start_sec = 0.0;
  frames.probs = accum_probs_;

  double audio_s = double(total_samples_) / config_.sample_rate;

  auto segs = pipeline::FramesToSegments(frames, config_.activity_threshold,
                                         config_.merge_gap_sec);
  segs = pipeline::CoalesceSegments(std::move(segs), config_.merge_gap_sec);

  conn.SendText(DiarizationSegmentsToJson(segs, audio_s, compute_sec_));

  // After a finalize, the session is complete; reset so a subsequent stream
  // starts fresh (clients that keep streaming should use "flush", not "end").
  if (finalize) ResetState();
}

void DiarizationWsHandler::OnClose() {}

}  // namespace net
}  // namespace orator
