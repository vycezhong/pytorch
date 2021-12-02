#pragma once

#include <c10/cuda/CUDAStream.h>
#include <c10/core/Allocator.h>

namespace at {
namespace cuda {

//
// A caching allocator for CUDA host allocations (pinned memory).
//
// This provides a drop-in replacement for THCudaHostAllocator, which re-uses
// freed pinned (page-locked) memory allocations. This avoids device
// synchronizations due to cudaFreeHost calls.
//
// To ensure correct behavior, THCCachingHostAllocator_recordEvent must be
// called anytime a pointer from this allocator is used in a cudaMemcpyAsync
// call between host and device. We implement this for storages and tensors in
// copy_from_cpu_async_ and copy_to_cpu_async_.
//
// Note that this allocator does not split larger allocations into smaller
// blocks, unlike the caching device allocator.
//
TORCH_CUDA_CPP_API c10::Allocator* getCachingHostAllocator();

// Records an event in the specified stream. The allocation 'ptr' will not be
// re-used until the event has occurred.
TORCH_CUDA_CPP_API cudaError_t
CachingHostAllocator_recordEvent(void* ptr, c10::cuda::CUDAStream stream);

// Releases cached pinned memory allocations via cudaHostFree
TORCH_CUDA_CPP_API void CachingHostAllocator_emptyCache();

inline TORCH_CUDA_CPP_API at::DataPtr HostAlloc(size_t size) {
  return getCachingHostAllocator()->allocate(size);
}

}}
