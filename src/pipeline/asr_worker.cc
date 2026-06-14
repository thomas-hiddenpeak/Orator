#include "pipeline/asr_worker.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <mutex>

#include "gpu/gpu_lock.h"
#include "pipeline/json_util.h"

namespace orator {
namespace pipeline {

namespace {
using Clock = std::chrono::steady_clock;
double Secs(Clock::time_point a, Clock::time_point b) {
  return std::chrono::duration<double>(b - a).count();
}
}  // namespace

AsrWorker::AsrWorker(model::Qwen3Asr* asr, StreamTimeline* timeline,
                     const Params& params, Emit emit, cudaStream_t stream)
    : asr_(asr), timeline_(timeline), params_(params), emit_(std::move(emit)),
  vad_(params), stream_(stream) {}

void AsrWorker::ProcessSpan(const float* samples, int n) {
  if (samples == nullptr || n <= 0) return;
  vad_.Push(samples, n);
  DrainUtterances(/*finalize=*/false);
  processed_samples_.fetch_add(n);
}

void AsrWorker::Finalize() { DrainUtterances(/*finalize=*/true); }

void AsrWorker::DrainUtterances(bool finalize) {
  for (;;) {
    int begin = 0, end = 0, consume = 0;
    if (!vad_.NextSpan(finalize, &begin, &end, &consume)) return;
    EmitUtterance(begin, end);
    vad_.Consume(consume);
    if (finalize) return;
  }
}

void AsrWorker::EmitUtterance(int begin, int end) {
  if (end <= begin) return;
  const int sr = params_.sample_rate;
  const auto t0 = Clock::now();
  std::string text;
  {
    std::lock_guard<std::mutex> gpu(gpu::DeviceLock());
    text = asr_->TranscribeText(vad_.data() + begin, end - begin, stream_);
  }
  compute_sec_ += Secs(t0, Clock::now());
  if (text.empty()) return;

  core::AsrToken tok;
  tok.start_sec = static_cast<double>(vad_.base_sample() + begin) / sr;
  tok.end_sec = static_cast<double>(vad_.base_sample() + end) / sr;
  tok.text = text;
  timeline_->AppendToken(tok);

  if (emit_) {
    char buf[128];
    std::snprintf(buf, sizeof(buf),
                  "{\"type\":\"asr\",\"start\":%.3f,\"end\":%.3f,\"text\":\"",
                  tok.start_sec, tok.end_sec);
    emit_(std::string(buf) + JsonEscape(tok.text) + "\"}");
  }
}

void AsrWorker::Reset() {
  vad_.Reset();
  processed_samples_.store(0);
  compute_sec_ = 0.0;
  asr_->Reset();
}

}  // namespace pipeline
}  // namespace orator
