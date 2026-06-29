#include "model/speaker_database.h"

#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

#include "gpu/kernels.h"
#include "gpu/memory.h"

namespace orator {
namespace model {

namespace {
constexpr uint32_t kMagic = 0x524B5053;  // SPKR
constexpr uint32_t kVersion = 1;
}  // namespace

SpeakerDatabase::SpeakerDatabase(int max_speakers, int embedding_dim)
    : max_speakers_(max_speakers),
      embedding_dim_(embedding_dim),
      size_(0),
      embeddings_(static_cast<size_t>(max_speakers) * embedding_dim *
                  sizeof(float)) {
  if (max_speakers <= 0 || embedding_dim <= 0) {
    throw std::invalid_argument("invalid speaker db shape");
  }
  speaker_ids_.reserve(static_cast<size_t>(max_speakers_));
  std::memset(embeddings_.data(), 0, embeddings_.size());
}

bool SpeakerDatabase::Enroll(const std::string& speaker_id,
                             const float* embedding) {
  if (speaker_id.empty() || embedding == nullptr) return false;
  if (Contains(speaker_id) || size_ >= max_speakers_) return false;
  const int index = size_;
  speaker_to_index_[speaker_id] = index;
  speaker_ids_.push_back(speaker_id);
  WriteEmbeddingAt(index, embedding);
  ++size_;
  return true;
}

bool SpeakerDatabase::Update(const std::string& speaker_id,
                             const float* embedding) {
  if (speaker_id.empty() || embedding == nullptr) return false;
  const int index = IndexOf(speaker_id);
  if (index < 0) return false;
  WriteEmbeddingAt(index, embedding);
  return true;
}

bool SpeakerDatabase::Contains(const std::string& speaker_id) const {
  return speaker_to_index_.find(speaker_id) != speaker_to_index_.end();
}

int SpeakerDatabase::IndexOf(const std::string& speaker_id) const {
  auto it = speaker_to_index_.find(speaker_id);
  if (it == speaker_to_index_.end()) return -1;
  return it->second;
}

std::string SpeakerDatabase::SpeakerIdAt(int index) const {
  if (index < 0 || index >= size_) return "";
  return speaker_ids_[static_cast<size_t>(index)];
}

void SpeakerDatabase::WriteEmbeddingAt(int index, const float* embedding) {
  auto* base = static_cast<float*>(embeddings_.data());
  float* dst = base + static_cast<size_t>(index) * embedding_dim_;
  std::memcpy(dst, embedding,
              static_cast<size_t>(embedding_dim_) * sizeof(float));
}

bool SpeakerDatabase::Save(const std::string& path) const {
  std::ofstream out(path, std::ios::binary);
  if (!out.is_open()) return false;

  out.write(reinterpret_cast<const char*>(&kMagic), sizeof(kMagic));
  out.write(reinterpret_cast<const char*>(&kVersion), sizeof(kVersion));
  out.write(reinterpret_cast<const char*>(&max_speakers_),
            sizeof(max_speakers_));
  out.write(reinterpret_cast<const char*>(&embedding_dim_),
            sizeof(embedding_dim_));
  out.write(reinterpret_cast<const char*>(&size_), sizeof(size_));

  for (int i = 0; i < size_; ++i) {
    const auto& id = speaker_ids_[static_cast<size_t>(i)];
    uint32_t len = static_cast<uint32_t>(id.size());
    out.write(reinterpret_cast<const char*>(&len), sizeof(len));
    out.write(id.data(), static_cast<std::streamsize>(len));
  }

  const size_t used_bytes =
      static_cast<size_t>(size_) * embedding_dim_ * sizeof(float);
  out.write(static_cast<const char*>(embeddings_.data()),
            static_cast<std::streamsize>(used_bytes));
  if (!out.good()) return false;

  // Display names persist in a sidecar so the binary registry format is
  // unchanged and names stay independently editable (Spec 010 R6).
  std::unordered_map<std::string, std::string> names;
  {
    std::lock_guard<std::mutex> lk(names_mutex_);
    names = names_;
  }
  if (!names.empty()) {
    std::ofstream nout(path + ".names");
    for (const auto& kv : names) {
      if (kv.second.empty()) continue;
      nout << kv.first << '\t' << kv.second << '\n';
    }
  }
  return true;
}

bool SpeakerDatabase::Load(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) return false;

  uint32_t magic = 0;
  uint32_t version = 0;
  int max_speakers = 0;
  int embedding_dim = 0;
  int size = 0;

  in.read(reinterpret_cast<char*>(&magic), sizeof(magic));
  in.read(reinterpret_cast<char*>(&version), sizeof(version));
  in.read(reinterpret_cast<char*>(&max_speakers), sizeof(max_speakers));
  in.read(reinterpret_cast<char*>(&embedding_dim), sizeof(embedding_dim));
  in.read(reinterpret_cast<char*>(&size), sizeof(size));

  if (!in.good() || magic != kMagic || version != kVersion) return false;
  if (max_speakers != max_speakers_ || embedding_dim != embedding_dim_)
    return false;
  if (size < 0 || size > max_speakers_) return false;

  speaker_ids_.clear();
  speaker_to_index_.clear();
  speaker_ids_.reserve(static_cast<size_t>(max_speakers_));

  for (int i = 0; i < size; ++i) {
    uint32_t len = 0;
    in.read(reinterpret_cast<char*>(&len), sizeof(len));
    if (!in.good()) return false;
    std::string id(len, '\0');
    in.read(id.data(), static_cast<std::streamsize>(len));
    if (!in.good()) return false;
    speaker_to_index_[id] = i;
    speaker_ids_.push_back(std::move(id));
  }

  const size_t used_bytes =
      static_cast<size_t>(size) * embedding_dim_ * sizeof(float);
  std::memset(embeddings_.data(), 0, embeddings_.size());
  in.read(static_cast<char*>(embeddings_.data()),
          static_cast<std::streamsize>(used_bytes));
  if (!in.good()) return false;
  size_ = size;

  // Optional display-name sidecar (absent is fine).
  {
    std::ifstream nin(path + ".names");
    std::lock_guard<std::mutex> lk(names_mutex_);
    names_.clear();
    std::string line;
    while (std::getline(nin, line)) {
      const auto tab = line.find('\t');
      if (tab == std::string::npos) continue;
      names_[line.substr(0, tab)] = line.substr(tab + 1);
    }
  }
  return true;
}

void SpeakerDatabase::SetDisplayName(const std::string& speaker_id,
                                    const std::string& name) {
  std::lock_guard<std::mutex> lk(names_mutex_);
  names_[speaker_id] = name;
}

std::string SpeakerDatabase::DisplayName(const std::string& speaker_id) const {
  std::lock_guard<std::mutex> lk(names_mutex_);
  auto it = names_.find(speaker_id);
  return it != names_.end() ? it->second : std::string();
}

int SpeakerDatabase::Match(const float* query_embedding, float threshold,
                           float* out_score) const {
  float* best_score = out_score;
  if (size_ <= 0 || query_embedding == nullptr) {
    if (best_score) *best_score = 0.0f;
    return -1;
  }

  gpu::DeviceBuffer query_dev(static_cast<size_t>(embedding_dim_) *
                              sizeof(float));
  gpu::DeviceBuffer scores_dev(static_cast<size_t>(size_) * sizeof(float));
  gpu::GpuMemory::CopyHostToDevice(
      query_dev.data(), query_embedding,
      static_cast<size_t>(embedding_dim_) * sizeof(float));

  gpu::Kernels::BatchCosineSimilarity(
      static_cast<const float*>(query_dev.data()), Embeddings(), size_,
      embedding_dim_, static_cast<float*>(scores_dev.data()));

  std::vector<float> scores(static_cast<size_t>(size_), 0.0f);
  gpu::GpuMemory::CopyDeviceToHost(scores.data(), scores_dev.data(),
                                   static_cast<size_t>(size_) * sizeof(float));

  int best_idx = -1;
  float best = threshold;
  for (int i = 0; i < size_; ++i) {
    if (scores[static_cast<size_t>(i)] > best) {
      best = scores[static_cast<size_t>(i)];
      best_idx = i;
    }
  }
  if (best_score) *best_score = (best_idx >= 0) ? best : 0.0f;
  return best_idx;
}

int SpeakerDatabase::MatchExcluding(const float* query_embedding,
                                    float threshold,
                                    const std::vector<std::string>& exclude_ids,
                                    float* out_score) const {
  if (size_ <= 0 || query_embedding == nullptr) {
    if (out_score) *out_score = 0.0f;
    return -1;
  }
  gpu::DeviceBuffer query_dev(static_cast<size_t>(embedding_dim_) *
                              sizeof(float));
  gpu::DeviceBuffer scores_dev(static_cast<size_t>(size_) * sizeof(float));
  gpu::GpuMemory::CopyHostToDevice(
      query_dev.data(), query_embedding,
      static_cast<size_t>(embedding_dim_) * sizeof(float));
  gpu::Kernels::BatchCosineSimilarity(
      static_cast<const float*>(query_dev.data()), Embeddings(), size_,
      embedding_dim_, static_cast<float*>(scores_dev.data()));
  std::vector<float> scores(static_cast<size_t>(size_), 0.0f);
  gpu::GpuMemory::CopyDeviceToHost(scores.data(), scores_dev.data(),
                                   static_cast<size_t>(size_) * sizeof(float));

  int best_idx = -1;
  float best = threshold;
  for (int i = 0; i < size_; ++i) {
    bool excluded = false;
    for (const auto& id : exclude_ids) {
      if (speaker_ids_[static_cast<size_t>(i)] == id) {
        excluded = true;
        break;
      }
    }
    if (excluded) continue;
    if (scores[static_cast<size_t>(i)] > best) {
      best = scores[static_cast<size_t>(i)];
      best_idx = i;
    }
  }
  if (out_score) *out_score = (best_idx >= 0) ? best : 0.0f;
  return best_idx;
}

}  // namespace model
}  // namespace orator
