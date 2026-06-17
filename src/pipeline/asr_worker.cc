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
      tb_(params.sample_rate, 0), stream_(stream) {
}

void AsrWorker::ProcessSpan(const float* samples, int n) {
  if (samples == nullptr || n <= 0) return;
  ProcessIncremental(samples, n, /*finalize=*/false);
  processed_samples_.fetch_add(n);
}

void AsrWorker::Finalize() {
  ProcessIncremental(nullptr, 0, /*finalize=*/true);
}

// Incremental KV-cache path: continuous audio is fed into the engine's
// streaming session, which carries KV context across its 8 s windows. The
// segment is committed (one timeline token) and the session reset at the
// segment cap or on finalize.
void AsrWorker::ProcessIncremental(const float* samples, int n, bool finalize) {
  const int sr = params_.sample_rate;
  const long seg_cap = std::max<long>(1, static_cast<long>(params_.segment_sec * sr));

  int off = 0;
  while (off < n) {
    if (inc_in_segment_) {
      if (inc_seg_samples_ >= seg_cap) {
        EmitIncrementalChunk(nullptr, 0, /*finalize=*/true);
        continue;
      }
      const int take =
          static_cast<int>(std::min<long>(seg_cap - inc_seg_samples_, n - off));
      EmitIncrementalChunk(samples + off, take, /*finalize=*/false);
      off += take;
    } else {
      const int take = static_cast<int>(std::min<long>(seg_cap, n - off));
      EmitIncrementalChunk(samples + off, take, /*finalize=*/false);
      off += take;
    }
  }
  if (finalize) EmitIncrementalChunk(nullptr, 0, /*finalize=*/true);
}

void AsrWorker::EmitIncrementalChunk(const float* samples, int n, bool finalize) {
  const int sr = params_.sample_rate;
  const long seg_cap = std::max<long>(1, static_cast<long>(params_.segment_sec * sr));

  const auto t0 = Clock::now();
  {
    gpu::DeviceGuard gpu(/*own_stream=*/true);
    if (n > 0) {
      if (!inc_in_segment_) {
        inc_seg_start_sample_ = inc_abs_pos_;
        inc_seg_samples_ = 0;
        inc_live_text_.clear();
        asr_->StreamReset(inc_seg_start_sample_);
        inc_in_segment_ = true;
      }
      inc_live_text_ = asr_->StreamChunk(samples, n, stream_);
      inc_seg_samples_ += n;
      inc_abs_pos_ += n;
      inc_seg_end_sample_ = inc_abs_pos_;
    }
    const bool hit_cap = inc_seg_samples_ >= seg_cap;
    if (inc_in_segment_ && (finalize || hit_cap)) {
      inc_live_text_ = asr_->StreamFinalize(stream_);
      inc_in_segment_ = false;
    }
  }
  compute_sec_ += Secs(t0, Clock::now());

  const bool segment_closed = !inc_in_segment_ && !inc_live_text_.empty();
  if (segment_closed) {
    core::AsrToken tok;
    tok.start_sec = tb_.SecondsAt(inc_seg_start_sample_);
    tok.end_sec = tb_.SecondsAt(inc_seg_end_sample_);
    tok.text = inc_live_text_;
    timeline_->AppendToken(tok);
    if (text_sink_) text_sink_(inc_text_id_, tok.start_sec, tok.end_sec, tok.text);
    ++inc_text_id_;
    inc_delivered_text_.clear();
    if (emit_) {
      char buf[128];
      std::snprintf(buf, sizeof(buf),
                    "{\"type\":\"asr\",\"source\":\"qwen3_asr\","
                    "\"start\":%.3f,\"end\":%.3f,\"text\":\"",
                    tok.start_sec, tok.end_sec);
      emit_(std::string(buf) + JsonEscape(tok.text) + "\"}");
    }
    inc_live_text_.clear();
  } else if (inc_in_segment_) {
    if (text_sink_ && inc_live_text_ != inc_delivered_text_) {
      text_sink_(inc_text_id_, tb_.SecondsAt(inc_seg_start_sample_),
                 tb_.SecondsAt(inc_seg_end_sample_), inc_live_text_);
      inc_delivered_text_ = inc_live_text_;
    }
    if (emit_ && !inc_live_text_.empty()) {
      char buf[160];
      std::snprintf(buf, sizeof(buf),
                    "{\"type\":\"asr_partial\",\"source\":\"qwen3_asr\","
                    "\"start\":%.3f,\"end\":%.3f,"
                    "\"text\":\"",
                    tb_.SecondsAt(inc_seg_start_sample_),
                    tb_.SecondsAt(inc_seg_end_sample_));
      emit_(std::string(buf) + JsonEscape(inc_live_text_) + "\"}");
    }
  }
}

void AsrWorker::Reset() {
  inc_in_segment_ = false;
  inc_abs_pos_ = 0;
  inc_seg_samples_ = 0;
  inc_live_text_.clear();
  inc_text_id_ = 0;
  inc_delivered_text_.clear();
  processed_samples_.store(0);
  compute_sec_ = 0.0;
  asr_->Reset();
}

}  // namespace pipeline
}  // namespace orator
