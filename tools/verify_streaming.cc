// Verify the C++ streaming forward (SortformerDiarizer::RunStreaming) against
// NeMo's forward_streaming oracle. Feeds NeMo's own processed_signal (mel) so
// the comparison isolates the streaming logic + encoder/decoder (already
// verified) from any mel front-end differences.
#include <cmath>
#include <cstdint>
#include <cstdio>
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
  auto proc = ReadF32(dir + "/ref_stream_proc.f32");    // [128, t_mel]
  auto ref = ReadF32(dir + "/ref_stream_total.f32");    // [diar, 4]
  std::ifstream mf(dir + "/ref_stream_meta.i32", std::ios::binary);
  int32_t meta[3];
  mf.read(reinterpret_cast<char*>(meta), sizeof(meta));
  const int t_mel = meta[0], valid_mel = meta[1], ref_frames = meta[2];
  const int n_mels = 128;
  std::cout << "t_mel=" << t_mel << " valid_mel=" << valid_mel
            << " ref_frames=" << ref_frames << std::endl;

  model::SortformerDiarizer diar;
  core::DiarizationConfig cfg;
  cfg.sample_rate = 16000;
  cfg.max_speakers = 4;
  diar.Initialize(cfg);
  diar.LoadWeights("models/sortformer_4spk_v2.safetensors");

  core::DiarizationFrames out =
      diar.RunStreaming(proc.data(), n_mels, t_mel, valid_mel, 0.0);
  std::cout << "ours frames=" << out.num_frames << " spk=" << out.num_speakers
            << std::endl;

  int valid = std::min(out.num_frames, ref_frames);
  double max_abs = 0, sum_abs = 0;
  long count = 0;
  int argmax_match = 0, argmax_total = 0;
  for (int t = 0; t < valid; ++t) {
    for (int s = 0; s < 4; ++s) {
      double diff = std::fabs(double(out.At(t, s)) - double(ref[t * 4 + s]));
      max_abs = std::max(max_abs, diff);
      sum_abs += diff;
      ++count;
    }
  }
  std::cout << "streaming total_preds vs NeMo: max abs " << max_abs << " mean "
            << (sum_abs / count) << std::endl;
  for (int t : {0, 100, 200, 300, 400, 500}) {
    if (t >= valid) break;
    std::printf("  frame %3d ours [%.4f %.4f %.4f %.4f] ref [%.4f %.4f %.4f %.4f]\n",
                t, out.At(t, 0), out.At(t, 1), out.At(t, 2), out.At(t, 3),
                ref[t * 4 + 0], ref[t * 4 + 1], ref[t * 4 + 2], ref[t * 4 + 3]);
  }
  std::cout << (max_abs < 1e-2 ? "STREAMING MATCH OK" : "STREAMING MISMATCH")
            << std::endl;
  return 0;
}
