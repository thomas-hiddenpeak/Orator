// End-to-end C++/CUDA Sortformer forward: processed_signal (mel) -> pre_encode
// -> xscale -> 17 Conformer layers -> decoder (encoder_proj + 18 transformer +
// speaker head) -> preds. Compares the final per-frame speaker probabilities to
// NeMo's reference (models/ref_preds.f32).
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <vector>

#include <cuda_runtime.h>

#include "io/safetensor.h"
#include "model/conformer_layer.h"
#include "model/conformer_preencode.h"
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

int main(int argc, char** argv) {
  std::string dir = argc > 1 ? argv[1] : "models/reference";
  std::string weights = "models/sortformer_4spk_v2.safetensors";
  const int n_mels = 128, n_frames = 1008, in_valid = 1000, D = 512;

  auto mel = ReadF32(dir + "/ref_processed_signal.f32");  // [128,1008]
  auto ref_preds = ReadF32(dir + "/ref_preds.f32");        // [126,4]

  io::SafeTensorReader reader(weights);

  // Stage 1: pre_encode.
  model::ConformerPreEncode pre;
  pre.LoadWeights(reader);
  int T = 0, valid = 0;
  auto pe = pre.Forward(mel.data(), n_mels, n_frames, in_valid, &T, &valid);
  std::cout << "pre_encode -> T=" << T << " valid=" << valid << std::endl;

  // xscale, move to device.
  const float xscale = std::sqrt(static_cast<float>(D));
  std::vector<float> xin(pe.size());
  for (size_t i = 0; i < pe.size(); ++i) xin[i] = pe[i] * xscale;
  float* dx;
  cudaMalloc(&dx, xin.size() * sizeof(float));
  cudaMemcpy(dx, xin.data(), xin.size() * sizeof(float), cudaMemcpyHostToDevice);

  // Stage 2: 17 Conformer layers.
  auto posemb = model::ConformerLayer::BuildPosEmb(T, D);
  float* dpe;
  cudaMalloc(&dpe, posemb.size() * sizeof(float));
  cudaMemcpy(dpe, posemb.data(), posemb.size() * sizeof(float), cudaMemcpyHostToDevice);
  for (int l = 0; l < 17; ++l) {
    model::ConformerLayer layer;
    layer.LoadWeights(reader, "encoder.layers." + std::to_string(l));
    layer.Forward(dx, T, valid, dpe);
  }

  // Stage 3: decoder -> preds.
  model::SortformerDecoder dec;
  dec.LoadWeights(reader);
  auto preds = dec.Forward(dx, T, valid);

  // Compare to oracle.
  double max_abs = 0, sum_abs = 0;
  long count = 0;
  for (int t = 0; t < valid; ++t)
    for (int s = 0; s < 4; ++s) {
      double diff = std::fabs(double(preds[t * 4 + s]) - double(ref_preds[t * 4 + s]));
      max_abs = std::max(max_abs, diff);
      sum_abs += diff;
      ++count;
    }
  std::cout << "END-TO-END preds vs NeMo:  max abs " << max_abs << "  mean "
            << (sum_abs / count) << std::endl;
  for (int t : {0, 50, 100, 124}) {
    std::cout << "  frame " << t << " C++ [";
    for (int s = 0; s < 4; ++s) std::printf("%.4f ", preds[t * 4 + s]);
    std::cout << "] NeMo [";
    for (int s = 0; s < 4; ++s) std::printf("%.4f ", ref_preds[t * 4 + s]);
    std::cout << "]" << std::endl;
  }
  bool ok = max_abs < 5e-3;
  std::cout << (ok ? "FULL FORWARD MATCH OK" : "FULL FORWARD MISMATCH") << std::endl;
  cudaFree(dx);
  cudaFree(dpe);
  return ok ? 0 : 1;
}
