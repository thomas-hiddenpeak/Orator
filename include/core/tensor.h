#pragma once

// Minimal device-aware tensor used at model boundaries (weights, activations).
//
// A Tensor is either:
//   * an owning unified-memory buffer (allocated via the GPU unified allocator),
//   * or a non-owning view into externally managed memory (e.g. mmap'd weights).
//
// This keeps the zero-copy weight-loading story honest: SafeTensor views point
// directly into the mapped file, while runtime activations own unified memory
// visible to both CPU and GPU on the Jetson unified architecture.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "gpu/memory.h"

namespace orator {
namespace core {

enum class DType { F32, F16, BF16, I32, I64, U8, Unknown };

enum class Device { CPU, GpuDevice, GpuUnified };

size_t DTypeSize(DType dtype);
const char* DTypeName(DType dtype);
DType DTypeFromString(const char* name);

class Tensor {
 public:
  Tensor() = default;

  // Owning tensor backed by unified memory (CPU+GPU coherent).
  static Tensor Unified(std::vector<int64_t> shape, DType dtype);

  // Non-owning view into externally owned memory.
  static Tensor View(void* data, std::vector<int64_t> shape, DType dtype,
                     Device device);

  const std::vector<int64_t>& shape() const { return shape_; }
  DType dtype() const { return dtype_; }
  Device device() const { return device_; }
  bool defined() const { return data_ != nullptr; }

  int64_t numel() const;
  size_t nbytes() const;

  void* data() { return data_; }
  const void* data() const { return data_; }

  template <typename T>
  T* data_as() {
    return static_cast<T*>(data_);
  }
  template <typename T>
  const T* data_as() const {
    return static_cast<const T*>(data_);
  }

 private:
  std::shared_ptr<gpu::UnifiedBuffer> storage_;  // owning backing (optional)
  void* data_ = nullptr;
  std::vector<int64_t> shape_;
  DType dtype_ = DType::F32;
  Device device_ = Device::CPU;
};

}  // namespace core
}  // namespace orator
