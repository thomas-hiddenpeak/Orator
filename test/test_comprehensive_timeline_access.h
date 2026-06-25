#pragma once

#include "pipeline/comprehensive_timeline.h"

namespace orator {
namespace pipeline {

struct TestComprehensiveTimeline : public ComprehensiveTimeline {
  using ComprehensiveTimeline::UpsertSpeaker;
  using ComprehensiveTimeline::ReplaceSpeakers;
  using ComprehensiveTimeline::UpsertText;
  using ComprehensiveTimeline::AddVad;
  using ComprehensiveTimeline::SpeakerInput;
};

}  // namespace pipeline
}  // namespace orator
