#include "pipeline/asr_worker.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <thread>

#include "gpu/gpu_lock.h"
#include "model/qwen3_asr.h"
#include "pipeline/json_util.h"
#include "pipeline/worker_common.h"

namespace orator {
namespace pipeline {

using namespace worker;

AsrWorker::AsrWorker(core::IAsr* asr,
                       const Params& params, Emit emit, core::TimeBase tb,
                       cudaStream_t stream)
    : asr_(asr), params_(params), emit_(std::move(emit)),
      tb_(std::move(tb)), stream_(stream),
      ring_buffer_(kRingBufferSamples) {
}

void AsrWorker::ProcessSpan(const float* samples, int n) {
  if (samples == nullptr || n <= 0) return;

  // Push into ring buffer regardless of VAD state (for lead buffer on speech onset)
  RingPush(samples, n, inc_abs_pos_);

  if (params_.asr_vad_gate) {
    const double current_sec = tb_.SecondsAt(inc_abs_pos_ + n);

    // Read VAD segments atomically (pushed by subscription from VAD thread).
    std::vector<std::pair<double, double>> vad_segs;
    {
      std::lock_guard<std::mutex> lk(vad_mutex_);
      vad_segs = vad_segments_cache_;
    }

    auto in_speech = [&](double sec) {
      for (const auto& [s, e] : vad_segs) {
        if (sec >= s && sec < e) return true;
      }
      return false;
    };

    // Re-evaluate when VAD segments arrive during fallback processing.
    // Three cases:
    //   in_speech=true              → VAD confirms speech at current position.
    //   current_sec past last VAD end ≥ trail_sec → VAD confirmed silence long
    //     enough for a segment boundary. Enter TRAILING so the trailing timer
    //     can commit the current segment.
    //   otherwise                   → stay IDLE, keep processing audio via line 97.
    if (vad_state_ == VadState::IDLE && !vad_segs.empty() && inc_in_segment_) {
      if (in_speech(current_sec)) {
        vad_state_ = VadState::PROCESSING;
      } else {
        // Only trail when VAD has confirmed speech within this ASR segment.
        // If no VAD segment START falls within [seg_start, current_sec), VAD
        // may still be processing this speech segment (async pipeline lag).
        // Trailing on stale data would cut the segment short.
        double seg_start_sec = tb_.SecondsAt(inc_seg_start_sample_);
        bool vad_confirmed_this_segment = false;
        for (const auto& s : vad_segs) {
          if (s.first >= seg_start_sec && s.first < current_sec) {
            vad_confirmed_this_segment = true;
            break;
          }
        }
        if (vad_confirmed_this_segment &&
            current_sec - vad_segs.back().second >= params_.asr_vad_trail_sec) {
          vad_state_ = VadState::TRAILING;
          vad_trail_start_sec_ = current_sec;
        }
      }
    }

    if (vad_state_ == VadState::IDLE) {
      if (in_speech(current_sec)) {
        const int lead_samples = std::max(0, params_.asr_vad_lead_ms * params_.sample_rate / 1000);
        std::vector<float> lead;
        RingPop(lead_samples, &lead);
        ProcessIncremental(lead.data(), static_cast<int>(lead.size()), /*finalize=*/false);
        vad_state_ = VadState::PROCESSING;
      } else if (!vad_segs.empty()) {
        // Only skip audio when current_sec is CONFIRMED non-speech: it falls
        // inside a known silence gap between two VAD speech segments.
        // If current_sec is past the last known segment end, VAD might still
        // detect speech here (async pipeline lag) — fall through to line 97
        // to process the audio rather than skip it.
        bool confirmed_silence = false;
        for (size_t i = 0; i + 1 < vad_segs.size(); ++i) {
          if (current_sec > vad_segs[i].second && current_sec < vad_segs[i + 1].first) {
            confirmed_silence = true;
            break;
          }
        }
        if (!confirmed_silence) {
          // VAD hasn't confirmed this position yet — process audio
          // (fall through to line 97)
        } else {
          inc_abs_pos_ += n;
          processed_samples_.fetch_add(n);
          return;
        }
      }
    }

    if (vad_state_ == VadState::PROCESSING) {
      if (!in_speech(current_sec)) {
        // Only transition to TRAILING when VAD has confirmed speech within
        // this ASR segment (i.e., a VAD segment START falls in this segment's
        // time range). Without this guard, VAD async lag causes premature
        // trailing: the cache reports no speech at current_sec because the
        // VAD segment hasn't been published yet, but VAD is still processing
        // it. Staying in PROCESSING lets VAD catch up and avoids splitting
        // long VAD segments. Once VAD confirms the segment, the normal
        // trail_sec silence window applies.
        double seg_start_sec = tb_.SecondsAt(inc_seg_start_sample_);
        bool vad_confirmed_this_segment = false;
        for (const auto& s : vad_segs) {
          if (s.first >= seg_start_sec && s.first < current_sec) {
            vad_confirmed_this_segment = true;
            break;
          }
        }
        if (vad_confirmed_this_segment &&
            current_sec > vad_segs.back().second + params_.asr_vad_trail_sec) {
          vad_state_ = VadState::TRAILING;
          vad_trail_start_sec_ = current_sec;
        }
        // If VAD hasn't confirmed speech in this segment: stay in PROCESSING,
        // keep processing audio. VAD segments will catch up eventually.
      }
    }

    if (vad_state_ == VadState::TRAILING) {
      if (current_sec - vad_trail_start_sec_ >= params_.asr_vad_trail_sec) {
        ProcessIncremental(nullptr, 0, /*finalize=*/true);
        vad_state_ = VadState::IDLE;
        inc_in_segment_ = false;
        inc_abs_pos_ += n;
        processed_samples_.fetch_add(n);
        return;
      }
    }
  }

  ProcessIncremental(samples, n, /*finalize=*/false);
  processed_samples_.fetch_add(n);
}



void AsrWorker::AddVadSegment(double start, double end) {
  std::lock_guard<std::mutex> lk(vad_mutex_);
  vad_segments_cache_.push_back({start, end});
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
  auto* qwen = dynamic_cast<model::Qwen3Asr*>(asr_);
  const auto t0 = Clock::now();
  {
    gpu::DeviceGuard gpu(/*own_stream=*/true);
    if (n > 0) {
      if (!inc_in_segment_) {
        inc_seg_start_sample_ = inc_abs_pos_;
        inc_seg_samples_ = 0;
        inc_live_text_.clear();
        qwen->StreamReset(inc_seg_start_sample_);
        inc_in_segment_ = true;
      }
      inc_live_text_ = qwen->StreamChunk(samples, n, stream_);
      inc_seg_samples_ += n;
      inc_abs_pos_ += n;
      inc_seg_end_sample_ = inc_abs_pos_;
    }
    if (inc_in_segment_ && finalize) {
      inc_live_text_ = qwen->StreamFinalize(stream_);
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
        qwen->stream_audio_tokens() >= params_.max_audio_tokens) {
      inc_live_text_ = qwen->StreamFinalize(stream_);
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
  compute_sec_ = 0.0;
  asr_->Reset();

  vad_state_ = VadState::IDLE;
  vad_trail_start_sec_ = 0.0;
  {
    std::lock_guard<std::mutex> lk(vad_mutex_);
    vad_segments_cache_.clear();
  }
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
