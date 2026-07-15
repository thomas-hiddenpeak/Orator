#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "core/tensor.h"
#include "io/safetensor.h"

using namespace orator;

int main() {
  std::cout << "Testing safetensors reader (zero-copy mmap)..." << std::endl;

  const std::string sortformer_path = std::string(ORATOR_TEST_SOURCE_DIR) +
                                      "/models/sortformer_4spk_v2.1.safetensors";

  io::SafeTensorReader reader(sortformer_path);
  std::cout << "Loaded sortformer (" << reader.GetWeightNames().size()
            << " weights)" << std::endl;
  assert(reader.GetWeightNames().size() > 0);

  const std::string& name = reader.GetWeightNames()[0];
  const auto& meta = reader.GetMetadata(name);
  std::cout << "  First weight: " << name << " dtype=" << meta.dtype
            << " shape=[";
  for (size_t i = 0; i < meta.shape.size(); ++i) {
    if (i > 0) std::cout << ",";
    std::cout << meta.shape[i];
  }
  std::cout << "] size=" << meta.data_size << " bytes" << std::endl;

  core::Tensor t = reader.GetTensorView(name);
  assert(t.numel() > 0);
  std::cout << "  Zero-copy view OK (numel=" << t.numel() << ")" << std::endl;

  std::vector<uint8_t> buf(static_cast<size_t>(meta.data_size));
  reader.ReadWeight(name, buf.data(), buf.size());
  const uint8_t* view_ptr = static_cast<const uint8_t*>(t.data());
  for (size_t i = 0; i < buf.size(); ++i) {
    assert(buf[i] == view_ptr[i]);
  }
  std::cout << "  ReadWeight copy path matches zero-copy view" << std::endl;

  std::cout << "\nAll safetensors tests passed!" << std::endl;
  return 0;
}
