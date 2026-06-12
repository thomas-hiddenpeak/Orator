#include "pipeline/auditory_stream.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

#include "pipeline/diar_postprocess.h"

namespace orator {
namespace pipeline {

namespace {
using Clock = std::chrono::steady_clock;
double Secs(Clock::time_point a, Clock::time_point b) {
  return std::chrono::duration<double>(b - a).count();
}

// Escape a UTF-8 string for embedding in a JSON value (quotes, backslash,
// control chars). Multi-byte UTF-8 passes through unchanged.
std::string JsonEscape(const std::string& s) {
  std::string o;
  o.reserve(s.size() + 8);
  for (char c : s) {
    switch (c) {
      case '"': o += "\\\""; break;
      case '\\': o += "\\\\"; break;
      case '\n': o += "\\n"; break;
      case '\r': o += "\\r"; break;
      case '\t': o += "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", c);
          o += buf;
        } else {
          o += c;
        }
    }
  }
  return o;
}
}  // namespace

AuditoryStream::AuditoryStream(const Config& config, Emit emit)
    : config_(config), emit_(std::move(emit)) {}

void AuditoryStream::Start() {
  diarizer_ = std::make_unique<model::SortformerDiarizer>();
  core::DiarizationConfig dc;
  dc.sample_rate = config_.sample_rate;
  dc.max_speakers = config_.max_speakers;
  dc.activity_threshold = config_.diar_threshold;
  diarizer_->Initialize(dc);
  diarizer_->LoadWeights(config_.diarizer_weights);

  if (!config_.asr_model_dir.empty()) {
    asr_ = std::make_unique<model::Qwen3Asr>();
    core::AsrConfig ac;
    ac.sample_rate = config_.sample_rate;
    ac.language = config_.asr_language;
    asr_->Initialize(ac);
    asr_->set_language(config_.asr_language);
    asr_->LoadWeights(config_.asr_model_dir);
  }
}

double AuditoryStream::audio_sec() const {
  return config_.sample_rate > 0
             ? static_cast<double>(total_samples_) / config_.sample_rate
             : 0.0;
}

void AuditoryStream::PushAudio(const float* samples, int n) {
  if (samples == nullptr || n <= 0) return;

  // --- DIARIZATION pipeline: incremental, persistent state, O(n). ---
  auto d0 = Clock::now();
  core::DiarizationFrames part =
      diarizer_->StreamAudio(samples, n, /*finalize=*/false);
  diar_compute_sec_ += Secs(d0, Clock::now());
  if (part.num_frames > 0) {
    diar_probs_.insert(diar_probs_.end(), part.probs.begin(), part.probs.end());
    diar_total_frames_ += part.num_frames;
  }

  total_samples_ += n;  // shared clock advances once for the ingested audio

  // --- ASR pipeline: buffer + endpoint independently. ---
  if (asr_) {
    asr_pcm_.insert(asr_pcm_.end(), samples, samples + n);
    DrainAsr(/*finalize=*/false);
  }
}

void AuditoryStream::DrainAsr(bool finalize) {
  const int sr = config_.sample_rate;
  const int frame = std::max(1, sr / 100);  // 10 ms RMS frames
  const int sil_frames = static_cast<int>(config_.asr_endpoint_silence_sec * 100);
  const int max_frames = static_cast<int>(config_.asr_max_utterance_sec * 100);
  const int min_frames = static_cast<int>(config_.asr_min_utterance_sec * 100);

  // Repeatedly pull complete utterances from the front of asr_pcm_.
  for (;;) {
    const int n = static_cast<int>(asr_pcm_.size());
    const int nf = n / frame;  // whole frames available
    if (nf == 0) return;

    // Frame RMS + update the monotonic session peak (drives the relative VAD).
    std::vector<float> rms(nf);
    for (int f = 0; f < nf; ++f) {
      const int b = f * frame;
      double s = 0.0;
      for (int i = 0; i < frame; ++i) s += static_cast<double>(asr_pcm_[b + i]) * asr_pcm_[b + i];
      rms[f] = static_cast<float>(std::sqrt(s / frame));
      asr_peak_rms_ = std::max(asr_peak_rms_, rms[f]);
    }
    if (asr_peak_rms_ <= 0.0f) {  // pure silence so far
      if (nf > sil_frames) {      // drop stale leading silence (bound memory)
        const int drop = (nf - sil_frames) * frame;
        asr_pcm_.erase(asr_pcm_.begin(), asr_pcm_.begin() + drop);
        asr_base_sample_ += drop;
      }
      return;
    }
    const float thr = config_.asr_vad_rel_threshold * asr_peak_rms_;

    // First voiced frame.
    int start_f = -1;
    for (int f = 0; f < nf; ++f)
      if (rms[f] > thr) { start_f = f; break; }
    if (start_f < 0) {  // only silence buffered: drop most of it.
      if (nf > sil_frames) {
        const int drop = (nf - sil_frames) * frame;
        asr_pcm_.erase(asr_pcm_.begin(), asr_pcm_.begin() + drop);
        asr_base_sample_ += drop;
      }
      return;
    }
    if (start_f > 0) {  // consume leading silence before the utterance.
      const int drop = start_f * frame;
      asr_pcm_.erase(asr_pcm_.begin(), asr_pcm_.begin() + drop);
      asr_base_sample_ += drop;
      continue;  // re-frame from the new front
    }

    // Walk forward to the end of the utterance: stop after `sil_frames`
    // consecutive silent frames, or at the max-utterance cap.
    int last_voiced = 0;
    int silence_run = 0;
    int end_f = nf;            // exclusive
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
    asr_pcm_.erase(asr_pcm_.begin(), asr_pcm_.begin() + consume);
    asr_base_sample_ += consume;
    if (finalize) return;  // single trailing utterance flushed
  }
}

void AuditoryStream::EmitUtterance(int begin, int end) {
  if (end <= begin) return;
  const int sr = config_.sample_rate;
  auto a0 = Clock::now();
  std::string text = asr_->TranscribeText(asr_pcm_.data() + begin, end - begin);
  asr_compute_sec_ += Secs(a0, Clock::now());
  if (text.empty()) return;

  core::AsrToken tok;
  tok.start_sec = static_cast<double>(asr_base_sample_ + begin) / sr;
  tok.end_sec = static_cast<double>(asr_base_sample_ + end) / sr;
  tok.text = text;
  transcript_.tokens.push_back(tok);

  char buf[128];
  std::snprintf(buf, sizeof(buf),
                "{\"type\":\"asr\",\"start\":%.3f,\"end\":%.3f,\"text\":\"",
                tok.start_sec, tok.end_sec);
  if (emit_) emit_(std::string(buf) + JsonEscape(tok.text) + "\"}");
}

void AuditoryStream::EmitTimeline(bool finalize) {
  // Drain pipeline tails when finalizing (each independently).
  if (finalize) {
    auto d0 = Clock::now();
    core::DiarizationFrames tail = diarizer_->StreamAudio(nullptr, 0, true);
    diar_compute_sec_ += Secs(d0, Clock::now());
    if (tail.num_frames > 0) {
      diar_probs_.insert(diar_probs_.end(), tail.probs.begin(), tail.probs.end());
      diar_total_frames_ += tail.num_frames;
    }
    if (asr_) DrainAsr(/*finalize=*/true);
  }

  // Diarization frames -> segments on the shared clock.
  diar_segments_.clear();
  if (diar_total_frames_ > 0) {
    core::DiarizationFrames frames;
    frames.num_frames = static_cast<int>(diar_total_frames_);
    frames.num_speakers = config_.max_speakers;
    frames.frame_period_sec = diarizer_->frame_period_sec();
    frames.t_start_sec = 0.0;
    frames.probs = diar_probs_;
    auto segs = FramesToSegments(frames, config_.diar_threshold,
                                 config_.diar_merge_gap_sec);
    diar_segments_ = CoalesceSegments(std::move(segs), config_.diar_merge_gap_sec);
  }

  // Build the unified timeline document: independent diarization + transcript
  // arrays on one clock (fusion/attribution is a downstream concern).
  std::string out = "{\"type\":\"timeline\",";
  char buf[256];
  std::snprintf(buf, sizeof(buf),
                "\"audio_sec\":%.3f,\"diar_compute_sec\":%.3f,"
                "\"asr_compute_sec\":%.3f,",
                audio_sec(), diar_compute_sec_, asr_compute_sec_);
  out += buf;

  out += "\"diarization\":[";
  for (size_t i = 0; i < diar_segments_.size(); ++i) {
    const auto& s = diar_segments_[i];
    std::snprintf(buf, sizeof(buf),
                  "{\"start\":%.3f,\"end\":%.3f,\"speaker\":%d,\"confidence\":%.3f}",
                  s.start_sec, s.end_sec, s.local_speaker, s.confidence);
    out += buf;
    if (i + 1 < diar_segments_.size()) out += ",";
  }
  out += "],\"transcript\":[";
  for (size_t i = 0; i < transcript_.tokens.size(); ++i) {
    const auto& t = transcript_.tokens[i];
    std::snprintf(buf, sizeof(buf), "{\"start\":%.3f,\"end\":%.3f,\"text\":\"",
                  t.start_sec, t.end_sec);
    out += std::string(buf) + JsonEscape(t.text) + "\"}";
    if (i + 1 < transcript_.tokens.size()) out += ",";
  }
  out += "]}";

  if (emit_) emit_(out);
}

void AuditoryStream::Reset() {
  diar_probs_.clear();
  diar_total_frames_ = 0;
  total_samples_ = 0;
  diar_compute_sec_ = 0.0;
  if (diarizer_) diarizer_->Reset();

  asr_pcm_.clear();
  asr_base_sample_ = 0;
  asr_peak_rms_ = 0.0f;
  transcript_.tokens.clear();
  asr_compute_sec_ = 0.0;
  if (asr_) asr_->Reset();
}

}  // namespace pipeline
}  // namespace orator
