#include "pipeline/diarization_worker.h"

#include <algorithm>
#include <chrono>
#include <cmath>

#include "model/streaming_sortformer.h"
#include "pipeline/diar_postprocess.h"
#include "pipeline/worker_common.h"

namespace orator {
namespace pipeline {

using namespace worker;

// ── Post-processing: sliding-window median smoothing ─────────────────
// Smooths per-frame speaker probabilities to eliminate single-frame noise
// that causes fragmentation. Window of 11 frames (~0.9s at 0.08s/frame).
static void SmoothDiarProbs(std::vector<float>& probs, int frames, int n_spk,
                             int window = 11) {
  if (frames <= 1 || window <= 1) return;
  std::vector<float> result(probs.size());
  const int half = window / 2;
  for (int f = 0; f < frames; ++f) {
    for (int s = 0; s < n_spk; ++s) {
      std::vector<float> vals;
      vals.reserve(window);
      for (int w = -half; w <= half; ++w) {
        int fi = f + w;
        if (fi < 0) fi = 0;
        if (fi >= frames) fi = frames - 1;
        vals.push_back(probs[static_cast<size_t>(fi) * n_spk + s]);
      }
      std::sort(vals.begin(), vals.end());
      result[static_cast<size_t>(f) * n_spk + s] = vals[vals.size() / 2];
    }
  }
  probs.swap(result);
}

// ── Post-processing: minimum segment duration filter ─────────────────
// Removes segments shorter than min_dur_sec, merging their content
// into adjacent same-speaker segments.
static void FilterShortSegments(std::vector<core::DiarSegment>& segs,
                                 double min_dur_sec) {
  if (segs.empty() || min_dur_sec <= 0.0) return;
  std::vector<core::DiarSegment> out;
  out.reserve(segs.size());
  for (auto& s : segs) {
    if (s.end_sec - s.start_sec < min_dur_sec && !out.empty()) {
      // Merge short segment into the last output segment
      auto& prev = out.back();
      prev.end_sec = s.end_sec;
      prev.confidence = std::max(prev.confidence, s.confidence);
    } else {
      out.push_back(s);
    }
  }
  segs.swap(out);
}

// ── Post-processing: cross-speaker_-1 merge ─────────────────────────
// Merges same-speaker segments separated by a brief speaker_-1 gap.
static void MergeAcrossUnknown(std::vector<core::DiarSegment>& segs,
                                double gap_threshold_sec) {
  if (segs.size() < 3 || gap_threshold_sec <= 0.0) return;
  std::vector<core::DiarSegment> out;
  out.push_back(segs[0]);
  for (size_t i = 1; i < segs.size(); ++i) {
    auto& prev = out.back();
    const auto& cur = segs[i];
    // If current is speaker_-1 and gap is short, skip it
    if (cur.speaker_id.empty() && cur.local_speaker < 0 &&
        cur.end_sec - cur.start_sec < gap_threshold_sec) {
      prev.end_sec = cur.end_sec;
      continue;
    }
    // If same speaker as prev and gap was brief unknown, merge
    if (cur.speaker_id == prev.speaker_id &&
        cur.local_speaker == prev.local_speaker &&
        cur.start_sec - prev.end_sec < gap_threshold_sec) {
      prev.end_sec = cur.end_sec;
      prev.confidence = std::max(prev.confidence, cur.confidence);
    } else {
      out.push_back(cur);
    }
  }
  segs.swap(out);
}

DiarizationWorker::DiarizationWorker(core::IDiarizer* diarizer,
                                       Params params, core::TimeBase tb,
                                       cudaStream_t stream)
    : diarizer_(diarizer), params_(params),
      tb_(std::move(tb)), stream_(stream) {}

void DiarizationWorker::DeliverSpeakers(bool force) {
  if (!speaker_sink_) return;
  const long now = processed_samples_.load();
  if (!force) {
    const long min_gap =
        static_cast<long>(params_.deliver_interval_sec * tb_.sample_rate());
    if (now - last_deliver_sample_ < min_gap) return;
  }
  last_deliver_sample_ = now;
  // Derive the whole current speaker view from internally accumulated frames.
  // (StreamTimeline was removed — frames live in diar_probs_/diar_speakers_.)
  core::DiarizationFrames frames;
  frames.probs = diar_probs_;
  frames.num_frames = diar_speakers_ > 0
      ? static_cast<int>(diar_probs_.size() / diar_speakers_) : 0;
  frames.num_speakers = diar_speakers_;
  frames.frame_period_sec = diar_frame_period_sec_;
  if (frames.num_frames <= 0 || frames.num_speakers <= 0) return;
  // The diarization frame stream begins at the common-clock origin (absolute
  // sample 0). Set the segment time origin through this pipeline's common base.
  frames.t_start_sec = tb_.SecondsAt(0);
  auto segs = OnsetOffsetSegments(frames, params_.onset, params_.offset,
                                   params_.pad_onset, params_.pad_offset,
                                   params_.min_dur_on, params_.min_dur_off);
  speaker_sink_(segs);
}

void DiarizationWorker::ProcessSpan(const float* samples, int n) {
  if (samples == nullptr || n <= 0) return;
  // TODO: Temporary bridge — IDiarizer interface only exposes ProcessChunk().
  // StreamAudio() is SortformerDiarizer-specific. Update the interface
  // to eliminate this cast.
  auto* sd = dynamic_cast<model::SortformerDiarizer*>(diarizer_);
  const auto t0 = Clock::now();
  core::DiarizationFrames part =
      sd->StreamAudio(samples, n, false, stream_);
  compute_sec_ += Secs(t0, Clock::now());
  // Accumulate frames internally (replaces StreamTimeline::AppendDiarFrames).
  if (part.num_frames > 0 && part.num_speakers > 0) {
    if (diar_speakers_ == 0) {
      diar_speakers_ = part.num_speakers;
      diar_frame_period_sec_ = part.frame_period_sec;
    }
    diar_probs_.insert(diar_probs_.end(), part.probs.begin(), part.probs.end());
  }
  processed_samples_.fetch_add(n);
  DeliverSpeakers(/*force=*/false);
}

void DiarizationWorker::Finalize() {
  // TODO: Temporary bridge — IDiarizer interface only exposes ProcessChunk().
  // StreamAudio() is SortformerDiarizer-specific. Update the interface
  // to eliminate this cast.
  auto* sd = dynamic_cast<model::SortformerDiarizer*>(diarizer_);
  const auto t0 = Clock::now();
  core::DiarizationFrames tail =
      sd->StreamAudio(nullptr, 0, true, stream_);
  compute_sec_ += Secs(t0, Clock::now());
  if (tail.num_frames > 0 && tail.num_speakers > 0) {
    if (diar_speakers_ == 0) {
      diar_speakers_ = tail.num_speakers;
      diar_frame_period_sec_ = tail.frame_period_sec;
    }
    diar_probs_.insert(diar_probs_.end(), tail.probs.begin(), tail.probs.end());
  }
  DeliverSpeakers(/*force=*/true);
}

void DiarizationWorker::Reset() {
  diarizer_->Reset();
  diar_probs_.clear();
  diar_speakers_ = 0;
  diar_frame_period_sec_ = 0.0;
  processed_samples_.store(0);
  last_deliver_sample_ = 0;
  compute_sec_ = 0.0;
}

}  // namespace pipeline
}  // namespace orator
