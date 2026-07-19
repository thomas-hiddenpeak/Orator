// Verifies strict snapshot parsing and one-call-per-snapshot replay mechanics
// for the offline speaker-identity evidence probe.

#include <cassert>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <unistd.h>

#include "tools/probes/speaker_identity_replay_input.h"

namespace {

class TemporaryFile {
 public:
  TemporaryFile(const std::string& suffix, const std::string& contents) {
    path_ =
        std::filesystem::temp_directory_path() /
        ("orator_speaker_replay_" + std::to_string(getpid()) + "_" + suffix);
    std::ofstream output(path_);
    assert(output);
    output << contents;
    assert(output.good());
  }

  ~TemporaryFile() { std::filesystem::remove(path_); }

  const std::string path() const { return path_.string(); }

 private:
  std::filesystem::path path_;
};

void ExpectReadFailure(const std::string& suffix, const std::string& contents) {
  TemporaryFile file(suffix, contents);
  bool failed = false;
  try {
    (void)orator::tools::ReadSpeakerIdentitySnapshots(file.path());
  } catch (const std::runtime_error&) {
    failed = true;
  }
  assert(failed);
}

void TestParseAndReplay() {
  TemporaryFile file(
      "valid.tsv",
      "snapshot_index\tstart_sec\tend_sec\tlocal_speaker\tconfidence\t"
      "captured_speaker_id\n"
      "4\t\t\t\t\t\n"
      "9\t0.100\t0.400\t0\t0.8\tspk_0\n"
      "9\t0.500\t0.700\t1\t0.7\t\n"
      "12\t0.100\t0.400\t0\t0.8\tspk_0\n");
  assert(orator::tools::IsSpeakerIdentitySnapshotInput(file.path()));
  const auto input = orator::tools::ReadSpeakerIdentitySnapshots(file.path());
  assert(input.has_captured_speaker_ids);
  assert(input.snapshots.size() == 3);
  assert(input.snapshots[0].segments.empty());
  assert(input.snapshots[1].segments.size() == 2);

  int calls = 0;
  const auto replay = orator::tools::ReplaySpeakerIdentitySnapshots(
      input, [&](std::vector<orator::core::DiarSegment>* segments) {
        ++calls;
        for (auto& segment : *segments) {
          assert(segment.speaker_id.empty());
          segment.speaker_id = segment.local_speaker == 0 ? "spk_0" : "spk_1";
        }
      });
  assert(calls == 3);
  assert(replay.processed_snapshots == 3);
  assert(replay.final_segments.size() == 1);
  assert(replay.comparison.captured_rows == 3);
  assert(replay.comparison.equal_rows == 2);
  assert(replay.comparison.different_rows == 1);
  assert(replay.comparison.first_different_snapshot == 9);
  assert(replay.comparison.first_different_row == 1);
  assert(replay.comparison.first_captured_speaker_id.empty());
  assert(replay.comparison.first_replayed_speaker_id == "spk_1");
}

void TestFinalSegmentInputDetection() {
  TemporaryFile file(
      "segments.csv",
      "start_sec,end_sec,local_speaker,confidence\n0.0,1.0,0,0.9\n");
  assert(!orator::tools::IsSpeakerIdentitySnapshotInput(file.path()));
}

void TestRejectsReturnedSnapshotGroup() {
  ExpectReadFailure(
      "returned.tsv",
      "snapshot_index\tstart_sec\tend_sec\tlocal_speaker\tconfidence\n"
      "1\t0.0\t0.1\t0\t0.9\n"
      "2\t0.1\t0.2\t0\t0.9\n"
      "1\t0.2\t0.3\t0\t0.9\n");
}

void TestRejectsUnorderedSegments() {
  ExpectReadFailure(
      "unordered.tsv",
      "snapshot_index\tstart_sec\tend_sec\tlocal_speaker\tconfidence\n"
      "1\t0.2\t0.3\t0\t0.9\n"
      "1\t0.1\t0.2\t0\t0.9\n");
}

void TestRejectsPartialEmptyMarker() {
  ExpectReadFailure(
      "partial.tsv",
      "snapshot_index\tstart_sec\tend_sec\tlocal_speaker\tconfidence\n"
      "1\t\t0.2\t0\t0.9\n");
}

void TestRejectsSegmentAfterEmptyMarker() {
  ExpectReadFailure(
      "mixed_empty.tsv",
      "snapshot_index\tstart_sec\tend_sec\tlocal_speaker\tconfidence\n"
      "1\t\t\t\t\n"
      "1\t0.1\t0.2\t0\t0.9\n");
}

}  // namespace

int main() {
  TestParseAndReplay();
  TestFinalSegmentInputDetection();
  TestRejectsReturnedSnapshotGroup();
  TestRejectsUnorderedSegments();
  TestRejectsPartialEmptyMarker();
  TestRejectsSegmentAfterEmptyMarker();
  return 0;
}
