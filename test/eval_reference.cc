// Manual evaluation harness against a real diarized-transcript reference.
//
// Usage: orator_eval <reference.txt> [num_preview_turns]
//
// What it does (and why it's a real test of implemented code):
//   1. Parses the reference into utterances {time, speaker, text}.
//   2. Derives a ground-truth diarization track (speaker turns) AND an
//      ASR-style transcript that carries text+timing but NO speaker labels.
//   3. Deposits both into the runtime ComprehensiveTimeline (Spec 004), which
//      re-attributes each text segment to a speaker purely by time overlap.
//   4. Reports attribution accuracy vs the reference and previews the produced
//      timeline so it can be compared by hand against the reference file.
//
// This exercises the runtime diarization->timeline alignment on real 4-speaker
// data even though the neural Sortformer weights cannot be loaded in this
// environment.

#include <iostream>
#include <string>
#include <unordered_map>

#include "io/reference_transcript.h"
#include "test_comprehensive_timeline_access.h"

using orator::pipeline::TestComprehensiveTimeline;

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
  std::cout << "Last timestamp: " << total_dur << " s (" << (total_dur / 60.0)
            << " min)\n\n";

  // Ground-truth diarization + ASR-style transcript (no speaker labels).
  auto diar = ref.ToDiarSegments(5.0);
  auto transcript = ref.ToTranscript(5.0);

  // Deposit into the runtime ComprehensiveTimeline. Diarization provides the
  // speaker set (who/when); ASR provides text (what/when); the timeline
  // attributes each text segment to a speaker purely by time overlap.
  pipeline::TestComprehensiveTimeline timeline;
  std::vector<pipeline::TestComprehensiveTimeline::SpeakerInput> spk;
  spk.reserve(diar.size());
  for (const auto& d : diar) {
    pipeline::TestComprehensiveTimeline::SpeakerInput s;
    s.start = d.start_sec;
    s.end = d.end_sec;
    s.speaker = d.speaker_id.empty()
                    ? ("speaker_" + std::to_string(d.local_speaker))
                    : d.speaker_id;
    s.conf = 1.0f;
    spk.push_back(s);
  }
  timeline.ReplaceSpeakers(spk);
  for (size_t i = 0; i < transcript.tokens.size(); ++i) {
    const auto& tok = transcript.tokens[i];
    timeline.UpsertText(static_cast<long>(i), tok.start_sec, tok.end_sec,
                        tok.text);
  }
  std::vector<pipeline::ComprehensiveTimeline::Entry> entries =
      timeline.Snapshot();

  // Attribution accuracy: each text entry's attributed speaker (by time
  // overlap) vs the reference speaker. Entries are keyed by text_id = utterance
  // index.
  int correct = 0;
  std::unordered_map<std::string, int> per_speaker_total, per_speaker_correct;
  std::unordered_map<long, std::string> attributed;
  for (const auto& e : entries) attributed[e.text_id] = e.speaker;
  for (size_t i = 0; i < ref.utterances.size(); ++i) {
    auto it = attributed.find(static_cast<long>(i));
    const std::string assigned = it == attributed.end() ? "" : it->second;
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

  std::cout << "=== Timeline-alignment attribution (vs reference) ===\n";
  std::cout << "Overall: " << correct << "/" << ref.utterances.size() << " = "
            << acc << "%\n";
  for (const auto& s : speakers) {
    std::cout << "  " << s << ": " << per_speaker_correct[s] << "/"
              << per_speaker_total[s] << "\n";
  }
  std::cout << "\n";

  std::cout << "=== Produced timeline preview (first " << preview
            << " entries) ===\n";
  for (int i = 0; i < preview && i < static_cast<int>(entries.size()); ++i) {
    const auto& e = entries[i];
    std::string text = e.text;
    if (text.size() > 48) text = text.substr(0, 48) + "...";
    std::printf("[%6.1f-%6.1f] %-10s %s\n", e.start, e.end, e.speaker.c_str(),
                text.c_str());
  }
  std::cout << "\nTotal timeline entries: " << entries.size() << "\n";
  return 0;
}
