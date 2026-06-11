// Verifies one C++/CUDA Conformer layer against NeMo's conformer_l0 oracle.
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
  std::string dir = argc > 1 ? argv[1] : "models";
  const int T = 126, D = 512, valid = 125;
  auto xin = ReadF32(dir + "/ref_conf_l0_in.f32");    // [126,512]
  auto ref = ReadF32(dir + "/ref_conf_l0_out.f32");   // [126,512]

  io::SafeTensorReader reader(dir + "/sortformer_4spk_v2.safetensors");
  model::ConformerLayer layer;
  layer.LoadWeights(reader, "encoder.layers.0");

  // Device buffers.
  float* dx;
  cudaMalloc(&dx, xin.size() * sizeof(float));
  cudaMemcpy(dx, xin.data(), xin.size() * sizeof(float), cudaMemcpyHostToDevice);

  auto pe = model::ConformerLayer::BuildPosEmb(T, D);
  float* dpe;
  cudaMalloc(&dpe, pe.size() * sizeof(float));
  cudaMemcpy(dpe, pe.data(), pe.size() * sizeof(float), cudaMemcpyHostToDevice);

  layer.Forward(dx, T, valid, dpe);

  std::vector<float> out(xin.size());
  cudaMemcpy(out.data(), dx, out.size() * sizeof(float), cudaMemcpyDeviceToHost);

  double max_abs = 0, sum_abs = 0;
  long count = 0;
  for (int t = 0; t < valid; ++t)
    for (int d = 0; d < D; ++d) {
      double a = out[t * D + d], b = ref[t * D + d];
      double diff = std::fabs(a - b);
      max_abs = std::max(max_abs, diff);
      sum_abs += diff;
      ++count;
    }
  std::cout << "valid frames " << valid << std::endl;
  std::cout << "max abs diff: " << max_abs << std::endl;
  std::cout << "mean abs diff: " << (sum_abs / count) << std::endl;
  std::cout << "frame 50: C++ [";
  for (int d = 0; d < 5; ++d) std::cout << out[50 * D + d] << " ";
  std::cout << "] NeMo [";
  for (int d = 0; d < 5; ++d) std::cout << ref[50 * D + d] << " ";
  std::cout << "]" << std::endl;

  bool ok = max_abs < 5e-2;
  std::cout << (ok ? "CONFORMER LAYER MATCH OK" : "CONFORMER LAYER MISMATCH") << std::endl;
  cudaFree(dx);
  cudaFree(dpe);
  return ok ? 0 : 1;
}
