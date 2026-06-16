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
                     const Params& params, Emit emit, cudaStream_t stream,
                     int rollback_tokens)
    : asr_(asr), timeline_(timeline), params_(params), emit_(std::move(emit)),
  preproc_(params.preproc), stream_(stream),
  rollback_tokens_(rollback_tokens) {
  // Construct the Silero VAD only on the paths that use it: the legacy utterance
  // path (!incremental) and the opt-in endpoint_reset. The default incremental
  // path never touches it, so it is not loaded.
  if (!params_.incremental || params_.endpoint_reset) {
    vad_ = std::make_unique<AsrSileroVad>(params_.vad);
  }
}

void AsrWorker::ProcessSpan(const float* samples, int n) {
  if (samples == nullptr || n <= 0) return;
  if (params_.incremental) {
    ProcessIncremental(samples, n, /*finalize=*/false);
  } else {
    // LEGACY Silero-VAD utterance path. DEACTIVATED by default (params_.incremental
    // is true in production) because it measured worse than the incremental
    // KV-cache path on both accuracy and speed (600 s CER 26.4% vs 11.6%; asr
    // 3.50x vs 4.78x). Retained only for regression comparison via
    // ORATOR_ASR_INCREMENTAL=0.
    vad_->Push(samples, n);
    DrainUtterances(/*finalize=*/false);
  }
  processed_samples_.fetch_add(n);
}

void AsrWorker::Finalize() {
  if (params_.incremental) {
    ProcessIncremental(nullptr, 0, /*finalize=*/true);
  } else {
    DrainUtterances(/*finalize=*/true);
  }
}

void AsrWorker::DrainUtterances(bool finalize) {
  for (;;) {
    int begin = 0, end = 0, consume = 0;
    if (!vad_->NextSpan(finalize, &begin, &end, &consume)) return;
    EmitUtterance(begin, end, finalize);
    vad_->Consume(consume);
    if (finalize) return;
  }
}

void AsrWorker::EmitUtterance(int begin, int end, bool finalize) {
  if (end <= begin) return;
  std::vector<float> enhanced;
  const float* asr_input = vad_->data() + begin;
  int asr_len = end - begin;
  preproc_.Process(asr_input, asr_len, &enhanced);
  if (!enhanced.empty()) {
    asr_input = enhanced.data();
    asr_len = static_cast<int>(enhanced.size());
  }
  const auto t0 = Clock::now();
  std::string continuation;
  {
    std::lock_guard<std::mutex> gpu(gpu::DeviceLock());
    if (rollback_tokens_ > 0) {
      continuation = asr_->TranscribeWindow(asr_input, asr_len,
                                            pending_prefix_, stream_);
    } else {
      continuation = asr_->TranscribeText(asr_input, asr_len,
                                          stream_);
    }
  }
  compute_sec_ += Secs(t0, Clock::now());
  if (continuation.empty() && !finalize) return;

  if (rollback_tokens_ <= 0) {
    core::AsrToken tok;
    tok.start_sec = tb_.SecondsAt(vad_->base_sample() + begin);
    tok.end_sec = tb_.SecondsAt(vad_->base_sample() + end);
    tok.text = continuation;
    if (tok.text.empty()) return;
    timeline_->AppendToken(tok);
    if (text_sink_) text_sink_(tok.start_sec, tok.end_sec, tok.text);
    if (emit_) {
      char buf[128];
      std::snprintf(buf, sizeof(buf),
                    "{\"type\":\"asr\",\"source\":\"qwen3_asr\","
                    "\"start\":%.3f,\"end\":%.3f,\"text\":\"",
                    tok.start_sec, tok.end_sec);
      emit_(std::string(buf) + JsonEscape(tok.text) + "\"}");
    }
    return;
  }

  std::string merged = pending_prefix_ + continuation;
  std::string emit_text;
  if (finalize || rollback_tokens_ <= 0) {
    emit_text = merged;
    pending_prefix_.clear();
  } else {
    std::vector<int> toks = asr_->tokenizer().Encode(merged);
    if (static_cast<int>(toks.size()) > rollback_tokens_) {
      std::vector<int> fixed(toks.begin(), toks.end() - rollback_tokens_);
      std::vector<int> tail(toks.end() - rollback_tokens_, toks.end());
      emit_text = asr_->tokenizer().Decode(fixed, /*skip_special=*/true);
      pending_prefix_ = asr_->tokenizer().Decode(tail, /*skip_special=*/true);
    } else {
      pending_prefix_ = merged;
      emit_text.clear();
    }
  }
  if (emit_text.empty()) return;

  core::AsrToken tok;
  tok.start_sec = tb_.SecondsAt(vad_->base_sample() + begin);
  tok.end_sec = tb_.SecondsAt(vad_->base_sample() + end);
  tok.text = emit_text;
  timeline_->AppendToken(tok);
  if (text_sink_) text_sink_(tok.start_sec, tok.end_sec, tok.text);

  if (emit_) {
    char buf[128];
    std::snprintf(buf, sizeof(buf),
                  "{\"type\":\"asr\",\"start\":%.3f,\"end\":%.3f,\"text\":\"",
                  tok.start_sec, tok.end_sec);
    emit_(std::string(buf) + JsonEscape(tok.text) + "\"}");
  }
}

// Incremental KV-cache path: continuous audio is fed into the engine's
// streaming session, which carries KV context across its 8 s windows. The
// segment is committed (one timeline token) and the session reset at the
// segment cap or on finalize. Continuous (untrimmed) audio is fed so the
// acoustic flow is preserved -- VAD trimming/concatenation degrades the
// streamed context. The Silero front VAD is bypassed in this mode; segment
// boundaries come from the cap (a natural-endpoint trigger can replace the cap
// later without changing this contract).
void AsrWorker::ProcessIncremental(const float* samples, int n, bool finalize) {
  const int sr = params_.vad.sample_rate;
  const long seg_cap = std::max<long>(1, static_cast<long>(params_.segment_sec * sr));
  const long min_seg =
      std::max<long>(1, static_cast<long>(params_.endpoint_min_segment_sec * sr));

  // Endpoint detection (T050): the Silero VAD runs in parallel purely as an
  // endpoint detector. The engine is fed the SAME continuous audio (the VAD
  // does not trim it); endpoints only choose where to close a segment so the
  // boundary lands on a natural pause (better speaker attribution + no
  // mid-word cut) rather than a fixed cap. We push the audio to the VAD and
  // drain endpoint positions into a queue.
  if (params_.endpoint_reset && n > 0) {
    vad_->Push(samples, n);
    long ep = 0;
    while (vad_->NextEndpoint(/*finalize=*/false, &ep)) inc_endpoints_.push_back(ep);
  }

  // Slice the input at the next segment boundary. The boundary is the earliest
  // of: (a) a VAD endpoint at least min_seg past the segment start, or (b) the
  // hard cap seg_cap. `take` is always > 0 so `off` strictly advances.
  int off = 0;
  while (off < n) {
    // Close a full or endpoint-reached segment before feeding more, so the next
    // sample starts a fresh segment (avoids a zero-length feed / spin).
    if (inc_in_segment_) {
      const long seg_start = inc_seg_start_sample_;
      const long cap_boundary = seg_start + seg_cap;
      long boundary = cap_boundary;
      if (params_.endpoint_reset) {
        while (!inc_endpoints_.empty() &&
               inc_endpoints_.front() < seg_start + min_seg)
          inc_endpoints_.pop_front();  // too early for this segment
        if (!inc_endpoints_.empty())
          boundary = std::min(boundary, inc_endpoints_.front());
      }
      if (inc_abs_pos_ >= boundary) {
        EmitIncrementalChunk(nullptr, 0, /*finalize=*/true);
        continue;  // re-evaluate with a closed segment
      }
      const int take =
          static_cast<int>(std::min<long>(boundary - inc_abs_pos_, n - off));
      EmitIncrementalChunk(samples + off, take, /*finalize=*/false);
      off += take;
    } else {
      // No open segment: feed up to the cap (a new segment opens on first feed).
      const int take = static_cast<int>(std::min<long>(seg_cap, n - off));
      EmitIncrementalChunk(samples + off, take, /*finalize=*/false);
      off += take;
    }
  }
  if (finalize) EmitIncrementalChunk(nullptr, 0, /*finalize=*/true);
}

void AsrWorker::EmitIncrementalChunk(const float* samples, int n, bool finalize) {
  const int sr = params_.vad.sample_rate;
  const long seg_cap = std::max<long>(1, static_cast<long>(params_.segment_sec * sr));

  const auto t0 = Clock::now();
  {
    std::lock_guard<std::mutex> gpu(gpu::DeviceLock());
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
    if (text_sink_) text_sink_(tok.start_sec, tok.end_sec, tok.text);
    if (emit_) {
      char buf[128];
      std::snprintf(buf, sizeof(buf),
                    "{\"type\":\"asr\",\"source\":\"qwen3_asr\","
                    "\"start\":%.3f,\"end\":%.3f,\"text\":\"",
                    tok.start_sec, tok.end_sec);
      emit_(std::string(buf) + JsonEscape(tok.text) + "\"}");
    }
    inc_live_text_.clear();
  } else if (n > 0 && emit_ && !inc_live_text_.empty()) {
    // Partial live update (not yet committed to the timeline).
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

void AsrWorker::Reset() {
  if (vad_) vad_->Reset();
  pending_prefix_.clear();
  inc_in_segment_ = false;
  inc_abs_pos_ = 0;
  inc_seg_samples_ = 0;
  inc_live_text_.clear();
  inc_endpoints_.clear();
  processed_samples_.store(0);
  compute_sec_ = 0.0;
  asr_->Reset();
}

}  // namespace pipeline
}  // namespace orator
