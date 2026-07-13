// Structural business-speaker projection harness using a diarized reference.
//
// Usage: orator_eval <reference.txt> [num_preview_turns]
//
// What it does:
//   1. Parses the reference into utterances {time, speaker, text}.
//   2. Derives a ground-truth diarization track (speaker turns) AND an
//      ASR-style transcript that carries text+timing but NO speaker labels.
//   3. Deposits both into the runtime typed store and runs the registered
//      business-speaker policy used by the product.
//   4. Reports projection self-consistency and previews the produced timeline.
//
// The speaker evidence is derived from the reference itself, not from
// Sortformer output. Therefore the percentage below is a mechanical policy
// diagnostic only. It is not model accuracy and cannot satisfy the contextual
// semantic review required by the Test Review Protocol and Spec 013.

#include <iostream>
#include <string>
#include <unordered_map>

#include "io/reference_transcript.h"
#include "test_business_speaker_pipeline_access.h"

using orator::pipeline::TestBusinessSpeakerPipeline;

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

  // Deposit into the runtime evidence store through the business-pipeline test
  // adapter. Diarization provides who/when and ASR provides what/when.
  pipeline::TestBusinessSpeakerPipeline timeline;
  std::vector<pipeline::TestBusinessSpeakerPipeline::SpeakerInput> spk;
  spk.reserve(diar.size());
  for (const auto& d : diar) {
    pipeline::TestBusinessSpeakerPipeline::SpeakerInput s;
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

  // Projection self-consistency: each text entry's attributed speaker vs the
  // same reference that supplied the diarization evidence. Entries are keyed
  // by text_id = utterance index.
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

  std::cout << "=== Projection self-consistency (not model accuracy) ===\n";
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
