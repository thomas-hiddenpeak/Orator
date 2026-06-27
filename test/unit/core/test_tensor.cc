#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <vector>

#include "core/tensor.h"
#include "gpu/memory.h"

using namespace orator;
using namespace orator::core;

static int fails = 0;

#define CHECK(cond, msg)                                                    \
  do {                                                                      \
    if (!(cond)) {                                                          \
      std::fprintf(stderr, "  FAIL [%s:%d] %s\n", __FILE__, __LINE__, msg); \
      ++fails;                                                              \
    } else {                                                                \
      std::printf("  OK %s\n", msg);                                        \
    }                                                                       \
  } while (0)

int main() {
  std::printf("=== Tensor unit tests ===\n\n");

  // ── DType helper functions ──────────────────────────────────────────
  std::printf("-- DType helpers --\n");
  CHECK(DTypeSize(DType::F32) == 4, "DTypeSize(F32) == 4");
  CHECK(DTypeSize(DType::F16) == 2, "DTypeSize(F16) == 2");
  CHECK(DTypeSize(DType::BF16) == 2, "DTypeSize(BF16) == 2");
  CHECK(DTypeSize(DType::I32) == 4, "DTypeSize(I32) == 4");
  CHECK(DTypeSize(DType::I64) == 8, "DTypeSize(I64) == 8");
  CHECK(DTypeSize(DType::U8) == 1, "DTypeSize(U8) == 1");
  CHECK(DTypeSize(DType::Unknown) == 0, "DTypeSize(Unknown) == 0");

  CHECK(std::strcmp(DTypeName(DType::F32), "F32") == 0,
        "DTypeName(F32) == F32");
  CHECK(std::strcmp(DTypeName(DType::Unknown), "UNKNOWN") == 0,
        "DTypeName(Unknown) == UNKNOWN");

  CHECK(DTypeFromString("F32") == DType::F32, "DTypeFromString(F32)");
  CHECK(DTypeFromString("I64") == DType::I64, "DTypeFromString(I64)");
  CHECK(DTypeFromString("bogus") == DType::Unknown,
        "DTypeFromString(bogus) == Unknown");

  // ── Default tensor ──────────────────────────────────────────────────
  std::printf("\n-- Default tensor --\n");
  {
    Tensor t;
    CHECK(!t.defined(), "default tensor is not defined");
    CHECK(t.shape().empty(), "default tensor has empty shape");
    CHECK(t.numel() == 0, "default tensor numel == 0");
    CHECK(t.nbytes() == 0, "default tensor nbytes == 0");
    CHECK(t.data() == nullptr, "default tensor data == nullptr");
  }

  // ── View tensor: 1-D ────────────────────────────────────────────────
  std::printf("\n-- View tensor 1-D --\n");
  {
    float data[8] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    Tensor t = Tensor::View(data, {8}, DType::F32, Device::CPU);

    CHECK(t.defined(), "1-D View tensor is defined");
    CHECK(t.shape().size() == 1, "1-D View shape rank == 1");
    CHECK(t.shape()[0] == 8, "1-D View shape[0] == 8");
    CHECK(t.dtype() == DType::F32, "1-D View dtype == F32");
    CHECK(t.device() == Device::CPU, "1-D View device == CPU");
    CHECK(t.numel() == 8, "1-D View numel == 8");
    CHECK(t.nbytes() == 8 * 4, "1-D View nbytes == 32");

    // Read via data_as
    const float* read = t.data_as<float>();
    CHECK(read != nullptr, "1-D View data_as<float> non-null");
    CHECK(read[0] == 1.0f, "1-D View read[0] == 1.0");
    CHECK(read[7] == 8.0f, "1-D View read[7] == 8.0");

    // Write via data_as
    float* write = t.data_as<float>();
    write[3] = 99.0f;
    CHECK(data[3] == 99.0f, "1-D View write propagates to backing buffer");
    data[3] = 4.0f;  // restore
  }

  // ── View tensor: 2-D ────────────────────────────────────────────────
  std::printf("\n-- View tensor 2-D --\n");
  {
    int32_t data[6] = {10, 20, 30, 40, 50, 60};
    Tensor t = Tensor::View(data, {2, 3}, DType::I32, Device::CPU);

    CHECK(t.shape().size() == 2, "2-D View shape rank == 2");
    CHECK(t.shape()[0] == 2, "2-D View shape[0] == 2");
    CHECK(t.shape()[1] == 3, "2-D View shape[1] == 3");
    CHECK(t.numel() == 6, "2-D View numel == 6");
    CHECK(t.nbytes() == 6 * 4, "2-D View nbytes == 24");
    CHECK(t.dtype() == DType::I32, "2-D View dtype == I32");

    const int32_t* read = t.data_as<int32_t>();
    CHECK(read[0] == 10, "2-D View read[0] == 10");
    CHECK(read[5] == 60, "2-D View read[5] == 60");
  }

  // ── View tensor: 0-D (scalar) ───────────────────────────────────────
  std::printf("\n-- View tensor 0-D (scalar) --\n");
  {
    double val = 3.14;
    // 0-D shape: empty vector
    Tensor t = Tensor::View(&val, {}, DType::I64, Device::CPU);
    // numel of empty shape = 0 (ProductOfShape returns 0 for empty)
    CHECK(t.numel() == 0, "0-D View numel == 0");
    CHECK(t.nbytes() == 0, "0-D View nbytes == 0");
    CHECK(t.shape().empty(), "0-D View shape is empty");
  }

  // ── Move semantics ──────────────────────────────────────────────────
  std::printf("\n-- Move semantics --\n");
  {
    float data[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    Tensor a = Tensor::View(data, {4}, DType::F32, Device::CPU);

    // Move construction
    Tensor b(std::move(a));
    CHECK(b.defined(), "move-constructed tensor is defined");
    CHECK(b.numel() == 4, "move-constructed tensor numel == 4");
    CHECK(b.data_as<float>()[0] == 1.0f, "move-constructed tensor data intact");
    // a should be in a valid but unspecified state (shape_ moved, data_ still
    // points)
    CHECK(a.shape().empty(), "moved-from tensor has empty shape");

    // Move assignment
    float data2[2] = {100.0f, 200.0f};
    Tensor c = Tensor::View(data2, {2}, DType::F32, Device::CPU);
    c = std::move(b);
    CHECK(c.defined(), "move-assigned tensor is defined");
    CHECK(c.numel() == 4, "move-assigned tensor numel == 4");
    CHECK(c.data_as<float>()[0] == 1.0f, "move-assigned tensor data intact");
    CHECK(b.shape().empty(), "moved-from (assigned) tensor has empty shape");
  }

  // ── Copy semantics ──────────────────────────────────────────────────
  std::printf("\n-- Copy semantics --\n");
  {
    float data[3] = {10.0f, 20.0f, 30.0f};
    Tensor a = Tensor::View(data, {3}, DType::F32, Device::CPU);

    // Copy construction
    Tensor b(a);
    CHECK(b.defined(), "copy-constructed tensor is defined");
    CHECK(b.numel() == 3, "copy-constructed tensor numel == 3");
    CHECK(b.shape()[0] == 3, "copy-constructed tensor shape[0] == 3");
    CHECK(b.data() == a.data(), "copy-constructed tensor shares same data ptr");

    // Copy assignment
    Tensor c;
    c = a;
    CHECK(c.defined(), "copy-assigned tensor is defined");
    CHECK(c.numel() == 3, "copy-assigned tensor numel == 3");
    CHECK(c.data() == a.data(), "copy-assigned tensor shares same data ptr");
  }

  // ── Unified tensor (GPU required) ───────────────────────────────────
  std::printf("\n-- Unified tensor --\n");
  {
    int dev_count = gpu::GpuMemory::GetDeviceCount();
    if (dev_count == 0) {
      std::printf("  [skip] no CUDA device\n");
    } else {
      Tensor t = Tensor::Unified({4, 8}, DType::F32);
      CHECK(t.defined(), "Unified tensor is defined");
      CHECK(t.shape().size() == 2, "Unified tensor shape rank == 2");
      CHECK(t.shape()[0] == 4, "Unified tensor shape[0] == 4");
      CHECK(t.shape()[1] == 8, "Unified tensor shape[1] == 8");
      CHECK(t.numel() == 32, "Unified tensor numel == 32");
      CHECK(t.nbytes() == 32 * 4, "Unified tensor nbytes == 128");
      CHECK(t.dtype() == DType::F32, "Unified tensor dtype == F32");
      CHECK(t.device() == Device::GpuUnified,
            "Unified tensor device == GpuUnified");
      CHECK(t.data() != nullptr, "Unified tensor data non-null");

      // Write and read back via unified memory
      float* ptr = t.data_as<float>();
      for (int i = 0; i < 32; ++i) ptr[i] = static_cast<float>(i);
      CHECK(ptr[0] == 0.0f, "Unified tensor write/read [0] == 0");
      CHECK(ptr[31] == 31.0f, "Unified tensor write/read [31] == 31");
    }
  }

  // ── Summary ─────────────────────────────────────────────────────────
  std::printf("\n");
  if (fails) {
    std::printf("FAIL: %d test(s) failed\n", fails);
    return 1;
  }
  std::printf("All tensor tests PASSED\n");
  return 0;
}
