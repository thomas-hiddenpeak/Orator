// Implements strict parsing and capture-ordered replay for the offline
// speaker-identity evidence probe. The code handles producer data only and
// contains no reference labels or product-accuracy decisions.

#include "tools/probes/speaker_identity_replay_input.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace orator::tools {
namespace {

std::vector<std::string> SplitTab(const std::string& line) {
  std::vector<std::string> values;
  std::size_t start = 0;
  while (true) {
    const std::size_t tab = line.find('\t', start);
    values.push_back(line.substr(start, tab - start));
    if (tab == std::string::npos) break;
    start = tab + 1;
  }
  return values;
}

std::size_t RequireColumn(const std::vector<std::string>& header,
                          const std::string& name) {
  const auto found = std::find(header.begin(), header.end(), name);
  if (found == header.end()) {
    throw std::runtime_error("snapshot TSV missing column: " + name);
  }
  return static_cast<std::size_t>(found - header.begin());
}

long ParseLong(const std::string& value, const std::string& field) {
  std::size_t parsed = 0;
  long result = 0;
  try {
    result = std::stol(value, &parsed);
  } catch (const std::exception&) {
    throw std::runtime_error("invalid " + field + ": " + value);
  }
  if (parsed != value.size()) {
    throw std::runtime_error("invalid " + field + ": " + value);
  }
  return result;
}

int ParseInt(const std::string& value, const std::string& field) {
  const long parsed = ParseLong(value, field);
  if (parsed < std::numeric_limits<int>::min() ||
      parsed > std::numeric_limits<int>::max()) {
    throw std::runtime_error("out-of-range " + field + ": " + value);
  }
  return static_cast<int>(parsed);
}

double ParseDouble(const std::string& value, const std::string& field) {
  std::size_t parsed = 0;
  double result = 0.0;
  try {
    result = std::stod(value, &parsed);
  } catch (const std::exception&) {
    throw std::runtime_error("invalid " + field + ": " + value);
  }
  if (parsed != value.size() || !std::isfinite(result)) {
    throw std::runtime_error("invalid " + field + ": " + value);
  }
  return result;
}

float ParseFloat(const std::string& value, const std::string& field) {
  const double parsed = ParseDouble(value, field);
  if (parsed < -std::numeric_limits<float>::max() ||
      parsed > std::numeric_limits<float>::max()) {
    throw std::runtime_error("out-of-range " + field + ": " + value);
  }
  return static_cast<float>(parsed);
}

bool SegmentOrderLess(const core::DiarSegment& left,
                      const core::DiarSegment& right) {
  if (left.start_sec != right.start_sec) {
    return left.start_sec < right.start_sec;
  }
  if (left.end_sec != right.end_sec) return left.end_sec < right.end_sec;
  return left.local_speaker < right.local_speaker;
}

void ValidateSnapshot(const SpeakerIdentityReplaySnapshot& snapshot,
                      bool has_captured_speaker_ids) {
  if (snapshot.snapshot_index < 0) {
    throw std::runtime_error("snapshot_index must be nonnegative");
  }
  if (has_captured_speaker_ids &&
      snapshot.captured_speaker_ids.size() != snapshot.segments.size()) {
    throw std::runtime_error(
        "captured identity count differs from segment count");
  }
  if (!has_captured_speaker_ids && !snapshot.captured_speaker_ids.empty()) {
    throw std::runtime_error("unexpected captured identities");
  }
  if (!std::is_sorted(snapshot.segments.begin(), snapshot.segments.end(),
                      SegmentOrderLess)) {
    throw std::runtime_error("segments are not ordered within snapshot " +
                             std::to_string(snapshot.snapshot_index));
  }
}

}  // namespace

bool IsSpeakerIdentitySnapshotInput(const std::string& path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error("cannot open segment input: " + path);
  std::string line;
  if (!std::getline(input, line)) {
    throw std::runtime_error("empty segment input: " + path);
  }
  const auto header = SplitTab(line);
  return !header.empty() && header.front() == "snapshot_index";
}

SpeakerIdentitySnapshotInput ReadSpeakerIdentitySnapshots(
    const std::string& path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error("cannot open snapshot TSV: " + path);

  std::string line;
  if (!std::getline(input, line)) {
    throw std::runtime_error("empty snapshot TSV: " + path);
  }
  const auto header = SplitTab(line);
  if (header.empty() || header.front() != "snapshot_index") {
    throw std::runtime_error(
        "snapshot TSV first column must be snapshot_index");
  }
  const std::size_t snapshot_column = RequireColumn(header, "snapshot_index");
  const std::size_t start_column = RequireColumn(header, "start_sec");
  const std::size_t end_column = RequireColumn(header, "end_sec");
  const std::size_t local_column = RequireColumn(header, "local_speaker");
  const std::size_t confidence_column = RequireColumn(header, "confidence");
  const auto captured =
      std::find(header.begin(), header.end(), "captured_speaker_id");
  const bool has_captured = captured != header.end();
  const std::size_t captured_column =
      has_captured ? static_cast<std::size_t>(captured - header.begin()) : 0;

  SpeakerIdentitySnapshotInput result;
  result.has_captured_speaker_ids = has_captured;
  long previous_snapshot = -1;
  bool current_snapshot_is_empty = false;
  while (std::getline(input, line)) {
    if (line.empty()) continue;
    const auto values = SplitTab(line);
    if (values.size() != header.size()) {
      throw std::runtime_error("invalid snapshot TSV row width");
    }
    const long snapshot_index =
        ParseLong(values[snapshot_column], "snapshot_index");
    if (snapshot_index < 0) {
      throw std::runtime_error("snapshot_index must be nonnegative");
    }
    if (snapshot_index < previous_snapshot) {
      throw std::runtime_error("snapshot groups are not chronological");
    }
    if (snapshot_index != previous_snapshot) {
      result.snapshots.push_back({snapshot_index, {}, {}});
      previous_snapshot = snapshot_index;
      current_snapshot_is_empty = false;
    }
    auto& snapshot = result.snapshots.back();

    const bool empty_marker =
        values[start_column].empty() && values[end_column].empty() &&
        values[local_column].empty() && values[confidence_column].empty();
    const bool partially_empty =
        values[start_column].empty() || values[end_column].empty() ||
        values[local_column].empty() || values[confidence_column].empty();
    if (empty_marker) {
      if (current_snapshot_is_empty || !snapshot.segments.empty()) {
        throw std::runtime_error(
            "empty snapshot marker is duplicated or follows segment rows");
      }
      if (has_captured && !values[captured_column].empty()) {
        throw std::runtime_error("empty snapshot marker has captured identity");
      }
      current_snapshot_is_empty = true;
      continue;
    }
    if (partially_empty) {
      throw std::runtime_error("partially empty snapshot segment");
    }
    if (current_snapshot_is_empty) {
      throw std::runtime_error("segment follows empty snapshot marker");
    }

    core::DiarSegment segment;
    segment.start_sec = ParseDouble(values[start_column], "start_sec");
    segment.end_sec = ParseDouble(values[end_column], "end_sec");
    segment.local_speaker = ParseInt(values[local_column], "local_speaker");
    segment.confidence = ParseFloat(values[confidence_column], "confidence");
    if (segment.start_sec < 0.0 || segment.end_sec <= segment.start_sec ||
        segment.local_speaker < 0) {
      throw std::runtime_error("invalid snapshot segment interval");
    }
    if (!snapshot.segments.empty() &&
        SegmentOrderLess(segment, snapshot.segments.back())) {
      throw std::runtime_error("segments are not ordered within snapshot " +
                               std::to_string(snapshot_index));
    }
    snapshot.segments.push_back(std::move(segment));
    if (has_captured) {
      snapshot.captured_speaker_ids.push_back(values[captured_column]);
    }
  }

  if (result.snapshots.empty()) {
    throw std::runtime_error("snapshot TSV contains no snapshots");
  }
  for (const auto& snapshot : result.snapshots) {
    ValidateSnapshot(snapshot, has_captured);
  }
  return result;
}

SpeakerIdentityReplayResult ReplaySpeakerIdentitySnapshots(
    const SpeakerIdentitySnapshotInput& input,
    const SpeakerIdentitySnapshotProcessor& processor) {
  if (!processor) throw std::runtime_error("snapshot processor is empty");
  SpeakerIdentityReplayResult result;
  for (const auto& snapshot : input.snapshots) {
    ValidateSnapshot(snapshot, input.has_captured_speaker_ids);
    auto replayed = snapshot.segments;
    for (auto& segment : replayed) segment.speaker_id.clear();
    processor(&replayed);
    if (replayed.size() != snapshot.segments.size()) {
      throw std::runtime_error("snapshot processor changed segment count");
    }

    if (input.has_captured_speaker_ids) {
      for (std::size_t row = 0; row < replayed.size(); ++row) {
        auto& comparison = result.comparison;
        ++comparison.captured_rows;
        if (replayed[row].speaker_id == snapshot.captured_speaker_ids[row]) {
          ++comparison.equal_rows;
          continue;
        }
        ++comparison.different_rows;
        if (comparison.first_different_snapshot < 0) {
          comparison.first_different_snapshot = snapshot.snapshot_index;
          comparison.first_different_row = row;
          comparison.first_captured_speaker_id =
              snapshot.captured_speaker_ids[row];
          comparison.first_replayed_speaker_id = replayed[row].speaker_id;
        }
      }
    }
    ++result.processed_snapshots;
    result.final_segments = std::move(replayed);
  }
  return result;
}

}  // namespace orator::tools
