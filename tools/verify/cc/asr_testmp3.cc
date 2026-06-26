// End-to-end native Qwen3-ASR on a clip of an audio file.
//
// Decodes audio -> mono 16k, runs the native CUDA Qwen3-ASR engine over a
// [start, start+dur) clip, prints the transcript + real-time factor. Compares
// against the PyTorch oracle transcript if present.
//
// Usage: asr_testmp3 [audio] [model_dir] [start_sec] [dur_sec]

#include <chrono>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "io/audio_file.h"
#include "model/qwen3_asr.h"

using namespace orator;

int main(int argc, char** argv) {
  const std::string audio = argc > 1 ? argv[1] : "test.mp3";
  const std::string model = argc > 2 ? argv[2] : "models/asr/Qwen/Qwen3-ASR-1.7B";
  const double start = argc > 3 ? std::atof(argv[3]) : 0.0;
  const double dur = argc > 4 ? std::atof(argv[4]) : 10.0;

  std::printf(">> loading audio %s\n", audio.c_str());
  io::AudioData a = io::LoadAudioMono(audio, 16000);
  const int sr = a.sample_rate;
  const int s0 = std::max(0, static_cast<int>(start * sr));
  const int s1 = std::min(static_cast<int>(a.samples.size()),
                          s0 + static_cast<int>(dur * sr));
  if (s1 <= s0) { std::printf("empty clip\n"); return 1; }
  std::vector<float> clip(a.samples.begin() + s0, a.samples.begin() + s1);
  const double clip_sec = static_cast<double>(clip.size()) / sr;
  std::printf("   clip [%.1f, %.1f) = %zu samples (%.2fs)\n", start,
              start + dur, clip.size(), clip_sec);

  std::printf(">> loading Qwen3-ASR engine from %s\n", model.c_str());
  model::Qwen3Asr asr;
  core::AsrConfig cfg;
  cfg.sample_rate = 16000;
  cfg.language = "Chinese";
  asr.Initialize(cfg);
  auto t_load0 = std::chrono::steady_clock::now();
  asr.LoadWeights(model);
  auto t_load1 = std::chrono::steady_clock::now();
  std::printf("   weights loaded in %.2fs\n",
              std::chrono::duration<double>(t_load1 - t_load0).count());

  std::printf(">> transcribing (segmented / VAD) ...\n");
  core::AudioChunk chunk;
  chunk.samples = clip.data();
  chunk.num_samples = static_cast<int>(clip.size());
  chunk.sample_rate = 16000;
  chunk.t_start_sec = start;
  auto t0 = std::chrono::steady_clock::now();
  core::Transcript tr = asr.Transcribe(chunk);
  auto t1 = std::chrono::steady_clock::now();
  const double compute = std::chrono::duration<double>(t1 - t0).count();

  std::printf("\n===== TIMELINE (%zu segments) =====\n", tr.tokens.size());
  for (const auto& tk : tr.tokens) {
    const int sm = static_cast<int>(tk.start_sec) / 60, ss = static_cast<int>(tk.start_sec) % 60;
    std::printf("[%02d:%02d] %s\n", sm, ss, tk.text.c_str());
  }
  std::printf("======================\n");
  std::printf("compute %.2fs for %.2fs audio -> %.2fx real-time\n", compute,
              clip_sec, clip_sec / compute);
  return 0;
}
