#include <cassert>
#include <iostream>

#include "pipeline/timeline_merger.h"

using namespace orator;

int main() {
  std::cout << "Testing timeline merger..." << std::endl;

  // Two diarization segments: alice [0,2], bob [2,4].
  std::vector<core::DiarSegment> diar;
  {
    core::DiarSegment s;
    s.start_sec = 0.0;
    s.end_sec = 2.0;
    s.local_speaker = 0;
    s.speaker_id = "alice";
    diar.push_back(s);
  }
  {
    core::DiarSegment s;
    s.start_sec = 2.0;
    s.end_sec = 4.0;
    s.local_speaker = 1;
    s.speaker_id = "bob";
    diar.push_back(s);
  }

  // ASR tokens straddling both speakers.
  core::Transcript asr;
  asr.tokens.push_back({0.0, 0.5, "hello"});
  asr.tokens.push_back({0.5, 1.5, "world"});
  asr.tokens.push_back({2.1, 2.6, "hi"});
  asr.tokens.push_back({2.6, 3.5, "there"});

  pipeline::OverlapTimelineMerger merger;
  core::Timeline tl = merger.Merge(diar, asr);

  assert(tl.segments.size() == 2);
  assert(tl.segments[0].speaker_id == "alice");
  assert(tl.segments[0].text == "hello world");
  assert(tl.segments[1].speaker_id == "bob");
  assert(tl.segments[1].text == "hi there");
  std::cout << "Speaker attribution and grouping correct" << std::endl;

  // Fallback labeling when speaker_id is empty -> "speaker_<local>".
  std::vector<core::DiarSegment> diar2;
  {
    core::DiarSegment s;
    s.start_sec = 0.0;
    s.end_sec = 2.0;
    s.local_speaker = 3;
    diar2.push_back(s);
  }
  core::Transcript asr2;
  asr2.tokens.push_back({0.1, 0.4, "x"});
  auto tl2 = merger.Merge(diar2, asr2);
  assert(tl2.segments.size() == 1);
  assert(tl2.segments[0].speaker_id == "speaker_3");
  std::cout << "Fallback labeling correct" << std::endl;

  std::cout << "\nAll timeline tests passed!" << std::endl;
  return 0;
}
