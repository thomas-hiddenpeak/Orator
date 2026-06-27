#pragma once

#include "pipeline/comprehensive_timeline.h"

namespace orator {
namespace pipeline {

struct TestComprehensiveTimeline : public ComprehensiveTimeline {
  using ComprehensiveTimeline::AddVad;
  using ComprehensiveTimeline::ReplaceSpeakers;
  using ComprehensiveTimeline::SpeakerInput;
  using ComprehensiveTimeline::UpsertAlign;
  using ComprehensiveTimeline::UpsertSpeaker;
  using ComprehensiveTimeline::UpsertText;
};

}  // namespace pipeline
}  // namespace orator
