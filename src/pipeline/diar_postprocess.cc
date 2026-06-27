#include "pipeline/diar_postprocess.h"

#include <algorithm>

namespace orator {
namespace pipeline {

using core::DiarizationFrames;
using core::DiarSegment;

std::vector<DiarSegment> FramesToSegments(const DiarizationFrames& frames,
                                          float threshold, double max_gap_sec) {
  std::vector<DiarSegment> segments;
  const double period = frames.frame_period_sec;

  for (int spk = 0; spk < frames.num_speakers; ++spk) {
    bool active = false;
    int run_start = 0;
    float prob_sum = 0.0f;

    auto close_run = [&](int run_end) {
      DiarSegment seg;
      seg.start_sec = frames.t_start_sec + run_start * period;
      seg.end_sec = frames.t_start_sec + run_end * period;
      seg.local_speaker = spk;
      const int count = run_end - run_start;
      seg.confidence = count > 0 ? prob_sum / count : 0.0f;
      segments.push_back(seg);
    };

    for (int f = 0; f < frames.num_frames; ++f) {
      const bool is_active = frames.At(f, spk) >= threshold;
      if (is_active) {
        if (!active) {
          active = true;
          run_start = f;
          prob_sum = 0.0f;
        }
        prob_sum += frames.At(f, spk);
      } else if (active) {
        active = false;
        close_run(f);
      }
    }
    if (active) {
      close_run(frames.num_frames);
    }
  }

  return CoalesceSegments(std::move(segments), max_gap_sec);
}

std::vector<DiarSegment> CoalesceSegments(std::vector<DiarSegment> segments,
                                          double max_gap_sec) {
  std::sort(segments.begin(), segments.end(),
            [](const DiarSegment& a, const DiarSegment& b) {
              if (a.local_speaker != b.local_speaker)
                return a.local_speaker < b.local_speaker;
              return a.start_sec < b.start_sec;
            });

  std::vector<DiarSegment> merged;
  for (auto& seg : segments) {
    if (!merged.empty()) {
      DiarSegment& last = merged.back();
      const bool same_speaker = last.local_speaker == seg.local_speaker;
      const bool contiguous = seg.start_sec - last.end_sec <= max_gap_sec;
      if (same_speaker && contiguous) {
        const double last_dur = last.end_sec - last.start_sec;
        const double seg_dur = seg.end_sec - seg.start_sec;
        const double total = last_dur + seg_dur;
        if (total > 0.0) {
          last.confidence = static_cast<float>(
              (last.confidence * last_dur + seg.confidence * seg_dur) / total);
        }
        last.end_sec = std::max(last.end_sec, seg.end_sec);
        continue;
      }
    }
    merged.push_back(seg);
  }

  std::sort(merged.begin(), merged.end(),
            [](const DiarSegment& a, const DiarSegment& b) {
              return a.start_sec < b.start_sec;
            });
  return merged;
}

std::vector<DiarSegment> OnsetOffsetSegments(const DiarizationFrames& frames,
                                             double onset, double offset,
                                             double pad_onset,
                                             double pad_offset,
                                             double min_dur_on,
                                             double min_dur_off) {
  std::vector<DiarSegment> segments;
  const double period = frames.frame_period_sec;
  // Onset/offset hysteresis (pyannote-style binarization): a speaker segment
  // STARTS when the probability rises to `onset` and only ENDS when it falls
  // below the lower `offset` threshold, so a segment is "sticky" across brief
  // dips (offset < onset). Using `onset` for both edges would chatter and
  // truncate segments early.
  for (int spk = 0; spk < frames.num_speakers; ++spk) {
    bool active = false;
    int seg_start = 0;

    for (int f = 0; f <= frames.num_frames; ++f) {
      float prob = (f < frames.num_frames) ? frames.At(f, spk) : 0.0f;

      if (!active && prob >= onset) {
        active = true;
        seg_start = f;
      } else if (active && prob < offset) {
        double dur = (f - seg_start) * period;
        if (dur >= min_dur_on) {
          DiarSegment seg;
          seg.start_sec = frames.t_start_sec + (seg_start * period) - pad_onset;
          seg.end_sec = frames.t_start_sec + (f * period) + pad_offset;
          seg.local_speaker = spk;
          seg.confidence = 0.0f;
          segments.push_back(seg);
        }
        active = false;
      }
    }
  }

  std::sort(segments.begin(), segments.end(),
            [](const DiarSegment& a, const DiarSegment& b) {
              if (a.local_speaker != b.local_speaker)
                return a.local_speaker < b.local_speaker;
              return a.start_sec < b.start_sec;
            });

  std::vector<DiarSegment> merged;
  for (auto& seg : segments) {
    if (!merged.empty()) {
      DiarSegment& last = merged.back();
      if (last.local_speaker == seg.local_speaker &&
          seg.start_sec - last.end_sec <= min_dur_off) {
        last.end_sec = seg.end_sec;
        continue;
      }
    }
    merged.push_back(seg);
  }

  std::sort(merged.begin(), merged.end(),
            [](const DiarSegment& a, const DiarSegment& b) {
              return a.start_sec < b.start_sec;
            });
  return merged;
}

}  // namespace pipeline
}  // namespace orator
