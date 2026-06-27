#include "model/forced_align_decode.h"

#include <algorithm>
#include <cstdint>

namespace orator {
namespace model {
namespace {

// Decode a UTF-8 string into Unicode code points.
std::vector<uint32_t> Utf8ToCodepoints(const std::string& s) {
  std::vector<uint32_t> out;
  out.reserve(s.size());
  size_t i = 0;
  const size_t n = s.size();
  while (i < n) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    uint32_t cp = 0;
    int extra = 0;
    if (c < 0x80) { cp = c; extra = 0; }
    else if ((c >> 5) == 0x6) { cp = c & 0x1F; extra = 1; }
    else if ((c >> 4) == 0xE) { cp = c & 0x0F; extra = 2; }
    else if ((c >> 3) == 0x1E) { cp = c & 0x07; extra = 3; }
    else { ++i; continue; }  // invalid lead byte; skip
    if (i + extra >= n) break;
    bool ok = true;
    for (int k = 1; k <= extra; ++k) {
      const unsigned char cc = static_cast<unsigned char>(s[i + k]);
      if ((cc >> 6) != 0x2) { ok = false; break; }
      cp = (cp << 6) | (cc & 0x3F);
    }
    if (!ok) { ++i; continue; }
    out.push_back(cp);
    i += extra + 1;
  }
  return out;
}

// Encode a single code point back to UTF-8 (for emitting per-char CJK words).
void AppendUtf8(std::string* out, uint32_t cp) {
  if (cp < 0x80) {
    out->push_back(static_cast<char>(cp));
  } else if (cp < 0x800) {
    out->push_back(static_cast<char>(0xC0 | (cp >> 6)));
    out->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else if (cp < 0x10000) {
    out->push_back(static_cast<char>(0xE0 | (cp >> 12)));
    out->push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else {
    out->push_back(static_cast<char>(0xF0 | (cp >> 18)));
    out->push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
    out->push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  }
}

// CJK ideograph test -- identical ranges to the reference `_is_cjk_char`.
bool IsCjk(uint32_t cp) {
  return (cp >= 0x4E00 && cp <= 0x9FFF) ||
         (cp >= 0x3400 && cp <= 0x4DBF) ||
         (cp >= 0x20000 && cp <= 0x2A6DF) ||
         (cp >= 0x2A700 && cp <= 0x2B73F) ||
         (cp >= 0x2B740 && cp <= 0x2B81F) ||
         (cp >= 0x2B820 && cp <= 0x2CEAF) ||
         (cp >= 0xF900 && cp <= 0xFAFF) ||
         (cp >= 0x2F800 && cp <= 0x2FA1F);
}

// Whitespace test (covers ASCII + common Unicode spaces).
bool IsSpace(uint32_t cp) {
  return cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r' || cp == '\f' ||
         cp == 0x0B || cp == 0x00A0 || cp == 0x3000 ||
         (cp >= 0x2000 && cp <= 0x200A);
}

// Letter / number test. Pragmatic stand-in for unicodedata category L*/N* over
// the aligner's supported languages (full table omitted to keep zero deps).
bool IsLetterOrNumber(uint32_t cp) {
  // ASCII letters/digits.
  if ((cp >= '0' && cp <= '9') || (cp >= 'A' && cp <= 'Z') ||
      (cp >= 'a' && cp <= 'z'))
    return true;
  if (IsCjk(cp)) return true;
  // Latin-1 Supplement + Latin Extended-A/B letters (fr/de/it/es/pt accents).
  if (cp >= 0x00C0 && cp <= 0x024F) return true;
  // Greek and Coptic.
  if (cp >= 0x0370 && cp <= 0x03FF) return true;
  // Cyrillic (ru).
  if (cp >= 0x0400 && cp <= 0x04FF) return true;
  // Hiragana + Katakana (ja default fallback).
  if (cp >= 0x3040 && cp <= 0x30FF) return true;
  // Hangul syllables (ko default fallback).
  if (cp >= 0xAC00 && cp <= 0xD7A3) return true;
  // Fullwidth digits / Latin letters.
  if (cp >= 0xFF10 && cp <= 0xFF19) return true;
  if ((cp >= 0xFF21 && cp <= 0xFF3A) || (cp >= 0xFF41 && cp <= 0xFF5A)) return true;
  return false;
}

// Reference `_is_kept_char`: apostrophe, or a letter/number, or CJK.
bool IsKept(uint32_t cp) {
  return cp == '\'' || IsLetterOrNumber(cp) || IsCjk(cp);
}

}  // namespace

std::vector<std::string> SplitWordsForAlignment(const std::string& text,
                                                const std::string& /*language*/) {
  const std::vector<uint32_t> cps = Utf8ToCodepoints(text);
  std::vector<std::string> tokens;
  std::string buffer;

  auto flush = [&]() {
    if (!buffer.empty()) {
      tokens.push_back(buffer);
      buffer.clear();
    }
  };

  for (uint32_t cp : cps) {
    if (IsCjk(cp)) {
      flush();
      std::string ch;
      AppendUtf8(&ch, cp);
      tokens.push_back(ch);
    } else if (IsSpace(cp)) {
      flush();
    } else if (IsKept(cp)) {
      AppendUtf8(&buffer, cp);
    }
    // Punctuation / other: dropped (no flush), matching the reference.
  }
  flush();
  return tokens;
}

namespace {
// Python-style floor division (rounds toward negative infinity).
int FloorDiv(int a, int b) {
  int q = a / b;
  if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
  return q;
}
}  // namespace

int AudioTokenLength(int mel_frames, int n_window) {
  const int chunk_len = n_window * 2;  // 100 mel frames per chunk
  const int remainder = mel_frames % chunk_len;       // frames in final chunk
  const int feat = FloorDiv(remainder - 1, 2) + 1;    // after conv1 (stride 2)
  const int per_chunk = FloorDiv(feat - 1, 2) + 1;    // after conv2 (stride 2)
  const int token = FloorDiv(per_chunk - 1, 2) + 1 +  // after conv3 (stride 2)
                    (mel_frames / chunk_len) * 13;    // + 13 per full chunk
  return token;
}

std::vector<int> BuildAlignerInputIds(
    const std::vector<std::vector<int>>& word_token_ids, int n_audio,
    int audio_start_id, int audio_pad_id, int audio_end_id, int timestamp_id) {
  std::vector<int> ids;
  ids.reserve(2 + n_audio + word_token_ids.size() * 4);
  ids.push_back(audio_start_id);
  for (int i = 0; i < n_audio; ++i) ids.push_back(audio_pad_id);
  ids.push_back(audio_end_id);
  for (const auto& w : word_token_ids) {
    ids.insert(ids.end(), w.begin(), w.end());
    ids.push_back(timestamp_id);
    ids.push_back(timestamp_id);
  }
  return ids;
}

std::vector<long> FixTimestamps(const std::vector<double>& raw_ms) {
  const int n = static_cast<int>(raw_ms.size());
  if (n == 0) return {};

  // Longest non-decreasing subsequence via O(n^2) DP (mirrors the reference).
  std::vector<int> dp(n, 1), parent(n, -1);
  for (int cur = 1; cur < n; ++cur) {
    for (int prev = 0; prev < cur; ++prev) {
      if (raw_ms[prev] <= raw_ms[cur] && dp[prev] + 1 > dp[cur]) {
        dp[cur] = dp[prev] + 1;
        parent[cur] = prev;
      }
    }
  }
  // First index achieving the maximum length (Python dp.index(max)).
  int max_len = 0, max_idx = 0;
  for (int i = 0; i < n; ++i) {
    if (dp[i] > max_len) { max_len = dp[i]; max_idx = i; }
  }
  std::vector<bool> is_normal(n, false);
  for (int idx = max_idx; idx != -1; idx = parent[idx]) is_normal[idx] = true;

  std::vector<double> result = raw_ms;
  int block_start = 0;
  while (block_start < n) {
    if (is_normal[block_start]) { ++block_start; continue; }
    int block_end = block_start;
    while (block_end < n && !is_normal[block_end]) ++block_end;
    const int anomaly_count = block_end - block_start;

    bool has_left = false, has_right = false;
    double left_val = 0.0, right_val = 0.0;
    for (int scan = block_start - 1; scan >= 0; --scan) {
      if (is_normal[scan]) { left_val = result[scan]; has_left = true; break; }
    }
    for (int scan = block_end; scan < n; ++scan) {
      if (is_normal[scan]) { right_val = result[scan]; has_right = true; break; }
    }

    if (anomaly_count <= 2) {
      // Short block: snap each position to the nearer good neighbour.
      for (int pos = block_start; pos < block_end; ++pos) {
        if (!has_left) result[pos] = right_val;
        else if (!has_right) result[pos] = left_val;
        else
          result[pos] = ((pos - (block_start - 1)) <= (block_end - pos))
                            ? left_val
                            : right_val;
      }
    } else {
      // Long block: linearly interpolate between the surrounding good values.
      if (has_left && has_right) {
        const double step = (right_val - left_val) / (anomaly_count + 1);
        for (int pos = block_start; pos < block_end; ++pos)
          result[pos] = left_val + step * (pos - block_start + 1);
      } else if (has_left) {
        for (int pos = block_start; pos < block_end; ++pos) result[pos] = left_val;
      } else if (has_right) {
        for (int pos = block_start; pos < block_end; ++pos) result[pos] = right_val;
      }
    }
    block_start = block_end;
  }

  std::vector<long> out(n);
  for (int i = 0; i < n; ++i)
    out[i] = static_cast<long>(result[i]);  // Python int(): truncate toward zero
  return out;
}

std::vector<AlignUnit> PairWordTimestamps(const std::vector<std::string>& words,
                                          const std::vector<long>& fixed_ms) {
  std::vector<AlignUnit> units;
  units.reserve(words.size());
  for (size_t k = 0; k < words.size(); ++k) {
    const size_t si = 2 * k, ei = 2 * k + 1;
    if (ei >= fixed_ms.size()) break;
    AlignUnit u;
    u.text = words[k];
    u.start_sec = static_cast<double>(fixed_ms[si]) / 1000.0;
    u.end_sec = static_cast<double>(fixed_ms[ei]) / 1000.0;
    units.push_back(std::move(u));
  }
  return units;
}

}  // namespace model
}  // namespace orator
