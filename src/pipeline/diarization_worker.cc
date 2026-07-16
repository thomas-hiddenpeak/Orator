#include "pipeline/diarization_worker.h"

#include <algorithm>
#include <chrono>
#include <cmath>

#include "pipeline/diar_postprocess.h"
#include "pipeline/worker_common.h"

namespace orator {
namespace pipeline {

using namespace worker;

DiarizationWorker::DiarizationWorker(core::IDiarizer* diarizer, Params params,
                                     core::TimeBase tb, cudaStream_t stream)
    : diarizer_(diarizer), params_(params), tb_(tb), stream_(stream) {}

void DiarizationWorker::DeliverSpeakers(bool force) {
  if (!speaker_sink_) return;
  const long now = processed_samples_.load();
  if (!force) {
    const long min_gap =
        static_cast<long>(params_.deliver_interval_sec * tb_.sample_rate());
    if (now - last_deliver_sample_ < min_gap) return;
  }
  last_deliver_sample_ = now;
  if (diar_speakers_ <= 0) return;
  const int total_frames =
      static_cast<int>(diar_probs_.size() / diar_speakers_);
  if (total_frames <= 0) return;

  // Derive segments PER diarizer session (Spec 010). Each session's slots are
  // independent (the spkcache was reset), so a slot is offset by
  // session_index * num_speakers and the session's times are anchored to its
  // absolute start. Identity is stitched across sessions by the voiceprint
  // stage, which matches each (now distinct) local slot to the global gallery.
  std::vector<core::DiarSegment> segs;
  for (size_t k = 0; k < session_bounds_.size(); ++k) {
    const int f0 = session_bounds_[k].first;
    const int f1 = (k + 1 < session_bounds_.size())
                       ? session_bounds_[k + 1].first
                       : total_frames;
    if (f1 <= f0) continue;
    core::DiarizationFrames frames;
    frames.probs.assign(
        diar_probs_.begin() + static_cast<size_t>(f0) * diar_speakers_,
        diar_probs_.begin() + static_cast<size_t>(f1) * diar_speakers_);
    frames.num_frames = f1 - f0;
    frames.num_speakers = diar_speakers_;
    frames.frame_period_sec = diar_frame_period_sec_;
    frames.t_start_sec = session_bounds_[k].second;
    auto ss = OnsetOffsetSegments(frames, params_.onset, params_.offset,
                                  params_.pad_onset, params_.pad_offset,
                                  params_.min_dur_on, params_.min_dur_off);
    const int offset = static_cast<int>(k) * diar_speakers_;
    for (auto& s : ss) {
      s.local_speaker += offset;
      segs.push_back(s);
    }
  }
  // Spec 010: resolve global voiceprint identities on the segment view before
  // delivery (no-op when speaker identity is disabled).
  if (segment_processor_) segment_processor_(segs);
  speaker_sink_(segs);
}

void DiarizationWorker::ProcessSpan(const float* samples, int n) {
  if (samples == nullptr || n <= 0) return;
  const auto t0 = Clock::now();
  core::DiarizationFrames part =
      diarizer_->StreamAudio(samples, n, false, stream_);
  compute_sec_.fetch_add(Secs(t0, Clock::now()), std::memory_order_relaxed);
  // Accumulate frames internally (replaces StreamTimeline::AppendDiarFrames).
  if (part.num_frames > 0 && part.num_speakers > 0) {
    if (diar_speakers_ == 0) {
      diar_speakers_ = part.num_speakers;
      diar_frame_period_sec_ = part.frame_period_sec;
    }
    const int session_start_frame = session_bounds_.back().first;
    const int existing_frames =
        static_cast<int>(diar_probs_.size() / diar_speakers_);
    part.t_start_sec =
        session_bounds_.back().second +
        (existing_frames - session_start_frame) * part.frame_period_sec;
    if (frame_sink_) {
      const int local_offset =
          static_cast<int>(session_bounds_.size() - 1) * part.num_speakers;
      frame_sink_(part, local_offset);
    }
    diar_probs_.insert(diar_probs_.end(), part.probs.begin(), part.probs.end());
  }
  processed_samples_.fetch_add(n);

  // Spec 010: periodically reset the diarizer's streaming state so each window
  // stays in the model's accurate regime. Flush the buffered tail first (no
  // audio is dropped), record the new session boundary (frame index + absolute
  // start), then reset; the voiceprint stage stitches the new slots back to
  // stable global identities.
  if (params_.reset_period_sec > 0.0) {
    const long elapsed = processed_samples_.load() - session_start_sample_;
    if (elapsed >= static_cast<long>(params_.reset_period_sec *
                                     tb_.sample_rate())) {
      core::DiarizationFrames tail =
          diarizer_->StreamAudio(nullptr, 0, true, stream_);
      if (tail.num_frames > 0 && diar_speakers_ > 0) {
        const int session_start_frame = session_bounds_.back().first;
        const int existing_frames =
            static_cast<int>(diar_probs_.size() / diar_speakers_);
        tail.t_start_sec =
            session_bounds_.back().second +
            (existing_frames - session_start_frame) *
                tail.frame_period_sec;
        if (frame_sink_) {
          const int local_offset =
              static_cast<int>(session_bounds_.size() - 1) *
              tail.num_speakers;
          frame_sink_(tail, local_offset);
        }
        diar_probs_.insert(diar_probs_.end(), tail.probs.begin(),
                           tail.probs.end());
      }
      diarizer_->Reset();
      const int frame_count =
          diar_speakers_ > 0
              ? static_cast<int>(diar_probs_.size() / diar_speakers_)
              : 0;
      session_bounds_.emplace_back(
          frame_count, tb_.SecondsAt(processed_samples_.load()));
      session_start_sample_ = processed_samples_.load();
    }
  }
  DeliverSpeakers(/*force=*/false);
}

void DiarizationWorker::Finalize() {
  const auto t0 = Clock::now();
  core::DiarizationFrames tail =
      diarizer_->StreamAudio(nullptr, 0, true, stream_);
  compute_sec_.fetch_add(Secs(t0, Clock::now()), std::memory_order_relaxed);
  if (tail.num_frames > 0 && tail.num_speakers > 0) {
    if (diar_speakers_ == 0) {
      diar_speakers_ = tail.num_speakers;
      diar_frame_period_sec_ = tail.frame_period_sec;
    }
    const int session_start_frame = session_bounds_.back().first;
    const int existing_frames =
        static_cast<int>(diar_probs_.size() / diar_speakers_);
    tail.t_start_sec =
        session_bounds_.back().second +
        (existing_frames - session_start_frame) * tail.frame_period_sec;
    if (frame_sink_) {
      const int local_offset =
          static_cast<int>(session_bounds_.size() - 1) * tail.num_speakers;
      frame_sink_(tail, local_offset);
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
  compute_sec_.store(0.0, std::memory_order_relaxed);
  session_bounds_.assign(1, {0, 0.0});
  session_start_sample_ = 0;
}

}  // namespace pipeline
}  // namespace orator
