#pragma once

// Qwen2 byte-level BPE tokenizer (vocab.json + merges.txt).
//
// Dependency-free. Supports:
//   * decode(ids) -> UTF-8 text (the ASR output path; special tokens skipped)
//   * encode(text) -> ids (used for the "language X" prompt prefix; byte-level
//     BPE with a GPT-2-style pre-tokenizer sufficient for ASCII prompt text)
//   * special-token id constants for prompt construction.
//
// Byte-level: each input byte maps to a printable Unicode code point (the GPT-2
// bytes-to-unicode table); vocab keys/merges live in that space.

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace orator {
namespace io {

class BpeTokenizer {
 public:
  // Loads vocab.json + merges.txt + tokenizer_config.json from model_dir.
  bool Load(const std::string& model_dir);

  std::vector<int> Encode(const std::string& text) const;        // ordinary text
  std::string Decode(const std::vector<int>& ids,
                     bool skip_special = true) const;

  int VocabSize() const { return static_cast<int>(id_to_token_.size()); }
  bool IsSpecial(int id) const { return special_ids_.count(id) != 0; }

 private:
  std::vector<std::string> BpeWord(const std::string& token) const;
  std::vector<std::string> PreTokenize(const std::string& text) const;

  std::unordered_map<std::string, int> token_to_id_;
  std::unordered_map<int, std::string> id_to_token_;
  std::map<std::pair<std::string, std::string>, int> merge_rank_;
  std::unordered_map<int, std::string> byte_to_unicode_;  // byte -> utf8(cp)
  std::unordered_map<std::string, int> unicode_to_byte_;  // utf8(cp) -> byte
  std::map<int, bool> special_ids_;
};

}  // namespace io
}  // namespace orator
