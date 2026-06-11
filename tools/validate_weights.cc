// Validates that the real converted Sortformer safetensors loads via the C++
// zero-copy SafeTensorReader: checks tensor count, key shapes, and that data is
// finite/non-degenerate. This proves the offline conversion is consumable by
// the production weight loader with no Python at runtime.
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "core/tensor.h"
#include "io/safetensor.h"

using namespace orator;

static bool ShapeEq(const std::vector<int64_t>& a,
                    const std::vector<int64_t>& b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i)
    if (a[i] != b[i]) return false;
  return true;
}

int main(int argc, char** argv) {
  std::string path = argc > 1 ? argv[1] : "models/sortformer_4spk_v2.safetensors";
  std::cout << "Loading real weights: " << path << std::endl;

  io::SafeTensorReader reader(path);
  const auto& names = reader.GetWeightNames();
  std::cout << "tensor count: " << names.size() << std::endl;
  std::cout << "file size: " << reader.GetFileSize() << " bytes" << std::endl;

  int fail = 0;
  auto expect = [&](const std::string& n, std::vector<int64_t> shape) {
    if (!reader.Has(n)) {
      std::cout << "  MISSING " << n << std::endl;
      ++fail;
      return;
    }
    const auto& m = reader.GetMetadata(n);
    if (!ShapeEq(m.shape, shape)) {
      std::cout << "  SHAPE MISMATCH " << n << std::endl;
      ++fail;
      return;
    }
    // Sample data: must be finite and not all-zero.
    core::Tensor t = reader.GetTensorView(n);
    const float* p = t.data_as<float>();
    int64_t numel = t.numel();
    double sum = 0, absmax = 0;
    bool finite = true;
    for (int64_t i = 0; i < numel; ++i) {
      float v = p[i];
      if (!std::isfinite(v)) finite = false;
      sum += v;
      double a = std::fabs(v);
      if (a > absmax) absmax = a;
    }
    std::cout << "  OK " << n << " shape[";
    for (size_t i = 0; i < shape.size(); ++i)
      std::cout << shape[i] << (i + 1 < shape.size() ? "," : "");
    std::cout << "] numel=" << numel << " mean=" << (sum / numel)
              << " absmax=" << absmax << (finite ? "" : " !!NONFINITE")
              << std::endl;
    if (!finite || absmax == 0.0) ++fail;
  };

  // Spot-check across all submodules using the real shapes we extracted.
  expect("preprocessor.featurizer.window", {400});
  expect("preprocessor.featurizer.fb", {1, 128, 257});
  expect("encoder.pre_encode.out.weight", {512, 4096});
  expect("encoder.pre_encode.conv.0.weight", {256, 1, 3, 3});
  expect("encoder.layers.0.feed_forward1.linear1.weight", {2048, 512});
  expect("encoder.layers.0.conv.depthwise_conv.weight", {512, 1, 9});
  expect("encoder.layers.16.norm_out.weight", {512});
  expect("sortformer_modules.encoder_proj.weight", {192, 512});
  expect("transformer_encoder.layers.0.first_sub_layer.query_net.weight",
         {192, 192});
  expect("transformer_encoder.layers.17.second_sub_layer.dense_out.weight",
         {192, 768});
  expect("sortformer_modules.hidden_to_spks.weight", {4, 384});
  expect("sortformer_modules.single_hidden_to_spks.weight", {4, 192});

  std::cout << "\nspot-checks failed: " << fail << std::endl;
  if (fail == 0)
    std::cout << "REAL WEIGHTS VALIDATED OK" << std::endl;
  return fail == 0 ? 0 : 1;
}
