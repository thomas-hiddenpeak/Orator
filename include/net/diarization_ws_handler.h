#pragma once

// WebSocket handler that turns a real-time PCM stream into diarized speaker
// segments. It bridges the dependency-free WebSocketServer to the verified
// streaming Sortformer diarizer.
//
// Wire protocol (client -> server):
//   * Binary frames: raw mono PCM at the configured sample rate. The sample
//     format is int16 little-endian by default (the common capture format);
//     send a text control {"format":"f32"} first to switch to float32.
//     Each binary frame is fed straight into the streaming diarizer as it
//     arrives (true incremental streaming): the diarizer keeps persistent
//     spkcache/FIFO/mel state so speaker identity is continuous across the
//     whole session, compute is O(n) (not O(n^2)), and memory is bounded.
//   * Text control messages (matched as substrings, tolerant of JSON):
//       "reset" -> reset diarizer state and drop the accumulated timeline
//       "flush" -> emit the timeline accumulated so far; streaming continues
//                  (speaker identity is preserved for subsequent audio)
//       "end"   -> finalize: emit any buffered tail frames, then the timeline
//
// Server -> client (text frames, JSON):
//   {"type":"diarization","audio_sec":S,"compute_sec":C,"rt_factor":R,
//    "segments":[{"start":..,"end":..,"speaker":k,"confidence":..}, ...]}

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
  // Retained for CLI/config compatibility. With true incremental streaming the
  // diarizer's internal buffers are already bounded (it processes each binary
  // frame as it arrives), so this no longer guards memory. 0 disables.
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
  // Emits the accumulated timeline. When finalize is true, flushes any buffered
  // tail frames out of the diarizer first; otherwise streaming continues with
  // speaker identity preserved.
  void Flush(WebSocketConnection& conn, bool finalize);
  void ResetState();

  DiarizationWsConfig config_;
  std::unique_ptr<model::SortformerDiarizer> diarizer_;
  std::vector<float> accum_probs_;  // full-session frames, [frame*n_spk + spk]
  long total_frames_ = 0;           // accumulated emitted frames
  long total_samples_ = 0;          // audio ingested this session (for rt stats)
  double compute_sec_ = 0.0;        // cumulative diarizer compute time
  bool float_format_ = false;
};

}  // namespace net
}  // namespace orator
