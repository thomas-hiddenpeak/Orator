// Verifies the C++/CUDA Conformer pre-encode (dw_striding x8) against NeMo.
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <vector>

#include "io/safetensor.h"
#include "model/conformer_preencode.h"

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
  std::string dir = argc > 1 ? argv[1] : "models/reference";
  std::string weights = "models/sortformer_4spk_v2.safetensors";

  const int n_mels = 128, n_frames = 1008, valid_len = 1000;
  auto mel = ReadF32(dir + "/ref_processed_signal.f32");  // [128,1008]
  auto ref = ReadF32(dir + "/ref_pre_encode.f32");        // [126,512]
  std::cout << "mel=" << mel.size() << " ref=" << ref.size() << std::endl;

  io::SafeTensorReader reader(weights);
  model::ConformerPreEncode pe;
  pe.LoadWeights(reader);

  int out_frames = 0, out_valid = 0;
  auto out = pe.Forward(mel.data(), n_mels, n_frames, valid_len, &out_frames,
                        &out_valid);
  const int D = 512;
  std::cout << "C++ out_frames=" << out_frames << " valid=" << out_valid
            << " (NeMo: 126 frames, valid 125)" << std::endl;

  const int T = static_cast<int>(ref.size()) / D;
  const int cmpT = std::min(T, out_frames);
  double max_abs = 0, sum_abs = 0;
  long count = 0;
  // Compare only the valid frames (NeMo masks frame >= valid identically; the
  // masked frame equals the linear bias in both).
  int cmp_valid = std::min(cmpT, out_valid);
  for (int t = 0; t < cmpT; ++t) {
    for (int d = 0; d < D; ++d) {
      double a = out[static_cast<size_t>(t) * D + d];
      double b = ref[static_cast<size_t>(t) * D + d];
      double diff = std::fabs(a - b);
      max_abs = std::max(max_abs, diff);
      sum_abs += diff;
      ++count;
    }
  }
  std::cout << "max abs diff (all " << cmpT << " frames):  " << max_abs << std::endl;
  std::cout << "mean abs diff: " << (sum_abs / count) << std::endl;
  std::cout << "valid frames compared: " << cmp_valid << std::endl;

  // Per-frame max diff to see if error concentrates at the masked boundary.
  for (int t = 0; t < cmpT; ++t) {
    double fmax = 0;
    for (int d = 0; d < D; ++d)
      fmax = std::max(fmax, std::fabs(double(out[t * D + d]) - double(ref[t * D + d])));
    if (t < 3 || t >= cmpT - 4 || fmax > 0.03)
      std::cout << "  frame " << t << " max diff " << fmax << std::endl;
  }

  std::cout << "frame 50: C++ [";
  for (int d = 0; d < 5; ++d) std::cout << out[50 * D + d] << " ";
  std::cout << "] NeMo [";
  for (int d = 0; d < 5; ++d) std::cout << ref[50 * D + d] << " ";
  std::cout << "]" << std::endl;

  bool ok = max_abs < 1e-2;
  std::cout << (ok ? "PRE_ENCODE MATCH OK" : "PRE_ENCODE MISMATCH") << std::endl;
  return ok ? 0 : 1;
}
