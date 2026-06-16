// test_diar_stream: a numeric gate for the streaming diarization engine
// (Spec 002 precondition). It runs SortformerDiarizer::RunStreaming on NeMo's
// own processed mel (models/reference/ref_stream_proc.f32) and asserts the
// per-frame speaker probabilities match NeMo's streaming oracle
// (ref_stream_total.f32) within the recorded tolerance.
//
// This is the re-validatable reference required before the diarization engine
// may be modified (e.g. routed onto a per-pipeline CUDA stream for Spec 002):
// any change that perturbs the diarizer's numerics fails this gate. It mirrors
// the historical `tools/verify_streaming.cc` probe but returns a PASS/FAIL exit
// code so it runs as a ctest. Run from the repository root (WORKING_DIRECTORY)
// so the relative model/reference paths resolve.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
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

int main() {
  const std::string dir = "models/reference";
  std::vector<float> proc, ref;
  int32_t meta[3] = {0, 0, 0};
  try {
    proc = ReadF32(dir + "/ref_stream_proc.f32");   // [128, t_mel]
    ref = ReadF32(dir + "/ref_stream_total.f32");    // [diar, 4]
    std::ifstream mf(dir + "/ref_stream_meta.i32", std::ios::binary);
    if (!mf) throw std::runtime_error("cannot open ref_stream_meta.i32");
    mf.read(reinterpret_cast<char*>(meta), sizeof(meta));
  } catch (const std::exception& e) {
    std::printf("FAIL: cannot read diarization reference: %s\n", e.what());
    return 1;
  }

  const int t_mel = meta[0], valid_mel = meta[1], ref_frames = meta[2];
  const int n_mels = 128;

  model::SortformerDiarizer diar;
  core::DiarizationConfig cfg;
  cfg.sample_rate = 16000;
  cfg.max_speakers = 4;
  try {
    diar.Initialize(cfg);
    diar.LoadWeights("models/sortformer_4spk_v2.safetensors");
  } catch (const std::exception& e) {
    std::printf("FAIL: cannot load diarizer weights: %s\n", e.what());
    return 1;
  }

  core::DiarizationFrames out =
      diar.RunStreaming(proc.data(), n_mels, t_mel, valid_mel, 0.0);

  const int valid = std::min(out.num_frames, ref_frames);
  if (valid <= 0) {
    std::printf("FAIL: no comparable frames (ours=%d ref=%d)\n", out.num_frames,
                ref_frames);
    return 1;
  }

  double max_abs = 0.0, sum_abs = 0.0;
  long count = 0;
  for (int t = 0; t < valid; ++t) {
    for (int s = 0; s < 4; ++s) {
      const double diff =
          std::fabs(double(out.At(t, s)) - double(ref[t * 4 + s]));
      max_abs = std::max(max_abs, diff);
      sum_abs += diff;
      ++count;
    }
  }
  const double mean_abs = count > 0 ? sum_abs / count : 0.0;

  // Tolerance of record: the streaming diarizer matches the NeMo oracle within
  // 1e-2 (the mel front-end propagates ~5e-3 in the log domain; the same bound
  // used by the historical verify_streaming probe).
  const double kTol = 1e-2;
  std::printf("diar streaming vs NeMo: frames=%d max_abs=%.6g mean_abs=%.6g "
              "(tol %.0e)\n",
              valid, max_abs, mean_abs, kTol);
  if (max_abs < kTol) {
    std::printf("test_diar_stream PASSED\n");
    return 0;
  }
  std::printf("test_diar_stream FAILED (max_abs %.6g >= tol %.0e)\n", max_abs,
              kTol);
  return 1;
}
