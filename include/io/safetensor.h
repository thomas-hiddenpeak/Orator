#pragma once

// SafeTensors reader/writer.
//
// Reader: memory-maps the file and exposes tensors as zero-copy views (the data
// pointer points directly into the mapped file - no per-tensor copy). This is
// the honest "zero-copy" path for weight loading on Jetson unified memory.
//
// Writer: serializes tensors to a valid .safetensors file, enabling round-trip
// tests and offline artifact generation without any Python tooling.

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/tensor.h"
#include "gpu/memory.h"

namespace orator {
namespace io {

struct SafeTensorMetadata {
  std::string name;
  std::vector<int64_t> shape;
  std::string dtype;       // "F32", "F16", "I32", ...
  int64_t data_offset;     // absolute byte offset within the file
  int64_t data_size;       // bytes
};

class SafeTensorReader {
 public:
  explicit SafeTensorReader(const std::string& filepath);

  const std::vector<std::string>& GetWeightNames() const { return weight_names_; }
  bool Has(const std::string& name) const;
  const SafeTensorMetadata& GetMetadata(const std::string& name) const;

  // Zero-copy CPU view into the mapped file. Valid for this reader's lifetime.
  core::Tensor GetTensorView(const std::string& name) const;

  // Copy tensor bytes into a caller-provided buffer.
  void ReadWeight(const std::string& name, void* output_buffer,
                  size_t buffer_size) const;

  size_t GetFileSize() const { return file_size_; }

 private:
  std::string filepath_;
  size_t file_size_;
  size_t header_size_;  // bytes of JSON header (excludes the 8-byte length)
  std::unique_ptr<gpu::MmapAllocator> mmap_;
  std::map<std::string, SafeTensorMetadata> metadata_;
  std::vector<std::string> weight_names_;

  void ParseHeader(const std::string& header_json);
  static core::DType ToDType(const std::string& s);
};

// Writer for producing .safetensors files.
class SafeTensorWriter {
 public:
  struct Entry {
    std::string name;
    std::string dtype;            // "F32", etc.
    std::vector<int64_t> shape;
    const void* data;             // contiguous source bytes
    size_t nbytes;
  };

  // Writes all entries to path. Returns false on I/O failure.
  static bool Write(const std::string& path, const std::vector<Entry>& entries);
};

}  // namespace io
}  // namespace orator
