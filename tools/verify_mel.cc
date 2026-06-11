// Verifies the C++/CUDA mel front-end against NeMo's AudioToMelSpectrogramPreprocessor.
// Loads the exact NeMo window + filterbank + the same input samples, runs the
// CUDA mel, and compares to NeMo's reference output frame-by-frame.
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <vector>

#include "feature/mel_spectrogram.h"

using namespace orator;

static std::vector<float> ReadF32(const std::string& path) {
  std::ifstream f(path, std::ios::binary | std::ios::ate);
  if (!f) throw std::runtime_error("cannot open " + path);
  std::streamsize n = f.tellg();
  f.seekg(0);
  std::vector<float> v(n / sizeof(float));
  f.read(reinterpret_cast<char*>(v.data()), n);
  return v;
}

int main(int argc, char** argv) {
  std::string dir = argc > 1 ? argv[1] : "models";
  auto wav = ReadF32(dir + "/ref_wav_10s.f32");
  auto window = ReadF32(dir + "/ref_window.f32");
  auto fb = ReadF32(dir + "/ref_fb.f32");
  auto ref = ReadF32(dir + "/ref_mel_valid.f32");  // [T,128] row-major
  std::cout << "wav=" << wav.size() << " window=" << window.size()
            << " fb=" << fb.size() << " ref=" << ref.size() << std::endl;

  feature::MelConfig cfg;  // defaults match NeMo (preemph 0.97, center, 2^-24)
  feature::MelSpectrogram mel(cfg, window, fb);

  int nframes = 0;
  auto out = mel.Compute(wav.data(), static_cast<int>(wav.size()), &nframes);
  const int n_mels = cfg.n_mels;
  std::cout << "C++ frames=" << nframes << " n_mels=" << n_mels << std::endl;

  const int T = static_cast<int>(ref.size()) / n_mels;
  const int cmpT = std::min(T, nframes);
  std::cout << "comparing " << cmpT << " frames" << std::endl;

  double max_abs = 0, sum_abs = 0;
  long count = 0;
  double max_rel = 0;
  for (int t = 0; t < cmpT; ++t) {
    for (int m = 0; m < n_mels; ++m) {
      float a = out[static_cast<size_t>(t) * n_mels + m];
      float b = ref[static_cast<size_t>(t) * n_mels + m];
      double d = std::fabs(double(a) - double(b));
      max_abs = std::max(max_abs, d);
      sum_abs += d;
      ++count;
      double denom = std::max(std::fabs(double(b)), 1e-3);
      max_rel = std::max(max_rel, d / denom);
    }
  }
  std::cout << "max abs diff:  " << max_abs << std::endl;
  std::cout << "mean abs diff: " << (sum_abs / count) << std::endl;
  std::cout << "max rel diff:  " << max_rel << std::endl;

  // Print a couple of sample values for sanity.
  std::cout << "sample frame 100: C++ [";
  for (int m = 0; m < 5; ++m)
    std::cout << out[100 * n_mels + m] << " ";
  std::cout << "]  NeMo [";
  for (int m = 0; m < 5; ++m)
    std::cout << ref[100 * n_mels + m] << " ";
  std::cout << "]" << std::endl;

  bool ok = max_abs < 1e-2;  // log-domain features; tol for fp32 DFT vs FFT
  std::cout << (ok ? "MEL MATCH OK" : "MEL MISMATCH") << std::endl;
  return ok ? 0 : 1;
}
