// Verifies the C++/CUDA Sortformer decoder (encoder_proj + 18 transformer
// layers + speaker head) against NeMo's encoder_proj/trans_l17/preds oracles.
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <vector>

#include <cuda_runtime.h>

#include "io/safetensor.h"
#include "model/sortformer_decoder.h"

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

static void Compare(const char* name, const std::vector<float>& a,
                    const std::vector<float>& b, int T, int D, int valid) {
  double max_abs = 0, sum_abs = 0;
  long count = 0;
  for (int t = 0; t < valid; ++t)
    for (int d = 0; d < D; ++d) {
      double diff = std::fabs(double(a[t * D + d]) - double(b[t * D + d]));
      max_abs = std::max(max_abs, diff);
      sum_abs += diff;
      ++count;
    }
  std::printf("%-14s max abs %.6g  mean %.6g\n", name, max_abs, sum_abs / count);
}

int main(int argc, char** argv) {
  std::string dir = argc > 1 ? argv[1] : "models";
  const int T = 126, valid = 125;
  auto conf = ReadF32(dir + "/ref_conf_l16_out.f32");  // [126,512] decoder input
  auto ref_preds = ReadF32(dir + "/ref_preds.f32");    // [126,4]

  io::SafeTensorReader reader(dir + "/sortformer_4spk_v2.safetensors");
  model::SortformerDecoder dec;
  dec.LoadWeights(reader);

  float* dconf;
  cudaMalloc(&dconf, conf.size() * sizeof(float));
  cudaMemcpy(dconf, conf.data(), conf.size() * sizeof(float), cudaMemcpyHostToDevice);

  auto preds = dec.Forward(dconf, T, valid);

  Compare("preds", preds, ref_preds, T, 4, valid);
  std::cout << "frame 50 preds: C++ [";
  for (int s = 0; s < 4; ++s) std::cout << preds[50 * 4 + s] << " ";
  std::cout << "] NeMo [";
  for (int s = 0; s < 4; ++s) std::cout << ref_preds[50 * 4 + s] << " ";
  std::cout << "]" << std::endl;

  double max_abs = 0;
  for (int t = 0; t < valid; ++t)
    for (int s = 0; s < 4; ++s)
      max_abs = std::max(max_abs, std::fabs(double(preds[t * 4 + s]) -
                                            double(ref_preds[t * 4 + s])));
  bool ok = max_abs < 1e-3;
  std::cout << (ok ? "DECODER/PREDS MATCH OK" : "DECODER/PREDS MISMATCH") << std::endl;
  cudaFree(dconf);
  return ok ? 0 : 1;
}
