#pragma once

// Multi-shard SafeTensors loader.
//
// Large checkpoints (e.g. Qwen3-ASR-1.7B) are split across several
// `model-0000k-of-0000n.safetensors` files with a `model.safetensors.index.json`
// mapping each tensor name to its shard. This wraps one SafeTensorReader per
// shard and resolves tensor lookups across them, preserving the same zero-copy
// mmap view semantics as the single-file reader.

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/tensor.h"
#include "io/safetensor.h"

namespace orator {
namespace io {

class ShardedSafeTensors {
 public:
  // model_dir must contain model.safetensors.index.json and the shard files it
  // references. If no index exists, falls back to a single model.safetensors.
  explicit ShardedSafeTensors(const std::string& model_dir);

  bool Has(const std::string& name) const;
  // Zero-copy CPU view into the mapped shard. Valid for this object's lifetime.
  core::Tensor GetTensorView(const std::string& name) const;
  const SafeTensorMetadata& GetMetadata(const std::string& name) const;

  const std::vector<std::string>& names() const { return names_; }
  size_t NumTensors() const { return names_.size(); }
  size_t NumShards() const { return shards_.size(); }

 private:
  std::string model_dir_;
  std::vector<std::unique_ptr<SafeTensorReader>> shards_;
  std::map<std::string, int> name_to_shard_;  // tensor name -> shard index
  std::vector<std::string> names_;

  static std::map<std::string, std::string> ParseIndex(const std::string& path);
};

}  // namespace io
}  // namespace orator
