// Verifies the full 17-layer Conformer stack against NeMo's conformer_l16.
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <vector>

#include <cuda_runtime.h>

#include "io/safetensor.h"
#include "model/conformer_layer.h"

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
  const int T = 126, D = 512, valid = 125, n_layers = 17;
  auto xin = ReadF32(dir + "/ref_conf_l0_in.f32");
  auto ref = ReadF32(dir + "/ref_conf_l16_out.f32");

  io::SafeTensorReader reader("models/sortformer_4spk_v2.1.safetensors");
  float* dx;
  cudaMalloc(&dx, xin.size() * sizeof(float));
  cudaMemcpy(dx, xin.data(), xin.size() * sizeof(float), cudaMemcpyHostToDevice);
  auto pe = model::ConformerLayer::BuildPosEmb(T, D);
  float* dpe;
  cudaMalloc(&dpe, pe.size() * sizeof(float));
  cudaMemcpy(dpe, pe.data(), pe.size() * sizeof(float), cudaMemcpyHostToDevice);

  for (int l = 0; l < n_layers; ++l) {
    model::ConformerLayer layer;
    layer.LoadWeights(reader, "encoder.layers." + std::to_string(l));
    layer.Forward(dx, T, valid, dpe);
  }

  std::vector<float> out(xin.size());
  cudaMemcpy(out.data(), dx, out.size() * sizeof(float), cudaMemcpyDeviceToHost);

  double max_abs = 0, sum_abs = 0;
  long count = 0;
  for (int t = 0; t < valid; ++t)
    for (int d = 0; d < D; ++d) {
      double diff = std::fabs(double(out[t * D + d]) - double(ref[t * D + d]));
      max_abs = std::max(max_abs, diff);
      sum_abs += diff;
      ++count;
    }
  std::cout << "17 layers, valid frames " << valid << std::endl;
  std::cout << "max abs diff: " << max_abs << std::endl;
  std::cout << "mean abs diff: " << (sum_abs / count) << std::endl;
  bool ok = max_abs < 5e-2;
  std::cout << (ok ? "FULL CONFORMER MATCH OK" : "FULL CONFORMER MISMATCH") << std::endl;
  cudaFree(dx);
  cudaFree(dpe);
  return ok ? 0 : 1;
}
