// Incremental-streaming continuity verification.
//
// Feeds test.mp3 through the diarizer two ways and asserts the per-frame
// sigmoids are bit-identical (the real-time StreamAudio path must match the
// offline ProcessChunk over the same audio):
//   (A) offline: one ProcessChunk over the whole signal.
//   (B) streaming: many small StreamAudio() increments + a final flush, with
//       persistent state (no Reset between increments).
//
// Usage: verify_streaming_incremental [test.mp3] [weights] [max_seconds] [chunk_ms]

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/stages.h"
#include "io/audio_file.h"
#include "model/streaming_sortformer.h"

using namespace orator;

int main(int argc, char** argv) {
  std::string mp3 = argc > 1 ? argv[1] : "test.mp3";
  std::string weights =
      argc > 2 ? argv[2] : "models/sortformer_4spk_v2.safetensors";
  double max_sec = argc > 3 ? std::atof(argv[3]) : 180.0;
  int chunk_ms = argc > 4 ? std::atoi(argv[4]) : 320;

  std::printf(">> decoding %s ...\n", mp3.c_str());
  auto audio = io::LoadAudioMono(mp3, 16000);
  int total = static_cast<int>(audio.samples.size());
  if (max_sec > 0) total = std::min(total, int(max_sec * 16000));
  std::printf("   using %d samples (%.2fs)\n", total, total / 16000.0);

  model::SortformerDiarizer diar;
  core::DiarizationConfig cfg;
  cfg.sample_rate = 16000;
  cfg.max_speakers = 4;
  diar.Initialize(cfg);
  diar.LoadWeights(weights);

  // (A) offline reference.
  core::AudioChunk chunk;
  chunk.samples = audio.samples.data();
  chunk.num_samples = total;
  chunk.sample_rate = 16000;
  chunk.t_start_sec = 0.0;
  std::printf(">> offline ProcessChunk ...\n");
  auto a0 = std::chrono::steady_clock::now();
  core::DiarizationFrames off = diar.ProcessChunk(chunk);
  auto a1 = std::chrono::steady_clock::now();
  double off_s = std::chrono::duration<double>(a1 - a0).count();
  std::printf("   offline frames=%d (compute %.2fs)\n", off.num_frames, off_s);

  // (B) incremental streaming.
  diar.Reset();
  const int step = std::max(1, chunk_ms * 16);  // samples per increment
  std::vector<float> stream_probs;
  int stream_frames = 0;
  std::printf(">> streaming StreamAudio in %d-sample (%dms) increments ...\n",
              step, chunk_ms);
  auto b0 = std::chrono::steady_clock::now();
  for (int off_i = 0; off_i < total; off_i += step) {
    int n = std::min(step, total - off_i);
    bool final = (off_i + n >= total);
    core::DiarizationFrames part =
        diar.StreamAudio(audio.samples.data() + off_i, n, final);
    if (part.num_frames > 0) {
      stream_probs.insert(stream_probs.end(), part.probs.begin(),
                          part.probs.end());
      stream_frames += part.num_frames;
    }
  }
  auto b1 = std::chrono::steady_clock::now();
  double str_s = std::chrono::duration<double>(b1 - b0).count();
  std::printf("   streaming frames=%d (compute %.2fs)\n", stream_frames, str_s);

  // Compare.
  const int n_spk = off.num_speakers;
  if (stream_frames != off.num_frames) {
    std::printf("FRAME COUNT MISMATCH: offline=%d streaming=%d\n",
                off.num_frames, stream_frames);
    return 1;
  }
  double max_abs = 0.0, mean_abs = 0.0;
  long n = static_cast<long>(off.num_frames) * n_spk;
  for (long i = 0; i < n; ++i) {
    double d = std::fabs(double(stream_probs[i]) - double(off.probs[i]));
    max_abs = std::max(max_abs, d);
    mean_abs += d;
  }
  mean_abs /= (n > 0 ? n : 1);
  std::printf("\n  STREAMING vs OFFLINE:  max abs %.3e  mean %.3e  (frames=%d)\n",
              max_abs, mean_abs, off.num_frames);
  for (int f = 0; f < off.num_frames; f += std::max(1, off.num_frames / 5)) {
    std::printf("  frame %4d  stream [", f);
    for (int s = 0; s < n_spk; ++s)
      std::printf("%.4f ", stream_probs[size_t(f) * n_spk + s]);
    std::printf("] offline [");
    for (int s = 0; s < n_spk; ++s)
      std::printf("%.4f ", off.probs[size_t(f) * n_spk + s]);
    std::printf("]\n");
  }
  if (max_abs < 1e-4) {
    std::printf("\nINCREMENTAL STREAMING MATCH OK\n");
    return 0;
  }
  std::printf("\nINCREMENTAL STREAMING MISMATCH (max abs %.3e)\n", max_abs);
  return 1;
}
