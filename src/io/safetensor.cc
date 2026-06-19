#include "io/safetensor.h"

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace orator {
namespace io {

namespace {

size_t DTypeBytes(const std::string& dtype) {
  if (dtype == "F32" || dtype == "I32" || dtype == "U32") return 4;
  if (dtype == "F64" || dtype == "I64") return 8;
  if (dtype == "F16" || dtype == "BF16" || dtype == "I16") return 2;
  if (dtype == "U8" || dtype == "I8" || dtype == "BOOL") return 1;
  throw std::runtime_error("Unknown safetensors dtype: " + dtype);
}

}  // namespace

SafeTensorReader::SafeTensorReader(const std::string& filepath)
    : filepath_(filepath), file_size_(0), header_size_(0) {
  mmap_ = std::make_unique<gpu::MmapAllocator>(filepath);
  file_size_ = mmap_->get_mapped_size();
  if (file_size_ < 8) {
    throw std::runtime_error("safetensors file too small: " + filepath);
  }

  const char* base = static_cast<const char*>(mmap_->get_mapped_ptr());
  uint64_t header_len = 0;
  std::memcpy(&header_len, base, 8);
  if (8 + header_len > file_size_) {
    throw std::runtime_error("safetensors header length out of range");
  }
  header_size_ = static_cast<size_t>(header_len);

  std::string header_json(base + 8, header_size_);
  ParseHeader(header_json);
}

void SafeTensorReader::ParseHeader(const std::string& header_json) {
  const size_t data_base = 8 + header_size_;  // absolute start of data section
  size_t pos = 0;
  while (pos < header_json.size()) {
    size_t quote_start = header_json.find('"', pos);
    if (quote_start == std::string::npos) break;
    size_t quote_end = header_json.find('"', quote_start + 1);
    if (quote_end == std::string::npos) break;

    std::string key =
        header_json.substr(quote_start + 1, quote_end - quote_start - 1);
    if (!key.empty() && key.front() == '_') {  // __metadata__
      // Skip its value object {...} too, not just the key, otherwise the keys
      // inside the metadata object (e.g. "format") get mis-parsed as tensors.
      size_t mb = header_json.find('{', quote_end);
      if (mb == std::string::npos) break;
      int mdepth = 1;
      size_t mp = mb + 1;
      while (mp < header_json.size() && mdepth > 0) {
        if (header_json[mp] == '{') mdepth++;
        if (header_json[mp] == '}') mdepth--;
        mp++;
      }
      pos = mp;
      continue;
    }

    size_t brace_start = header_json.find('{', quote_end);
    if (brace_start == std::string::npos) break;
    int depth = 1;
    size_t brace_end = brace_start + 1;
    while (brace_end < header_json.size() && depth > 0) {
      if (header_json[brace_end] == '{') depth++;
      if (header_json[brace_end] == '}') depth--;
      brace_end++;
    }
    const std::string obj =
        header_json.substr(brace_start, brace_end - brace_start);

    SafeTensorMetadata meta;
    meta.name = key;

    size_t shape_pos = obj.find("\"shape\"");
    if (shape_pos != std::string::npos) {
      size_t b0 = obj.find('[', shape_pos);
      size_t b1 = obj.find(']', b0);
      std::istringstream iss(obj.substr(b0 + 1, b1 - b0 - 1));
      std::string tok;
      while (std::getline(iss, tok, ',')) {
        tok.erase(0, tok.find_first_not_of(" \t\n"));
        const size_t last = tok.find_last_not_of(" \t\n");
        if (last != std::string::npos) tok.erase(last + 1);
        if (!tok.empty()) meta.shape.push_back(std::stoll(tok));
      }
    }

    size_t dtype_pos = obj.find("\"dtype\"");
    if (dtype_pos != std::string::npos) {
      size_t q0 = obj.find('"', dtype_pos + 7);
      size_t q1 = obj.find('"', q0 + 1);
      meta.dtype = obj.substr(q0 + 1, q1 - q0 - 1);
    }

    size_t off_pos = obj.find("\"data_offsets\"");
    if (off_pos != std::string::npos) {
      size_t b0 = obj.find('[', off_pos);
      size_t b1 = obj.find(']', b0);
      std::istringstream iss(obj.substr(b0 + 1, b1 - b0 - 1));
      std::string tok;
      std::vector<int64_t> offs;
      while (std::getline(iss, tok, ',')) {
        tok.erase(0, tok.find_first_not_of(" \t\n"));
        const size_t last = tok.find_last_not_of(" \t\n");
        if (last != std::string::npos) tok.erase(last + 1);
        if (!tok.empty()) offs.push_back(std::stoll(tok));
      }
      if (offs.size() >= 2) {
        meta.data_offset = static_cast<int64_t>(data_base) + offs[0];
        meta.data_size = offs[1] - offs[0];
      }
    }

    metadata_[key] = meta;
    weight_names_.push_back(key);
    pos = brace_end;
  }
}

bool SafeTensorReader::Has(const std::string& name) const {
  return metadata_.find(name) != metadata_.end();
}

const SafeTensorMetadata& SafeTensorReader::GetMetadata(
    const std::string& name) const {
  auto it = metadata_.find(name);
  if (it == metadata_.end()) {
    throw std::runtime_error("Weight not found: " + name);
  }
  return it->second;
}

core::DType SafeTensorReader::ToDType(const std::string& s) {
  if (s == "F32") return core::DType::F32;
  if (s == "F16") return core::DType::F16;
  if (s == "BF16") return core::DType::BF16;
  if (s == "I32") return core::DType::I32;
  if (s == "I64") return core::DType::I64;
  if (s == "U8") return core::DType::U8;
  return core::DType::Unknown;
}

core::Tensor SafeTensorReader::GetTensorView(const std::string& name) const {
  const auto& meta = GetMetadata(name);
  char* base = static_cast<char*>(mmap_->get_mapped_ptr());
  void* ptr = base + meta.data_offset;
  return core::Tensor::View(ptr, meta.shape, ToDType(meta.dtype),
                            core::Device::CPU);
}

void SafeTensorReader::ReadWeight(const std::string& name, void* output_buffer,
                                  size_t buffer_size) const {
  const auto& meta = GetMetadata(name);
  if (buffer_size < static_cast<size_t>(meta.data_size)) {
    throw std::runtime_error("Buffer too small for weight: " + name);
  }
  const char* base = static_cast<const char*>(mmap_->get_mapped_ptr());
  std::memcpy(output_buffer, base + meta.data_offset,
              static_cast<size_t>(meta.data_size));
}

bool SafeTensorWriter::Write(const std::string& path,
                             const std::vector<Entry>& entries) {
  // Build header JSON and compute contiguous data offsets.
  std::ostringstream header;
  header << "{";
  size_t offset = 0;
  for (size_t i = 0; i < entries.size(); ++i) {
    const auto& e = entries[i];
    const size_t elem = DTypeBytes(e.dtype);
    int64_t numel = 1;
    for (int64_t d : e.shape) numel *= d;
    const size_t expected = static_cast<size_t>(numel) * elem;
    if (e.shape.empty()) {
      return false;  // scalars unsupported here
    }
    if (e.nbytes != expected) {
      return false;  // shape/dtype/nbytes mismatch
    }

    header << "\"" << e.name << "\":{\"dtype\":\"" << e.dtype
           << "\",\"shape\":[";
    for (size_t d = 0; d < e.shape.size(); ++d) {
      header << e.shape[d];
      if (d + 1 < e.shape.size()) header << ",";
    }
    header << "],\"data_offsets\":[" << offset << "," << (offset + e.nbytes)
           << "]}";
    if (i + 1 < entries.size()) header << ",";
    offset += e.nbytes;
  }
  header << "}";

  std::string header_str = header.str();
  // Pad header to 8-byte alignment with spaces (safetensors convention).
  while ((8 + header_str.size()) % 8 != 0) header_str += ' ';

  std::ofstream out(path, std::ios::binary);
  if (!out.is_open()) return false;

  uint64_t header_len = header_str.size();
  out.write(reinterpret_cast<const char*>(&header_len), 8);
  out.write(header_str.data(), static_cast<std::streamsize>(header_str.size()));
  for (const auto& e : entries) {
    out.write(static_cast<const char*>(e.data),
              static_cast<std::streamsize>(e.nbytes));
  }
  out.flush();
  out.close();
  return true;
}

}  // namespace io
}  // namespace orator
