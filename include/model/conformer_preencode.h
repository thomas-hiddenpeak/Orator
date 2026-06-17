#pragma once

#include <cuda_runtime.h>

#include <memory>
#include <vector>

#include "gpu/memory.h"
#include "io/safetensor.h"

// Conformer pre-encode (dw_striding subsampling, factor 8) for Sortformer.
//
// Replicates NeMo's ConvSubsampling(subsampling="dw_striding", factor=8):
//   mel [C=128, T] (transposed to [T,128] then treated as a [1,T,128] image)
//   -> Conv2d(1->256, k3, s2, p1) + ReLU
//   -> depthwise Conv2d(256->256, k3, s2, p1, groups=256) + pointwise Conv2d(k1) + ReLU
//   -> depthwise + pointwise + ReLU
//   -> flatten [256,T'',16] (channel-major per frame) -> Linear(4096->512)
// with NeMo's length masking so padded tail frames match exactly.
//
// Weights are loaded (copied) from a SafeTensorReader into unified memory so
// the kernels can read them on the Jetson unified architecture.

namespace orator {
namespace model {

class ConformerPreEncode {
 public:
  ConformerPreEncode();

  // Loads encoder.pre_encode.* weights from the reader.
  void LoadWeights(const io::SafeTensorReader& reader,
                   const std::string& prefix = "encoder.pre_encode");

  // Runs the subsampling. `mel` is [n_mels, n_frames] row-major (channel/freq
  // is the first dim = NeMo's feat dim). `valid_len` is the number of valid
  // input frames (<= n_frames); the rest are masked to zero like NeMo.
  // `stream` is the CUDA stream for kernel launches and synchronization.
  // Returns [out_frames, 512] row-major in a host-readable vector and sets
  // out_frames / out_valid_len.
  std::vector<float> Forward(const float* mel, int n_mels, int n_frames,
                              int valid_len, int* out_frames, int* out_valid_len,
                              cudaStream_t stream = nullptr);

 private:
  // Each weight copied into unified memory for GPU access.
  std::unique_ptr<gpu::UnifiedBuffer> w0_, b0_;       // conv0 [256,1,3,3]
  std::unique_ptr<gpu::UnifiedBuffer> w2_, b2_;       // dw    [256,1,3,3]
  std::unique_ptr<gpu::UnifiedBuffer> w3_, b3_;       // pw    [256,256,1,1]
  std::unique_ptr<gpu::UnifiedBuffer> w5_, b5_;       // dw    [256,1,3,3]
  std::unique_ptr<gpu::UnifiedBuffer> w6_, b6_;       // pw    [256,256,1,1]
  std::unique_ptr<gpu::UnifiedBuffer> wout_, bout_;   // linear [512,4096]
  int conv_channels_ = 256;
  int d_model_ = 512;
  bool loaded_ = false;
};

}  // namespace model
}  // namespace orator
