#include "core/tensor.h"

#include <cstring>
#include <stdexcept>

namespace orator {
namespace core {

size_t DTypeSize(DType dtype) {
  switch (dtype) {
    case DType::F32:
    case DType::I32:
      return 4;
    case DType::F16:
    case DType::BF16:
      return 2;
    case DType::I64:
      return 8;
    case DType::U8:
      return 1;
    default:
      return 0;
  }
}

const char* DTypeName(DType dtype) {
  switch (dtype) {
    case DType::F32:
      return "F32";
    case DType::F16:
      return "F16";
    case DType::BF16:
      return "BF16";
    case DType::I32:
      return "I32";
    case DType::I64:
      return "I64";
    case DType::U8:
      return "U8";
    default:
      return "UNKNOWN";
  }
}

DType DTypeFromString(const char* name) {
  if (std::strcmp(name, "F32") == 0) return DType::F32;
  if (std::strcmp(name, "F16") == 0) return DType::F16;
  if (std::strcmp(name, "BF16") == 0) return DType::BF16;
  if (std::strcmp(name, "I32") == 0) return DType::I32;
  if (std::strcmp(name, "I64") == 0) return DType::I64;
  if (std::strcmp(name, "U8") == 0) return DType::U8;
  return DType::Unknown;
}

static int64_t ProductOfShape(const std::vector<int64_t>& shape) {
  int64_t n = 1;
  for (int64_t d : shape) n *= d;
  return shape.empty() ? 0 : n;
}

Tensor Tensor::Unified(std::vector<int64_t> shape, DType dtype) {
  Tensor t;
  t.shape_ = std::move(shape);
  t.dtype_ = dtype;
  t.device_ = Device::GpuUnified;
  const size_t bytes =
      static_cast<size_t>(ProductOfShape(t.shape_)) * DTypeSize(dtype);
  t.storage_ = std::make_shared<gpu::UnifiedBuffer>(bytes);
  t.data_ = t.storage_->data();
  return t;
}

Tensor Tensor::View(void* data, std::vector<int64_t> shape, DType dtype,
                    Device device) {
  Tensor t;
  t.shape_ = std::move(shape);
  t.dtype_ = dtype;
  t.device_ = device;
  t.data_ = data;
  return t;
}

int64_t Tensor::numel() const { return ProductOfShape(shape_); }

size_t Tensor::nbytes() const {
  return static_cast<size_t>(numel()) * DTypeSize(dtype_);
}

}  // namespace core
}  // namespace orator
