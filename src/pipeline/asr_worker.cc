#include "pipeline/asr_worker.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <thread>

#include "gpu/gpu_lock.h"
#include "pipeline/json_util.h"
#include "pipeline/worker_common.h"

namespace orator {
namespace pipeline {

using namespace worker;

AsrWorker::AsrWorker(core::IAsr* asr,
                       const Params& params, Emit emit, core::TimeBase tb,
                       cudaStream_t stream,
                       VadCache* vad_cache)
    : asr_(asr), params_(params), emit_(std::move(emit)),
      tb_(std::move(tb)),
      vad_cache_(vad_cache),
      stream_(stream),
      ring_buffer_(kRingBufferSamples) {
}

void AsrWorker::ProcessSpan(const float* samples, int n) {
  if (samples == nullptr || n <= 0) return;

  if (!params_.asr_vad_gate) {
    ProcessIncremental(samples, n, /*finalize=*/false);
    processed_samples_.fetch_add(n);
    return;
  }

  // Event-driven, non-blocking VAD gate. The incoming span may be a few
  // milliseconds (real-time pacing) or several minutes (flooded ingest, when
  // WaitAndRead returns the whole backlog). Cut it at the VAD segment boundaries
  // that fall inside it so each sub-span lies entirely within one VAD region,
  // then act per sub-span in ProcessGateSubSpan. Because the cut points are VAD
  // audio times, coverage no longer depends on the span size: previously a large
  // flooded span whose END landed in a silence gap was dropped in full
  // (`inc_abs_pos_ += n; return`), discarding all the speech inside it -- the
  // measured flood coverage collapse. inc_abs_pos_ advances by exactly the
  // consumed audio per sub-span, so it stays equal to the absolute clock head
  // and the cut math below is exact.
  const long span_start = inc_abs_pos_;
  const long span_end = inc_abs_pos_ + n;
  const std::vector<long> cuts = VadCutSamples(span_start, span_end);
  long pos = span_start;
  size_t ci = 0;
  while (pos < span_end) {
    while (ci < cuts.size() && cuts[ci] <= pos) ++ci;
    long next = span_end;
    if (ci < cuts.size() && cuts[ci] < next) next = cuts[ci];
    const int sub_n = static_cast<int>(next - pos);
    ProcessGateSubSpan(samples + (pos - span_start), sub_n);
    pos = next;
  }
}

// Absolute sample indices of the VAD segment boundaries (each segment's start
// and end) that fall strictly inside (span_start, span_end), sorted unique.
// These are the only points where the speech/silence classification can change,
// so cutting the span here makes each sub-span homogeneous.
std::vector<long> AsrWorker::VadCutSamples(long span_start, long span_end) const {
  std::vector<long> cuts;
  if (!vad_cache_) return cuts;
  const auto segs = vad_cache_->GetAll();
  cuts.reserve(segs.size() * 2);
  for (const auto& [s, e] : segs) {
    const long ss = tb_.SampleAt(s);
    const long es = tb_.SampleAt(e);
    if (ss > span_start && ss < span_end) cuts.push_back(ss);
    if (es > span_start && es < span_end) cuts.push_back(es);
  }
  std::sort(cuts.begin(), cuts.end());
  cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());
  return cuts;
}

// Act on one VAD-boundary-aligned sub-span [inc_abs_pos_, inc_abs_pos_+sub_n).
// Policy (priority: never drop speech > save GPU on confirmed silence; exact
// segmentation is irrelevant -- downstream alignment provides the time codes):
//   * speech                 -> feed the engine;
//   * confirmed silence       -> commit the open utterance after its trailing
//                                window, then skip the rest (the GPU saving);
//   * unconfirmed (past the VAD horizon, e.g. the first flooded span before VAD
//                                has caught up) -> feed, never skip.
// Non-blocking: it only reads what VAD has already published (Constitution
// Art. III -- ASR does not wait for VAD).
void AsrWorker::ProcessGateSubSpan(const float* sub, int sub_n) {
  if (sub_n <= 0) return;
  const auto vad_segs = vad_cache_ ? vad_cache_->GetAll()
                                   : std::vector<std::pair<double, double>>{};
  const long base = inc_abs_pos_;
  const double mid_sec = tb_.SecondsAt(base + sub_n / 2);
  const double end_sec = tb_.SecondsAt(base + sub_n);
  // Horizon = how far VAD has confirmed its decision. Use the later of the last
  // published speech segment's end (always valid) and the VAD progress horizon
  // (advances through silence at real-time pacing). A silence sub-span is
  // skippable only when its end is within this horizon.
  double horizon = vad_segs.empty() ? -1e9 : vad_segs.back().second;
  if (vad_cache_) horizon = std::max(horizon, vad_cache_->horizon());

  bool is_speech = false;
  for (const auto& [s, e] : vad_segs) {
    if (mid_sec >= s && mid_sec < e) { is_speech = true; break; }
  }

  if (is_speech) {
    ProcessIncremental(sub, sub_n, /*finalize=*/false);
    processed_samples_.fetch_add(sub_n);
    vad_state_ = VadState::PROCESSING;
    last_speech_end_sec_ = end_sec;
    return;
  }

  // Gap / silence sub-span. "Confirmed" means VAD has already labeled this far
  // (its end is within the published range), i.e. it is genuinely a gap between
  // known speech segments -- safe to skip. Beyond the horizon VAD may still
  // detect speech here, so we must process rather than drop.
  const bool confirmed = end_sec <= horizon;
  if (confirmed) {
    if (inc_in_segment_) {
      // Feed the trailing window (acoustic context for the last word), commit,
      // then skip the remaining silence.
      const long trail_end =
          tb_.SampleAt(last_speech_end_sec_ + params_.asr_vad_trail_sec);
      const long feed_l =
          std::min<long>(sub_n, std::max<long>(0, trail_end - base));
      const int feed_n = static_cast<int>(feed_l);
      if (feed_n > 0) ProcessIncremental(sub, feed_n, /*finalize=*/false);
      ProcessIncremental(nullptr, 0, /*finalize=*/true);
      inc_abs_pos_ += (sub_n - feed_n);
    } else {
      inc_abs_pos_ += sub_n;  // nothing open; skip the whole gap
    }
    processed_samples_.fetch_add(sub_n);
    vad_state_ = VadState::IDLE;
    return;
  }

  // Unconfirmed: process rather than skip, so no speech is lost. Under burst
  // this may spend some GPU on silence until VAD catches up; acceptable, since
  // correctness (full coverage) outranks the GPU saving.
  ProcessIncremental(sub, sub_n, /*finalize=*/false);
  processed_samples_.fetch_add(sub_n);
  vad_state_ = VadState::PROCESSING;
}



void AsrWorker::Finalize() {
  vad_state_ = VadState::IDLE;
  ProcessIncremental(nullptr, 0, /*finalize=*/true);
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

void AsrWorker::EmitIncrementalChunk(const float* samples, int n, bool finalize) {
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
    // KV-cache safety cap: force-finalize before GPU memory crash.
    // Root cause: GqaDecodeAttnKernel at layer 0 has an illegal memory access
    // when the KV-cache position exceeds ~1800 (verified empirically: crashes at
    // audio_tokens=1768 during the 18th encoding window; max_audio_tokens=1664
    // passes but 1792 crashes). The crash is NOT a buffer overflow (all positions
    // within max_seq_len=2048, shared memory within kMaxCtx=2048). A known safe
    // upper bound of ~1700 total cache positions limits single-segment audio
    // tokens to 1664 (1700-31 system-5 suffix margin). With max_audio_tokens=1500
    // the margin increases and the per-segment cap is ~115s of audio. VAD
    // trailing window normally closes segments before the cap is reached.
    if (inc_in_segment_ && params_.max_audio_tokens > 0 &&
        asr_->stream_audio_tokens() >= params_.max_audio_tokens) {
      inc_live_text_ = asr_->StreamFinalize(stream_);
      inc_in_segment_ = false;
    }
  }
  compute_sec_.fetch_add(Secs(t0, Clock::now()), std::memory_order_relaxed);

  const bool segment_closed = !inc_in_segment_ && !inc_live_text_.empty();
  if (segment_closed) {
    core::AsrToken tok;
    tok.start_sec = tb_.SecondsAt(inc_seg_start_sample_);
    tok.end_sec = tb_.SecondsAt(inc_seg_end_sample_);
    tok.text = inc_live_text_;
    // Text segment goes through ProtocolTimeline → comp_ via text_sink_.
    // StreamTimeline was removed; raw text is read from comp_.SnapshotRawTexts().
    if (text_sink_) text_sink_(inc_text_id_, tok.start_sec, tok.end_sec, tok.text);
    ++inc_text_id_;
    inc_delivered_text_.clear();
    if (emit_) {
      char buf[160];
      std::snprintf(buf, sizeof(buf),
                    "{\"type\":\"asr\",\"source\":\"qwen3_asr\","
                    "\"text_id\":%ld,\"start\":%.3f,\"end\":%.3f,\"text\":\"",
                    inc_text_id_, tok.start_sec, tok.end_sec);
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
      char buf[192];
      std::snprintf(buf, sizeof(buf),
                    "{\"type\":\"asr_partial\",\"source\":\"qwen3_asr\","
                    "\"text_id\":%ld,\"start\":%.3f,\"end\":%.3f,"
                    "\"text\":\"",
                    inc_text_id_,
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
  compute_sec_.store(0.0, std::memory_order_relaxed);
  asr_->Reset();

  vad_state_ = VadState::IDLE;
  vad_trail_start_sec_ = 0.0;
  last_speech_end_sec_ = -1e9;
  ring_write_pos_ = 0;
  ring_count_ = 0;
  ring_base_abs_pos_ = 0;
  std::fill(ring_buffer_.begin(), ring_buffer_.end(), 0.0f);
}

void AsrWorker::RingPush(const float* samples, int n, long abs_pos) {
  if (n <= 0) return;
  for (int i = 0; i < n; ++i) {
    ring_buffer_[ring_write_pos_] = samples[i];
    ring_write_pos_ = (ring_write_pos_ + 1) % kRingBufferSamples;
    if (ring_count_ < kRingBufferSamples) {
      ring_count_++;
    }
  }
  ring_base_abs_pos_ = abs_pos;
}

void AsrWorker::RingPop(int n, std::vector<float>* out) {
  if (n <= 0 || ring_count_ == 0) return;
  const int pop = std::min(n, ring_count_);
  out->reserve(out->size() + pop);

  int read_pos;
  if (ring_count_ == kRingBufferSamples) {
    read_pos = ring_write_pos_;
  } else {
    read_pos = (ring_write_pos_ - ring_count_ + kRingBufferSamples) % kRingBufferSamples;
  }

  for (int i = 0; i < pop; ++i) {
    out->push_back(ring_buffer_[read_pos]);
    read_pos = (read_pos + 1) % kRingBufferSamples;
  }
  ring_count_ -= pop;
}

}  // namespace pipeline
}  // namespace orator
