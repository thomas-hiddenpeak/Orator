#include "pipeline/diarization_worker.h"

#include <chrono>

#include "pipeline/diar_postprocess.h"
#include "core/time_base.h"

namespace orator {
namespace pipeline {

namespace {
using Clock = std::chrono::steady_clock;
double Secs(Clock::time_point a, Clock::time_point b) {
  return std::chrono::duration<double>(b - a).count();
}
}  // namespace

DiarizationWorker::DiarizationWorker(model::SortformerDiarizer* diarizer,
                                      StreamTimeline* timeline,
                                      Params params, cudaStream_t stream)
    : diarizer_(diarizer), timeline_(timeline), params_(params),
      stream_(stream) {}

void DiarizationWorker::DeliverSpeakers(bool force) {
  if (!speaker_sink_) return;
  const long now = processed_samples_.load();
  if (!force) {
    const long min_gap =
        static_cast<long>(params_.deliver_interval_sec * params_.sample_rate);
    if (now - last_deliver_sample_ < min_gap) return;
  }
  last_deliver_sample_ = now;
  // Derive the whole current speaker view from all accumulated frames (a global
  // derivation: boundaries shift as frames arrive), then deliver it.
  core::DiarizationFrames frames = timeline_->SnapshotDiarFrames();
  if (frames.num_frames <= 0 || frames.num_speakers <= 0) return;
  // The diarization frame stream begins at the common-clock origin (absolute
  // sample 0). Set the segment time origin explicitly through the common base
  // so diar segment times are on the same clock as every other pipeline.
  const core::TimeBase tb(params_.sample_rate, 0);
  frames.t_start_sec = tb.SecondsAt(0);
  auto segs = FramesToSegments(frames, params_.threshold, params_.merge_gap_sec);
  segs = CoalesceSegments(std::move(segs), params_.merge_gap_sec);
  speaker_sink_(segs);
}

void DiarizationWorker::ProcessSpan(const float* samples, int n) {
  if (samples == nullptr || n <= 0) return;
  const auto t0 = Clock::now();
  core::DiarizationFrames part =
      diarizer_->StreamAudio(samples, n, false, stream_);
  compute_sec_ += Secs(t0, Clock::now());
  timeline_->AppendDiarFrames(part);
  processed_samples_.fetch_add(n);
  DeliverSpeakers(/*force=*/false);
}

void DiarizationWorker::Finalize() {
  const auto t0 = Clock::now();
  core::DiarizationFrames tail =
      diarizer_->StreamAudio(nullptr, 0, true, stream_);
  compute_sec_ += Secs(t0, Clock::now());
  timeline_->AppendDiarFrames(tail);
  DeliverSpeakers(/*force=*/true);
}

void DiarizationWorker::Reset() {
  diarizer_->Reset();
  processed_samples_.store(0);
  last_deliver_sample_ = 0;
  compute_sec_ = 0.0;
}

}  // namespace pipeline
}  // namespace orator
