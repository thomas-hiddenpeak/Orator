// asr_stream_incremental_probe (Spec 003 T031): drive the incremental
// KV-cache streaming session and measure per-step cost, sustained real-time
// factor, and memory, and write the transcript for CER (T040).
//
// Feeds audio to Qwen3Asr::StreamChunk in small slices (simulating a live
// stream). The session encodes each completed 8 s window standalone, appends
// its audio-token KV to the persistent cache, re-prefills only the short suffix
// and decodes. Per decode step (one per completed window) the probe records the
// wall time; the cost should stay flat across the stream (O(one window)) rather
// than growing linearly as the unbounded-window probe did.
//
// A boundary reset (StreamFinalize + StreamReset) bounds the cache length: pass
// a segment cap in seconds; the probe commits and restarts at the cap.
//
// Usage: asr_stream_incremental_probe <audio> <asr_model_dir> [dur_sec]
//        [feed_sec] [segment_cap_sec] [out_json]

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "io/audio_file.h"
#include "model/qwen3_asr.h"

using namespace orator;

namespace {
long VmRssKb() {
  std::ifstream f("/proc/self/status");
  std::string key, unit;
  long val = 0;
  while (f >> key) {
    if (key == "VmRSS:") { f >> val >> unit; return val; }
    std::getline(f, unit);
  }
  return 0;
}

std::string JsonEscape(const std::string& s) {
  std::string o;
  for (char c : s) {
    switch (c) {
      case '"': o += "\\\""; break;
      case '\\': o += "\\\\"; break;
      case '\n': o += "\\n"; break;
      case '\r': break;
      case '\t': o += "\\t"; break;
      default: o += c;
    }
  }
  return o;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    std::fprintf(stderr,
                 "usage: %s <audio> <asr_model_dir> [dur_sec] [feed_sec] "
                 "[segment_cap_sec] [out_json]\n",
                 argv[0]);
    return 2;
  }
  const std::string audio_path = argv[1];
  const std::string model_dir = argv[2];
  const double dur_sec = argc > 3 ? std::atof(argv[3]) : 120.0;
  const double feed_sec = argc > 4 ? std::atof(argv[4]) : 1.0;
  const double seg_cap_sec = argc > 5 ? std::atof(argv[5]) : 0.0;  // 0 = no reset
  const std::string out_json = argc > 6 ? argv[6] : "";
  const int sr = 16000;

  std::printf(">> decoding %s\n", audio_path.c_str());
  io::AudioData a = io::LoadAudioMono(audio_path, sr);
  long total = std::min(static_cast<long>(dur_sec * sr),
                        static_cast<long>(a.samples.size()));
  std::printf(">> %ld samples (%.1fs); feed=%.1fs seg_cap=%.1fs\n", total,
              total / double(sr), feed_sec, seg_cap_sec);

  model::Qwen3Asr asr;
  asr.LoadWeights(model_dir);

  const int feed = static_cast<int>(feed_sec * sr);
  const long seg_cap = static_cast<long>(seg_cap_sec * sr);

  auto now = [] { return std::chrono::steady_clock::now(); };
  auto ms = [](auto x, auto y) {
    return std::chrono::duration<double, std::milli>(y - x).count();
  };

  std::vector<double> step_ms;     // per decode-step wall time
  std::string committed;            // committed text across segments
  std::string live;
  long seg_consumed = 0;            // samples in current segment

  auto t_start = now();
  asr.StreamReset(0);
  int last_chunk_id = 0;
  for (long off = 0; off < total; off += feed) {
    const int n = static_cast<int>(std::min<long>(feed, total - off));
    auto s0 = now();
    std::string t = asr.StreamChunk(a.samples.data() + off, n);
    auto s1 = now();
    seg_consumed += n;
    // A new decode step happened iff the window counter advanced.
    if (asr.stream_chunk_id() != last_chunk_id) {
      last_chunk_id = asr.stream_chunk_id();
      step_ms.push_back(ms(s0, s1));
      live = t;
      std::printf("   step %2zu  audio=%6.1fs  tokens=%4d  wall=%7.1fms  "
                  "VmRSS=%ldMB\n",
                  step_ms.size(), (off + n) / double(sr),
                  asr.stream_audio_tokens(), step_ms.back(), VmRssKb() / 1024);
    }
    // Boundary reset to bound the cache length.
    if (seg_cap > 0 && seg_consumed >= seg_cap) {
      std::string seg_text = asr.StreamFinalize();
      if (!committed.empty()) committed += " ";
      committed += seg_text;
      asr.StreamReset(off + n);
      last_chunk_id = 0;
      seg_consumed = 0;
      std::printf("   -- segment reset @%.1fs --\n", (off + n) / double(sr));
    }
  }
  std::string tail = asr.StreamFinalize();
  if (!tail.empty()) {
    if (!committed.empty()) committed += " ";
    committed += tail;
  }
  auto t_end = now();

  const double wall = ms(t_start, t_end) / 1000.0;
  const double audio = total / double(sr);
  std::printf("\n>> wall %.2fs for %.1fs audio -> RTF %.2fx\n", wall, audio,
              audio / wall);
  if (!step_ms.empty()) {
    const size_t third = step_ms.size() / 3;
    double first_avg = 0, last_avg = 0;
    for (size_t i = 0; i < third; ++i) first_avg += step_ms[i];
    for (size_t i = step_ms.size() - third; i < step_ms.size(); ++i)
      last_avg += step_ms[i];
    if (third > 0) { first_avg /= third; last_avg /= third; }
    std::printf(">> per-step wall: first-third avg %.1fms  last-third avg %.1fms"
                "  ratio %.2f (bounded if near 1, O(n) if growing)\n",
                first_avg, last_avg,
                first_avg > 0 ? last_avg / first_avg : 0.0);
  }
  std::printf(">> final transcript (%zu chars):\n%s\n", committed.size(),
              committed.c_str());

  if (!out_json.empty()) {
    std::ofstream f(out_json);
    f << "{\"comprehensive\":[{\"start\":0,\"end\":" << audio
      << ",\"speaker\":\"\",\"text\":\"" << JsonEscape(committed) << "\"}]}\n";
    std::printf(">> wrote %s\n", out_json.c_str());
  }
  return 0;
}
