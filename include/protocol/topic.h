#pragma once

// Topic & TopicPattern (Spec 004 Phase 7): hierarchical topic value types.
//
// Topic is a parsed, ordered sequence of `/`-separated levels. TopicPattern
// supports MQTT-style wildcards (`+` for one level, `#` for zero-or-more
// trailing levels) for subscription matching.

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace orator {
namespace protocol {

class Topic {
 public:
  Topic() = default;
  explicit Topic(const std::string& s);

  static Topic FromString(std::string_view s) { return Topic{std::string{s}}; }

  const std::vector<std::string>& levels() const { return levels_; }
  size_t level_count() const { return levels_.size(); }

  bool operator==(Topic const& other) const { return levels_ == other.levels_; }

  bool operator<(Topic const& other) const {
    return std::lexicographical_compare(levels_.begin(), levels_.end(),
                                        other.levels_.begin(),
                                        other.levels_.end());
  }

  std::string to_string() const;

 private:
  std::vector<std::string> levels_;
};

// Standard topic constants for the pipeline.
inline Topic kAudioRaw{"audio/raw"};
inline Topic kVadSpeechSegment{"vad/speech_segment"};
inline Topic kVadProgress{"vad/progress"};
inline Topic kAsrTranscript{"asr/transcript"};
inline Topic kAsrTranscriptPartial{"asr/transcript_partial"};
inline Topic kDiarSpeakerSegment{"diar/speaker_segment"};
inline Topic kAlignUnits{"align/units"};
inline Topic kBusinessSpeakerRevision{"business/speaker_revision"};
inline Topic kSystemPipelineOnline{"system/pipeline/online"};
inline Topic kSystemPipelineOffline{"system/pipeline/offline"};
inline Topic kSystemGpuTelemetry{"system/gpu_telemetry"};

class TopicPattern {
 public:
  TopicPattern() = default;
  explicit TopicPattern(std::string s);

  // Match a Topic against this pattern.
  //   "+" matches exactly one level.
  //   "#" matches zero or more remaining levels (must be the last level).
  bool Matches(Topic const& topic) const;

  std::string to_string() const { return pattern_; }

 private:
  std::vector<std::string> levels_;
  std::string pattern_;
};

// ---------------------------------------------------------------------------
// Topic implementation
// ---------------------------------------------------------------------------

inline Topic::Topic(const std::string& s) {
  if (s.empty()) return;
  std::string::size_type start = 0;
  while (start < s.size()) {
    std::string::size_type slash = s.find('/', start);
    if (slash == std::string::npos) {
      levels_.push_back(s.substr(start));
      break;
    }
    if (slash != start) levels_.push_back(s.substr(start, slash - start));
    start = slash + 1;
  }
}

inline std::string Topic::to_string() const {
  std::string out;
  for (size_t i = 0; i < levels_.size(); ++i) {
    if (i > 0) out += '/';
    out += levels_[i];
  }
  return out;
}

// ---------------------------------------------------------------------------
// TopicPattern implementation
// ---------------------------------------------------------------------------

inline TopicPattern::TopicPattern(std::string s) : pattern_{std::move(s)} {
  if (pattern_.empty()) return;
  std::string::size_type start = 0;
  while (start < pattern_.size()) {
    std::string::size_type slash = pattern_.find('/', start);
    if (slash == std::string::npos) {
      levels_.push_back(pattern_.substr(start));
      break;
    }
    if (slash != start)
      levels_.push_back(pattern_.substr(start, slash - start));
    start = slash + 1;
  }
}

inline bool TopicPattern::Matches(Topic const& topic) const {
  const auto& tlevels = topic.levels();
  size_t ti = 0;  // topic level index

  for (size_t pi = 0; pi < levels_.size(); ++pi) {
    if (levels_[pi] == "#" && pi == levels_.size() - 1) {
      // '#' must be the last level in pattern.
      // Matches zero or more remaining topic levels — always succeeds.
      return true;
    }

    if (levels_[pi] == "+") {
      // '+' matches exactly one level.
      if (ti >= tlevels.size()) return false;
      ++ti;
      continue;
    }

    // Exact match required.
    if (ti >= tlevels.size()) return false;
    if (levels_[pi] != tlevels[ti]) return false;
    ++ti;
  }

  // All pattern levels consumed. Topic must also be fully consumed.
  return ti == tlevels.size();
}

}  // namespace protocol
}  // namespace orator
