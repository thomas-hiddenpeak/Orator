#include "pipeline/diarization_worker.h"

#include <chrono>

#include "model/streaming_sortformer.h"
#include "pipeline/diar_postprocess.h"
#include "pipeline/worker_common.h"

namespace orator {
namespace pipeline {

using namespace worker;

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
  auto segs = FramesToSegments(frames, params_.threshold, params_.merge_gap_sec);
  segs = CoalesceSegments(std::move(segs), params_.merge_gap_sec);
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
