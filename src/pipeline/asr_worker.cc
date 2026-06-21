#include "pipeline/asr_worker.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <thread>

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

AsrWorker::AsrWorker(model::Qwen3Asr* asr,
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

    // Query VAD segments from ProtocolTimeline (incremental).
    // Each message on vad/speech_segment has data: {"start":S,"end":E,...}.
    // Only fetch segments newer than last_vad_replay_sec_ to avoid O(n) full replay.
    {
      if (protocol_timeline_) {
        auto msgs = protocol_timeline_->Replay("vad/speech_segment", last_vad_replay_sec_);
        for (const auto& msg : msgs) {
          auto sp = msg.data.find("\"start\":");
          auto ep = msg.data.find("\"end\":");
          if (sp != std::string::npos && ep != std::string::npos) {
            auto sv = sp + 8;
            auto se = msg.data.find_first_of(",}", sv);
            auto ev = ep + 6;
            auto ee = msg.data.find_first_of(",}", ev);
            if (se != std::string::npos && ee != std::string::npos) {
              double s = 0.0, e = 0.0;
              try {
                s = std::stod(msg.data.substr(sv, se - sv));
                e = std::stod(msg.data.substr(ev, ee - ev));
              } catch (const std::exception&) {
                continue;  // malformed VAD segment — skip
              }
              vad_segments_cache_.push_back({s, e});
              if (e > last_vad_replay_sec_) {
                last_vad_replay_sec_ = e;
              }
            }
          }
        }
      }
    }
    const auto& vad_segs = vad_segments_cache_;

    auto in_speech = [&](double sec) {
      for (const auto& [s, e] : vad_segs) {
        if (sec >= s && sec < e) return true;
      }
      return false;
    };

    // Re-evaluate when VAD segments first arrive during fallback processing.
    if (vad_state_ == VadState::IDLE && !vad_segs.empty() && inc_in_segment_) {
      if (in_speech(current_sec)) {
        vad_state_ = VadState::PROCESSING;
      } else {
        vad_state_ = VadState::TRAILING;
        vad_trail_start_sec_ = current_sec;
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
        inc_abs_pos_ += n;
        processed_samples_.fetch_add(n);
        return;
      }
    }

    if (vad_state_ == VadState::PROCESSING) {
      if (!in_speech(current_sec)) {
        vad_state_ = VadState::TRAILING;
        vad_trail_start_sec_ = current_sec;
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

void AsrWorker::Finalize() {
  vad_state_ = VadState::IDLE;
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
  last_vad_replay_sec_ = 0.0;
  vad_segments_cache_.clear();
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
