#include <cuda_runtime.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "gpu/memory.h"
#include "model/gemm.cuh"

namespace {

bool RunShape(int rows, int inner, int columns, int batch) {
  const size_t input_count =
      static_cast<size_t>(batch) * rows * inner;
  const size_t weight_count =
      static_cast<size_t>(batch) * columns * inner;
  const size_t output_count =
      static_cast<size_t>(batch) * rows * columns;
  orator::gpu::UnifiedBuffer input(input_count * sizeof(float));
  orator::gpu::UnifiedBuffer weight(weight_count * sizeof(float));
  orator::gpu::UnifiedBuffer output(output_count * sizeof(float));
  auto* input_values = static_cast<float*>(input.data());
  auto* weight_values = static_cast<float*>(weight.data());
  auto* output_values = static_cast<float*>(output.data());
  for (size_t index = 0; index < input_count; ++index) {
    input_values[index] = static_cast<float>(index % 17) / 17.0f;
  }
  for (size_t index = 0; index < weight_count; ++index) {
    weight_values[index] = static_cast<float>(index % 13) / 13.0f;
  }
  std::fill_n(output_values, output_count, 0.0f);

  orator::gemm::LaunchSgemmBatched(
      input_values, weight_values, output_values, rows, inner, columns,
      /*act=*/0, batch, static_cast<long>(rows) * inner,
      static_cast<long>(columns) * inner,
      static_cast<long>(rows) * columns);
  orator::gpu::CUDA_CHECK(cudaDeviceSynchronize());

  double max_abs = 0.0;
  for (int b = 0; b < batch; ++b) {
    for (int row = 0; row < rows; ++row) {
      for (int column = 0; column < columns; ++column) {
        double expected = 0.0;
        for (int k = 0; k < inner; ++k) {
          expected +=
              input_values[(static_cast<size_t>(b) * rows + row) * inner + k] *
              weight_values[
                  (static_cast<size_t>(b) * columns + column) * inner + k];
        }
        const double actual = output_values[
            (static_cast<size_t>(b) * rows + row) * columns + column];
        max_abs = std::max(max_abs, std::fabs(actual - expected));
      }
    }
  }
  std::printf(
      "batched SGEMM rows=%d inner=%d columns=%d batch=%d max_abs=%.6g\n",
      rows, inner, columns, batch, max_abs);
  return max_abs < 1e-4;
}

}  // namespace

int main() {
  try {
    if (!RunShape(13, 64, 13, 8) || !RunShape(14, 64, 14, 8)) {
      std::printf("test_gemm_batched FAILED\n");
      return 1;
    }
    std::printf("test_gemm_batched PASSED\n");
    return 0;
  } catch (const std::exception& error) {
    std::printf("FAIL: %s\n", error.what());
    return 1;
  }
}
