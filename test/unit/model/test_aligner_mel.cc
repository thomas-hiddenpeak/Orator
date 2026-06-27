// Diagnostic: compare the WhisperMel front-end against the aligner's
// Qwen3ASRFeatureExtractor mel (oracle dump) on the same PCM. The end-to-end
// aligner depends on this matching; a mismatch can flip a boundary timestamp.

#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "feature/whisper_mel.h"

using namespace orator;

static std::vector<float> ReadF32(const std::string& p, bool* ok) {
  std::ifstream f(p, std::ios::binary | std::ios::ate);
  if (!f) {
    *ok = false;
    return {};
  }
  std::streamsize n = f.tellg();
  f.seekg(0);
  std::vector<float> v(n / sizeof(float));
  f.read(reinterpret_cast<char*>(v.data()), n);
  *ok = true;
  return v;
}

int main() {
  const std::string dump = "tools/reference/aligner_dump/";
  bool a, b;
  auto audio = ReadF32(dump + "audio.f32", &a);
  auto ref = ReadF32(dump + "mel.f32", &b);  // [128, 600]
  if (!(a && b)) {
    std::printf("[skip] need oracle dump (aligner_oracle.py)\n");
    return 0;
  }
  const int n_mels = 128;
  const int ref_frames = static_cast<int>(ref.size()) / n_mels;

  feature::WhisperMel mel;
  int n_frames = 0;
  auto out =
      mel.Compute(audio.data(), static_cast<int>(audio.size()), &n_frames);
  std::printf("  ref frames=%d  ours frames=%d\n", ref_frames, n_frames);

  const int F = std::min(n_frames, ref_frames);
  double max_abs = 0, sum_abs = 0;
  for (int m = 0; m < n_mels; ++m)
    for (int t = 0; t < F; ++t) {
      const double e = std::abs(static_cast<double>(out[m * n_frames + t]) -
                                ref[m * ref_frames + t]);
      max_abs = e > max_abs ? e : max_abs;
      sum_abs += e;
    }
  std::printf("  mel[:, :%d] max abs=%.3e mean abs=%.3e\n", F, max_abs,
              sum_abs / (n_mels * F));
  if (n_frames != ref_frames || max_abs > 5e-3) {
    std::printf(
        "FAIL: WhisperMel diverges from the aligner feature extractor\n");
    return 1;
  }
  std::printf("Aligner mel test PASSED\n");
  return 0;
}
