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
                     const Params& params, Emit emit)
    : asr_(asr), timeline_(timeline), params_(params), emit_(std::move(emit)) {}

void AsrWorker::ProcessSpan(const float* samples, int n) {
  if (samples == nullptr || n <= 0) return;
  pcm_.insert(pcm_.end(), samples, samples + n);
  DrainUtterances(/*finalize=*/false);
  processed_samples_.fetch_add(n);
}

void AsrWorker::Finalize() { DrainUtterances(/*finalize=*/true); }

void AsrWorker::DrainUtterances(bool finalize) {
  const int sr = params_.sample_rate;
  const int frame = std::max(1, sr / 100);  // 10 ms RMS frames
  const int sil_frames = static_cast<int>(params_.endpoint_silence_sec * 100);
  const int max_frames = static_cast<int>(params_.max_utterance_sec * 100);
  const int min_frames = static_cast<int>(params_.min_utterance_sec * 100);

  // Repeatedly pull complete utterances from the front of pcm_.
  for (;;) {
    const int n = static_cast<int>(pcm_.size());
    const int nf = n / frame;  // whole frames available
    if (nf == 0) return;

    // Frame RMS + update the monotonic session peak (drives the relative VAD).
    std::vector<float> rms(nf);
    for (int f = 0; f < nf; ++f) {
      const int b = f * frame;
      double s = 0.0;
      for (int i = 0; i < frame; ++i)
        s += static_cast<double>(pcm_[b + i]) * pcm_[b + i];
      rms[f] = static_cast<float>(std::sqrt(s / frame));
      peak_rms_ = std::max(peak_rms_, rms[f]);
    }
    if (peak_rms_ <= 0.0f) {  // pure silence so far
      if (nf > sil_frames) {  // drop stale leading silence (bound memory)
        const int drop = (nf - sil_frames) * frame;
        pcm_.erase(pcm_.begin(), pcm_.begin() + drop);
        base_sample_ += drop;
      }
      return;
    }
    const float thr = params_.vad_rel_threshold * peak_rms_;

    // First voiced frame.
    int start_f = -1;
    for (int f = 0; f < nf; ++f)
      if (rms[f] > thr) { start_f = f; break; }
    if (start_f < 0) {  // only silence buffered: drop most of it.
      if (nf > sil_frames) {
        const int drop = (nf - sil_frames) * frame;
        pcm_.erase(pcm_.begin(), pcm_.begin() + drop);
        base_sample_ += drop;
      }
      return;
    }
    if (start_f > 0) {  // consume leading silence before the utterance.
      const int drop = start_f * frame;
      pcm_.erase(pcm_.begin(), pcm_.begin() + drop);
      base_sample_ += drop;
      continue;  // re-frame from the new front
    }

    // Walk forward to the end of the utterance: stop after `sil_frames`
    // consecutive silent frames, or at the max-utterance cap.
    int last_voiced = 0;
    int silence_run = 0;
    int end_f = nf;  // exclusive
    bool endpointed = false;
    for (int f = 0; f < nf; ++f) {
      if (rms[f] > thr) {
        last_voiced = f;
        silence_run = 0;
      } else if (++silence_run >= sil_frames && last_voiced + 1 >= min_frames) {
        end_f = last_voiced + 1;
        endpointed = true;
        break;
      }
      if (f + 1 >= max_frames) {  // hard length cap reached
        end_f = f + 1;
        endpointed = true;
        break;
      }
    }

    if (!endpointed && !finalize) return;  // utterance still open; wait for more
    if (last_voiced + 1 < min_frames && !finalize) return;
    if (finalize) end_f = std::min(nf, last_voiced + 1);

    const int begin_s = 0;
    const int end_s = end_f * frame;
    EmitUtterance(begin_s, end_s);

    // Consume the utterance (plus the silence we already scanned through).
    int consume = end_s;
    if (endpointed) consume = std::min(n, end_s + sil_frames * frame);
    pcm_.erase(pcm_.begin(), pcm_.begin() + consume);
    base_sample_ += consume;
    if (finalize) return;  // single trailing utterance flushed
  }
}

void AsrWorker::EmitUtterance(int begin, int end) {
  if (end <= begin) return;
  const int sr = params_.sample_rate;
  const auto t0 = Clock::now();
  std::string text;
  {
    // Serialize GPU access against the diarization worker (one shared device).
    std::lock_guard<std::mutex> gpu(gpu::DeviceLock());
    text = asr_->TranscribeText(pcm_.data() + begin, end - begin);
  }
  compute_sec_ += Secs(t0, Clock::now());
  if (text.empty()) return;

  core::AsrToken tok;
  tok.start_sec = static_cast<double>(base_sample_ + begin) / sr;
  tok.end_sec = static_cast<double>(base_sample_ + end) / sr;
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
  pcm_.clear();
  base_sample_ = 0;
  peak_rms_ = 0.0f;
  processed_samples_.store(0);
  compute_sec_ = 0.0;
  asr_->Reset();
}

}  // namespace pipeline
}  // namespace orator
