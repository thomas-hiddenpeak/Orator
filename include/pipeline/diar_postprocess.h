#pragma once

// Converts frame-level diarization probabilities into speaker segments.

#include <vector>

#include "core/types.h"

namespace orator {
namespace pipeline {

// Threshold + run-length merge per speaker. Overlapping speech is preserved
// (a frame may be active for multiple speakers). Adjacent active frames for the
// same speaker, separated by at most max_gap_sec of inactivity, are merged.
std::vector<core::DiarSegment> FramesToSegments(const core::DiarizationFrames& frames,
                                                float threshold,
                                                double max_gap_sec = 0.0);

// Merges segments of the same local speaker that are separated by a small gap,
// across chunk boundaries. Input need not be sorted; output is sorted by start.
std::vector<core::DiarSegment> CoalesceSegments(std::vector<core::DiarSegment> segments,
                                                double max_gap_sec);

}  // namespace pipeline
}  // namespace orator
