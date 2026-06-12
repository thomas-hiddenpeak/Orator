#include "io/sharded_safetensor.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace orator {
namespace io {

namespace {

std::string JoinPath(const std::string& dir, const std::string& file) {
  if (dir.empty()) return file;
  return dir.back() == '/' ? dir + file : dir + "/" + file;
}

bool FileExists(const std::string& path) {
  std::ifstream f(path);
  return f.good();
}

}  // namespace

// Minimal parse of the {"weight_map": {"<name>": "<shard file>", ...}} block.
// Avoids a JSON dependency; the index format is flat and well-defined.
std::map<std::string, std::string> ShardedSafeTensors::ParseIndex(
    const std::string& path) {
  std::ifstream f(path, std::ios::binary | std::ios::ate);
  if (!f) throw std::runtime_error("cannot open index: " + path);
  std::streamsize n = f.tellg();
  f.seekg(0);
  std::string json(static_cast<size_t>(n), '\0');
  f.read(&json[0], n);

  std::map<std::string, std::string> map;
  size_t wm = json.find("\"weight_map\"");
  if (wm == std::string::npos) throw std::runtime_error("index missing weight_map");
  size_t brace = json.find('{', wm);
  if (brace == std::string::npos) throw std::runtime_error("malformed weight_map");

  // Walk "key": "value" pairs until the matching close brace.
  int depth = 1;
  size_t pos = brace + 1;
  while (pos < json.size() && depth > 0) {
    if (json[pos] == '{') { depth++; pos++; continue; }
    if (json[pos] == '}') { depth--; pos++; continue; }
    size_t k0 = json.find('"', pos);
    if (k0 == std::string::npos || k0 > json.find('}', pos)) break;
    size_t k1 = json.find('"', k0 + 1);
    if (k1 == std::string::npos) break;
    std::string key = json.substr(k0 + 1, k1 - k0 - 1);
    size_t colon = json.find(':', k1);
    size_t v0 = json.find('"', colon);
    size_t v1 = json.find('"', v0 + 1);
    if (v0 == std::string::npos || v1 == std::string::npos) break;
    std::string val = json.substr(v0 + 1, v1 - v0 - 1);
    map[key] = val;
    pos = v1 + 1;
  }
  return map;
}

ShardedSafeTensors::ShardedSafeTensors(const std::string& model_dir)
    : model_dir_(model_dir) {
  const std::string index_path =
      JoinPath(model_dir, "model.safetensors.index.json");

  if (FileExists(index_path)) {
    auto weight_map = ParseIndex(index_path);

    // Open each distinct shard once, in deterministic order.
    std::map<std::string, int> shard_index;
    for (const auto& kv : weight_map) {
      const std::string& shard_file = kv.second;
      if (shard_index.find(shard_file) == shard_index.end()) {
        const int idx = static_cast<int>(shards_.size());
        shard_index[shard_file] = idx;
        shards_.push_back(std::make_unique<SafeTensorReader>(
            JoinPath(model_dir, shard_file)));
      }
    }
    for (const auto& kv : weight_map) {
      name_to_shard_[kv.first] = shard_index[kv.second];
      names_.push_back(kv.first);
    }
  } else {
    // Single-file fallback.
    const std::string single = JoinPath(model_dir, "model.safetensors");
    if (!FileExists(single)) {
      throw std::runtime_error("no safetensors index or model.safetensors in " +
                               model_dir);
    }
    shards_.push_back(std::make_unique<SafeTensorReader>(single));
    for (const auto& name : shards_[0]->GetWeightNames()) {
      name_to_shard_[name] = 0;
      names_.push_back(name);
    }
  }
}

bool ShardedSafeTensors::Has(const std::string& name) const {
  return name_to_shard_.find(name) != name_to_shard_.end();
}

const SafeTensorMetadata& ShardedSafeTensors::GetMetadata(
    const std::string& name) const {
  auto it = name_to_shard_.find(name);
  if (it == name_to_shard_.end()) {
    throw std::runtime_error("tensor not found: " + name);
  }
  return shards_[it->second]->GetMetadata(name);
}

core::Tensor ShardedSafeTensors::GetTensorView(const std::string& name) const {
  auto it = name_to_shard_.find(name);
  if (it == name_to_shard_.end()) {
    throw std::runtime_error("tensor not found: " + name);
  }
  return shards_[it->second]->GetTensorView(name);
}

}  // namespace io
}  // namespace orator
