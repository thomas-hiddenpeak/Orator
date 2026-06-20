#include "model/qwen3_asr.h"

#include "core/log.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace orator {
namespace model {

namespace {
int EnvIntOr(const char* name, int fallback) {
  const char* v = std::getenv(name);
  if (v == nullptr || *v == '\0') return fallback;
  char* end = nullptr;
  long parsed = std::strtol(v, &end, 10);
  if (end == v || (end != nullptr && *end != '\0')) return fallback;
  if (parsed <= 0) return fallback;
  return static_cast<int>(parsed);
}
}  // namespace

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
  seg_pcm_.clear();
  seg_encoded_frames_ = 0;
  stream_cache_ckpt_ = 0;
  stream_audio_tokens_ = 0;
  stream_chunk_id_ = 0;
  stream_raw_decoded_.clear();
  stream_active_ = false;
}

// Build the prompt, inject the encoder output at the audio_pad slots, prefill,
// then greedily decode until EOS. Returns the decoded text (language prefix
// stripped).
std::string Qwen3Asr::BuildAndRun(const std::vector<float>& encoder_out,
                                  int n_tokens, const std::string& prefix_text,
                                  cudaStream_t stream) {
  const int H = decoder_->hidden_size();

  // ---- prompt token ids ----
  // Optional system prompt, configurable via ORATOR_ASR_SYSTEM_PROMPT.
  // Default: a short Chinese ASR guidance string proven to stabilise output.
  const char* sys_env = std::getenv("ORATOR_ASR_SYSTEM_PROMPT");
  const std::string sys_prompt = (sys_env != nullptr && sys_env[0] != '\0')
      ? std::string(sys_env)
      : "你是一个专业的中文普通话语音识别系统，请准确识别并转录所有语音内容。";

  std::vector<int> prompt;
  prompt.insert(prompt.end(), {kImStart, kSystem, kNewline});
  for (int t : tokenizer_.Encode(sys_prompt)) prompt.push_back(t);
  prompt.insert(prompt.end(), {kImEnd, kNewline,
                               kImStart, kUser, kNewline, kAudioStart});
  const int audio_pad_start = static_cast<int>(prompt.size());
  for (int i = 0; i < n_tokens; ++i) prompt.push_back(kAudioPad);
  prompt.insert(prompt.end(), {kAudioEnd, kImEnd, kNewline, kImStart,
                               kAssistant, kNewline});
  if (!language_.empty()) {
    for (int t : tokenizer_.Encode("language " + language_))
      prompt.push_back(t);
  }
  prompt.push_back(kAsrText);
  // Committed text prefix: the model continues from it (growing-window
  // streaming). Empty for the single-segment path.
  if (!prefix_text.empty())
    for (int t : tokenizer_.Encode(prefix_text)) prompt.push_back(t);

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
  decoder_->Prefill(embeds.data(), T, stream);
  auto p1 = now();

  // ---- greedy autoregressive decode, driven entirely on the GPU. The per-token
  // body (embed-gather -> 28-layer forward -> argmax) is captured as a CUDA graph
  // and replayed in small batches with a host sync per batch. The stop condition
  // (EOS / repetition) is checked only at batch boundaries, so the batch size
  // bounds how many tokens are computed past EOS. batch=4 stops within a few
  // tokens of EOS (short utterances dominate streaming) while keeping the
  // per-token sync overhead amortized. ORATOR_ASR_BATCH overrides it. ----
  const int ban_steps = EnvIntOr("ORATOR_ASR_BAN_STEPS", 3);
  const int decode_batch = EnvIntOr("ORATOR_ASR_DECODE_BATCH", 4);
  std::vector<int> out_tokens =
      decoder_->DecodeGreedy(T, max_new_tokens_, kImEnd, kEndOfText,
                             ban_steps, decode_batch, stream);
  if (prof) {
    auto p2 = now();
    auto ms = [](auto a, auto b) {
      return std::chrono::duration<double, std::milli>(b - a).count();
    };
    LOG_INFO("[asr-profile]   prefill(T=%d)=%.1fms  decode(%zu tok)=%.1fms"
             " (%.1fms/tok)\n",
             T, ms(p0, p1), out_tokens.size(), ms(p1, p2),
             out_tokens.empty() ? 0.0 : ms(p1, p2) / out_tokens.size());
  }

  std::string text = tokenizer_.Decode(out_tokens, /*skip_special=*/true);
  // Strip the "language X" tag wherever the model echoed it (it sometimes
  // re-emits it mid-stream, not just at the start).
  if (!language_.empty()) {
    const std::string tag = "language " + language_;
    for (size_t p = text.find(tag); p != std::string::npos; p = text.find(tag))
      text.erase(p, tag.size());
  }
  return text;
}

std::string Qwen3Asr::TranscribeText(const float* samples, int num_samples,
                                     cudaStream_t stream) {
  if (!loaded_) throw std::runtime_error("Qwen3Asr: weights not loaded");
  if (samples == nullptr || num_samples <= 0) return "";

  const bool prof = std::getenv("ORATOR_ASR_PROFILE") != nullptr;
  auto now = [] { return std::chrono::steady_clock::now(); };
  auto t0 = now();

  int n_frames = 0;
  std::vector<float> mel = mel_->Compute(samples, num_samples, &n_frames, stream);
  if (n_frames <= 0) return "";
  auto t_mel = now();

  int n_tokens = 0;
  std::vector<float> enc = encoder_->Forward(mel.data(), n_frames, &n_tokens, stream);
  if (n_tokens <= 0) return "";
  auto t_enc = now();

  std::string text = BuildAndRun(enc, n_tokens, /*prefix_text=*/"", stream);
  auto t_dec = now();

  if (prof) {
    auto ms = [](auto a, auto b) {
      return std::chrono::duration<double, std::milli>(b - a).count();
    };
    LOG_INFO("[asr-profile] mel=%.1fms encoder=%.1fms decode=%.1fms (tokens=%d)\n",
             ms(t0, t_mel), ms(t_mel, t_enc), ms(t_enc, t_dec), n_tokens);
  }
  return text;
}

std::string Qwen3Asr::TranscribeWindow(const float* samples, int num_samples,
                                       const std::string& prefix_text,
                                       cudaStream_t stream) {
  if (!loaded_) throw std::runtime_error("Qwen3Asr: weights not loaded");
  if (samples == nullptr || num_samples <= 0) return "";

  int n_frames = 0;
  std::vector<float> mel = mel_->Compute(samples, num_samples, &n_frames, stream);
  if (n_frames <= 0) return "";

  int n_tokens = 0;
  std::vector<float> enc = encoder_->Forward(mel.data(), n_frames, &n_tokens, stream);
  if (n_tokens <= 0) return "";

  // Returns only the newly generated continuation; the caller holds the prefix.
  return BuildAndRun(enc, n_tokens, prefix_text, stream);
}

// ---- Incremental KV-cache streaming session (Spec 003) --------------------
// StreamReset prefills the fixed system prefix (up to <audio_start>) ONCE and
// records the checkpoint. StreamChunk encodes each completed 8 s window
// standalone and appends its audio-token KV after the checkpoint (reusing the
// persistent system + earlier-audio KV), then re-prefills the short suffix and
// decodes. StreamFinalize flushes the residual tail.

void Qwen3Asr::StreamReset(long base_sample) {
  if (!loaded_) throw std::runtime_error("Qwen3Asr: weights not loaded");
  decoder_->ResetCache();
  seg_pcm_.clear();
  seg_base_sample_ = base_sample;
  seg_encoded_frames_ = 0;
  stream_audio_tokens_ = 0;
  stream_chunk_id_ = 0;
  stream_raw_decoded_.clear();

  // Fixed system prefix up to and including <audio_start>. Matches BuildAndRun.
  const char* sys_env = std::getenv("ORATOR_ASR_SYSTEM_PROMPT");
  const std::string sys_prompt = (sys_env != nullptr && sys_env[0] != '\0')
      ? std::string(sys_env)
      : "你是一个专业的中文普通话语音识别系统，请准确识别并转录所有语音内容。";
  std::vector<int> prefix;
  prefix.insert(prefix.end(), {kImStart, kSystem, kNewline});
  for (int t : tokenizer_.Encode(sys_prompt)) prefix.push_back(t);
  prefix.insert(prefix.end(),
                {kImEnd, kNewline, kImStart, kUser, kNewline, kAudioStart});

  const int H = decoder_->hidden_size();
  const int P = static_cast<int>(prefix.size());
  std::vector<float> embeds(static_cast<size_t>(P) * H);
  for (int i = 0; i < P; ++i)
    decoder_->Embed(prefix[i], embeds.data() + static_cast<size_t>(i) * H);
  decoder_->PrefillAt(embeds.data(), P, 0);
  stream_cache_ckpt_ = P;
  stream_active_ = true;
}

std::string Qwen3Asr::StreamChunk(const float* pcm, int n, cudaStream_t stream) {
  if (!loaded_) throw std::runtime_error("Qwen3Asr: weights not loaded");
  if (!stream_active_) StreamReset(0);
  if (pcm != nullptr && n > 0)
    seg_pcm_.insert(seg_pcm_.end(), pcm, pcm + n);

  // Only do work once at least one full 8 s window of new audio is available.
  const int hop = 160;  // WhisperFeatureExtractor hop
  const int avail_frames = static_cast<int>(seg_pcm_.size() / hop);
  if (avail_frames - seg_encoded_frames_ < kStreamWindowMel)
    return CurrentLiveText();

  // Mel over the full (bounded) segment; interior frames are stream-stable.
  int n_frames = 0;
  std::vector<float> mel =
      mel_->Compute(seg_pcm_.data(), static_cast<int>(seg_pcm_.size()),
                    &n_frames, stream);

  bool changed = false;
  const int F = 128;
  while (n_frames - seg_encoded_frames_ >= kStreamWindowMel) {
    // Slice this window's mel columns [seg_encoded_frames_, +kStreamWindowMel)
    // into a contiguous [128, 800] for a standalone (chunk-local) encode.
    std::vector<float> sub(static_cast<size_t>(F) * kStreamWindowMel);
    for (int f = 0; f < F; ++f)
      for (int t = 0; t < kStreamWindowMel; ++t)
        sub[static_cast<size_t>(f) * kStreamWindowMel + t] =
            mel[static_cast<size_t>(f) * n_frames + (seg_encoded_frames_ + t)];

    int toks = 0;
    std::vector<float> enc =
        encoder_->Forward(sub.data(), kStreamWindowMel, &toks, stream);
    if (toks <= 0) break;
    // Append the new window's audio-token KV after the cached checkpoint. The
    // audio-pad embedding is overwritten by the encoder output, so the encoder
    // output IS the embedding at those positions.
    decoder_->PrefillAt(enc.data(), toks, stream_cache_ckpt_, stream);
    stream_cache_ckpt_ += toks;
    stream_audio_tokens_ += toks;
    seg_encoded_frames_ += kStreamWindowMel;
    changed = true;
  }
  if (!changed) return CurrentLiveText();
  return StreamDecodeStep(stream);
}

std::string Qwen3Asr::StreamDecodeStep(cudaStream_t stream) {
  const int H = decoder_->hidden_size();

  // Rollback the unfixed tail: first `unfixed_chunks` windows use an empty
  // prefix; afterwards, drop the last `unfixed_tokens` tokens of the running
  // decode (utf-8 safe) and re-decode them as the continuation is revised.
  std::string prefix_str;
  if (stream_chunk_id_ >= stream_unfixed_chunks_ &&
      !stream_raw_decoded_.empty()) {
    std::vector<int> cur = tokenizer_.Encode(stream_raw_decoded_);
    int k = stream_unfixed_tokens_;
    while (true) {
      int end = std::max(0, static_cast<int>(cur.size()) - k);
      if (end <= 0) { prefix_str.clear(); break; }
      std::vector<int> sub(cur.begin(), cur.begin() + end);
      prefix_str = tokenizer_.Decode(sub, /*skip_special=*/true);
      // Guard against cutting a multi-byte UTF-8 character (U+FFFD = EF BF BD).
      if (prefix_str.find("\xEF\xBF\xBD") == std::string::npos) break;
      ++k;
    }
  }

  // Suffix after the audio block: audio_end, assistant header, language tag,
  // <asr_text>, then the committed prefix text.
  std::vector<int> suffix = {kAudioEnd, kImEnd, kNewline,
                             kImStart, kAssistant, kNewline};
  if (!language_.empty())
    for (int t : tokenizer_.Encode("language " + language_))
      suffix.push_back(t);
  suffix.push_back(kAsrText);
  if (!prefix_str.empty())
    for (int t : tokenizer_.Encode(prefix_str)) suffix.push_back(t);

  const int S = static_cast<int>(suffix.size());
  std::vector<float> embeds(static_cast<size_t>(S) * H);
  for (int i = 0; i < S; ++i)
    decoder_->Embed(suffix[i], embeds.data() + static_cast<size_t>(i) * H);
  // Prefill the suffix after the persistent audio block (does not advance the
  // checkpoint; truncated after decode).
  decoder_->PrefillAt(embeds.data(), S, stream_cache_ckpt_, stream);

  const int ban_steps = EnvIntOr("ORATOR_ASR_BAN_STEPS", 3);
  const int decode_batch = EnvIntOr("ORATOR_ASR_DECODE_BATCH", 4);
  std::vector<int> gen =
      decoder_->DecodeGreedy(stream_cache_ckpt_ + S, max_new_tokens_, kImEnd,
                             kEndOfText, ban_steps, decode_batch, stream);
  // Drop the suffix + generated KV; keep the persistent [system][audio] cache.
  decoder_->TruncateCache(stream_cache_ckpt_);

  std::string gen_text = tokenizer_.Decode(gen, /*skip_special=*/true);
  stream_raw_decoded_ = prefix_str + gen_text;
  ++stream_chunk_id_;
  return CurrentLiveText();
}

std::string Qwen3Asr::StreamFinalize(cudaStream_t stream) {
  if (!stream_active_) return CurrentLiveText();

  // Flush the residual (< 8 s) tail, if any: encode the remaining mel frames
  // (the last conv chunk is padded, as in the single-segment path) and append.
  const int hop = 160;
  const int avail_frames = static_cast<int>(seg_pcm_.size() / hop);
  bool appended = false;
  if (avail_frames - seg_encoded_frames_ > 0) {
    int n_frames = 0;
    std::vector<float> mel =
        mel_->Compute(seg_pcm_.data(), static_cast<int>(seg_pcm_.size()),
                      &n_frames, stream);
    const int rem = n_frames - seg_encoded_frames_;
    if (rem > 0) {
      const int F = 128;
      std::vector<float> sub(static_cast<size_t>(F) * rem);
      for (int f = 0; f < F; ++f)
        for (int t = 0; t < rem; ++t)
          sub[static_cast<size_t>(f) * rem + t] =
              mel[static_cast<size_t>(f) * n_frames + (seg_encoded_frames_ + t)];
      int toks = 0;
      std::vector<float> enc = encoder_->Forward(sub.data(), rem, &toks, stream);
      if (toks > 0) {
        decoder_->PrefillAt(enc.data(), toks, stream_cache_ckpt_, stream);
        stream_cache_ckpt_ += toks;
        stream_audio_tokens_ += toks;
        seg_encoded_frames_ = n_frames;
        appended = true;
      }
    }
  }

  // Decode again only if new tail audio was appended; otherwise the last
  // StreamChunk already produced the final text. With no audio at all in the
  // segment, return empty (avoids the model echoing the system prompt).
  std::string text;
  if (appended) {
    text = StreamDecodeStep(stream);
  } else if (stream_audio_tokens_ > 0) {
    text = CurrentLiveText();
  }
  stream_active_ = false;
  return text;
}

std::string Qwen3Asr::CurrentLiveText() const {
  std::string text = stream_raw_decoded_;
  if (!language_.empty()) {
    const std::string tag = "language " + language_;
    for (size_t p = text.find(tag); p != std::string::npos; p = text.find(tag))
      text.erase(p, tag.size());
  }
  return text;
}


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
