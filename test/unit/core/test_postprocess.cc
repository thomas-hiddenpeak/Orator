#include <cassert>
#include <cmath>
#include <iostream>

#include "pipeline/diar_postprocess.h"

using namespace orator;

static core::DiarizationFrames MakeFrames(int num_frames, int num_speakers,
                                          double period) {
  core::DiarizationFrames f;
  f.num_frames = num_frames;
  f.num_speakers = num_speakers;
  f.frame_period_sec = period;
  f.t_start_sec = 0.0;
  f.probs.assign(static_cast<size_t>(num_frames) * num_speakers, 0.0f);
  return f;
}

int main() {
  std::cout << "Testing diarization post-processing..." << std::endl;

  // 10 frames, 2 speakers, 0.1s/frame.
  auto frames = MakeFrames(10, 2, 0.1);
  // Speaker 0 active frames 0..4, speaker 1 active frames 6..9.
  for (int f = 0; f < 5; ++f) frames.probs[f * 2 + 0] = 0.9f;
  for (int f = 6; f < 10; ++f) frames.probs[f * 2 + 1] = 0.8f;

  auto segs = pipeline::FramesToSegments(frames, 0.5f, 0.0);
  assert(segs.size() == 2);
  std::cout << "Got " << segs.size() << " segments" << std::endl;

  // Sorted by start: first is speaker 0 [0.0, 0.5], second speaker 1
  // [0.6, 1.0].
  assert(segs[0].local_speaker == 0);
  assert(std::abs(segs[0].start_sec - 0.0) < 1e-6);
  assert(std::abs(segs[0].end_sec - 0.5) < 1e-6);
  assert(segs[1].local_speaker == 1);
  assert(std::abs(segs[1].start_sec - 0.6) < 1e-6);
  assert(std::abs(segs[1].end_sec - 1.0) < 1e-6);
  std::cout << "Segment boundaries correct" << std::endl;

  // Gap bridging: speaker 0 active 0..1 and 3..4 with a 1-frame gap.
  auto gap = MakeFrames(5, 1, 0.1);
  gap.probs[0] = 0.9f;
  gap.probs[1] = 0.9f;
  gap.probs[3] = 0.9f;
  gap.probs[4] = 0.9f;
  auto no_merge = pipeline::FramesToSegments(gap, 0.5f, 0.0);
  assert(no_merge.size() == 2);
  auto merged = pipeline::FramesToSegments(gap, 0.5f, 0.15);
  assert(merged.size() == 1);
  std::cout << "Gap bridging correct" << std::endl;

  // Overlap: both speakers active simultaneously => 2 overlapping segments.
  auto ov = MakeFrames(4, 2, 0.1);
  for (int f = 0; f < 4; ++f) {
    ov.probs[f * 2 + 0] = 0.9f;
    ov.probs[f * 2 + 1] = 0.9f;
  }
  auto ov_segs = pipeline::FramesToSegments(ov, 0.5f, 0.0);
  assert(ov_segs.size() == 2);
  std::cout << "Overlap preserved" << std::endl;

  std::cout << "\nAll post-processing tests passed!" << std::endl;
  return 0;
}
