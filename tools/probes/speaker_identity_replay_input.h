// Parses and replays capture-ordered speaker-identity snapshots for the
// offline identity evidence probe. This module never evaluates speaker
// correctness; captured identity equality is only a producer contract.

#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "core/types.h"

namespace orator::tools {

struct SpeakerIdentityReplaySnapshot {
  long snapshot_index = -1;
  std::vector<core::DiarSegment> segments;
  std::vector<std::string> captured_speaker_ids;
};

struct SpeakerIdentitySnapshotInput {
  bool has_captured_speaker_ids = false;
  std::vector<SpeakerIdentityReplaySnapshot> snapshots;
};

struct SpeakerIdentityReplayComparison {
  std::size_t captured_rows = 0;
  std::size_t equal_rows = 0;
  std::size_t different_rows = 0;
  long first_different_snapshot = -1;
  std::size_t first_different_row = 0;
  std::string first_captured_speaker_id;
  std::string first_replayed_speaker_id;
};

struct SpeakerIdentityReplayResult {
  std::size_t processed_snapshots = 0;
  std::vector<core::DiarSegment> final_segments;
  SpeakerIdentityReplayComparison comparison;
};

using SpeakerIdentitySnapshotProcessor =
    std::function<void(std::vector<core::DiarSegment>*)>;

bool IsSpeakerIdentitySnapshotInput(const std::string& path);

SpeakerIdentitySnapshotInput ReadSpeakerIdentitySnapshots(
    const std::string& path);

SpeakerIdentityReplayResult ReplaySpeakerIdentitySnapshots(
    const SpeakerIdentitySnapshotInput& input,
    const SpeakerIdentitySnapshotProcessor& processor);

}  // namespace orator::tools
