#pragma once

#include "pipeline/comprehensive_timeline.h"

namespace orator {
namespace pipeline {

struct TestComprehensiveTimeline : public ComprehensiveTimeline {
  using ComprehensiveTimeline::SpeakerInput;

  std::vector<Revision> UpsertSpeaker(double start, double end,
                                      const std::string& speaker, float conf) {
    return DepositDiarizationSegment({start, end, speaker, conf, ""});
  }

  std::vector<Revision> ReplaceSpeakers(
      const std::vector<SpeakerInput>& segments) {
    return DepositDiarization(segments);
  }

  std::vector<Revision> UpsertText(long id, double start, double end,
                                   const std::string& text) {
    return DepositAsrFinal({id, start, end, text});
  }

  void AddVad(double start, double end) { DepositVad({start, end}); }

  std::vector<Revision> UpsertAlign(long text_id, double start, double end,
                                    const std::vector<AlignUnitSeg>& units) {
    return DepositAlignment({text_id, start, end, units});
  }
};

}  // namespace pipeline
}  // namespace orator
