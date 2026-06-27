#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>

#include "io/reference_transcript.h"

using namespace orator;

int main() {
  std::cout << "Testing reference transcript parser..." << std::endl;

  // Timestamp parsing.
  assert(io::ParseTimestamp("00:00:03") == 3.0);
  assert(io::ParseTimestamp("00:01:05") == 65.0);
  assert(io::ParseTimestamp("01:02:03") == 3723.0);
  assert(io::ParseTimestamp("bad") < 0);
  std::cout << "Timestamp parsing OK" << std::endl;

  // Write a small reference file mirroring asrTest2Final.txt format.
  const char* path = "/tmp/orator_ref.txt";
  {
    std::ofstream f(path);
    f << "\n";
    f << "00:00:03 Alice\n";
    f << "Hello there everyone.\n";
    f << "00:00:10 Bob\n";
    f << "Hi Alice, good to see you.\n";
    f << "It has been a while.\n";  // continuation line
    f << "00:00:20 Alice\n";
    f << "Indeed it has.\n";
  }

  auto ref = io::ParseReferenceTranscript(path);
  assert(ref.utterances.size() == 3);
  assert(ref.utterances[0].speaker == "Alice");
  assert(ref.utterances[0].start_sec == 3.0);
  assert(ref.utterances[1].speaker == "Bob");
  // Continuation line merged.
  assert(ref.utterances[1].text ==
         "Hi Alice, good to see you. It has been a while.");
  std::cout << "Utterance parsing + continuation OK" << std::endl;

  auto speakers = ref.Speakers();
  assert(speakers.size() == 2);
  assert(speakers[0] == "Alice");
  assert(speakers[1] == "Bob");
  std::cout << "Speaker extraction OK" << std::endl;

  auto diar = ref.ToDiarSegments(5.0);
  assert(diar.size() == 3);
  // First segment spans until second utterance start.
  assert(diar[0].start_sec == 3.0 && diar[0].end_sec == 10.0);
  assert(diar[1].start_sec == 10.0 && diar[1].end_sec == 20.0);
  assert(diar[2].end_sec == 25.0);  // last + tail
  assert(diar[0].speaker_id == "Alice");
  std::cout << "Ground-truth diarization segments OK" << std::endl;

  auto tr = ref.ToTranscript(5.0);
  assert(tr.tokens.size() == 3);
  // Transcript carries text + timing but NO speaker label.
  assert(tr.tokens[0].text == "Hello there everyone.");
  std::cout << "ASR-style transcript OK" << std::endl;

  std::remove(path);
  std::cout << "\nAll reference parser tests passed!" << std::endl;
  return 0;
}
