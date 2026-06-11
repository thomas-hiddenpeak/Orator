#include "gpu/memory.h"

#include <cuda_runtime.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sstream>
#include <cstring>

namespace orator {
namespace gpu {

void CheckCudaError(int result, const char* file, int line) {
  if (result != cudaSuccess) {
    std::ostringstream oss;
    oss << "CUDA Error at " << file << ":" << line << " - "
        << cudaGetErrorString(static_cast<cudaError_t>(result));
    throw GpuMemoryException(oss.str().c_str());
  }
}

// DeviceAllocator implementation
void* DeviceAllocator::allocate(size_t bytes) {
  void* ptr = nullptr;
  CUDA_CHECK(cudaMalloc(&ptr, bytes));
  return ptr;
}

void DeviceAllocator::deallocate(void* ptr) {
  if (ptr) {
    CUDA_CHECK(cudaFree(ptr));
  }
}

// UnifiedAllocator implementation
void* UnifiedAllocator::allocate(size_t bytes) {
  void* ptr = nullptr;
  CUDA_CHECK(cudaMallocManaged(&ptr, bytes, cudaMemAttachGlobal));
  return ptr;
}

void UnifiedAllocator::deallocate(void* ptr) {
  if (ptr) {
    CUDA_CHECK(cudaFree(ptr));
  }
}

// HostAllocator implementation
void* HostAllocator::allocate(size_t bytes) {
  return std::malloc(bytes);
}

void HostAllocator::deallocate(void* ptr) {
  if (ptr) {
    std::free(ptr);
  }
}

// MmapAllocator implementation
MmapAllocator::MmapAllocator(const std::string& filepath)
    : fd_(-1), mapped_ptr_(nullptr), mapped_size_(0) {
  fd_ = open(filepath.c_str(), O_RDONLY);
  if (fd_ < 0) {
    throw GpuMemoryException(std::string("Cannot open file: " + filepath).c_str());
  }

  // Get file size
  off_t file_size = lseek(fd_, 0, SEEK_END);
  if (file_size < 0) {
    close(fd_);
    throw GpuMemoryException("Cannot seek in file");
  }

  mapped_size_ = static_cast<size_t>(file_size);

  // Map file to memory
  mapped_ptr_ = mmap(nullptr, mapped_size_, PROT_READ, MAP_SHARED, fd_, 0);
  if (mapped_ptr_ == MAP_FAILED) {
    close(fd_);
    mapped_ptr_ = nullptr;
    throw GpuMemoryException("mmap failed");
  }
}

MmapAllocator::~MmapAllocator() {
  if (mapped_ptr_ != nullptr && mapped_ptr_ != MAP_FAILED) {
    munmap(mapped_ptr_, mapped_size_);
  }
  if (fd_ >= 0) {
    close(fd_);
  }
}

void MmapAllocator::deallocate(void* ptr) {
  // Mmap memory is managed by destructor
  (void)ptr;
}

// GpuMemory static methods
static DeviceAllocator g_device_allocator;
static UnifiedAllocator g_unified_allocator;
static HostAllocator g_host_allocator;

int GpuMemory::GetDeviceCount() {
  int count = 0;
  CUDA_CHECK(cudaGetDeviceCount(&count));
  return count;
}

void GpuMemory::SetDevice(int device_id) {
  CUDA_CHECK(cudaSetDevice(device_id));
}

DeviceAllocator& GpuMemory::device_allocator() {
  return g_device_allocator;
}

UnifiedAllocator& GpuMemory::unified_allocator() {
  return g_unified_allocator;
}

HostAllocator& GpuMemory::host_allocator() {
  return g_host_allocator;
}

void GpuMemory::CopyDeviceToDevice(void* dst, const void* src, size_t bytes) {
  CUDA_CHECK(cudaMemcpy(dst, src, bytes, cudaMemcpyDeviceToDevice));
}

void GpuMemory::CopyHostToDevice(void* dst, const void* src, size_t bytes) {
  CUDA_CHECK(cudaMemcpy(dst, src, bytes, cudaMemcpyHostToDevice));
}

void GpuMemory::CopyDeviceToHost(void* dst, const void* src, size_t bytes) {
  CUDA_CHECK(cudaMemcpy(dst, src, bytes, cudaMemcpyDeviceToHost));
}

void GpuMemory::CopyUnified(void* dst, const void* src, size_t bytes) {
  // For unified memory, memcpy is sufficient; CUDA handles coherency
  std::memcpy(dst, src, bytes);
}

// Template specializations for default allocators
template <>
DeviceAllocator& GpuBuffer<DeviceAllocator>::get_default_allocator() {
  return GpuMemory::device_allocator();
}

template <>
UnifiedAllocator& GpuBuffer<UnifiedAllocator>::get_default_allocator() {
  return GpuMemory::unified_allocator();
}

template <>
HostAllocator& GpuBuffer<HostAllocator>::get_default_allocator() {
  return GpuMemory::host_allocator();
}

}  // namespace gpu
}  // namespace orator

