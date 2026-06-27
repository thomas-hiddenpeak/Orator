#include "asr_vad_cpu.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <stdexcept>

#include "io/safetensor.h"

namespace orator {
namespace pipeline {

namespace {

template <typename T>
void LoadVec(const io::SafeTensorReader& r, const char* name,
             std::vector<T>* dst) {
  const auto meta = r.GetMetadata(name);
  const size_t n = static_cast<size_t>(meta.data_size) / sizeof(T);
  dst->resize(n);
  r.ReadWeight(name, dst->data(), n * sizeof(T));
}

}  // namespace

AsrSileroVad::AsrSileroVad(const Params& params) : params_(params) {
  if (!InitModel()) {
    throw std::runtime_error("AsrSileroVad: failed to initialize model from " +
                             params_.silero_model_path);
  }
}

bool AsrSileroVad::InitModel() {
  io::SafeTensorReader reader(params_.silero_model_path);

  LoadVec(reader, "stft.forward_basis_buffer", &stft_basis_);

  const char* enc_w_names[] = {
      "encoder.0.reparam_conv.weight", "encoder.1.reparam_conv.weight",
      "encoder.2.reparam_conv.weight", "encoder.3.reparam_conv.weight"};
  const char* enc_b_names[] = {
      "encoder.0.reparam_conv.bias", "encoder.1.reparam_conv.bias",
      "encoder.2.reparam_conv.bias", "encoder.3.reparam_conv.bias"};
  for (int i = 0; i < 4; ++i) {
    LoadVec(reader, enc_w_names[i], &enc_w_[i]);
    LoadVec(reader, enc_b_names[i], &enc_b_[i]);
  }

  LoadVec(reader, "decoder.rnn.weight_ih", &lstm_wih_);
  LoadVec(reader, "decoder.rnn.weight_hh", &lstm_whh_);
  LoadVec(reader, "decoder.rnn.bias_ih", &lstm_bih_);
  LoadVec(reader, "decoder.rnn.bias_hh", &lstm_bhh_);
  LoadVec(reader, "decoder.decoder.2.weight", &dec_w_);

  if (reader.Has("decoder.decoder.2.bias")) {
    std::vector<float> b;
    LoadVec(reader, "decoder.decoder.2.bias", &b);
    dec_b_ = b.empty() ? 0.0f : b[0];
  }

  if (dec_w_.size() > static_cast<size_t>(kLstmHidden)) {
    dec_w_.resize(kLstmHidden);
  } else if (dec_w_.size() < static_cast<size_t>(kLstmHidden)) {
    throw std::runtime_error("AsrSileroVad: decoder weight has invalid size");
  }

  h_state_.assign(kLstmHidden, 0.0f);
  c_state_.assign(kLstmHidden, 0.0f);
  context_.assign(kContextSize, 0.0f);

  initialized_ = true;
  return true;
}

void AsrSileroVad::Push(const float* samples, int n) {
  if (samples == nullptr || n <= 0) return;
  pcm_.insert(pcm_.end(), samples, samples + n);
}

AsrSileroVad::StepResult AsrSileroVad::ProcessWindow(const float* pcm,
                                                     int n_samples) {
  StepResult out;
  if (!initialized_ || pcm == nullptr) return out;

  float input[kTotalInput];
  std::memcpy(input, context_.data(), kContextSize * sizeof(float));
  const int copy_len = std::min(n_samples, kWindowSize);
  std::memcpy(input + kContextSize, pcm, copy_len * sizeof(float));
  if (copy_len < kWindowSize) {
    std::memset(input + kContextSize + copy_len, 0,
                (kWindowSize - copy_len) * sizeof(float));
  }

  float padded[kPaddedLen];
  ReflectPadRight(input, kTotalInput, kPadRight, padded);

  constexpr int kStftOut = 258;
  float stft_out[kStftOut * kStftFrames];
  Conv1d(padded, 1, kPaddedLen, stft_basis_.data(), nullptr, kStftOut, kNfft,
         kHopLength, 0, stft_out, kStftFrames);

  float magnitude[kStftBins * kStftFrames];
  for (int f = 0; f < kStftBins; ++f) {
    for (int t = 0; t < kStftFrames; ++t) {
      const float re = stft_out[f * kStftFrames + t];
      const float im = stft_out[(kStftBins + f) * kStftFrames + t];
      magnitude[f * kStftFrames + t] = std::sqrt(re * re + im * im);
    }
  }

  float enc0[kEnc0Out * kEnc0Frames];
  Conv1d(magnitude, kStftBins, kStftFrames, enc_w_[0].data(), enc_b_[0].data(),
         kEnc0Out, 3, 1, 1, enc0, kEnc0Frames);
  ReluInplace(enc0, kEnc0Out * kEnc0Frames);

  float enc1[kEnc1Out * kEnc1Frames];
  Conv1d(enc0, kEnc0Out, kEnc0Frames, enc_w_[1].data(), enc_b_[1].data(),
         kEnc1Out, 3, 2, 1, enc1, kEnc1Frames);
  ReluInplace(enc1, kEnc1Out * kEnc1Frames);

  float enc2[kEnc2Out * kEnc2Frames];
  Conv1d(enc1, kEnc1Out, kEnc1Frames, enc_w_[2].data(), enc_b_[2].data(),
         kEnc2Out, 3, 2, 1, enc2, kEnc2Frames);
  ReluInplace(enc2, kEnc2Out * kEnc2Frames);

  float enc3[kEnc3Out * kEnc3Frames];
  Conv1d(enc2, kEnc2Out, kEnc2Frames, enc_w_[3].data(), enc_b_[3].data(),
         kEnc3Out, 3, 1, 1, enc3, kEnc3Frames);
  ReluInplace(enc3, kEnc3Out * kEnc3Frames);

  float lstm_in[kLstmHidden];
  for (int i = 0; i < kLstmHidden; ++i) lstm_in[i] = enc3[i];

  LstmCellStep(lstm_in, kLstmHidden, lstm_wih_.data(), lstm_whh_.data(),
               lstm_bih_.data(), lstm_bhh_.data(), h_state_.data(),
               c_state_.data(), kLstmHidden);

  float dot = dec_b_;
  for (int i = 0; i < kLstmHidden; ++i) {
    float v = h_state_[i];
    if (v < 0.0f) v = 0.0f;
    dot += v * dec_w_[i];
  }
  const float prob = 1.0f / (1.0f + std::exp(-dot));

  std::memcpy(context_.data(), input + kTotalInput - kContextSize,
              kContextSize * sizeof(float));

  out.probability = prob;
  out.is_speech = prob >= params_.silero_threshold;
  if (out.is_speech) {
    silence_samples_ = 0;
    speech_samples_ += n_samples;
    const int min_speech =
        params_.silero_min_speech_ms * params_.sample_rate / 1000;
    if (!in_speech_ && speech_samples_ >= min_speech) {
      in_speech_ = true;
      out.segment_start = true;
    }
  } else {
    if (in_speech_) {
      silence_samples_ += n_samples;
      const int min_silence =
          params_.silero_min_silence_ms * params_.sample_rate / 1000;
      if (silence_samples_ >= min_silence) {
        in_speech_ = false;
        out.segment_end = true;
        speech_samples_ = 0;
      }
    } else {
      speech_samples_ = 0;
    }
  }

  return out;
}

bool AsrSileroVad::NextSpan(bool finalize, int* begin, int* end, int* consume) {
  if (begin == nullptr || end == nullptr || consume == nullptr) return false;

  const int sr = params_.sample_rate;
  const int min_len = static_cast<int>(params_.min_utterance_sec * sr);
  const int max_len = static_cast<int>(params_.max_utterance_sec * sr);
  const int pad = params_.silero_speech_pad_ms * sr / 1000;

  while (cursor_ + kWindowSize <= static_cast<int>(pcm_.size())) {
    const int win_begin = cursor_;
    const int win_end = cursor_ + kWindowSize;
    const StepResult st = ProcessWindow(pcm_.data() + win_begin, kWindowSize);
    cursor_ = win_end;

    if (!segment_open_ && st.segment_start) {
      segment_open_ = true;
      segment_start_ = win_begin;
      last_voiced_end_ = win_end;
    }

    if (segment_open_) {
      if (st.is_speech) last_voiced_end_ = win_end;
      const bool hit_cap =
          max_len > 0 && (last_voiced_end_ - segment_start_) >= max_len;
      if (st.segment_end || hit_cap) {
        const int out_begin = std::max(0, segment_start_ - pad);
        const int out_end =
            std::min(static_cast<int>(pcm_.size()), last_voiced_end_ + pad);
        const int out_consume = std::max(out_end, cursor_);
        segment_open_ = false;
        if (out_end - out_begin >= min_len) {
          *begin = out_begin;
          *end = out_end;
          *consume = out_consume;
          return true;
        }
      }
    }
  }

  if (!segment_open_ && cursor_ > 2 * kWindowSize) {
    Consume(cursor_ - kWindowSize);
  }

  if (finalize && segment_open_) {
    const int out_begin = std::max(0, segment_start_ - pad);
    const int out_end =
        std::min(static_cast<int>(pcm_.size()), last_voiced_end_ + pad);
    segment_open_ = false;
    if (out_end - out_begin >= min_len) {
      *begin = out_begin;
      *end = out_end;
      *consume = static_cast<int>(pcm_.size());
      return true;
    }
  }

  return false;
}

bool AsrSileroVad::NextEndpoint(bool finalize, long* endpoint_abs_sample) {
  if (endpoint_abs_sample == nullptr) return false;

  // Run the detector forward over buffered audio. Each window updates the LSTM
  // state and reports a segment_end when a min-silence gap closes a speech run.
  // We report the absolute sample index at that window's end as the endpoint,
  // and keep one window of audio as context for the LSTM on the next call.
  while (cursor_ + kWindowSize <= static_cast<int>(pcm_.size())) {
    const int win_end = cursor_ + kWindowSize;
    const StepResult st = ProcessWindow(pcm_.data() + cursor_, kWindowSize);
    cursor_ = win_end;
    if (st.segment_end) {
      *endpoint_abs_sample = base_sample_ + win_end;
      if (cursor_ > 2 * kWindowSize) Consume(cursor_ - kWindowSize);
      return true;
    }
  }
  // Bound memory between endpoints (keep one window of context for the LSTM).
  if (cursor_ > 2 * kWindowSize) Consume(cursor_ - kWindowSize);
  (void)finalize;
  return false;
}

void AsrSileroVad::Consume(int n) {
  if (n <= 0) return;
  const int drop = std::min<int>(n, static_cast<int>(pcm_.size()));
  if (drop <= 0) return;
  pcm_.erase(pcm_.begin(), pcm_.begin() + drop);
  base_sample_ += drop;
  cursor_ = std::max(0, cursor_ - drop);
  segment_start_ = std::max(0, segment_start_ - drop);
  last_voiced_end_ = std::max(0, last_voiced_end_ - drop);
}

void AsrSileroVad::Reset() {
  std::fill(h_state_.begin(), h_state_.end(), 0.0f);
  std::fill(c_state_.begin(), c_state_.end(), 0.0f);
  std::fill(context_.begin(), context_.end(), 0.0f);
  in_speech_ = false;
  speech_samples_ = 0;
  silence_samples_ = 0;

  pcm_.clear();
  base_sample_ = 0;
  cursor_ = 0;
  segment_open_ = false;
  segment_start_ = 0;
  last_voiced_end_ = 0;
}

std::vector<float> AsrSileroVad::DebugWindowProbs(const float* pcm, int n) {
  Reset();
  std::vector<float> probs;
  if (pcm == nullptr || n <= 0) return probs;
  for (int off = 0; off + kWindowSize <= n; off += kWindowSize) {
    const StepResult st = ProcessWindow(pcm + off, kWindowSize);
    probs.push_back(st.probability);
  }
  return probs;
}

void AsrSileroVad::ReflectPadRight(const float* in, int len, int pad,
                                   float* out) {
  std::memcpy(out, in, len * sizeof(float));
  for (int i = 0; i < pad; ++i) out[len + i] = in[len - 2 - i];
}

void AsrSileroVad::Conv1d(const float* input, int C_in, int L_in,
                          const float* weight, const float* bias, int C_out,
                          int K, int stride, int pad, float* output,
                          int L_out) {
  for (int co = 0; co < C_out; ++co) {
    const float b = bias ? bias[co] : 0.0f;
    for (int t = 0; t < L_out; ++t) {
      float sum = b;
      const int t_start = t * stride - pad;
      for (int ci = 0; ci < C_in; ++ci) {
        for (int k = 0; k < K; ++k) {
          const int pos = t_start + k;
          if (pos >= 0 && pos < L_in) {
            sum += input[ci * L_in + pos] * weight[co * C_in * K + ci * K + k];
          }
        }
      }
      output[co * L_out + t] = sum;
    }
  }
}

void AsrSileroVad::ReluInplace(float* data, int n) {
  for (int i = 0; i < n; ++i) {
    if (data[i] < 0.0f) data[i] = 0.0f;
  }
}

void AsrSileroVad::LstmCellStep(const float* x, int input_size,
                                const float* Wih, const float* Whh,
                                const float* bih, const float* bhh, float* h,
                                float* c, int hidden) {
  const int H4 = 4 * hidden;
  float gates[512];
  assert(H4 <= 512);

  for (int g = 0; g < H4; ++g) {
    float val = bih[g] + bhh[g];
    const float* wr = Wih + g * input_size;
    for (int j = 0; j < input_size; ++j) val += wr[j] * x[j];
    const float* wh = Whh + g * hidden;
    for (int j = 0; j < hidden; ++j) val += wh[j] * h[j];
    gates[g] = val;
  }

  for (int i = 0; i < hidden; ++i) {
    const float gi = 1.0f / (1.0f + std::exp(-gates[0 * hidden + i]));
    const float gf = 1.0f / (1.0f + std::exp(-gates[1 * hidden + i]));
    const float gg = std::tanh(gates[2 * hidden + i]);
    const float go = 1.0f / (1.0f + std::exp(-gates[3 * hidden + i]));
    c[i] = gf * c[i] + gi * gg;
    h[i] = go * std::tanh(c[i]);
  }
}

}  // namespace pipeline
}  // namespace orator
