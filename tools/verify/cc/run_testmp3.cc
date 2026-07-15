// Full end-to-end run on test.mp3 using OUR CUDA mel + streaming Sortformer.
// Decodes the mp3 with the vendored minimp3 reader, runs the streaming
// diarizer over the whole file, converts frames to speaker segments, and
// prints a time-aligned table against the reference turns parsed from
// asrTest2Final.txt so the speaker assignment can be inspected by hand.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "core/types.h"
#include "io/audio_file.h"
#include "pipeline/diar_postprocess.h"
#include "model/streaming_sortformer.h"

using namespace orator;

struct RefTurn {
  double t_sec;
  std::string name;
};

// Parse "HH:MM:SS <name>" turn headers from asrTest2Final.txt.
static std::vector<RefTurn> ParseRef(const std::string& path) {
  std::vector<RefTurn> turns;
  std::ifstream f(path);
  std::string line;
  while (std::getline(f, line)) {
    if (line.size() < 9 || line[2] != ':' || line[5] != ':') continue;
    int hh = std::atoi(line.substr(0, 2).c_str());
    int mm = std::atoi(line.substr(3, 2).c_str());
    int ss = std::atoi(line.substr(6, 2).c_str());
    std::string name = line.substr(8);
    // trim
    size_t b = name.find_first_not_of(" \t\r\n");
    size_t e = name.find_last_not_of(" \t\r\n");
    if (b == std::string::npos) continue;
    name = name.substr(b, e - b + 1);
    turns.push_back({double(hh * 3600 + mm * 60 + ss), name});
  }
  return turns;
}

int main(int argc, char** argv) {
  std::string mp3 = argc > 1 ? argv[1] : "test.mp3";
  std::string weights =
      argc > 2 ? argv[2] : "models/sortformer_4spk_v2.1.safetensors";
  std::string ref_path = argc > 3 ? argv[3] : "asrTest2Final.txt";

  std::cout << ">> decoding " << mp3 << " ..." << std::endl;
  io::AudioData audio = io::LoadAudioMono(mp3, 16000);
  std::cout << "   samples=" << audio.samples.size() << " sr="
            << audio.sample_rate << " dur=" << audio.DurationSec() << "s"
            << std::endl;

  // Optional duration cap (seconds) for timing/throughput measurement.
  if (const char* lim = std::getenv("ORATOR_MAX_SECONDS")) {
    size_t cap = static_cast<size_t>(std::atof(lim) * 16000);
    if (cap > 0 && cap < audio.samples.size()) {
      audio.samples.resize(cap);
      std::cout << "   [capped to " << lim << "s -> " << audio.samples.size()
                << " samples]" << std::endl;
    }
  }

  model::SortformerDiarizer diar;
  core::DiarizationConfig cfg;
  cfg.sample_rate = 16000;
  cfg.max_speakers = 4;
  diar.Initialize(cfg);
  diar.LoadWeights(weights);

  core::AudioChunk chunk;
  chunk.samples = audio.samples.data();
  chunk.num_samples = static_cast<int>(audio.samples.size());
  chunk.sample_rate = 16000;
  chunk.t_start_sec = 0.0;

  std::cout << ">> running streaming diarizer over full file ..." << std::endl;
  auto t_c0 = std::chrono::steady_clock::now();
  core::DiarizationFrames frames = diar.ProcessChunk(chunk);
  auto t_c1 = std::chrono::steady_clock::now();
  double compute_s =
      std::chrono::duration<double>(t_c1 - t_c0).count();
  double audio_s = double(audio.samples.size()) / 16000.0;
  std::cout << "   diar frames=" << frames.num_frames << " spk="
            << frames.num_speakers << " period=" << frames.frame_period_sec
            << "s -> " << frames.num_frames * frames.frame_period_sec << "s"
            << std::endl;
  std::printf("   [compute %.2fs for %.2fs audio -> %.2fx real-time]\n",
              compute_s, audio_s, audio_s / compute_s);

  // Frames -> segments (0.5 threshold, merge gaps <= 0.5s).
  auto segs = pipeline::FramesToSegments(frames, 0.5f, 0.5);
  segs = pipeline::CoalesceSegments(std::move(segs), 0.5);
  std::cout << "   segments=" << segs.size() << std::endl;

  // Reference turns.
  auto turns = ParseRef(ref_path);
  std::cout << ">> reference turns parsed: " << turns.size() << std::endl;

  const int n_spk = frames.num_speakers;
  const double period = frames.frame_period_sec;

  // For each reference turn, compute our dominant speaker over [t, t_next).
  std::cout << "\n  time      ref_name   ours(dom)  mean_probs[spk0..3]\n";
  std::cout << "  --------------------------------------------------------\n";
  std::map<std::string, std::map<int, int>> name_to_spk;  // confusion
  for (size_t i = 0; i < turns.size(); ++i) {
    double t0 = turns[i].t_sec;
    double t1 = (i + 1 < turns.size()) ? turns[i + 1].t_sec : t0 + 4.0;
    int f0 = std::max(0, int(t0 / period));
    int f1 = std::min(frames.num_frames, int(t1 / period));
    if (f1 <= f0) f1 = std::min(frames.num_frames, f0 + 1);
    std::vector<double> mean(n_spk, 0.0);
    int cnt = 0;
    for (int f = f0; f < f1; ++f) {
      for (int s = 0; s < n_spk; ++s) mean[s] += frames.At(f, s);
      ++cnt;
    }
    int dom = 0;
    for (int s = 0; s < n_spk; ++s) {
      if (cnt) mean[s] /= cnt;
      if (mean[s] > mean[dom]) dom = s;
    }
    bool silent = mean[dom] < 0.5;
    int dom_lbl = silent ? -1 : dom;
    name_to_spk[turns[i].name][dom_lbl]++;
    int hh = int(t0) / 3600, mm = (int(t0) % 3600) / 60, ss = int(t0) % 60;
    std::printf("  %02d:%02d:%02d  %-9s  %s     [%.2f %.2f %.2f %.2f]\n", hh, mm,
                ss, turns[i].name.c_str(),
                silent ? "(sil)" : ("spk" + std::to_string(dom)).c_str(),
                mean[0], mean[1], mean[2], mean[3]);
  }

  // Confusion summary: which spk index each ref name maps to.
  std::cout << "\n  === ref-name -> our-speaker confusion ===\n";
  for (auto& kv : name_to_spk) {
    std::cout << "  " << kv.first << " :";
    for (auto& sk : kv.second)
      std::cout << " spk" << sk.first << "=" << sk.second;
    std::cout << "\n";
  }
  return 0;
}
