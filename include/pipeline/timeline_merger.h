#pragma once

// Default timeline merger: attributes ASR tokens to diarization speakers.

#include "core/stages.h"

namespace orator {
namespace pipeline {

// Assigns each ASR token to the diarization speaker with the greatest temporal
// overlap, then groups consecutive same-speaker tokens into timeline segments.
class OverlapTimelineMerger final : public core::ITimelineMerger {
 public:
  core::Timeline Merge(const std::vector<core::DiarSegment>& diarization,
                       const core::Transcript& transcript) const override;
};

}  // namespace pipeline
}  // namespace orator
