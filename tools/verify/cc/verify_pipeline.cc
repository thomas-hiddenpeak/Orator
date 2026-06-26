// Integration check: run the wired SortformerDiarizer end-to-end on 10s of real
// audio (using our own CUDA mel front-end) and compare the per-frame speaker
// probabilities to NeMo's reference preds (models/ref_preds.f32).
#include <cmath>
#include <fstream>
#include <iostream>
#include <vector>

#include "core/stages.h"
#include "model/streaming_sortformer.h"

using namespace orator;

static std::vector<float> ReadF32(const std::string& p) {
  std::ifstream f(p, std::ios::binary | std::ios::ate);
  if (!f) throw std::runtime_error("cannot open " + p);
  std::streamsize n = f.tellg();
  f.seekg(0);
  std::vector<float> v(n / sizeof(float));
  f.read(reinterpret_cast<char*>(v.data()), n);
  return v;
}

int main(int argc, char** argv) {
  std::string dir = argc > 1 ? argv[1] : "models/reference";
  auto wav = ReadF32(dir + "/ref_wav_10s.f32");
  auto ref = ReadF32(dir + "/ref_preds.f32");

  model::SortformerDiarizer diar;
  core::DiarizationConfig cfg;
  cfg.sample_rate = 16000;
  cfg.max_speakers = 4;
  diar.Initialize(cfg);
  diar.LoadWeights("models/sortformer_4spk_v2.safetensors");

  core::AudioChunk chunk;
  chunk.samples = wav.data();
  chunk.num_samples = static_cast<int>(wav.size());
  chunk.sample_rate = 16000;
  chunk.t_start_sec = 0.0;

  core::DiarizationFrames out = diar.ProcessChunk(chunk);
  std::cout << "frames=" << out.num_frames << " spk=" << out.num_speakers
            << " period=" << out.frame_period_sec << std::endl;

  int valid = std::min(out.num_frames, static_cast<int>(ref.size()) / 4);
  double max_abs = 0, sum_abs = 0;
  long count = 0;
  for (int t = 0; t < valid; ++t)
    for (int s = 0; s < 4; ++s) {
      double diff = std::fabs(double(out.At(t, s)) - double(ref[t * 4 + s]));
      max_abs = std::max(max_abs, diff);
      sum_abs += diff;
      ++count;
    }
  std::cout << "preds vs NeMo (our mel): max abs " << max_abs << " mean "
            << (sum_abs / count) << std::endl;
  for (int t : {0, 50, 100}) {
    std::cout << "  frame " << t << " ours [";
    for (int s = 0; s < 4; ++s) std::printf("%.4f ", out.At(t, s));
    std::cout << "] NeMo [";
    for (int s = 0; s < 4; ++s) std::printf("%.4f ", ref[t * 4 + s]);
    std::cout << "]\n";
  }
  // Our mel (minimp3-independent here; same librosa wav) differs from NeMo mel
  // by ~5e-3 in log-domain, so allow a small tolerance on the propagated preds.
  bool ok = max_abs < 2e-2;
  std::cout << (ok ? "PIPELINE OK" : "PIPELINE MISMATCH") << std::endl;
  return ok ? 0 : 1;
}
