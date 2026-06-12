#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "feature/whisper_mel.h"

using namespace orator;

static std::vector<float> ReadF32(const std::string& p, bool* ok) {
  std::ifstream f(p, std::ios::binary | std::ios::ate);
  if (!f) { *ok = false; return {}; }
  std::streamsize n = f.tellg();
  f.seekg(0);
  std::vector<float> v(n / sizeof(float));
  f.read(reinterpret_cast<char*>(v.data()), n);
  *ok = true;
  return v;
}

int main() {
  std::printf("Testing Whisper mel front-end vs PyTorch input_features...\n");
  const std::string dir = "models/reference/asr/";
  bool a, b;
  auto wav = ReadF32(dir + "wav.f32", &a);
  auto ref = ReadF32(dir + "input_features.f32", &b);  // [1,128,T] row-major
  if (!(a && b)) {
    std::printf("[skip] reference not found; run tools/asr_oracle.py --dump\n");
    return 0;
  }

  const int n_mels = 128;
  const int ref_frames = static_cast<int>(ref.size()) / n_mels;
  std::printf("  wav=%zu samples, ref mel=[%d, %d]\n", wav.size(), n_mels, ref_frames);

  feature::WhisperMel mel{};
  int frames = 0;
  auto out = mel.Compute(wav.data(), static_cast<int>(wav.size()), &frames);
  std::printf("  ours mel=[%d, %d]\n", n_mels, frames);
  if (frames != ref_frames) {
    std::printf("FAIL: frame count mismatch (%d vs %d)\n", frames, ref_frames);
    return 1;
  }

  // Both are [n_mels, frames] row-major. Compare elementwise.
  double max_abs = 0.0, sum_abs = 0.0;
  const size_t n = static_cast<size_t>(n_mels) * frames;
  for (size_t i = 0; i < n; ++i) {
    const double e = std::abs(static_cast<double>(out[i]) - ref[i]);
    max_abs = e > max_abs ? e : max_abs;
    sum_abs += e;
  }
  std::printf("  max abs err = %.3e, mean abs err = %.3e\n", max_abs, sum_abs / n);

  // The reference itself notes ~1e-5 tolerance vs Whisper's torch path; our
  // direct-DFT power spectrum + Slaney filterbank should be well within 1e-2.
  if (max_abs > 1e-2) {
    std::printf("FAIL: mel exceeds tolerance\n");
    return 1;
  }
  std::printf("Whisper mel test PASSED\n");
  return 0;
}
