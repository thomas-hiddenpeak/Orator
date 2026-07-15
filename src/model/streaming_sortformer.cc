#include "model/streaming_sortformer.h"

#include <cuda_runtime.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <limits>
#include <numeric>
#include <stdexcept>

#include "gpu/memory.h"

namespace orator {
namespace model {

using ::orator::gpu::CheckCudaError;

namespace {

constexpr float kPredScoreThreshold = 0.25f;
constexpr float kScoresBoostLatest = 0.05f;
constexpr float kSilThreshold = 0.2f;
constexpr float kStrongBoostRate = 0.75f;
constexpr float kWeakBoostRate = 1.5f;
constexpr float kMinPosScoresRate = 0.5f;
const float kNegInf = -std::numeric_limits<float>::infinity();
const float kPosInf = std::numeric_limits<float>::infinity();

// preds/scores are row-major [n_frames, n_spk].

// _get_log_pred_scores: high for confident non-overlapped speech.
void GetLogPredScores(const std::vector<float>& preds, int n, int n_spk,
                      std::vector<float>& scores) {
  scores.assign(static_cast<size_t>(n) * n_spk, 0.0f);
  std::vector<float> log_p(static_cast<size_t>(n) * n_spk);
  std::vector<float> log_1p(static_cast<size_t>(n) * n_spk);
  for (int f = 0; f < n; ++f) {
    for (int s = 0; s < n_spk; ++s) {
      float p = preds[static_cast<size_t>(f) * n_spk + s];
      log_p[static_cast<size_t>(f) * n_spk + s] =
          std::log(std::max(p, kPredScoreThreshold));
      log_1p[static_cast<size_t>(f) * n_spk + s] =
          std::log(std::max(1.0f - p, kPredScoreThreshold));
    }
  }
  for (int f = 0; f < n; ++f) {
    float sum1p = 0.0f;
    for (int s = 0; s < n_spk; ++s)
      sum1p += log_1p[static_cast<size_t>(f) * n_spk + s];
    for (int s = 0; s < n_spk; ++s) {
      size_t i = static_cast<size_t>(f) * n_spk + s;
      scores[i] = log_p[i] - log_1p[i] + sum1p - std::log(0.5f);
    }
  }
}

// _disable_low_scores: non-speech -> -inf; non-positive -> -inf if a speaker
// has at least min_pos positive-scored frames.
void DisableLowScores(const std::vector<float>& preds, int n, int n_spk,
                      int min_pos, std::vector<float>& scores) {
  for (int f = 0; f < n; ++f)
    for (int s = 0; s < n_spk; ++s) {
      size_t i = static_cast<size_t>(f) * n_spk + s;
      if (!(preds[i] > 0.5f)) scores[i] = kNegInf;
    }
  std::vector<int> pos_count(n_spk, 0);
  for (int f = 0; f < n; ++f)
    for (int s = 0; s < n_spk; ++s)
      if (scores[static_cast<size_t>(f) * n_spk + s] > 0.0f) pos_count[s]++;
  for (int f = 0; f < n; ++f)
    for (int s = 0; s < n_spk; ++s) {
      size_t i = static_cast<size_t>(f) * n_spk + s;
      bool is_speech = preds[i] > 0.5f;
      bool is_pos = scores[i] > 0.0f;
      if ((!is_pos) && is_speech && pos_count[s] >= min_pos)
        scores[i] = kNegInf;
    }
}

// _boost_topk_scores: boost the n_boost highest scores per speaker by
// -scale*log(0.5). '-inf' entries remain '-inf'.
void BoostTopkScores(int n, int n_spk, int n_boost, float scale,
                     std::vector<float>& scores) {
  if (n_boost <= 0) return;
  const float delta = -scale * std::log(0.5f);
  for (int s = 0; s < n_spk; ++s) {
    std::vector<int> idx(n);
    std::iota(idx.begin(), idx.end(), 0);
    std::partial_sort(idx.begin(), idx.begin() + std::min(n_boost, n),
                      idx.end(), [&](int a, int b) {
                        float va = scores[static_cast<size_t>(a) * n_spk + s];
                        float vb = scores[static_cast<size_t>(b) * n_spk + s];
                        if (va != vb) return va > vb;
                        return a < b;
                      });
    int lim = std::min(n_boost, n);
    for (int k = 0; k < lim; ++k) {
      size_t i = static_cast<size_t>(idx[k]) * n_spk + s;
      scores[i] += delta;  // -inf + finite stays -inf
    }
  }
}

// _get_silence_profile: update running mean silence embedding.
void UpdateSilenceProfile(const std::vector<float>& embs,
                          const std::vector<float>& preds, int n, int n_spk,
                          int emb_dim, std::vector<float>& mean_sil_emb,
                          long& n_sil_frames) {
  std::vector<char> is_sil(n, 0);
  long sil_count = 0;
  for (int f = 0; f < n; ++f) {
    float sump = 0.0f;
    for (int s = 0; s < n_spk; ++s)
      sump += preds[static_cast<size_t>(f) * n_spk + s];
    if (sump < kSilThreshold) {
      is_sil[f] = 1;
      ++sil_count;
    }
  }
  if (sil_count == 0) return;
  long upd_n = n_sil_frames + sil_count;
  for (int d = 0; d < emb_dim; ++d) {
    float sil_sum = 0.0f;
    for (int f = 0; f < n; ++f)
      if (is_sil[f]) sil_sum += embs[static_cast<size_t>(f) * emb_dim + d];
    float old_sum = mean_sil_emb[d] * static_cast<float>(n_sil_frames);
    mean_sil_emb[d] =
        (old_sum + sil_sum) / static_cast<float>(std::max(upd_n, 1L));
  }
  n_sil_frames = upd_n;
}

// _compress_spkcache (permute_spk=false): keep the spkcache_len most important
// frames out of n_frames, ordered by original index. emb_seq:[n,emb_dim],
// preds:[n,n_spk]. Outputs spkcache:[spkcache_len,emb_dim], preds:[..,n_spk].
void CompressSpkcache(const std::vector<float>& emb_seq,
                      const std::vector<float>& preds, int n, int n_spk,
                      int emb_dim, int spkcache_len,
                      int spkcache_sil_frames_per_spk,
                      const std::vector<float>& mean_sil_emb,
                      std::vector<float>& out_emb,
                      std::vector<float>& out_preds) {
  const int spkcache_len_per_spk =
      spkcache_len / n_spk - spkcache_sil_frames_per_spk;
  const int strong =
      static_cast<int>(std::floor(spkcache_len_per_spk * kStrongBoostRate));
  const int weak =
      static_cast<int>(std::floor(spkcache_len_per_spk * kWeakBoostRate));
  const int min_pos =
      static_cast<int>(std::floor(spkcache_len_per_spk * kMinPosScoresRate));

  std::vector<float> scores;
  GetLogPredScores(preds, n, n_spk, scores);
  DisableLowScores(preds, n, n_spk, min_pos, scores);
  // boost newly added frames (index >= spkcache_len)
  if (kScoresBoostLatest > 0.0f) {
    for (int f = spkcache_len; f < n; ++f)
      for (int s = 0; s < n_spk; ++s) {
        size_t i = static_cast<size_t>(f) * n_spk + s;
        if (scores[i] != kNegInf) scores[i] += kScoresBoostLatest;
      }
  }
  BoostTopkScores(n, n_spk, strong, 2.0f, scores);
  BoostTopkScores(n, n_spk, weak, 1.0f, scores);

  // Append one configured block of +inf silence placeholders. NeMo includes
  // these in cache selection and replaces selected placeholders with the
  // running mean silence embedding.
  const int n_pad = n + spkcache_sil_frames_per_spk;
  std::vector<float> scores_pad(static_cast<size_t>(n_pad) * n_spk);
  std::copy(scores.begin(), scores.end(), scores_pad.begin());
  for (int f = n; f < n_pad; ++f)
    for (int s = 0; s < n_spk; ++s)
      scores_pad[static_cast<size_t>(f) * n_spk + s] = kPosInf;

  // _get_topk_indices: flatten speaker-major (index = s*n_pad + f), take the
  // spkcache_len highest, sort ascending, mark '-inf' picks and silence-pad
  // rows as disabled.
  const long total = static_cast<long>(n_spk) * n_pad;
  std::vector<long> all(total);
  std::iota(all.begin(), all.end(), 0L);
  auto val_of = [&](long flat) {
    int s = static_cast<int>(flat / n_pad);
    int f = static_cast<int>(flat % n_pad);
    return scores_pad[static_cast<size_t>(f) * n_spk + s];
  };
  std::partial_sort(all.begin(), all.begin() + spkcache_len, all.end(),
                    [&](long a, long b) {
                      float va = val_of(a), vb = val_of(b);
                      if (va != vb) return va > vb;
                      return a < b;
                    });
  std::vector<long> topk(all.begin(), all.begin() + spkcache_len);
  const long kMaxIndex = 99999;
  for (long& v : topk)
    if (val_of(v) == kNegInf) v = kMaxIndex;
  std::sort(topk.begin(), topk.end());
  std::vector<int> frame_idx(spkcache_len);
  std::vector<char> disabled(spkcache_len, 0);
  for (int k = 0; k < spkcache_len; ++k) {
    long v = topk[k];
    if (v == kMaxIndex) {
      disabled[k] = 1;
      frame_idx[k] = 0;
      continue;
    }
    int f = static_cast<int>(v % n_pad);  // remainder by padded n_frames
    if (f >= n) {                         // silence-pad row
      disabled[k] = 1;
      f = 0;
    }
    frame_idx[k] = f;
  }

  // _gather_spkcache_and_preds
  out_emb.assign(static_cast<size_t>(spkcache_len) * emb_dim, 0.0f);
  out_preds.assign(static_cast<size_t>(spkcache_len) * n_spk, 0.0f);
  for (int k = 0; k < spkcache_len; ++k) {
    if (disabled[k]) {
      for (int d = 0; d < emb_dim; ++d)
        out_emb[static_cast<size_t>(k) * emb_dim + d] = mean_sil_emb[d];
      // preds stay zero
    } else {
      int f = frame_idx[k];
      for (int d = 0; d < emb_dim; ++d)
        out_emb[static_cast<size_t>(k) * emb_dim + d] =
            emb_seq[static_cast<size_t>(f) * emb_dim + d];
      for (int s = 0; s < n_spk; ++s)
        out_preds[static_cast<size_t>(k) * n_spk + s] =
            preds[static_cast<size_t>(f) * n_spk + s];
    }
  }
}

}  // namespace

bool SortformerConfig::Validate() const {
  if (sample_rate <= 0 || mel_features <= 0 || n_fft <= 0) return false;
  if (window_size_sec <= 0 || hop_size_sec <= 0) return false;
  if (max_num_speakers <= 0 || max_num_speakers > 12) return false;
  if (encoder_subsampling_factor <= 0) return false;
  if (chunk_len <= 0 || spkcache_len <= 0) return false;
  if (spkcache_update_period <= 0) return false;
  if (chunk_left_context < 0 || chunk_right_context < 0) return false;
  if (fifo_len < 0 || spkcache_sil_frames_per_spk < 0) return false;
  if (spkcache_len <
      (1 + spkcache_sil_frames_per_spk) * max_num_speakers) {
    return false;
  }
  return true;
}

SortformerState::SortformerState(const SortformerConfig& cfg)
    : spkcache(static_cast<size_t>(cfg.spkcache_len) *
               cfg.transformer_hidden_size * sizeof(float)),
      spkcache_lengths(sizeof(int32_t)),
      spkcache_preds(static_cast<size_t>(cfg.spkcache_len) *
                     cfg.max_num_speakers * sizeof(float)),
      fifo(static_cast<size_t>(std::max(cfg.fifo_len, 1)) *
           cfg.transformer_hidden_size * sizeof(float)),
      fifo_lengths(sizeof(int32_t)),
      fifo_preds(static_cast<size_t>(std::max(cfg.fifo_len, 1)) *
                 cfg.max_num_speakers * sizeof(float)),
      spk_perm(static_cast<size_t>(cfg.max_num_speakers) * sizeof(int32_t)),
      mean_sil_emb(static_cast<size_t>(cfg.max_num_speakers) *
                   cfg.transformer_hidden_size * sizeof(float)),
      n_sil_frames(static_cast<size_t>(cfg.max_num_speakers) *
                   sizeof(int32_t)) {
  Clear();
}

void SortformerState::Clear() {
  std::memset(spkcache.data(), 0, spkcache.size());
  std::memset(spkcache_lengths.data(), 0, spkcache_lengths.size());
  std::memset(spkcache_preds.data(), 0, spkcache_preds.size());
  std::memset(fifo.data(), 0, fifo.size());
  std::memset(fifo_lengths.data(), 0, fifo_lengths.size());
  std::memset(fifo_preds.data(), 0, fifo_preds.size());
  std::memset(spk_perm.data(), 0, spk_perm.size());
  std::memset(mean_sil_emb.data(), 0, mean_sil_emb.size());
  std::memset(n_sil_frames.data(), 0, n_sil_frames.size());
}

SortformerDiarizer::SortformerDiarizer() = default;

SortformerDiarizer::SortformerDiarizer(const SortformerConfig& cfg)
    : config_(cfg) {}

void SortformerDiarizer::ApplyStreamingTuning(const SortformerTuning& tuning) {
  if (initialized_) {
    throw std::logic_error(
        "Sortformer streaming tuning must be applied before Initialize");
  }
  if (tuning.spkcache_len > 0) config_.spkcache_len = tuning.spkcache_len;
  if (tuning.chunk_len > 0) {
    config_.chunk_len = tuning.chunk_len;
  }
  if (tuning.spkcache_update_period > 0)
    config_.spkcache_update_period = tuning.spkcache_update_period;
  if (tuning.chunk_left_context >= 0)
    config_.chunk_left_context = tuning.chunk_left_context;
  if (tuning.chunk_right_context >= 0)
    config_.chunk_right_context = tuning.chunk_right_context;
  if (tuning.spkcache_sil_frames >= 0)
    config_.spkcache_sil_frames_per_spk = tuning.spkcache_sil_frames;
  if (tuning.fifo_len >= 0) config_.fifo_len = tuning.fifo_len;
  if (tuning.show_progress >= 0)
    config_.show_progress = (tuning.show_progress != 0);
  if (!config_.Validate()) {
    throw std::invalid_argument("invalid Sortformer streaming tuning");
  }
}

void SortformerDiarizer::Initialize(const core::DiarizationConfig& config) {
  config_.sample_rate = config.sample_rate;
  if (config.max_speakers > 0) {
    config_.max_num_speakers = config.max_speakers;
  }
  if (!config_.Validate()) {
    throw std::invalid_argument("invalid SortformerConfig");
  }
  state_ = std::make_unique<SortformerState>(config_);
  stream_state_.mean_sil_emb.assign(config_.encoder_d_model, 0.0f);
  stream_state_.Clear();
  stream_time_sec_ = 0.0;
  initialized_ = true;
}

void SortformerDiarizer::LoadWeights(const std::string& path) {
  if (path.empty()) {
    throw std::invalid_argument("weights path must not be empty");
  }
  reader_ = std::make_unique<io::SafeTensorReader>(path);

  // Mel front-end with the model's stored window + filterbank for exact parity.
  feature::MelConfig mcfg;
  mcfg.sample_rate = config_.sample_rate;
  mcfg.n_fft = config_.n_fft;
  mcfg.n_mels = config_.mel_features;
  const auto& wmeta = reader_->GetMetadata("preprocessor.featurizer.window");
  std::vector<float> window(wmeta.data_size / sizeof(float));
  reader_->ReadWeight("preprocessor.featurizer.window", window.data(),
                      wmeta.data_size);
  const auto& fbmeta = reader_->GetMetadata("preprocessor.featurizer.fb");
  std::vector<float> fb(fbmeta.data_size / sizeof(float));  // [1,128,257]->flat
  reader_->ReadWeight("preprocessor.featurizer.fb", fb.data(),
                      fbmeta.data_size);
  mcfg.win_length = static_cast<int>(window.size());
  mel_ = std::make_unique<feature::MelSpectrogram>(mcfg, window, fb);

  pre_encode_ = std::make_unique<ConformerPreEncode>();
  pre_encode_->LoadWeights(*reader_);

  conformer_layers_.clear();
  for (int l = 0; l < num_conformer_layers_; ++l) {
    auto layer = std::make_unique<ConformerLayer>(config_.encoder_d_model);
    layer->LoadWeights(*reader_, "encoder.layers." + std::to_string(l));
    conformer_layers_.push_back(std::move(layer));
  }

  decoder_ = std::make_unique<SortformerDecoder>(
      config_.encoder_d_model, config_.transformer_hidden_size,
      config_.transformer_heads, /*d_ff=*/768, config_.transformer_layers,
      config_.max_num_speakers);
  decoder_->LoadWeights(*reader_);

  weights_path_ = path;
  weights_loaded_ = true;
}

void SortformerDiarizer::Reset() {
  if (state_) state_->Clear();
  stream_state_.Clear();
  stream_time_sec_ = 0.0;
  // Incremental streaming state.
  sig_.clear();
  sig_abs_ = 0;
  last_raw_ = 0.0f;
  stream_started_ = false;
  mel_seq_.clear();
  mel_base_ = 0;
  mel_avail_ = 0;
  mel_w_ = 0;
  stt_feat_ = 0;
  emitted_frames_ = 0;
}

std::vector<float> SortformerDiarizer::ForwardEncoderDecoder(
    const std::vector<float>& emb_seq, int T, int valid, cudaStream_t stream) {
  const int D = config_.encoder_d_model;
  const float xscale = std::sqrt(static_cast<float>(D));
  std::vector<float> xin(emb_seq.size());
  for (size_t i = 0; i < emb_seq.size(); ++i) xin[i] = emb_seq[i] * xscale;

  gpu::DeviceScratch& scr = enc_scratch_;
  float* dx = scr.GetT<float>(0, xin.size());
  CUDA_CHECK(cudaMemcpyAsync(dx, xin.data(), xin.size() * sizeof(float),
                             cudaMemcpyHostToDevice, stream));
  std::vector<float> posemb = ConformerLayer::BuildPosEmb(T, D);
  float* dpe = scr.GetT<float>(1, posemb.size());
  CUDA_CHECK(cudaMemcpyAsync(dpe, posemb.data(), posemb.size() * sizeof(float),
                             cudaMemcpyHostToDevice, stream));
  for (auto& layer : conformer_layers_) {
    layer->Forward(dx, T, valid, dpe, stream);
  }
  std::vector<float> preds = decoder_->Forward(dx, T, valid, stream);

  // apply_mask_to_preds: zero frames at/after the valid length.
  const int n_spk = config_.max_num_speakers;
  for (int f = valid; f < T; ++f)
    for (int s = 0; s < n_spk; ++s)
      preds[static_cast<size_t>(f) * n_spk + s] = 0.0f;
  return preds;
}

core::DiarizationFrames SortformerDiarizer::RunStreaming(const float* mel_fm,
                                                         int n_mels, int t_mel,
                                                         int valid_mel,
                                                         double t_start_sec) {
  const int sub = config_.encoder_subsampling_factor;
  const int chunk_mel = config_.chunk_len * sub;

  stream_state_.Clear();

  std::vector<float> total_preds;
  int total_frames = 0;

  int stt = 0;
  int chunk_idx = 0;
  const int num_chunks = (t_mel + chunk_mel - 1) / chunk_mel;
  while (stt < t_mel) {
    if (config_.show_progress) {
      std::fprintf(stderr, "\r  streaming chunk %d/%d (t=%.0fs)   ",
                   chunk_idx + 1, num_chunks, stt * config_.hop_size_sec);
      std::fflush(stderr);
    }
    // Offline path: the whole mel buffer is available, base frame = 0,
    // available == total mel frames.
    StreamMelChunk(mel_fm, n_mels, t_mel, /*buf_base_frame=*/0,
                   /*stt_abs=*/stt, /*valid_abs=*/valid_mel,
                   /*avail_abs=*/t_mel, total_preds, total_frames);
    stt = std::min(stt + chunk_mel, t_mel);
    ++chunk_idx;
  }
  if (config_.show_progress) std::fprintf(stderr, "\n");

  core::DiarizationFrames out;
  out.num_frames = total_frames;
  out.num_speakers = config_.max_num_speakers;
  out.frame_period_sec = config_.FramePeriodSec();
  out.t_start_sec = t_start_sec;
  out.probs = std::move(total_preds);
  stream_time_sec_ = t_start_sec + total_frames * out.frame_period_sec;
  return out;
}

// One streaming chunk step over a freq-major mel buffer that covers absolute
// mel frames [buf_base_frame, buf_base_frame + buf_len). Processes the chunk
// starting at absolute frame stt_abs, using chunk_left/right_context clamped to
// the absolute stream bounds (start = frame 0, end = avail_abs). Appends this
// chunk's center-frame sigmoids to out_preds and advances the persistent
// stream_state_ (spkcache). This is the shared core of both the offline
// RunStreaming loop and the incremental StreamAudio path, so the two are
// bit-identical for identical mel content.
void SortformerDiarizer::StreamMelChunk(const float* mel_buf, int n_mels,
                                        int buf_len, long buf_base_frame,
                                        long stt_abs, long valid_abs,
                                        long avail_abs,
                                        std::vector<float>& out_preds,
                                        int& out_frames, cudaStream_t stream) {
  const int D = config_.encoder_d_model;
  const int n_spk = config_.max_num_speakers;
  const int sub = config_.encoder_subsampling_factor;
  const int chunk_mel = config_.chunk_len * sub;
  const int spkcache_len = config_.spkcache_len;

  const int lc = static_cast<int>(
      std::min<long>(config_.chunk_left_context * sub, stt_abs));
  const long end_abs = std::min<long>(stt_abs + chunk_mel, avail_abs);
  const int rc = static_cast<int>(
      std::min<long>(config_.chunk_right_context * sub, avail_abs - end_abs));
  const long s0_abs = stt_abs - lc;
  const long s1_abs = end_abs + rc;
  const int mlen = static_cast<int>(s1_abs - s0_abs);
  const int s0_rel = static_cast<int>(s0_abs - buf_base_frame);

  // feat_length: valid (non-pad) mel frames inside this slice.
  int feat_len = static_cast<int>(valid_abs - stt_abs + lc);
  feat_len = std::max(0, std::min(feat_len, mlen));

  // Slice the freq-major mel [n_mels, buf_len] into [n_mels, mlen].
  std::vector<float> slice(static_cast<size_t>(n_mels) * mlen);
  for (int m = 0; m < n_mels; ++m)
    for (int t = 0; t < mlen; ++t)
      slice[static_cast<size_t>(m) * mlen + t] =
          mel_buf[static_cast<size_t>(m) * buf_len + (s0_rel + t)];

  // pre_encode -> chunk embeddings [chunk_T, D].
  int chunk_T = 0, chunk_valid = 0;
  std::vector<float> chunk_embs = pre_encode_->Forward(
      slice.data(), n_mels, mlen, feat_len, &chunk_T, &chunk_valid, stream);

  const int lc_sub =
      static_cast<int>(std::lround(static_cast<double>(lc) / sub));
  const int rc_sub = static_cast<int>(std::ceil(static_cast<double>(rc) / sub));
  const int chunk_center_len = chunk_T - lc_sub - rc_sub;
  const int valid_center_len =
      std::clamp(chunk_valid - lc_sub, 0, chunk_center_len);

  const int fifo_cap = config_.fifo_len;
  if (fifo_cap > 0 && stream_state_.fifo_max_len == 0) {
    stream_state_.InitFifo(D, n_spk, fifo_cap);
  }
  const int fifo_len = fifo_cap > 0 ? stream_state_.fifo_count : 0;

  // NeMo's async input order is [spkcache, fifo, current chunk].
  const int spk_len = stream_state_.spk_len;
  const int Tcat = spk_len + fifo_len + chunk_T;
  std::vector<float> enc_in(static_cast<size_t>(Tcat) * D);
  std::copy(stream_state_.spkcache.begin(),
            stream_state_.spkcache.begin() + static_cast<size_t>(spk_len) * D,
            enc_in.begin());
  if (fifo_len > 0) {
    std::copy(
        stream_state_.fifo_embs.begin(),
        stream_state_.fifo_embs.begin() + static_cast<size_t>(fifo_len) * D,
        enc_in.begin() + static_cast<size_t>(spk_len) * D);
  }
  std::copy(chunk_embs.begin(), chunk_embs.end(),
            enc_in.begin() + static_cast<size_t>(spk_len + fifo_len) * D);
  const int valid_cat = spk_len + fifo_len + chunk_valid;

  std::vector<float> preds =
      ForwardEncoderDecoder(enc_in, Tcat, valid_cat, stream);

  for (int frame = 0; frame < fifo_len; ++frame) {
    std::copy_n(
        preds.begin() + static_cast<size_t>(spk_len + frame) * n_spk, n_spk,
        stream_state_.fifo_preds.begin() + static_cast<size_t>(frame) * n_spk);
  }

  const int cp_start = spk_len + fifo_len + lc_sub;
  for (int frame = 0; frame < chunk_center_len; ++frame) {
    std::copy_n(preds.begin() + static_cast<size_t>(cp_start + frame) * n_spk,
                n_spk, std::back_inserter(out_preds));
  }
  out_frames += chunk_center_len;

  int pop_out_len = 0;
  std::vector<float> pop_embs;
  std::vector<float> pop_preds;

  if (fifo_cap > 0) {
    // NeMo's async updater stores only valid center frames. The returned chunk
    // still keeps its fixed padded shape, but padding must not enter FIFO/cache
    // state if another batch item or a later incremental step follows.
    const int combined_count = fifo_len + valid_center_len;
    std::vector<float> combined_embs(static_cast<size_t>(combined_count) * D);
    std::vector<float> combined_preds(static_cast<size_t>(combined_count) *
                                      n_spk);
    std::copy(
        stream_state_.fifo_embs.begin(),
        stream_state_.fifo_embs.begin() + static_cast<size_t>(fifo_len) * D,
        combined_embs.begin());
    std::copy(stream_state_.fifo_preds.begin(),
              stream_state_.fifo_preds.begin() +
                  static_cast<size_t>(fifo_len) * n_spk,
              combined_preds.begin());
    for (int frame = 0; frame < valid_center_len; ++frame) {
      std::copy_n(
          chunk_embs.begin() + static_cast<size_t>(lc_sub + frame) * D, D,
          combined_embs.begin() + static_cast<size_t>(fifo_len + frame) * D);
      std::copy_n(preds.begin() + static_cast<size_t>(cp_start + frame) * n_spk,
                  n_spk,
                  combined_preds.begin() +
                      static_cast<size_t>(fifo_len + frame) * n_spk);
    }

    if (combined_count > fifo_cap) {
      pop_out_len = std::max(config_.spkcache_update_period,
                             chunk_center_len - fifo_cap + fifo_len);
      pop_out_len = std::min(pop_out_len, combined_count);
    }
    pop_embs.assign(
        combined_embs.begin(),
        combined_embs.begin() + static_cast<size_t>(pop_out_len) * D);
    pop_preds.assign(
        combined_preds.begin(),
        combined_preds.begin() + static_cast<size_t>(pop_out_len) * n_spk);

    const int remaining = combined_count - pop_out_len;
    if (remaining > fifo_cap) {
      throw std::runtime_error("Sortformer FIFO overflow was not drained");
    }
    std::fill(stream_state_.fifo_embs.begin(), stream_state_.fifo_embs.end(),
              0.0f);
    std::fill(stream_state_.fifo_preds.begin(), stream_state_.fifo_preds.end(),
              0.0f);
    std::copy(combined_embs.begin() + static_cast<size_t>(pop_out_len) * D,
              combined_embs.end(), stream_state_.fifo_embs.begin());
    std::copy(combined_preds.begin() + static_cast<size_t>(pop_out_len) * n_spk,
              combined_preds.end(), stream_state_.fifo_preds.begin());
    stream_state_.fifo_count = remaining;
  } else {
    pop_out_len = chunk_center_len;
    pop_embs.resize(static_cast<size_t>(pop_out_len) * D);
    pop_preds.resize(static_cast<size_t>(pop_out_len) * n_spk);
    for (int frame = 0; frame < pop_out_len; ++frame) {
      std::copy_n(chunk_embs.begin() + static_cast<size_t>(lc_sub + frame) * D,
                  D, pop_embs.begin() + static_cast<size_t>(frame) * D);
      std::copy_n(preds.begin() + static_cast<size_t>(cp_start + frame) * n_spk,
                  n_spk,
                  pop_preds.begin() + static_cast<size_t>(frame) * n_spk);
    }
  }

  if (pop_out_len <= 0) return;

  UpdateSilenceProfile(pop_embs, pop_preds, pop_out_len, n_spk, D,
                       stream_state_.mean_sil_emb, stream_state_.n_sil_frames);

  const int new_len = spk_len + pop_out_len;
  stream_state_.spkcache.insert(stream_state_.spkcache.end(), pop_embs.begin(),
                                pop_embs.end());
  if (fifo_cap > 0 && !stream_state_.spkcache_preds_valid) {
    stream_state_.spkcache_preds.assign(static_cast<size_t>(spk_len) * n_spk,
                                        0.0f);
    for (int frame = 0; frame < spk_len; ++frame) {
      std::copy_n(preds.begin() + static_cast<size_t>(frame) * n_spk, n_spk,
                  stream_state_.spkcache_preds.begin() +
                      static_cast<size_t>(frame) * n_spk);
    }
    stream_state_.spkcache_preds_valid = true;
  }
  if (stream_state_.spkcache_preds_valid) {
    stream_state_.spkcache_preds.insert(stream_state_.spkcache_preds.end(),
                                        pop_preds.begin(), pop_preds.end());
  }
  stream_state_.spk_len = new_len;

  if (new_len <= spkcache_len) return;

  if (!stream_state_.spkcache_preds_valid) {
    stream_state_.spkcache_preds.assign(static_cast<size_t>(new_len) * n_spk,
                                        0.0f);
    for (int frame = 0; frame < spk_len; ++frame) {
      std::copy_n(preds.begin() + static_cast<size_t>(frame) * n_spk, n_spk,
                  stream_state_.spkcache_preds.begin() +
                      static_cast<size_t>(frame) * n_spk);
    }
    std::copy(pop_preds.begin(), pop_preds.end(),
              stream_state_.spkcache_preds.begin() +
                  static_cast<size_t>(spk_len) * n_spk);
    stream_state_.spkcache_preds_valid = true;
  }
  std::vector<float> compressed_embeddings;
  std::vector<float> compressed_predictions;
  CompressSpkcache(stream_state_.spkcache, stream_state_.spkcache_preds,
                   new_len, n_spk, D, spkcache_len,
                   config_.spkcache_sil_frames_per_spk,
                   stream_state_.mean_sil_emb, compressed_embeddings,
                   compressed_predictions);
  stream_state_.spkcache.swap(compressed_embeddings);
  stream_state_.spkcache_preds.swap(compressed_predictions);
  stream_state_.spk_len = spkcache_len;
}

core::DiarizationFrames SortformerDiarizer::ProcessChunk(
    const core::AudioChunk& chunk) {
  if (!initialized_) {
    throw std::runtime_error("SortformerDiarizer not initialized");
  }
  if (chunk.samples == nullptr || chunk.num_samples <= 0) {
    throw std::invalid_argument("invalid audio chunk");
  }
  if (!weights_loaded_) {
    throw std::runtime_error("SortformerDiarizer weights not loaded");
  }

  const int n_mels = config_.mel_features;

  // 1) Log-mel front-end -> [T_mel, n_mels] (frame-major).
  int t_mel = 0;
  std::vector<float> mel =
      mel_->Compute(chunk.samples, chunk.num_samples, &t_mel);
  // Transpose to freq-major [n_mels, T_mel] for the encoder path.
  std::vector<float> mel_fm(static_cast<size_t>(n_mels) * t_mel);
  for (int t = 0; t < t_mel; ++t)
    for (int m = 0; m < n_mels; ++m)
      mel_fm[static_cast<size_t>(m) * t_mel + t] =
          mel[static_cast<size_t>(t) * n_mels + m];

  // NeMo center-pads the STFT by n_fft/2; valid (non-pad) frames =
  // floor(L/hop).
  const int hop =
      static_cast<int>(std::lround(config_.hop_size_sec * config_.sample_rate));
  int valid_mel = std::min(t_mel, chunk.num_samples / hop);

  // 2) Streaming chunked forward over the full mel.
  return RunStreaming(mel_fm.data(), n_mels, t_mel, valid_mel,
                      chunk.t_start_sec);
}

void SortformerDiarizer::AppendRaw(const float* samples, int num_samples) {
  const auto& mc = mel_->config();
  const float preemph = mc.preemph;
  // Append new samples with pre-emphasis continuity. Offline does y[0]=x[0]
  // only for the very first sample of the stream; thereafter
  // y[n]=x[n]-a*x[n-1].
  sig_.reserve(sig_.size() + std::max(0, num_samples));
  for (int i = 0; i < num_samples; ++i) {
    const float x = samples[i];
    const float y = stream_started_ ? (x - preemph * last_raw_) : x;
    sig_.push_back(y);
    last_raw_ = x;
    stream_started_ = true;
  }
}

void SortformerDiarizer::EnsureMel(bool final, cudaStream_t stream) {
  const int n_mels = config_.mel_features;
  const auto& mc = mel_->config();
  const int hop = mc.hop_length;

  // A frame t is stable (its STFT window is fully covered by real samples, so
  // it will never change as more audio arrives) iff t*hop + rmargin <=
  // total_abs, with rmargin = win_off + win_length - pad_left. On final, emit
  // all floor(total/hop) frames (the trailing ones use the zero tail, exactly
  // as the offline center-padded STFT does).
  const int win_off = (mc.n_fft - mc.win_length) / 2;
  const int pad_left = mc.center ? mc.n_fft / 2 : 0;
  const long rmargin = static_cast<long>(win_off) + mc.win_length - pad_left;
  const long total_abs = sig_abs_ + static_cast<long>(sig_.size());
  long target;
  if (final)
    target = total_abs / hop;
  else
    target = (total_abs >= rmargin) ? (total_abs - rmargin) / hop + 1 : 0;
  if (target <= mel_avail_) return;

  const int k = static_cast<int>(target - mel_avail_);
  const int input_offset = static_cast<int>(mel_avail_ * hop - sig_abs_);
  std::vector<float> fm = mel_->ComputeStreamFrames(
      sig_.data(), static_cast<int>(sig_.size()), input_offset, k, stream);

  // Append the k frame-major rows as freq-major columns to mel_seq_.
  const int newW = mel_w_ + k;
  std::vector<float> nm(static_cast<size_t>(n_mels) * newW);
  for (int m = 0; m < n_mels; ++m) {
    for (int j = 0; j < mel_w_; ++j)
      nm[static_cast<size_t>(m) * newW + j] =
          mel_seq_[static_cast<size_t>(m) * mel_w_ + j];
    for (int c = 0; c < k; ++c)
      nm[static_cast<size_t>(m) * newW + (mel_w_ + c)] =
          fm[static_cast<size_t>(c) * n_mels + m];
  }
  mel_seq_.swap(nm);
  mel_w_ = newW;
  mel_avail_ = target;
}

void SortformerDiarizer::TrimStreamingBuffers() {
  const auto& mc = mel_->config();
  const int hop = mc.hop_length;
  const int n_mels = config_.mel_features;
  const int sub = config_.encoder_subsampling_factor;
  const int lc_max = config_.chunk_left_context * sub;

  // mel: only frames in [stt_feat_ - lc_max, mel_avail_) are still needed.
  long keep_from = stt_feat_ - lc_max;
  if (keep_from < 0) keep_from = 0;
  long drop = keep_from - mel_base_;
  if (drop > 0 && drop <= mel_w_) {
    const int newW = mel_w_ - static_cast<int>(drop);
    std::vector<float> nm(static_cast<size_t>(n_mels) * newW);
    for (int m = 0; m < n_mels; ++m)
      for (int j = 0; j < newW; ++j)
        nm[static_cast<size_t>(m) * newW + j] =
            mel_seq_[static_cast<size_t>(m) * mel_w_ + (drop + j)];
    mel_seq_.swap(nm);
    mel_w_ = newW;
    mel_base_ += drop;
  }

  // signal: keep samples needed by the next mel frame (frame mel_avail_), whose
  // earliest sample is mel_avail_*hop + win_off - pad_left, with a margin.
  const int win_off = (mc.n_fft - mc.win_length) / 2;
  const int pad_left = mc.center ? mc.n_fft / 2 : 0;
  long earliest = mel_avail_ * hop + win_off - pad_left;
  long keep = earliest - 2 * hop;
  if (keep < 0) keep = 0;
  long drop_s = keep - sig_abs_;
  if (drop_s > 0 && drop_s <= static_cast<long>(sig_.size())) {
    sig_.erase(sig_.begin(), sig_.begin() + drop_s);
    sig_abs_ += drop_s;
  }
}

core::DiarizationFrames SortformerDiarizer::StreamAudio(const float* samples,
                                                        int num_samples,
                                                        bool final,
                                                        cudaStream_t stream) {
  if (!initialized_)
    throw std::runtime_error("SortformerDiarizer not initialized");
  if (!weights_loaded_)
    throw std::runtime_error("SortformerDiarizer weights not loaded");

  if (samples != nullptr && num_samples > 0) AppendRaw(samples, num_samples);

  const int n_mels = config_.mel_features;
  const int sub = config_.encoder_subsampling_factor;
  const int chunk_mel = config_.chunk_len * sub;
  const int rc_max = config_.chunk_right_context * sub;

  std::vector<float> out_preds;
  int out_frames = 0;

  // Defer (batch) mel + chunk processing: a non-final chunk can only fire once
  // a full chunk plus its right context is stable. Computing mel one tiny slice
  // per binary frame would launch thousands of tiny GPU kernels and rebuild
  // mel_seq_ each time; instead we only compute mel when enough audio has
  // accumulated to advance at least one chunk (or on final). This keeps the hot
  // per-frame path to a cheap CPU append while preserving bit-exact frames.
  if (!final) {
    const auto& mc = mel_->config();
    const int hop = mc.hop_length;
    const int win_off = (mc.n_fft - mc.win_length) / 2;
    const int pad_left = mc.center ? mc.n_fft / 2 : 0;
    const long rmargin = static_cast<long>(win_off) + mc.win_length - pad_left;
    const long total_abs = sig_abs_ + static_cast<long>(sig_.size());
    const long stable_target =
        (total_abs >= rmargin) ? (total_abs - rmargin) / hop + 1 : 0;
    // Need mel out to the next chunk's end + right context to fire it.
    if (stable_target < stt_feat_ + chunk_mel + rc_max) {
      core::DiarizationFrames out;
      out.num_frames = 0;
      out.num_speakers = config_.max_num_speakers;
      out.frame_period_sec = config_.FramePeriodSec();
      out.t_start_sec = emitted_frames_ * out.frame_period_sec;
      return out;
    }
  }

  EnsureMel(final, stream);

  while (stt_feat_ < mel_avail_) {
    const long end_abs = std::min<long>(stt_feat_ + chunk_mel, mel_avail_);
    const bool full = (end_abs - stt_feat_ == chunk_mel);
    if (!final) {
      // Only finalize a chunk once a full chunk plus its right context is
      // available; otherwise wait for more audio (keeps frames bit-exact).
      if (!full || end_abs + rc_max > mel_avail_) break;
    }
    StreamMelChunk(mel_seq_.data(), n_mels, mel_w_, mel_base_, stt_feat_,
                   /*valid_abs=*/mel_avail_, /*avail_abs=*/mel_avail_,
                   out_preds, out_frames, stream);
    stt_feat_ = end_abs;
    TrimStreamingBuffers();
  }

  core::DiarizationFrames out;
  out.num_frames = out_frames;
  out.num_speakers = config_.max_num_speakers;
  out.frame_period_sec = config_.FramePeriodSec();
  out.t_start_sec = emitted_frames_ * out.frame_period_sec;
  out.probs = std::move(out_preds);
  emitted_frames_ += out_frames;
  return out;
}

}  // namespace model
}  // namespace orator
