#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>

namespace orator {
namespace gpu {

class GpuMemoryException : public std::runtime_error {
 public:
  explicit GpuMemoryException(const char* message)
      : std::runtime_error(message) {}
};

// Check CUDA operations and throw on error
void CheckCudaError(int result, const char* file, int line);

#define CUDA_CHECK(result) CheckCudaError((result), __FILE__, __LINE__)

// Allocator interface
class Allocator {
 public:
  virtual ~Allocator() = default;
  virtual void* allocate(size_t bytes) = 0;
  virtual void deallocate(void* ptr) = 0;
};

// Device allocator: cudaMalloc (GPU-only memory)
// Preferred for KV cache, intermediate activations, scratch buffers
class DeviceAllocator : public Allocator {
 public:
  void* allocate(size_t bytes) override;
  void deallocate(void* ptr) override;
};

// Unified allocator: cudaMallocManaged (CPU/GPU shared memory)
// Used for data structures accessible from both CPU and GPU
class UnifiedAllocator : public Allocator {
 public:
  void* allocate(size_t bytes) override;
  void deallocate(void* ptr) override;
};

// Pinned allocator: cudaHostAlloc (page-locked host memory, DMA-accessible by
// the device). The host can read/write it freely regardless of what GPU kernels
// are in flight (no managed-memory migration hazard). Use for small scalars
// that are written by device kernels on one stream and read by the host after
// cudaStreamSynchronize, while another stream may still be executing.
class PinnedAllocator : public Allocator {
 public:
  void* allocate(size_t bytes) override;
  void deallocate(void* ptr) override;
};

// Host allocator: malloc (CPU-only memory)
class HostAllocator : public Allocator {
 public:
  void* allocate(size_t bytes) override;
  void deallocate(void* ptr) override;
};

// Mmap allocator: memory-mapped file (for zero-copy weight loading)
class MmapAllocator : public Allocator {
 public:
  explicit MmapAllocator(const std::string& filepath);
  ~MmapAllocator();

  // Mmap doesn't support arbitrary allocation - memory is the mapped file.
  void* allocate(size_t /*bytes*/) override {
    throw std::runtime_error("MmapAllocator: use get_mapped_ptr()");
  }
  void deallocate(void* ptr) override;

  // Get mapped pointer for file
  void* get_mapped_ptr() const { return mapped_ptr_; }
  size_t get_mapped_size() const { return mapped_size_; }

 private:
  int fd_;
  void* mapped_ptr_;
  size_t mapped_size_;
};

// GPU memory utilities
class GpuMemory {
 public:
  // Check device availability
  static int GetDeviceCount();
  static void SetDevice(int device_id);

  // Get global allocators
  static DeviceAllocator& device_allocator();
  static UnifiedAllocator& unified_allocator();
  static HostAllocator& host_allocator();

  // Direct memory operations
  static void CopyDeviceToDevice(void* dst, const void* src, size_t bytes);
  static void CopyHostToDevice(void* dst, const void* src, size_t bytes);
  static void CopyDeviceToHost(void* dst, const void* src, size_t bytes);
  static void CopyUnified(void* dst, const void* src, size_t bytes);
};

// RAII wrappers for different memory types
template <typename AllocatorT>
class GpuBuffer {
 public:
  explicit GpuBuffer(size_t bytes, AllocatorT& allocator = get_default_allocator())
      : size_(bytes), ptr_(nullptr), allocator_(allocator) {
    if (bytes > 0) {
      ptr_ = allocator_.allocate(bytes);
    }
  }

  ~GpuBuffer() {
    if (ptr_) {
      allocator_.deallocate(ptr_);
    }
  }

  // Move semantics
  GpuBuffer(GpuBuffer&& other) noexcept
      : size_(other.size_), ptr_(other.ptr_), allocator_(other.allocator_) {
    other.ptr_ = nullptr;
    other.size_ = 0;
  }

  GpuBuffer& operator=(GpuBuffer&& other) noexcept {
    if (this != &other) {
      if (ptr_) {
        allocator_.deallocate(ptr_);
      }
      size_ = other.size_;
      ptr_ = other.ptr_;
      allocator_ = other.allocator_;
      other.ptr_ = nullptr;
      other.size_ = 0;
    }
    return *this;
  }

  // Prevent copy
  GpuBuffer(const GpuBuffer&) = delete;
  GpuBuffer& operator=(const GpuBuffer&) = delete;

  void* data() { return ptr_; }
  const void* data() const { return ptr_; }
  size_t size() const { return size_; }

 private:
  size_t size_;
  void* ptr_;
  AllocatorT& allocator_;

  static AllocatorT& get_default_allocator();
};

// Specializations for common buffer types
using DeviceBuffer = GpuBuffer<DeviceAllocator>;
using UnifiedBuffer = GpuBuffer<UnifiedAllocator>;
using HostBuffer = GpuBuffer<HostAllocator>;
using PinnedBuffer = GpuBuffer<PinnedAllocator>;

}  // namespace gpu
}  // namespace orator
