#include "pipeline/timeline_merger.h"

#include <algorithm>
#include <string>

namespace orator {
namespace pipeline {

using core::AsrToken;
using core::DiarSegment;
using core::Timeline;
using core::TimelineSegment;
using core::Transcript;

namespace {

double Overlap(double a0, double a1, double b0, double b1) {
  return std::max(0.0, std::min(a1, b1) - std::max(a0, b0));
}

// Resolve the best speaker label for a token's time span.
std::string LabelForToken(const AsrToken& token,
                          const std::vector<DiarSegment>& diar) {
  double best_overlap = 0.0;
  const DiarSegment* best = nullptr;
  for (const auto& seg : diar) {
    const double ov =
        Overlap(token.start_sec, token.end_sec, seg.start_sec, seg.end_sec);
    if (ov > best_overlap) {
      best_overlap = ov;
      best = &seg;
    }
  }
  if (best == nullptr) {
    // No overlap: fall back to the nearest segment by midpoint distance.
    const double mid = 0.5 * (token.start_sec + token.end_sec);
    double best_dist = -1.0;
    for (const auto& seg : diar) {
      const double seg_mid = 0.5 * (seg.start_sec + seg.end_sec);
      const double dist = std::abs(seg_mid - mid);
      if (best_dist < 0.0 || dist < best_dist) {
        best_dist = dist;
        best = &seg;
      }
    }
  }
  if (best == nullptr) return "unknown";
  return best->speaker_id.empty()
             ? ("speaker_" + std::to_string(best->local_speaker))
             : best->speaker_id;
}

}  // namespace

Timeline OverlapTimelineMerger::Merge(const std::vector<DiarSegment>& diarization,
                                      const Transcript& transcript) const {
  Timeline timeline;

  for (const auto& token : transcript.tokens) {
    const std::string label = LabelForToken(token, diarization);

    if (!timeline.segments.empty() &&
        timeline.segments.back().speaker_id == label) {
      TimelineSegment& seg = timeline.segments.back();
      seg.end_sec = token.end_sec;
      if (!token.text.empty()) {
        if (!seg.text.empty()) seg.text += " ";
        seg.text += token.text;
      }
    } else {
      TimelineSegment seg;
      seg.start_sec = token.start_sec;
      seg.end_sec = token.end_sec;
      seg.speaker_id = label;
      seg.text = token.text;
      timeline.segments.push_back(std::move(seg));
    }
  }

  return timeline;
}

}  // namespace pipeline
}  // namespace orator
