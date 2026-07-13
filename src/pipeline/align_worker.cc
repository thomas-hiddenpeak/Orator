#include "pipeline/align_worker.h"

#include <chrono>
#include <exception>
#include <utility>

#include "core/log.h"

namespace orator {
namespace pipeline {

AlignWorker::AlignWorker(core::IForcedAligner* aligner,
                         RetainedAudioBuffer* audio, const Params& params,
                         core::TimeBase tb)
    : aligner_(aligner), audio_(audio), params_(params), tb_(tb) {}

AlignWorker::~AlignWorker() { Stop(); }

void AlignWorker::Start() {
  if (running_) return;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stop_ = false;
  }
  running_ = true;
  thread_ = std::thread([this] { Loop(); });
}

void AlignWorker::Stop() {
  if (!running_) return;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stop_ = true;
  }
  cv_.notify_all();
  if (thread_.joinable()) thread_.join();
  running_ = false;
}

void AlignWorker::FinalizeExtent(long total_samples) {
  if (running_ || total_samples < 0) return;
  processed_samples_.store(total_samples);
}

void AlignWorker::Enqueue(long id, double start, double end,
                          const std::string& text) {
  if (text.empty()) return;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stop_) return;
    queue_.push_back(Job{id, start, end, text});
  }
  cv_.notify_one();
}

void AlignWorker::Loop() {
  for (;;) {
    Job job;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this] { return stop_ || !queue_.empty(); });
      if (queue_.empty()) return;  // stop_ requested and queue drained
      job = std::move(queue_.front());
      queue_.pop_front();
    }
    Process(job);
  }
}

void AlignWorker::Process(const Job& job) {
  if (job.end <= job.start) return;
  if (job.end - job.start > params_.max_segment_sec) return;  // safety cap

  const long s0 = tb_.SampleAt(job.start);
  const long s1 = tb_.SampleAt(job.end);
  if (s1 <= s0) return;

  std::vector<float> pcm = audio_->ReadSpan(s0, s1);
  if (pcm.empty()) return;  // audio trimmed from the window or not yet present

  const auto t0 = std::chrono::steady_clock::now();
  std::vector<core::AlignUnit> units;
  try {
    units = aligner_->Align(pcm.data(), static_cast<int>(pcm.size()), job.text,
                            params_.language);
  } catch (const std::exception& e) {
    // The aligner runs on the GPU and may fail (e.g. transient device error).
    // Per Art. III the pipeline must never crash the server: log and skip this
    // segment, leaving ASR/diar/VAD unaffected.
    LOG_WARN("[align] alignment failed for segment [%.2f-%.2f]: %s\n",
             job.start, job.end, e.what());
    return;
  }
  const auto t1 = std::chrono::steady_clock::now();
  compute_sec_.fetch_add(std::chrono::duration<double>(t1 - t0).count());

  // A unit's timestamp cannot lie outside its segment's audio. On mismatched or
  // hallucinated transcript text the aligner may emit labels beyond the span
  // length; clamp each unit to [0, span_dur] before mapping onto the common
  // clock so reported times always lie within the segment bounds.
  const double span_dur = static_cast<double>(pcm.size()) / params_.sample_rate;
  for (auto& u : units) {
    if (u.start_sec < 0.0) u.start_sec = 0.0;
    if (u.start_sec > span_dur) u.start_sec = span_dur;
    if (u.end_sec < u.start_sec) u.end_sec = u.start_sec;
    if (u.end_sec > span_dur) u.end_sec = span_dur;
    // Map unit times (relative to the span start) onto the common clock.
    u.start_sec += job.start;
    u.end_sec += job.start;
  }
  aligned_segments_.fetch_add(1);

  if (sink_) sink_(job.id, job.start, job.end, units);
}

}  // namespace pipeline
}  // namespace orator
