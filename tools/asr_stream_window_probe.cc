// asr_stream_window_probe: evaluate the official Qwen3-ASR growing-window
// streaming method (audio_accum + committed-prefix rollback) to observe where
// its cost and memory grow as the stream lengthens.
//
// The official streaming algorithm (qwen_asr/inference/qwen3_asr.py):
//   - buffer PCM; every chunk_size_sec, append the chunk to audio_accum
//     (all audio from the stream start, never trimmed) and re-run the model
//     over the entire accumulation;
//   - the prompt is base + committed-text-prefix, where the prefix is the
//     previously decoded text with the last `unfixed_token_num` tokens rolled
//     back; the first `unfixed_chunk_num` chunks use an empty prefix.
//
// This tool reports, per step: elapsed audio, step wall time, the step's
// real-time factor, and process resident memory (VmRSS). It exposes the O(n)
// per-step growth (and therefore O(n^2) total) of the unbounded window so we
// can decide whether a bounded sliding window is required.
//
// Usage: asr_stream_window_probe <audio> <asr_model_dir> [dur_sec]
//        [chunk_sec] [unfixed_chunk_num] [unfixed_token_num]

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
  std::string key;
  long val = 0;
  std::string unit;
  while (f >> key) {
    if (key == "VmRSS:") {
      f >> val >> unit;
      return val;
    }
    std::getline(f, unit);
  }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    std::fprintf(stderr,
                 "usage: %s <audio> <asr_model_dir> [dur_sec] [chunk_sec] "
                 "[unfixed_chunk_num] [unfixed_token_num]\n",
                 argv[0]);
    return 2;
  }
  const std::string audio_path = argv[1];
  const std::string model_dir = argv[2];
  const double dur_sec = argc > 3 ? std::atof(argv[3]) : 120.0;
  const double chunk_sec = argc > 4 ? std::atof(argv[4]) : 2.0;
  const int unfixed_chunk_num = argc > 5 ? std::atoi(argv[5]) : 2;
  const int unfixed_token_num = argc > 6 ? std::atoi(argv[6]) : 5;
  const int sr = 16000;

  std::printf(">> decoding %s\n", audio_path.c_str());
  io::AudioData a = io::LoadAudioMono(audio_path, sr);
  long total = std::min(static_cast<long>(dur_sec * sr),
                        static_cast<long>(a.samples.size()));
  std::printf("   %.1fs audio, chunk=%.1fs, unfixed_chunk=%d unfixed_token=%d\n",
              total / double(sr), chunk_sec, unfixed_chunk_num, unfixed_token_num);

  model::Qwen3Asr asr;
  core::AsrConfig cfg;
  cfg.sample_rate = sr;
  cfg.language = "Chinese";
  asr.Initialize(cfg);
  asr.set_language("Chinese");
  asr.LoadWeights(model_dir);
  std::printf(">> loaded; VmRSS=%ld MB\n", VmRssKb() / 1024);

  const int chunk_samples = static_cast<int>(chunk_sec * sr);
  std::string raw_decoded;  // committed decoded text
  int chunk_id = 0;
  const long rss0 = VmRssKb();
  auto wall0 = std::chrono::steady_clock::now();

  std::printf("\n  step  audio_s  step_ms  step_rt  accum_text_len  VmRSS_MB  dRSS_MB\n");
  for (long pos = 0; pos < total; pos += chunk_samples, ++chunk_id) {
    const long accum_n = std::min(total, pos + chunk_samples);

    // Build the committed prefix: empty for the first unfixed_chunk_num chunks,
    // else the decoded text with the last unfixed_token_num tokens rolled back.
    std::string prefix;
    if (chunk_id >= unfixed_chunk_num && !raw_decoded.empty()) {
      std::vector<int> ids = asr.tokenizer().Encode(raw_decoded);
      const int end = std::max(0, static_cast<int>(ids.size()) - unfixed_token_num);
      if (end > 0)
        prefix = asr.tokenizer().Decode(
            std::vector<int>(ids.begin(), ids.begin() + end), true);
    }

    auto s0 = std::chrono::steady_clock::now();
    std::string cont = asr.TranscribeWindow(a.samples.data(), static_cast<int>(accum_n), prefix);
    auto s1 = std::chrono::steady_clock::now();
    const double step_ms = std::chrono::duration<double, std::milli>(s1 - s0).count();

    raw_decoded = prefix + cont;
    const double audio_s = accum_n / double(sr);
    const double step_audio_s = chunk_samples / double(sr);
    const long rss = VmRssKb();
    std::printf("  %4d  %7.1f  %7.0f  %6.2fx  %14zu  %8ld  %+7.1f\n",
                chunk_id, audio_s, step_ms, step_ms > 0 ? step_audio_s * 1000.0 / step_ms : 0.0,
                raw_decoded.size(), rss / 1024, (rss - rss0) / 1024.0);
    std::fflush(stdout);
  }
  auto wall1 = std::chrono::steady_clock::now();
  const double wall = std::chrono::duration<double>(wall1 - wall0).count();
  const double audio = total / double(sr);
  std::printf("\n=== summary ===\n");
  std::printf("audio=%.1fs  total_wall=%.1fs  overall_rt=%.2fx\n", audio, wall,
              wall > 0 ? audio / wall : 0.0);
  std::printf("VmRSS start=%ld MB end=%ld MB growth=%+ld MB\n", rss0 / 1024,
              VmRssKb() / 1024, (VmRssKb() - rss0) / 1024);
  std::printf("\nfinal accumulated text:\n%s\n", raw_decoded.c_str());
  return 0;
}
