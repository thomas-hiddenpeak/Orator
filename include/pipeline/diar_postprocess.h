#pragma once

// Converts frame-level diarization probabilities into speaker segments.

#include <vector>

#include "core/types.h"

namespace orator {
namespace pipeline {

// Threshold + run-length merge per speaker. Overlapping speech is preserved
// (a frame may be active for multiple speakers). Adjacent active frames for the
// same speaker, separated by at most max_gap_sec of inactivity, are merged.
std::vector<core::DiarSegment> FramesToSegments(
    const core::DiarizationFrames& frames, float threshold,
    double max_gap_sec = 0.0);

// Merges segments of the same local speaker that are separated by a small gap,
// across chunk boundaries. Input need not be sorted; output is sorted by start.
std::vector<core::DiarSegment> CoalesceSegments(
    std::vector<core::DiarSegment> segments, double max_gap_sec);

// NeMo-style onset/offset double-threshold post-processing.
// onset: probability must exceed this to start a segment (default 0.56)
// offset: probability must drop below this to end a segment (default 1.0).
//         offset > onset prevents fragmentation by requiring strong evidence to
//         end.
// pad_onset: extra time added before each segment start (default 0.063s)
// pad_offset: extra time added after each segment end (default 0.002s)
// min_dur_on: minimum speech segment duration (default 0.007s, removes noise
// splatter) min_dur_off: minimum silence duration (default 0.15s, merges brief
// pauses)
std::vector<core::DiarSegment> OnsetOffsetSegments(
    const core::DiarizationFrames& frames, double onset, double offset,
    double pad_onset, double pad_offset, double min_dur_on, double min_dur_off);

}  // namespace pipeline
}  // namespace orator
