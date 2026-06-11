#include "model/streaming_sortformer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace orator {
namespace model {

namespace {

constexpr float kPredScoreThreshold = 0.25f;
constexpr float kScoresBoostLatest = 0.05f;
constexpr float kSilThreshold = 0.2f;
constexpr float kStrongBoostRate = 0.75f;
constexpr float kWeakBoostRate = 1.5f;
constexpr float kMinPosScoresRate = 0.5f;
constexpr int kSilFramesPerSpk = 3;
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
    for (int s = 0; s < n_spk; ++s) sum1p += log_1p[static_cast<size_t>(f) * n_spk + s];
    for (int s = 0; s < n_spk; ++s) {
      size_t i = static_cast<size_t>(f) * n_spk + s;
      scores[i] = log_p[i] - log_1p[i] + sum1p - std::log(0.5f);
    }
  }
}

// _disable_low_scores: non-speech -> -inf; non-positive -> -inf if a speaker has
// at least min_pos positive-scored frames.
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
      if ((!is_pos) && is_speech && pos_count[s] >= min_pos) scores[i] = kNegInf;
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
    std::partial_sort(
        idx.begin(), idx.begin() + std::min(n_boost, n), idx.end(),
        [&](int a, int b) {
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
    for (int s = 0; s < n_spk; ++s) sump += preds[static_cast<size_t>(f) * n_spk + s];
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
                      const std::vector<float>& mean_sil_emb,
                      std::vector<float>& out_emb,
                      std::vector<float>& out_preds) {
  const int spkcache_len_per_spk = spkcache_len / n_spk - kSilFramesPerSpk;
  const int strong = static_cast<int>(std::floor(spkcache_len_per_spk * kStrongBoostRate));
  const int weak = static_cast<int>(std::floor(spkcache_len_per_spk * kWeakBoostRate));
  const int min_pos = static_cast<int>(std::floor(spkcache_len_per_spk * kMinPosScoresRate));

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

  // Append kSilFramesPerSpk rows of +inf (silence placeholders).
  const int n_pad = n + kSilFramesPerSpk;
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
    if (f >= n) {                          // silence-pad row
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
  return true;
}

SortformerState::SortformerState(const SortformerConfig& cfg)
    : spkcache(static_cast<size_t>(cfg.spkcache_len) * cfg.transformer_hidden_size *
               sizeof(float)),
      spkcache_lengths(sizeof(int32_t)),
      spkcache_preds(static_cast<size_t>(cfg.spkcache_len) * cfg.max_num_speakers *
                     sizeof(float)),
      fifo(static_cast<size_t>(std::max(cfg.fifo_len, 1)) *
           cfg.transformer_hidden_size * sizeof(float)),
      fifo_lengths(sizeof(int32_t)),
      fifo_preds(static_cast<size_t>(std::max(cfg.fifo_len, 1)) *
                 cfg.max_num_speakers * sizeof(float)),
      spk_perm(static_cast<size_t>(cfg.max_num_speakers) * sizeof(int32_t)),
      mean_sil_emb(static_cast<size_t>(cfg.max_num_speakers) *
                   cfg.transformer_hidden_size * sizeof(float)),
      n_sil_frames(static_cast<size_t>(cfg.max_num_speakers) * sizeof(int32_t)) {
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
  reader_->ReadWeight("preprocessor.featurizer.fb", fb.data(), fbmeta.data_size);
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
}

std::vector<float> SortformerDiarizer::ForwardEncoderDecoder(
    const std::vector<float>& emb_seq, int T, int valid) {
  const int D = config_.encoder_d_model;
  const float xscale = std::sqrt(static_cast<float>(D));
  std::vector<float> xin(emb_seq.size());
  for (size_t i = 0; i < emb_seq.size(); ++i) xin[i] = emb_seq[i] * xscale;

  gpu::DeviceBuffer dx(xin.size() * sizeof(float));
  gpu::GpuMemory::CopyHostToDevice(dx.data(), xin.data(),
                                   xin.size() * sizeof(float));
  std::vector<float> posemb = ConformerLayer::BuildPosEmb(T, D);
  gpu::DeviceBuffer dpe(posemb.size() * sizeof(float));
  gpu::GpuMemory::CopyHostToDevice(dpe.data(), posemb.data(),
                                   posemb.size() * sizeof(float));
  for (auto& layer : conformer_layers_) {
    layer->Forward(static_cast<float*>(dx.data()), T, valid,
                   static_cast<const float*>(dpe.data()));
  }
  std::vector<float> preds =
      decoder_->Forward(static_cast<float*>(dx.data()), T, valid);

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
  const int D = config_.encoder_d_model;
  const int n_spk = config_.max_num_speakers;
  const int sub = config_.encoder_subsampling_factor;
  const int chunk_mel = config_.chunk_len * sub;
  const int spkcache_len = config_.spkcache_len;

  stream_state_.Clear();

  std::vector<float> total_preds;
  int total_frames = 0;

  int stt = 0;
  int chunk_idx = 0;
  const int num_chunks = (t_mel + chunk_mel - 1) / chunk_mel;
  while (stt < t_mel) {
    if (std::getenv("ORATOR_STREAM_PROGRESS")) {
      std::fprintf(stderr, "\r  streaming chunk %d/%d (t=%.0fs)   ",
                   chunk_idx + 1, num_chunks,
                   stt * config_.hop_size_sec);
      std::fflush(stderr);
    }
    const int lc = std::min(config_.chunk_left_context * sub, stt);
    const int end = std::min(stt + chunk_mel, t_mel);
    const int rc = std::min(config_.chunk_right_context * sub, t_mel - end);
    const int s0 = stt - lc;
    const int s1 = end + rc;
    const int mlen = s1 - s0;

    // feat_length: valid (non-pad) mel frames inside this slice (offset=0).
    int feat_len = valid_mel - stt + lc;
    feat_len = std::max(0, std::min(feat_len, mlen));

    // Slice the freq-major mel [n_mels, t_mel] into [n_mels, mlen].
    std::vector<float> slice(static_cast<size_t>(n_mels) * mlen);
    for (int m = 0; m < n_mels; ++m)
      for (int t = 0; t < mlen; ++t)
        slice[static_cast<size_t>(m) * mlen + t] =
            mel_fm[static_cast<size_t>(m) * t_mel + (s0 + t)];

    // pre_encode -> chunk embeddings [chunk_T, D].
    int chunk_T = 0, chunk_valid = 0;
    std::vector<float> chunk_embs =
        pre_encode_->Forward(slice.data(), n_mels, mlen, feat_len, &chunk_T,
                             &chunk_valid);

    const int lc_sub = static_cast<int>(std::lround(static_cast<double>(lc) / sub));
    const int rc_sub = static_cast<int>(std::ceil(static_cast<double>(rc) / sub));
    const int chunk_center_len = chunk_T - lc_sub - rc_sub;

    // concat [spkcache, chunk_embs] -> encoder input [Tcat, D].
    const int spk_len = stream_state_.spk_len;
    const int Tcat = spk_len + chunk_T;
    std::vector<float> enc_in(static_cast<size_t>(Tcat) * D);
    std::copy(stream_state_.spkcache.begin(),
              stream_state_.spkcache.begin() +
                  static_cast<size_t>(spk_len) * D,
              enc_in.begin());
    std::copy(chunk_embs.begin(), chunk_embs.end(),
              enc_in.begin() + static_cast<size_t>(spk_len) * D);
    const int valid_cat = spk_len + chunk_valid;

    // encoder + decoder -> preds [Tcat, n_spk].
    std::vector<float> preds = ForwardEncoderDecoder(enc_in, Tcat, valid_cat);

    // chunk_preds = preds[spk_len + lc_sub : spk_len + lc_sub + chunk_center_len]
    const int cp_start = spk_len + lc_sub;
    for (int f = 0; f < chunk_center_len; ++f) {
      int src = cp_start + f;
      for (int s = 0; s < n_spk; ++s)
        total_preds.push_back(preds[static_cast<size_t>(src) * n_spk + s]);
    }
    total_frames += chunk_center_len;

    // --- streaming_update (sync, fifo_len=0) ---
    // chunk center embeddings + preds become the pop-out appended to spkcache.
    std::vector<float> pop_embs(static_cast<size_t>(chunk_center_len) * D);
    std::vector<float> pop_preds(static_cast<size_t>(chunk_center_len) * n_spk);
    for (int f = 0; f < chunk_center_len; ++f) {
      int ce = lc_sub + f;  // chunk[lc:chunk_len+lc]
      for (int d = 0; d < D; ++d)
        pop_embs[static_cast<size_t>(f) * D + d] =
            chunk_embs[static_cast<size_t>(ce) * D + d];
      int pp = cp_start + f;
      for (int s = 0; s < n_spk; ++s)
        pop_preds[static_cast<size_t>(f) * n_spk + s] =
            preds[static_cast<size_t>(pp) * n_spk + s];
    }

    UpdateSilenceProfile(pop_embs, pop_preds, chunk_center_len, n_spk, D,
                         stream_state_.mean_sil_emb,
                         stream_state_.n_sil_frames);

    // append pop-out to spkcache
    const int new_len = spk_len + chunk_center_len;
    stream_state_.spkcache.insert(stream_state_.spkcache.end(),
                                  pop_embs.begin(), pop_embs.end());
    if (stream_state_.spkcache_preds_valid) {
      stream_state_.spkcache_preds.insert(stream_state_.spkcache_preds.end(),
                                          pop_preds.begin(), pop_preds.end());
    }
    stream_state_.spk_len = new_len;

    if (new_len > spkcache_len) {
      if (!stream_state_.spkcache_preds_valid) {
        // first compression: spkcache_preds = [preds[:spk_len], pop_preds]
        stream_state_.spkcache_preds.assign(
            static_cast<size_t>(new_len) * n_spk, 0.0f);
        for (int f = 0; f < spk_len; ++f)
          for (int s = 0; s < n_spk; ++s)
            stream_state_.spkcache_preds[static_cast<size_t>(f) * n_spk + s] =
                preds[static_cast<size_t>(f) * n_spk + s];
        std::copy(pop_preds.begin(), pop_preds.end(),
                  stream_state_.spkcache_preds.begin() +
                      static_cast<size_t>(spk_len) * n_spk);
        stream_state_.spkcache_preds_valid = true;
      }
      std::vector<float> comp_emb, comp_preds;
      CompressSpkcache(stream_state_.spkcache, stream_state_.spkcache_preds,
                       new_len, n_spk, D, spkcache_len,
                       stream_state_.mean_sil_emb, comp_emb, comp_preds);
      stream_state_.spkcache.swap(comp_emb);
      stream_state_.spkcache_preds.swap(comp_preds);
      stream_state_.spk_len = spkcache_len;
    }

    stt = end;
    ++chunk_idx;
  }
  if (std::getenv("ORATOR_STREAM_PROGRESS")) std::fprintf(stderr, "\n");

  core::DiarizationFrames out;
  out.num_frames = total_frames;
  out.num_speakers = n_spk;
  out.frame_period_sec = config_.FramePeriodSec();
  out.t_start_sec = t_start_sec;
  out.probs = std::move(total_preds);
  stream_time_sec_ = t_start_sec + total_frames * out.frame_period_sec;
  return out;
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

  // NeMo center-pads the STFT by n_fft/2; valid (non-pad) frames = floor(L/hop).
  const int hop = static_cast<int>(std::lround(config_.hop_size_sec *
                                                config_.sample_rate));
  int valid_mel = std::min(t_mel, chunk.num_samples / hop);

  // 2) Streaming chunked forward over the full mel.
  return RunStreaming(mel_fm.data(), n_mels, t_mel, valid_mel,
                      chunk.t_start_sec);
}

}  // namespace model
}  // namespace orator
