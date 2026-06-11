#pragma once

// WebSocket handler that turns a real-time PCM stream into diarized speaker
// segments. It bridges the dependency-free WebSocketServer to the verified
// streaming Sortformer diarizer.
//
// Wire protocol (client -> server):
//   * Binary frames: raw mono PCM at the configured sample rate. The sample
//     format is int16 little-endian by default (the common capture format);
//     send a text control {"format":"f32"} first to switch to float32.
//   * Text control messages (matched as substrings, tolerant of JSON):
//       "reset" -> drop accumulated audio and reset diarizer state
//       "flush" -> run diarization now and emit current timeline
//       "end"   -> same as flush (typically the last message)
//
// Server -> client (text frames, JSON):
//   {"type":"diarization","audio_sec":S,"compute_sec":C,"rt_factor":R,
//    "segments":[{"start":..,"end":..,"speaker":k,"confidence":..}, ...]}
//   {"type":"warning","code":"buffer_cap_reached",...}  (un-flushed audio hit
//    max_buffer_sec; excess dropped — flush+reset to continue)

#include <memory>
#include <string>
#include <vector>

#include "core/stages.h"
#include "model/streaming_sortformer.h"
#include "net/websocket_server.h"

namespace orator {
namespace net {

struct DiarizationWsConfig {
  std::string weights = "models/sortformer_4spk_v2.safetensors";
  int sample_rate = 16000;
  int max_speakers = 4;
  float activity_threshold = 0.5f;
  double merge_gap_sec = 0.5;
  // Hard cap on accumulated (un-flushed) audio to bound memory and prevent the
  // sample-count integer overflow on extremely long sessions. Audio beyond the
  // cap is dropped and the client is notified once; it should flush+reset
  // periodically (the supported real-time pattern). 0 disables the cap.
  double max_buffer_sec = 1800.0;
};

// Builds a diarization JSON document from segments + timing (exposed for tests).
std::string DiarizationSegmentsToJson(const std::vector<core::DiarSegment>& segs,
                                      double audio_sec, double compute_sec);

class DiarizationWsHandler final : public WebSocketHandler {
 public:
  explicit DiarizationWsHandler(const DiarizationWsConfig& config);

  void OnOpen(WebSocketConnection& conn) override;
  void OnBinary(WebSocketConnection& conn, const uint8_t* data,
                size_t n) override;
  void OnText(WebSocketConnection& conn, const std::string& text) override;
  void OnClose() override;

 private:
  void Flush(WebSocketConnection& conn);

  DiarizationWsConfig config_;
  std::unique_ptr<model::SortformerDiarizer> diarizer_;
  std::vector<float> pcm_;  // accumulated mono float samples
  bool float_format_ = false;
  bool buffer_capped_ = false;  // true once max_buffer_sec was hit (one-shot warn)
};

}  // namespace net
}  // namespace orator
