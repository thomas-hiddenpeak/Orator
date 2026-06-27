#include <cassert>
#include <iostream>

#include "gpu/memory.h"

using namespace orator::gpu;

int main() {
  std::cout << "Testing GPU Memory Management..." << std::endl;

  // Test device availability
  int device_count = GpuMemory::GetDeviceCount();
  assert(device_count > 0 && "No CUDA devices found");
  std::cout << "✓ Found " << device_count << " CUDA device(s)" << std::endl;

  // Test allocation and deallocation
  void* gpu_ptr = GpuMemory::device_allocator().allocate(1024 * 1024);  // 1MB
  assert(gpu_ptr != nullptr && "GPU allocation failed");
  std::cout << "✓ Allocated 1MB on GPU" << std::endl;

  GpuMemory::device_allocator().deallocate(gpu_ptr);
  std::cout << "✓ Freed GPU memory" << std::endl;

  // Test host-device transfer
  const size_t size = 1024;
  float* host_data = new float[size];
  for (size_t i = 0; i < size; ++i) {
    host_data[i] = static_cast<float>(i);
  }

  void* device_data =
      GpuMemory::device_allocator().allocate(size * sizeof(float));
  GpuMemory::CopyHostToDevice(device_data, host_data, size * sizeof(float));
  std::cout << "✓ Copied host data to device" << std::endl;

  float* host_result = new float[size];
  GpuMemory::CopyDeviceToHost(host_result, device_data, size * sizeof(float));
  std::cout << "✓ Copied device data back to host" << std::endl;

  // Verify data integrity
  for (size_t i = 0; i < size; ++i) {
    assert(host_data[i] == host_result[i] && "Data mismatch after transfer");
  }
  std::cout << "✓ Data integrity verified" << std::endl;

  GpuMemory::device_allocator().deallocate(device_data);
  delete[] host_data;
  delete[] host_result;

  std::cout << "\n✅ All memory tests passed!" << std::endl;
  return 0;
}
