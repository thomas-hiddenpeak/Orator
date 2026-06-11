// Manual evaluation harness against a real diarized-transcript reference.
//
// Usage: orator_eval <reference.txt> [num_preview_turns]
//
// What it does (and why it's a real test of implemented code):
//   1. Parses the reference into utterances {time, speaker, text}.
//   2. Derives a ground-truth diarization track (speaker turns) AND an
//      ASR-style transcript that carries text+timing but NO speaker labels.
//   3. Runs the implemented OverlapTimelineMerger to RE-ATTRIBUTE each
//      utterance's speaker purely from diarization overlap.
//   4. Reports attribution accuracy vs the reference and previews the produced
//      timeline so it can be compared by hand against the reference file.
//
// This exercises the diarization->timeline fusion on real 4-speaker data even
// though the neural Sortformer weights cannot be loaded in this environment.

#include <iostream>
#include <string>
#include <unordered_map>

#include "io/reference_transcript.h"
#include "pipeline/timeline_merger.h"

using namespace orator;

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <reference.txt> [preview]\n";
    return 2;
  }
  const std::string path = argv[1];
  const int preview = argc >= 3 ? std::atoi(argv[2]) : 15;

  io::ReferenceTranscript ref = io::ParseReferenceTranscript(path);
  const auto speakers = ref.Speakers();

  double total_dur = 0.0;
  if (!ref.utterances.empty()) {
    total_dur = ref.utterances.back().start_sec;
  }

  std::cout << "=== Reference summary ===\n";
  std::cout << "Utterances: " << ref.utterances.size() << "\n";
  std::cout << "Speakers (" << speakers.size() << "):";
  for (const auto& s : speakers) std::cout << " " << s;
  std::cout << "\n";
  std::cout << "Last timestamp: " << total_dur << " s ("
            << (total_dur / 60.0) << " min)\n\n";

  // Ground-truth diarization + ASR-style transcript (no speaker labels).
  auto diar = ref.ToDiarSegments(5.0);
  auto transcript = ref.ToTranscript(5.0);

  // Fuse: the merger must recover speakers from diarization overlap alone.
  pipeline::OverlapTimelineMerger merger;
  core::Timeline timeline = merger.Merge(diar, transcript);

  // Attribution accuracy: re-attribute each utterance independently and compare
  // to the reference speaker.
  int correct = 0;
  std::unordered_map<std::string, int> per_speaker_total, per_speaker_correct;
  for (size_t i = 0; i < ref.utterances.size(); ++i) {
    core::Transcript one;
    one.tokens.push_back(transcript.tokens[i]);
    core::Timeline t = merger.Merge(diar, one);
    const std::string assigned =
        t.segments.empty() ? "" : t.segments.front().speaker_id;
    const std::string truth = ref.utterances[i].speaker;
    per_speaker_total[truth]++;
    if (assigned == truth) {
      ++correct;
      per_speaker_correct[truth]++;
    }
  }
  const double acc =
      ref.utterances.empty()
          ? 0.0
          : 100.0 * correct / static_cast<double>(ref.utterances.size());

  std::cout << "=== Timeline-fusion attribution (vs reference) ===\n";
  std::cout << "Overall: " << correct << "/" << ref.utterances.size() << " = "
            << acc << "%\n";
  for (const auto& s : speakers) {
    std::cout << "  " << s << ": " << per_speaker_correct[s] << "/"
              << per_speaker_total[s] << "\n";
  }
  std::cout << "\n";

  std::cout << "=== Produced timeline preview (first " << preview
            << " turns) ===\n";
  for (int i = 0; i < preview && i < static_cast<int>(timeline.segments.size());
       ++i) {
    const auto& seg = timeline.segments[i];
    std::string text = seg.text;
    if (text.size() > 48) text = text.substr(0, 48) + "...";
    std::printf("[%6.1f-%6.1f] %-10s %s\n", seg.start_sec, seg.end_sec,
                seg.speaker_id.c_str(), text.c_str());
  }
  std::cout << "\nTotal timeline turns: " << timeline.segments.size() << "\n";
  return 0;
}
