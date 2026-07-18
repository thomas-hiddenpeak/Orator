#include "pipeline/asr_worker.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <limits>

#include "gpu/gpu_lock.h"
#include "pipeline/comprehensive_timeline.h"
#include "pipeline/json_util.h"
#include "pipeline/worker_common.h"

namespace orator {
namespace pipeline {

namespace {

struct VadSnapshot {
  std::shared_ptr<const std::vector<ComprehensiveTimeline::VadSeg>> segments;
  double horizon = -1e9;
  bool in_speech = false;
  double active_start = -1e9;
  double active_horizon = -1e9;
};

struct VadRegion {
  long start = 0;
  long end = 0;
  bool finalized = false;
};

VadSnapshot ReadVadSnapshot(const ComprehensiveTimeline* timeline) {
  if (timeline == nullptr) return {};
  const auto evidence = timeline->SnapshotVadEvidence();
  return {evidence.segments, evidence.horizon, evidence.in_speech,
          evidence.active_start, evidence.active_horizon};
}

double VadOverlapSec(
    double start, double end,
    const std::vector<ComprehensiveTimeline::VadSeg>& vad_segments) {
  double overlap = 0.0;
  for (const auto& segment : vad_segments) {
    const double a = std::max(start, segment.start);
    const double b = std::min(end, segment.end);
    if (b > a) overlap += b - a;
  }
  return overlap;
}

bool VadSupportsLiveText(double start, double end, const VadSnapshot& vad) {
  if (vad.segments && VadOverlapSec(start, end, *vad.segments) > 0.0) {
    return true;
  }
  return vad.in_speech && vad.active_start < end &&
         vad.active_horizon + 1e-9 >= start;
}

}  // namespace

using namespace worker;

AsrWorker::AsrWorker(core::IAsr* asr, const Params& params, Emit emit,
                     core::TimeBase tb, cudaStream_t stream,
                     const ComprehensiveTimeline* timeline)
    : asr_(asr),
      params_(params),
      emit_(std::move(emit)),
      tb_(tb),
      timeline_(timeline),
      stream_(stream) {}

void AsrWorker::ProcessSpan(const float* samples, int n) {
  if (samples == nullptr || n <= 0) return;

  if (!params_.asr_vad_gate) {
    ProcessIncremental(samples, n, /*finalize=*/false);
    processed_samples_.fetch_add(n);
    return;
  }

  pending_audio_.insert(pending_audio_.end(), samples, samples + n);
  received_samples_ += n;
  processed_samples_.store(received_samples_);
  DrainVadGate(/*final=*/false);
}

void AsrWorker::DrainVadGate(bool final) {
  const VadSnapshot vad = ReadVadSnapshot(timeline_);
  std::vector<VadRegion> regions;
  long decision_limit = -1;

  if (vad.segments) {
    regions.reserve(vad.segments->size() + 1);
    for (const auto& segment : *vad.segments) {
      const long start = std::max(0L, tb_.SampleAt(segment.start));
      const long end = std::min(received_samples_, tb_.SampleAt(segment.end));
      if (end <= start) continue;
      regions.push_back({start, end, true});
      decision_limit = std::max(decision_limit, end);
    }
  }
  if (vad.horizon > -1e8) {
    decision_limit = std::max(
        decision_limit, std::min(received_samples_, tb_.SampleAt(vad.horizon)));
  }
  if (vad.in_speech && vad.active_start > -1e8 &&
      vad.active_horizon + 1e-9 >= vad.active_start) {
    const long start = std::max(0L, tb_.SampleAt(vad.active_start));
    const long end =
        std::min(received_samples_, tb_.SampleAt(vad.active_horizon));
    if (end > start) {
      regions.push_back({start, end, false});
      decision_limit = std::max(decision_limit, end);
    }
  }
  if (final) decision_limit = received_samples_;
  if (decision_limit < inc_abs_pos_) return;
  decision_limit = std::min(decision_limit, received_samples_);

  std::stable_sort(regions.begin(), regions.end(),
                   [](const VadRegion& a, const VadRegion& b) {
                     if (a.start != b.start) return a.start < b.start;
                     return a.end < b.end;
                   });

  const long lead_samples =
      std::max<long>(0, static_cast<long>(params_.asr_vad_lead_ms) *
                            params_.sample_rate / 1000);
  const long trail_samples = std::max<long>(
      0, static_cast<long>(
             std::llround(params_.asr_vad_trail_sec * params_.sample_rate)));

  while (inc_abs_pos_ < decision_limit && PendingSamples() > 0) {
    const VadRegion* next = nullptr;
    for (const auto& region : regions) {
      if (region.end > inc_abs_pos_ && region.start < decision_limit) {
        next = &region;
        break;
      }
    }

    if (inc_in_segment_ && last_speech_end_sample_ >= 0) {
      const long endpoint = last_speech_end_sample_ + trail_samples;
      const long next_start =
          next == nullptr ? std::numeric_limits<long>::max() : next->start;
      if (next_start > endpoint && (decision_limit >= endpoint || final)) {
        // Keep the source-clock tail for forced alignment, but leave those
        // samples pending. A later speech onset may need them as its TOML lead;
        // retaining the cursor makes that overlap independent of when the
        // later VAD region is published.
        inc_seg_end_sample_ = std::min(endpoint, received_samples_);
        ProcessIncremental(nullptr, 0, /*finalize=*/true);
        last_speech_end_sample_ = -1;
        continue;
      }
      if (next_start > endpoint) {
        // Do not consume a partially confirmed tail yet. A later VAD update
        // may still reveal speech inside the trailing interval, in which case
        // the exact gap samples must be fed to the same decoder session.
        break;
      }
    }

    if (next == nullptr) {
      long skip_end = decision_limit;
      if (!final) {
        skip_end = std::max(inc_abs_pos_, decision_limit - lead_samples);
      }
      if (skip_end <= inc_abs_pos_) break;
      SkipPendingUntil(skip_end);
      continue;
    }

    if (inc_abs_pos_ < next->start) {
      if (inc_in_segment_) {
        // Speech resumed inside the TOML trailing interval. Preserve the
        // natural pause in the decoder input instead of concatenating two
        // regions whose source time still contains that pause.
        FeedPendingUntil(std::min(next->start, decision_limit),
                         /*exact_end=*/true);
        continue;
      }
      const long lead_start = std::max(0L, next->start - lead_samples);
      if (inc_abs_pos_ < lead_start) {
        SkipPendingUntil(std::min(lead_start, decision_limit));
        continue;
      }
    }

    const long before = inc_abs_pos_;
    FeedPendingUntil(std::min(next->end, decision_limit),
                     next->finalized || final);
    if (inc_abs_pos_ == before) break;
    last_speech_end_sample_ = inc_abs_pos_;
  }
}

void AsrWorker::Finalize() {
  finalizing_ = true;
  if (params_.asr_vad_gate) DrainVadGate(/*final=*/true);
  ProcessIncremental(nullptr, 0, /*finalize=*/true);
  finalizing_ = false;
}

void AsrWorker::FeedPendingUntil(long end_sample, bool exact_end) {
  const long chunk_samples =
      std::max<long>(1, static_cast<long>(params_.asr_vad_gate_chunk_ms) *
                            params_.sample_rate / 1000);
  while (inc_abs_pos_ < end_sample && PendingSamples() > 0) {
    const long remaining = end_sample - inc_abs_pos_;
    if (!exact_end && remaining < chunk_samples) return;
    const int take = static_cast<int>(
        std::min({remaining, chunk_samples, PendingSamples()}));
    if (take <= 0) return;
    ProcessIncremental(PendingData(), take, /*finalize=*/false);
    ConsumePending(take);
  }
}

void AsrWorker::SkipPendingUntil(long end_sample) {
  const long take = std::min(end_sample - inc_abs_pos_, PendingSamples());
  if (take <= 0) return;
  inc_abs_pos_ += take;
  ConsumePending(take);
}

void AsrWorker::ConsumePending(long n) {
  if (n <= 0) return;
  pending_offset_ += static_cast<size_t>(n);
  if (pending_offset_ == pending_audio_.size()) {
    pending_audio_.clear();
    pending_offset_ = 0;
  } else if (pending_offset_ >= 65536 &&
             pending_offset_ * 2 >= pending_audio_.size()) {
    pending_audio_.erase(pending_audio_.begin(),
                         pending_audio_.begin() + pending_offset_);
    pending_offset_ = 0;
  }
}

long AsrWorker::PendingSamples() const {
  return static_cast<long>(pending_audio_.size() - pending_offset_);
}

const float* AsrWorker::PendingData() const {
  return pending_audio_.data() + pending_offset_;
}

// Incremental KV-cache path: continuous audio is fed into the engine's
// streaming session, which carries KV context across its 8 s windows. The
// segment is committed (one timeline token) and the session reset at the
// segment cap or on finalize.
//
// Large chunks (from WaitAndRead reading all remaining audio) are split into
// small (<= 8 s) pieces so the KV-cache safety cap can fire between them.
// Without this, a single 256 s chunk encodes 32 windows at once, overflowing
// the KV-cache (max_seq_len=2048) before the per-chunk cap check is reached.
void AsrWorker::ProcessIncremental(const float* samples, int n, bool finalize) {
  const int max_chunk = 8 * params_.sample_rate;  // one 8s encoded window max
  int off = 0;
  while (off < n) {
    const int take = std::min(n - off, max_chunk);
    EmitIncrementalChunk(samples ? samples + off : nullptr, take,
                         /*finalize=*/false);
    off += take;
  }
  if (finalize) EmitIncrementalChunk(nullptr, 0, /*finalize=*/true);
}

void AsrWorker::EmitIncrementalChunk(const float* samples, int n,
                                     bool finalize) {
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
    if (inc_in_segment_ && finalize) {
      inc_live_text_ = asr_->StreamFinalize(stream_);
      inc_in_segment_ = false;
    }
    // VAD groups still need a hard duration cap for continuous speech and
    // decoder-cache safety. Downstream forced alignment supplies fine time
    // codes when a long utterance is split here.
    if (inc_in_segment_ && params_.segment_sec > 0.0 &&
        inc_seg_samples_ >=
            static_cast<long>(params_.segment_sec * params_.sample_rate)) {
      inc_live_text_ = asr_->StreamFinalize(stream_);
      inc_in_segment_ = false;
    }
    // KV-cache safety cap: force-finalize before GPU memory crash.
    // Root cause: GqaDecodeAttnKernel at layer 0 has an illegal memory access
    // when the KV-cache position exceeds ~1800 (verified empirically: crashes
    // at audio_tokens=1768 during the 18th encoding window;
    // max_audio_tokens=1664 passes but 1792 crashes). The crash is NOT a buffer
    // overflow (all positions within max_seq_len=2048, shared memory within
    // kMaxCtx=2048). A known safe upper bound of ~1700 total cache positions
    // limits single-segment audio tokens to 1664 (1700-31 system-5 suffix
    // margin). With max_audio_tokens=1500 the margin increases and the
    // per-segment cap is ~115s of audio. VAD trailing window normally closes
    // segments before the cap is reached.
    if (inc_in_segment_ && params_.max_audio_tokens > 0 &&
        asr_->stream_audio_tokens() >= params_.max_audio_tokens) {
      inc_live_text_ = asr_->StreamFinalize(stream_);
      inc_in_segment_ = false;
    }
  }
  compute_sec_.fetch_add(Secs(t0, Clock::now()), std::memory_order_relaxed);

  const bool segment_closed = !inc_in_segment_ && !inc_live_text_.empty();
  if (segment_closed) {
    const double seg_start = tb_.SecondsAt(inc_seg_start_sample_);
    const double seg_end = tb_.SecondsAt(inc_seg_end_sample_);
    // The deterministic gate feeds only typed VAD-backed audio. Keep the
    // overlap guard as a final defense against a time-capped provisional active
    // segment whose endpoint is later rejected by finalized VAD evidence.
    bool keep = true;
    if (params_.asr_vad_gate && timeline_) {
      const VadSnapshot vad = ReadVadSnapshot(timeline_);
      if (!vad.segments || vad.segments->empty()) {
        if (finalizing_ ||
            vad.horizon >= seg_start + params_.asr_vad_trail_sec) {
          keep = false;
        }
      } else {
        const double overlap = VadOverlapSec(seg_start, seg_end, *vad.segments);
        if (overlap >= params_.asr_vad_min_overlap_sec) {
          keep = true;
        } else {
          const bool confirmed_by_vad = finalizing_ || seg_end <= vad.horizon;
          keep = !confirmed_by_vad;
        }
      }
    }
    if (!keep) {
      if (emit_ && !inc_delivered_text_.empty()) {
        emit_(
            "{\"type\":\"asr_retract\",\"source\":\"qwen3_asr\","
            "\"text_id\":" +
            std::to_string(inc_text_id_) + "}");
      }
      inc_live_text_.clear();
      inc_delivered_text_.clear();
    } else {
      core::AsrToken tok;
      tok.start_sec = seg_start;
      tok.end_sec = seg_end;
      tok.text = inc_live_text_;
      // The controller deposits this finalized record into
      // ComprehensiveTimeline before mirroring it to protocol transport.
      const long text_id = inc_text_id_++;
      if (text_sink_)
        text_sink_(text_id, tok.start_sec, tok.end_sec, tok.text,
                   /*is_final=*/true);
      inc_delivered_text_.clear();
      if (emit_) {
        char buf[160];
        std::snprintf(buf, sizeof(buf),
                      "{\"type\":\"asr\",\"source\":\"qwen3_asr\","
                      "\"text_id\":%ld,\"start\":%.9f,\"end\":%.9f,\"text\":\"",
                      text_id, tok.start_sec, tok.end_sec);
        emit_(std::string(buf) + JsonEscape(tok.text) + "\"}");
      }
      inc_live_text_.clear();
    }
  } else if (inc_in_segment_) {
    bool expose_partial = true;
    if (params_.asr_vad_gate && timeline_) {
      const VadSnapshot vad = ReadVadSnapshot(timeline_);
      expose_partial =
          VadSupportsLiveText(tb_.SecondsAt(inc_seg_start_sample_),
                              tb_.SecondsAt(inc_seg_end_sample_), vad);
    }
    if (expose_partial && text_sink_ && inc_live_text_ != inc_delivered_text_) {
      text_sink_(inc_text_id_, tb_.SecondsAt(inc_seg_start_sample_),
                 tb_.SecondsAt(inc_seg_end_sample_), inc_live_text_,
                 /*is_final=*/false);
      inc_delivered_text_ = inc_live_text_;
    }
    if (expose_partial && emit_ && !inc_live_text_.empty()) {
      char buf[192];
      std::snprintf(buf, sizeof(buf),
                    "{\"type\":\"asr_partial\",\"source\":\"qwen3_asr\","
                    "\"text_id\":%ld,\"start\":%.9f,\"end\":%.9f,"
                    "\"text\":\"",
                    inc_text_id_, tb_.SecondsAt(inc_seg_start_sample_),
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
  finalizing_ = false;
  processed_samples_.store(0);
  compute_sec_.store(0.0, std::memory_order_relaxed);
  asr_->Reset();

  pending_audio_.clear();
  pending_offset_ = 0;
  received_samples_ = 0;
  last_speech_end_sample_ = -1;
}

}  // namespace pipeline
}  // namespace orator
