#include "model/qwen3_asr.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace orator {
namespace model {

Qwen3Asr::Qwen3Asr() = default;

void Qwen3Asr::Initialize(const core::AsrConfig& config) {
  cfg_ = config;
  if (!config.language.empty()) language_ = config.language;
}

void Qwen3Asr::LoadWeights(const std::string& path) {
  weights_ = std::make_unique<io::ShardedSafeTensors>(path);
  mel_ = std::make_unique<feature::WhisperMel>();
  encoder_ = std::make_unique<AsrAudioTower>();
  encoder_->LoadWeights(*weights_);
  decoder_ = std::make_unique<AsrTextDecoder>();
  decoder_->LoadWeights(*weights_);
  if (!tokenizer_.Load(path)) {
    throw std::runtime_error("Qwen3Asr: failed to load tokenizer from " + path);
  }
  loaded_ = true;
}

void Qwen3Asr::Reset() {
  if (decoder_) decoder_->ResetCache();
}

// Build the prompt, inject the encoder output at the audio_pad slots, prefill,
// then greedily decode until EOS. Returns the decoded text (language prefix
// stripped).
std::string Qwen3Asr::BuildAndRun(const std::vector<float>& encoder_out,
                                  int n_tokens) {
  const int H = decoder_->hidden_size();

  // ---- prompt token ids ----
  std::vector<int> prompt;
  prompt.insert(prompt.end(), {kImStart, kSystem, kNewline, kImEnd, kNewline,
                               kImStart, kUser, kNewline, kAudioStart});
  const int audio_pad_start = static_cast<int>(prompt.size());
  for (int i = 0; i < n_tokens; ++i) prompt.push_back(kAudioPad);
  prompt.insert(prompt.end(), {kAudioEnd, kImEnd, kNewline, kImStart,
                               kAssistant, kNewline});
  for (int t : tokenizer_.Encode("language " + language_)) prompt.push_back(t);
  prompt.push_back(kAsrText);

  const int T = static_cast<int>(prompt.size());

  // ---- input embeddings: lookup, then overwrite audio_pad with encoder out ----
  std::vector<float> embeds(static_cast<size_t>(T) * H);
  for (int i = 0; i < T; ++i) decoder_->Embed(prompt[i], embeds.data() + static_cast<size_t>(i) * H);
  for (int i = 0; i < n_tokens; ++i) {
    std::copy(encoder_out.begin() + static_cast<size_t>(i) * H,
              encoder_out.begin() + static_cast<size_t>(i + 1) * H,
              embeds.begin() + static_cast<size_t>(audio_pad_start + i) * H);
  }

  // ---- prefill ----
  const bool prof = std::getenv("ORATOR_ASR_PROFILE") != nullptr;
  auto now = [] { return std::chrono::steady_clock::now(); };
  auto p0 = now();
  decoder_->ResetCache();
  decoder_->Prefill(embeds.data(), T);
  auto p1 = now();

  // ---- greedy autoregressive decode, driven entirely on the GPU. The per-token
  // body (embed-gather -> 28-layer forward -> argmax) is captured as a CUDA graph
  // and replayed in small batches with a host sync per batch. The stop condition
  // (EOS / repetition) is checked only at batch boundaries, so the batch size
  // bounds how many tokens are computed past EOS. batch=4 stops within a few
  // tokens of EOS (short utterances dominate streaming) while keeping the
  // per-token sync overhead amortized. ORATOR_ASR_BATCH overrides it. ----
  std::vector<int> out_tokens =
      decoder_->DecodeGreedy(T, max_new_tokens_, kImEnd, kEndOfText,
                             /*ban_steps=*/3, /*batch=*/4);
  if (prof) {
    auto p2 = now();
    auto ms = [](auto a, auto b) {
      return std::chrono::duration<double, std::milli>(b - a).count();
    };
    std::fprintf(stderr,
                 "[asr-profile]   prefill(T=%d)=%.1fms  decode(%zu tok)=%.1fms"
                 " (%.1fms/tok)\n",
                 T, ms(p0, p1), out_tokens.size(), ms(p1, p2),
                 out_tokens.empty() ? 0.0 : ms(p1, p2) / out_tokens.size());
  }

  std::string text = tokenizer_.Decode(out_tokens, /*skip_special=*/true);
  // Strip the "language X" tag wherever the model echoed it (it sometimes
  // re-emits it mid-stream, not just at the start).
  const std::string tag = "language " + language_;
  for (size_t p = text.find(tag); p != std::string::npos; p = text.find(tag))
    text.erase(p, tag.size());
  return text;
}

std::string Qwen3Asr::TranscribeText(const float* samples, int num_samples) {
  if (!loaded_) throw std::runtime_error("Qwen3Asr: weights not loaded");
  if (samples == nullptr || num_samples <= 0) return "";

  const bool prof = std::getenv("ORATOR_ASR_PROFILE") != nullptr;
  auto now = [] { return std::chrono::steady_clock::now(); };
  auto t0 = now();

  int n_frames = 0;
  std::vector<float> mel = mel_->Compute(samples, num_samples, &n_frames);
  if (n_frames <= 0) return "";
  auto t_mel = now();

  int n_tokens = 0;
  std::vector<float> enc = encoder_->Forward(mel.data(), n_frames, &n_tokens);
  if (n_tokens <= 0) return "";
  auto t_enc = now();

  std::string text = BuildAndRun(enc, n_tokens);
  auto t_dec = now();

  if (prof) {
    auto ms = [](auto a, auto b) {
      return std::chrono::duration<double, std::milli>(b - a).count();
    };
    std::fprintf(stderr,
                 "[asr-profile] mel=%.1fms encoder=%.1fms decode=%.1fms (tokens=%d)\n",
                 ms(t0, t_mel), ms(t_mel, t_enc), ms(t_enc, t_dec), n_tokens);
  }
  return text;
}

// Energy-VAD: frame the signal, mark frames above a relative-RMS threshold as
// speech, then GREEDILY PACK voiced runs into as few segments as possible. Each
// segment grows up to max_segment_sec, absorbing short internal pauses; a new
// segment starts only on a long silence (>= min_silence_sec) or the length cap.
// Few large segments amortise the fixed per-call mel+encoder cost and let the
// decoder's CUDA-graph fast path dominate. Trims leading/trailing silence.
std::vector<Qwen3Asr::Span> Qwen3Asr::SegmentSpeech(const float* x, int n,
                                                    int sr) const {
  std::vector<Span> spans;
  if (x == nullptr || n <= 0) return spans;
  const int frame = std::max(1, sr / 100);  // 10 ms frames
  const int nf = (n + frame - 1) / frame;
  std::vector<float> rms(nf, 0.0f);
  float peak = 0.0f;
  for (int f = 0; f < nf; ++f) {
    const int b = f * frame, e = std::min(n, b + frame);
    double s = 0.0;
    for (int i = b; i < e; ++i) s += static_cast<double>(x[i]) * x[i];
    rms[f] = static_cast<float>(std::sqrt(s / std::max(1, e - b)));
    peak = std::max(peak, rms[f]);
  }
  if (peak <= 0.0f) return spans;
  const float thr = vad_rel_threshold_ * peak;
  const int max_frames = static_cast<int>(max_segment_sec_ * 100);
  const int split_gap = static_cast<int>(min_silence_sec_ * 100);  // silence that splits
  const int min_frames = static_cast<int>(min_speech_sec_ * 100);
  const int pad = static_cast<int>(speech_pad_sec_ * 100);

  // Pass 1: contiguous voiced runs [b,e) in frames.
  std::vector<std::pair<int, int>> runs;
  int rb = -1;
  for (int f = 0; f < nf; ++f) {
    if (rms[f] > thr) {
      if (rb < 0) rb = f;
    } else if (rb >= 0) {
      runs.push_back({rb, f});
      rb = -1;
    }
  }
  if (rb >= 0) runs.push_back({rb, nf});
  if (runs.empty()) return spans;

  // Pass 2: greedily pack runs into segments. Extend the current segment across
  // a gap only if the gap is short (< split_gap) AND the result stays under the
  // length cap; otherwise close it and open a new one at the next run.
  auto emit = [&](int seg_b, int seg_e) {
    int b = std::max(0, seg_b - pad);
    int e = std::min(nf, seg_e + pad);
    if (e - b >= min_frames) spans.push_back({b * frame, std::min(n, e * frame)});
  };
  int seg_b = runs[0].first, seg_e = runs[0].second;
  for (size_t i = 1; i < runs.size(); ++i) {
    const int gap = runs[i].first - seg_e;
    const int span_len = runs[i].second - seg_b;
    if (gap < split_gap && span_len <= max_frames) {
      seg_e = runs[i].second;  // absorb the short pause + next run
    } else {
      emit(seg_b, seg_e);
      seg_b = runs[i].first;
      seg_e = runs[i].second;
    }
  }
  emit(seg_b, seg_e);
  return spans;
}

core::Transcript Qwen3Asr::Transcribe(const core::AudioChunk& audio) {
  core::Transcript out;
  if (audio.samples == nullptr || audio.num_samples <= 0) return out;
  const int sr = audio.sample_rate > 0 ? audio.sample_rate : cfg_.sample_rate;

  std::vector<Span> spans;
  if (vad_enabled_)
    spans = SegmentSpeech(audio.samples, audio.num_samples, sr);
  if (spans.empty())  // VAD off or all-speech: one bounded span
    spans.push_back({0, audio.num_samples});

  for (const Span& s : spans) {
    const int len = s.end - s.begin;
    if (len <= 0) continue;
    std::string text = TranscribeText(audio.samples + s.begin, len);
    if (text.empty()) continue;
    core::AsrToken tok;
    tok.start_sec = audio.t_start_sec + static_cast<double>(s.begin) / sr;
    tok.end_sec = audio.t_start_sec + static_cast<double>(s.end) / sr;
    tok.text = std::move(text);
    out.tokens.push_back(std::move(tok));
  }
  return out;
}

}  // namespace model
}  // namespace orator
