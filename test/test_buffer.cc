#include <cassert>
#include <iostream>
#include <cstring>

#include "gpu/buffer.h"
#include "gpu/memory.h"

using namespace orator::gpu;

int main() {
  std::cout << "Testing GPU Audio Buffer..." << std::endl;

  // Create buffer for 1 second @ 16kHz
  const size_t buffer_capacity = 16000;
  AudioBuffer buffer(buffer_capacity);

  // Test 1: Simple write and read
  std::cout << "\nTest 1: Simple write and read" << std::endl;
  const size_t write_size = 1000;
  float* host_write_data = new float[write_size];
  for (size_t i = 0; i < write_size; ++i) {
    host_write_data[i] = static_cast<float>(i) * 0.1f;
  }

  size_t write_pos = buffer.Write(host_write_data, write_size);
  assert(write_pos == 0 && "First write should start at 0");
  std::cout << "✓ Wrote " << write_size << " samples at position " << write_pos
            << std::endl;

  float* host_read_data = new float[write_size];
  buffer.Read(write_pos, write_size, host_read_data);

  for (size_t i = 0; i < write_size; ++i) {
    assert(std::abs(host_read_data[i] - host_write_data[i]) < 1e-6 &&
           "Data mismatch");
  }
  std::cout << "✓ Read data verified" << std::endl;

  // Test 2: Multiple writes
  std::cout << "\nTest 2: Multiple writes" << std::endl;
  const size_t write_size2 = 2000;
  float* host_write_data2 = new float[write_size2];
  for (size_t i = 0; i < write_size2; ++i) {
    host_write_data2[i] = static_cast<float>(i) * 0.2f;
  }

  size_t write_pos2 = buffer.Write(host_write_data2, write_size2);
  assert(write_pos2 == write_size && "Second write should follow first");
  std::cout << "✓ Wrote " << write_size2 << " samples at position "
            << write_pos2 << std::endl;

  // Test 3: Ring buffer wrap-around
  std::cout << "\nTest 3: Ring buffer wrap-around" << std::endl;
  const size_t wrap_size = buffer_capacity - write_size - write_size2 + 500;
  float* host_wrap_data = new float[wrap_size];
  for (size_t i = 0; i < wrap_size; ++i) {
    host_wrap_data[i] = static_cast<float>(i) * 0.3f;
  }

  size_t write_pos3 = buffer.Write(host_wrap_data, wrap_size);
  std::cout << "✓ Wrap-around write at position " << write_pos3 << std::endl;

  float* host_wrap_read = new float[wrap_size];
  buffer.Read(write_pos3, wrap_size, host_wrap_read);

  for (size_t i = 0; i < wrap_size; ++i) {
    assert(std::abs(host_wrap_read[i] - host_wrap_data[i]) < 1e-6 &&
           "Wrap-around data mismatch");
  }
  std::cout << "✓ Wrap-around data verified" << std::endl;

  // Cleanup
  delete[] host_write_data;
  delete[] host_read_data;
  delete[] host_write_data2;
  delete[] host_wrap_data;
  delete[] host_wrap_read;

  std::cout << "\n✅ All buffer tests passed!" << std::endl;
  return 0;
}
