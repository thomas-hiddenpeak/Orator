#include <cassert>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <vector>

#include "core/tensor.h"
#include "io/safetensor.h"

using namespace orator;

int main() {
  std::cout << "Testing safetensors writer + zero-copy reader..." << std::endl;

  const char* path = "/tmp/orator_test.safetensors";

  // Two tensors of different dtypes; offsets are non-trivial so the old
  // "+8 but forgot header_len" bug would be caught here.
  std::vector<float> w0(12);
  for (int i = 0; i < 12; ++i) w0[i] = static_cast<float>(i) * 0.5f - 2.0f;
  std::vector<int32_t> w1(5);
  for (int i = 0; i < 5; ++i) w1[i] = i * 100 - 50;

  std::vector<io::SafeTensorWriter::Entry> entries = {
      {"encoder.weight", "F32", {3, 4}, w0.data(), w0.size() * sizeof(float)},
      {"encoder.bias", "I32", {5}, w1.data(), w1.size() * sizeof(int32_t)},
  };
  assert(io::SafeTensorWriter::Write(path, entries));
  std::cout << "Wrote safetensors file" << std::endl;

  io::SafeTensorReader reader(path);
  assert(reader.Has("encoder.weight"));
  assert(reader.Has("encoder.bias"));
  assert(reader.GetWeightNames().size() == 2);

  // Metadata.
  const auto& m0 = reader.GetMetadata("encoder.weight");
  assert(m0.dtype == "F32");
  assert(m0.shape.size() == 2 && m0.shape[0] == 3 && m0.shape[1] == 4);
  assert(m0.data_size == static_cast<int64_t>(w0.size() * sizeof(float)));
  std::cout << "Metadata parsed correctly" << std::endl;

  // Zero-copy view: bytes must match exactly (validates correct offsets).
  core::Tensor t0 = reader.GetTensorView("encoder.weight");
  assert(t0.numel() == 12);
  const float* tp = t0.data_as<float>();
  for (int i = 0; i < 12; ++i) {
    assert(tp[i] == w0[i]);
  }
  std::cout << "F32 zero-copy view matches exactly" << std::endl;

  core::Tensor t1 = reader.GetTensorView("encoder.bias");
  const int32_t* ip = t1.data_as<int32_t>();
  for (int i = 0; i < 5; ++i) {
    assert(ip[i] == w1[i]);
  }
  std::cout << "I32 zero-copy view matches exactly" << std::endl;

  // ReadWeight copy path consistency.
  std::vector<float> copy(12);
  reader.ReadWeight("encoder.weight", copy.data(), copy.size() * sizeof(float));
  for (int i = 0; i < 12; ++i) assert(copy[i] == w0[i]);
  std::cout << "ReadWeight copy path matches" << std::endl;

  std::remove(path);
  std::cout << "\nAll safetensors tests passed!" << std::endl;
  return 0;
}
