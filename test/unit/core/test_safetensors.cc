#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

#include "core/tensor.h"
#include "io/safetensor.h"

using namespace orator;

namespace {

std::string GetWorkspaceRoot() {
  constexpr char kProcSelfExe[] = "/proc/self/exe";
  char exe[4096];
  ssize_t len = readlink(kProcSelfExe, exe, sizeof(exe) - 1);
  if (len < 0) return ".";
  exe[len] = '\0';

  std::string dir(exe);
  size_t pos = dir.rfind('/');
  if (pos == std::string::npos) return ".";
  dir = dir.substr(0, pos);

  for (int i = 0; i < 2; ++i) {
    pos = dir.rfind('/');
    if (pos == std::string::npos) {
      dir = ".";
      break;
    }
    dir = dir.substr(0, pos);
  }
  if (dir.empty()) dir = ".";
  return dir;
}

}  // namespace

int main() {
  std::cout << "Testing safetensors reader (zero-copy mmap)..." << std::endl;

  std::string root = GetWorkspaceRoot();
  std::string sortformer_path = root + "/models/sortformer_4spk_v2.safetensors";

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
