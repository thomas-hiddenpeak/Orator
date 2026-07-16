#pragma once

// Shared implementation helpers for business-speaker base projection and the
// internal fusion policy. This header is private to pipeline translation units.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace orator {
namespace pipeline {
namespace business_speaker_internal {

inline double Overlap(double a0, double a1, double b0, double b1) {
  return std::max(0.0, std::min(a1, b1) - std::max(a0, b0));
}

inline bool NearEqual(double a, double b) {
  return std::abs(a - b) <= 1e-9;
}

inline std::vector<std::pair<double, double>> MergeIntervals(
    std::vector<std::pair<double, double>> intervals) {
  if (intervals.empty()) return {};
  std::sort(intervals.begin(), intervals.end());
  std::vector<std::pair<double, double>> merged;
  merged.reserve(intervals.size());
  for (const auto& interval : intervals) {
    if (interval.second <= interval.first + 1e-9) continue;
    if (!merged.empty() && interval.first <= merged.back().second + 1e-9) {
      merged.back().second = std::max(merged.back().second, interval.second);
    } else {
      merged.push_back(interval);
    }
  }
  return merged;
}

inline double CoveredDuration(
    const std::vector<std::pair<double, double>>& intervals) {
  double total = 0.0;
  for (const auto& interval : intervals) {
    total += interval.second - interval.first;
  }
  return total;
}

inline double MaxGap(
    double start, double end,
    const std::vector<std::pair<double, double>>& intervals) {
  if (end <= start + 1e-9) return 0.0;
  if (intervals.empty()) return end - start;
  double max_gap = std::max(0.0, intervals.front().first - start);
  for (std::size_t i = 1; i < intervals.size(); ++i) {
    max_gap = std::max(max_gap, intervals[i].first - intervals[i - 1].second);
  }
  return std::max(max_gap, end - intervals.back().second);
}

inline std::vector<std::size_t> Utf8Offsets(const std::string& text) {
  std::vector<std::size_t> offsets;
  offsets.reserve(text.size() + 1);
  for (std::size_t i = 0; i < text.size();) {
    offsets.push_back(i);
    const unsigned char byte = static_cast<unsigned char>(text[i]);
    int advance = 1;
    if (byte >= 0xF0) {
      advance = 4;
    } else if (byte >= 0xE0) {
      advance = 3;
    } else if (byte >= 0xC0) {
      advance = 2;
    }
    i += advance;
  }
  offsets.push_back(text.size());
  return offsets;
}

}  // namespace business_speaker_internal
}  // namespace pipeline
}  // namespace orator
