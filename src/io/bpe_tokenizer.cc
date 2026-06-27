#include "io/bpe_tokenizer.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace orator {
namespace io {

namespace {

// Encode a Unicode code point as UTF-8.
std::string Utf8(int cp) {
  std::string s;
  if (cp < 0x80) {
    s += static_cast<char>(cp);
  } else if (cp < 0x800) {
    s += static_cast<char>(0xC0 | (cp >> 6));
    s += static_cast<char>(0x80 | (cp & 0x3F));
  } else if (cp < 0x10000) {
    s += static_cast<char>(0xE0 | (cp >> 12));
    s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    s += static_cast<char>(0x80 | (cp & 0x3F));
  } else {
    s += static_cast<char>(0xF0 | (cp >> 18));
    s += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
    s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    s += static_cast<char>(0x80 | (cp & 0x3F));
  }
  return s;
}

// Split a UTF-8 string into its code-point substrings.
std::vector<std::string> Utf8Chars(const std::string& s) {
  std::vector<std::string> out;
  size_t i = 0;
  while (i < s.size()) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    size_t len = 1;
    if ((c & 0x80) == 0)
      len = 1;
    else if ((c & 0xE0) == 0xC0)
      len = 2;
    else if ((c & 0xF0) == 0xE0)
      len = 3;
    else if ((c & 0xF8) == 0xF0)
      len = 4;
    out.push_back(s.substr(i, len));
    i += len;
  }
  return out;
}

// GPT-2 bytes-to-unicode table.
void BuildByteUnicode(std::unordered_map<int, std::string>* b2u,
                      std::unordered_map<std::string, int>* u2b) {
  std::vector<int> bs;
  for (int b = '!'; b <= '~'; ++b) bs.push_back(b);
  for (int b = 0xA1; b <= 0xAC; ++b) bs.push_back(b);
  for (int b = 0xAE; b <= 0xFF; ++b) bs.push_back(b);
  std::vector<int> cs = bs;
  int n = 0;
  for (int b = 0; b < 256; ++b) {
    if (std::find(bs.begin(), bs.end(), b) == bs.end()) {
      bs.push_back(b);
      cs.push_back(256 + n);
      ++n;
    }
  }
  for (size_t i = 0; i < bs.size(); ++i) {
    const std::string u = Utf8(cs[i]);
    (*b2u)[bs[i]] = u;
    (*u2b)[u] = bs[i];
  }
}

// Minimal JSON string unescape (handles \", \\, \/, \n, \t, \uXXXX).
std::string Unescape(const std::string& s) {
  std::string out;
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] != '\\') {
      out += s[i];
      continue;
    }
    ++i;
    if (i >= s.size()) break;
    switch (s[i]) {
      case 'n':
        out += '\n';
        break;
      case 't':
        out += '\t';
        break;
      case 'r':
        out += '\r';
        break;
      case 'b':
        out += '\b';
        break;
      case 'f':
        out += '\f';
        break;
      case '"':
        out += '"';
        break;
      case '\\':
        out += '\\';
        break;
      case '/':
        out += '/';
        break;
      case 'u': {
        if (i + 4 < s.size()) {
          int cp = 0;
          try {
            cp = std::stoi(s.substr(i + 1, 4), nullptr, 16);
          } catch (const std::exception&) {
            cp = 0xFFFD;
          }
          out += Utf8(cp);
          i += 4;
        }
        break;
      }
      default:
        out += s[i];
        break;
    }
  }
  return out;
}

}  // namespace

bool BpeTokenizer::Load(const std::string& model_dir) {
  std::string dir = model_dir;
  if (!dir.empty() && dir.back() != '/') dir += '/';

  BuildByteUnicode(&byte_to_unicode_, &unicode_to_byte_);

  // ---- vocab.json: {"token": id, ...} on a single line ----
  std::ifstream vf(dir + "vocab.json", std::ios::binary | std::ios::ate);
  if (!vf) return false;
  std::streamsize vn = vf.tellg();
  vf.seekg(0);
  std::string vjson(static_cast<size_t>(vn), '\0');
  vf.read(&vjson[0], vn);

  size_t pos = vjson.find('{');
  if (pos == std::string::npos) return false;
  ++pos;
  while (pos < vjson.size()) {
    size_t q0 = vjson.find('"', pos);
    if (q0 == std::string::npos) break;
    // find closing quote, respecting escapes
    size_t q1 = q0 + 1;
    while (q1 < vjson.size()) {
      if (vjson[q1] == '\\') {
        q1 += 2;
        continue;
      }
      if (vjson[q1] == '"') break;
      ++q1;
    }
    if (q1 >= vjson.size()) break;
    std::string key = Unescape(vjson.substr(q0 + 1, q1 - q0 - 1));
    size_t colon = vjson.find(':', q1);
    size_t ns = colon + 1;
    while (ns < vjson.size() && (vjson[ns] == ' ' || vjson[ns] == '\t')) ++ns;
    size_t ne = ns;
    while (ne < vjson.size() && (isdigit(vjson[ne]) || vjson[ne] == '-')) ++ne;
    if (ne == ns) break;
    int id = -1;
    try {
      id = std::stoi(vjson.substr(ns, ne - ns));
    } catch (const std::exception&) {
      break;
    }
    token_to_id_[key] = id;
    id_to_token_[id] = key;
    pos = ne;
    size_t comma = vjson.find(',', pos);
    if (comma == std::string::npos) break;
    pos = comma + 1;
  }

  // ---- merges.txt: "A B" per line, rank = order (skip header) ----
  std::ifstream mf(dir + "merges.txt");
  if (!mf) return false;
  std::string line;
  int rank = 0;
  while (std::getline(mf, line)) {
    if (line.empty() || line[0] == '#') continue;
    size_t sp = line.find(' ');
    if (sp == std::string::npos) continue;
    std::string a = line.substr(0, sp);
    std::string b = line.substr(sp + 1);
    if (!b.empty() && b.back() == '\r') b.pop_back();
    merge_rank_[{a, b}] = rank++;
  }

  // ---- special tokens: parse added_tokens_decoder ids from tokenizer_config
  // ----
  std::ifstream tf(dir + "tokenizer_config.json",
                   std::ios::binary | std::ios::ate);
  if (tf) {
    std::streamsize tn = tf.tellg();
    tf.seekg(0);
    std::string tj(static_cast<size_t>(tn), '\0');
    tf.read(&tj[0], tn);
    size_t adt = tj.find("\"added_tokens_decoder\"");
    if (adt != std::string::npos) {
      // Each entry: "<id>": { ... "special": true }.
      size_t p = adt;
      while (true) {
        size_t q0 = tj.find('"', p + 1);
        if (q0 == std::string::npos) break;
        size_t q1 = tj.find('"', q0 + 1);
        if (q1 == std::string::npos) break;
        std::string key = tj.substr(q0 + 1, q1 - q0 - 1);
        bool numeric =
            !key.empty() && std::all_of(key.begin(), key.end(), ::isdigit);
        if (numeric) {
          size_t brace = tj.find('{', q1);
          size_t braceEnd = tj.find('}', brace);
          if (brace != std::string::npos && braceEnd != std::string::npos) {
            std::string obj = tj.substr(brace, braceEnd - brace);
            if (obj.find("\"special\": true") != std::string::npos ||
                obj.find("\"special\":true") != std::string::npos) {
              try {
                special_ids_[std::stoi(key)] = true;
              } catch (const std::exception&) { /* skip malformed key */
              }
            }
            p = braceEnd;
            continue;
          }
        }
        p = q1;
        if (tj.compare(p, 2, "}}") == 0) {
        }
        if (p > adt + 200000) break;  // safety bound
      }
    }
  }
  return !token_to_id_.empty();
}

std::vector<std::string> BpeTokenizer::BpeWord(const std::string& token) const {
  std::vector<std::string> word = Utf8Chars(token);
  if (word.size() < 2) return word;
  while (true) {
    int best_rank = -1;
    size_t best_i = 0;
    for (size_t i = 0; i + 1 < word.size(); ++i) {
      auto it = merge_rank_.find({word[i], word[i + 1]});
      if (it != merge_rank_.end() &&
          (best_rank < 0 || it->second < best_rank)) {
        best_rank = it->second;
        best_i = i;
      }
    }
    if (best_rank < 0) break;
    std::vector<std::string> merged;
    for (size_t i = 0; i < word.size();) {
      if (i == best_i) {
        merged.push_back(word[i] + word[i + 1]);
        i += 2;
      } else {
        merged.push_back(word[i]);
        ++i;
      }
    }
    word.swap(merged);
  }
  return word;
}

// GPT-2-style pre-tokenizer, sufficient for the ASCII prompt prefix: split on
// whitespace, attaching a leading space to the following word.
std::vector<std::string> BpeTokenizer::PreTokenize(
    const std::string& text) const {
  std::vector<std::string> pieces;
  size_t i = 0;
  while (i < text.size()) {
    std::string piece;
    if (text[i] == ' ') {
      piece += text[i++];
      while (i < text.size() && text[i] != ' ') piece += text[i++];
    } else {
      while (i < text.size() && text[i] != ' ') piece += text[i++];
    }
    if (!piece.empty()) pieces.push_back(piece);
  }
  return pieces;
}

std::vector<int> BpeTokenizer::Encode(const std::string& text) const {
  std::vector<int> ids;
  for (const std::string& piece : PreTokenize(text)) {
    // byte-level: map each byte to its unicode representative
    std::string mapped;
    for (unsigned char c : piece) mapped += byte_to_unicode_.at(c);
    for (const std::string& tok : BpeWord(mapped)) {
      auto it = token_to_id_.find(tok);
      if (it != token_to_id_.end()) ids.push_back(it->second);
    }
  }
  return ids;
}

std::string BpeTokenizer::Decode(const std::vector<int>& ids,
                                 bool skip_special) const {
  std::string bytes;
  for (int id : ids) {
    if (skip_special && IsSpecial(id)) continue;
    auto it = id_to_token_.find(id);
    if (it == id_to_token_.end()) continue;
    // token is a sequence of byte-unicode code points; map each back to a byte
    for (const std::string& ch : Utf8Chars(it->second)) {
      auto bit = unicode_to_byte_.find(ch);
      if (bit != unicode_to_byte_.end())
        bytes += static_cast<char>(bit->second);
    }
  }
  return bytes;  // already valid UTF-8
}

}  // namespace io
}  // namespace orator
