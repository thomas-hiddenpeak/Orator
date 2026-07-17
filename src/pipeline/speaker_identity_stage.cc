// SpeakerIdentityStage implementation (Spec 010). See header for the contract.
//
// Division of labour: Sortformer separates speakers fast and flags high-quality
// spans (confidence x duration, no overlap); TitaNet runs only on those best
// spans and builds an accuracy-first centroid voiceprint per speaker, matched
// 1:N against the persistent registry. No VAD dependency: the end-to-end
// diarizer already marks single-speaker speech.

#include "pipeline/speaker_identity_stage.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <set>

#include "core/log.h"

namespace orator {
namespace pipeline {

SpeakerIdentityStage::SpeakerIdentityStage(core::ISpeakerEmbedder* embedder,
                                           model::SpeakerDatabase* db,
                                           core::TimeBase tb,
                                           SpeakerIdConfig config)
    : embedder_(embedder),
      db_(db),
      tb_(tb),
      config_(config),
      audio_(tb, config.retain_sec) {
  next_global_id_ = db_ ? db_->Size() : 0;
}

void SpeakerIdentityStage::AppendAudio(const float* samples, int n) {
  audio_.Append(samples, n);
}

bool SpeakerIdentityStage::IsClean(
    const core::DiarSegment& s,
    const std::vector<core::DiarSegment>& all) const {
  if (s.end_sec - s.start_sec < config_.min_embed_sec) return false;
  if (s.confidence < config_.min_confidence) return false;
  const auto window = EmbeddingWindow(s.start_sec, s.end_sec);
  if (window.second <= window.first) return false;
  // Single speaker: no other-speaker segment overlaps the audio window that
  // TitaNet will actually embed. A long turn can have brief boundary crosstalk,
  // but if the centre voiceprint window is clean it is useful evidence.
  for (const auto& o : all) {
    if (&o == &s) continue;
    if (o.local_speaker == s.local_speaker) continue;
    const double a = std::max(window.first, o.start_sec);
    const double b = std::min(window.second, o.end_sec);
    if (b - a > config_.overlap_eps_sec) return false;
  }
  return true;
}

SpeakerIdentityStage::LocalEpoch& SpeakerIdentityStage::EnsureEpoch(
    int local, double start_sec) {
  auto& epochs = local_epochs_[local];
  if (epochs.empty()) {
    LocalEpoch e;
    // The first clean span validates the slot; earlier segments from the same
    // local slot should use the same identity when the full view is reprojected.
    e.start_sec = 0.0;
    epochs.push_back(std::move(e));
  }
  (void)start_sec;
  return epochs.back();
}

SpeakerIdentityStage::LocalEpoch& SpeakerIdentityStage::StartEpoch(
    int local, double start_sec, bool allow_same_session_match) {
  auto& epochs = local_epochs_[local];
  LocalEpoch e;
  e.start_sec = start_sec;
  e.allow_same_session_match = allow_same_session_match;
  epochs.push_back(std::move(e));
  return epochs.back();
}

SpeakerIdentityStage::LocalEpoch* SpeakerIdentityStage::ActiveEpoch(
    int local, double at_sec) {
  auto it = local_epochs_.find(local);
  if (it == local_epochs_.end() || it->second.empty()) return nullptr;
  LocalEpoch* out = &it->second.front();
  for (auto& epoch : it->second) {
    if (epoch.start_sec <= at_sec + 1e-6) out = &epoch;
  }
  return out;
}

const SpeakerIdentityStage::LocalEpoch* SpeakerIdentityStage::ActiveEpoch(
    int local, double at_sec) const {
  auto it = local_epochs_.find(local);
  if (it == local_epochs_.end() || it->second.empty()) return nullptr;
  const LocalEpoch* out = &it->second.front();
  for (const auto& epoch : it->second) {
    if (epoch.start_sec <= at_sec + 1e-6) out = &epoch;
  }
  return out;
}

float SpeakerIdentityStage::Cosine(const std::vector<float>& a,
                                   const std::vector<float>& b) const {
  if (static_cast<int>(a.size()) != config_.embedding_dim ||
      static_cast<int>(b.size()) != config_.embedding_dim) {
    return 0.0f;
  }
  double cos = 0.0;
  for (int i = 0; i < config_.embedding_dim; ++i) cos += a[i] * b[i];
  return static_cast<float>(cos);
}

bool SpeakerIdentityStage::ShouldSplitEpoch(
    const LocalEpoch& epoch, double start_sec, double end_sec,
    const std::vector<float>& emb) const {
  if (config_.local_drift_threshold <= 0.0f) return false;
  if (epoch.global_id.empty() || epoch.centroid.empty()) return false;
  if (end_sec - start_sec < config_.local_drift_min_span_sec) return false;
  if (start_sec - epoch.start_sec < config_.local_drift_min_epoch_sec) {
    return false;
  }
  return Cosine(epoch.centroid, emb) < config_.local_drift_threshold;
}

double SpeakerIdentityStage::BackfillStartForLocal(
    const core::DiarSegment& s, const std::vector<core::DiarSegment>& all,
    int local) const {
  if (config_.local_drift_competing_backfill_gap_sec <= 0.0) {
    return s.start_sec;
  }
  std::vector<const core::DiarSegment*> same_local;
  for (const auto& o : all) {
    if (o.local_speaker == local && o.end_sec <= s.start_sec + 1e-6) {
      same_local.push_back(&o);
    }
  }
  std::sort(same_local.begin(), same_local.end(),
            [](const auto* a, const auto* b) {
              if (a->start_sec != b->start_sec)
                return a->start_sec < b->start_sec;
              return a->end_sec < b->end_sec;
            });
  double start = s.start_sec;
  double cursor = s.start_sec;
  for (auto it = same_local.rbegin(); it != same_local.rend(); ++it) {
    const core::DiarSegment& prev = **it;
    if (cursor - prev.end_sec >
        config_.local_drift_competing_backfill_gap_sec) {
      break;
    }
    start = std::min(start, prev.start_sec);
    cursor = prev.start_sec;
  }
  return start;
}

std::pair<double, double> SpeakerIdentityStage::EmbeddingWindow(
    double start_sec, double end_sec) const {
  double a = start_sec;
  double b = end_sec;
  if ((b - a) > 2 * config_.edge_margin_sec + 0.5) {
    a += config_.edge_margin_sec;
    b -= config_.edge_margin_sec;
  }
  if (b - a > config_.max_embed_window_sec) {
    const double mid = 0.5 * (a + b);
    a = mid - 0.5 * config_.max_embed_window_sec;
    b = mid + 0.5 * config_.max_embed_window_sec;
  }
  return {a, b};
}

std::set<std::string> SpeakerIdentityStage::OverlappingGlobalIds(
    const core::DiarSegment& s, const std::vector<core::DiarSegment>& all,
    int local) const {
  std::set<std::string> blocked;
  const auto window = EmbeddingWindow(s.start_sec, s.end_sec);
  if (window.second <= window.first) return blocked;
  for (const auto& o : all) {
    if (&o == &s) continue;
    if (o.local_speaker == local) continue;
    const double a = std::max(window.first, o.start_sec);
    const double b = std::min(window.second, o.end_sec);
    if (b - a <= config_.overlap_eps_sec) continue;
    const LocalEpoch* other = ActiveEpoch(o.local_speaker, o.start_sec);
    if (other != nullptr && !other->global_id.empty())
      blocked.insert(other->global_id);
  }
  return blocked;
}

std::string SpeakerIdentityStage::BestCompetingGlobal(
    const LocalEpoch& epoch, const std::vector<float>& emb,
    const std::set<std::string>& blocked_ids, float threshold, float margin,
    float* own_score, float* best_score) const {
  if (own_score != nullptr) *own_score = 0.0f;
  if (best_score != nullptr) *best_score = 0.0f;
  if (threshold <= 0.0f) return {};
  if (epoch.global_id.empty() || epoch.centroid.empty()) return {};

  const float own = Cosine(epoch.centroid, emb);
  if (own_score != nullptr) *own_score = own;

  std::string best_id;
  float best = threshold;
  for (const auto& kv : global_centroid_) {
    if (kv.first == epoch.global_id) continue;
    if (blocked_ids.count(kv.first) > 0) continue;
    const float score = Cosine(kv.second, emb);
    if (score > best) {
      best = score;
      best_id = kv.first;
    }
  }
  if (best_score != nullptr) *best_score = best_id.empty() ? 0.0f : best;
  if (best_id.empty()) return {};
  if (best < own + margin) return {};
  return best_id;
}

bool SpeakerIdentityStage::RecordPendingCompeting(
    int local, const core::DiarSegment& s, double backfill_start,
    double quality, const std::vector<float>& emb,
    const std::string& global_id, float own_score, float competing_score) {
  PendingCompetingEpoch& pending = pending_competing_[local];
  if (pending.valid && pending.global_id == global_id &&
      s.start_sec - pending.clean_start_sec <=
          config_.local_drift_competing_backfill_sec) {
    pending.start_sec = std::min(pending.start_sec, backfill_start);
    if (s.start_sec + 1e-6 >= pending.end_sec) {
      pending.end_sec = s.end_sec;
      pending.own_score = own_score;
      pending.competing_score = competing_score;
      ++pending.confirmations;
    }
    const int required =
        config_.local_drift_competing_candidate_min_confirmations;
    LOG_INFO(
        "[speaker-id] local %d repeated competing drift at %.3f -> %s "
        "(%d/%d confirmations, score=%.3f, own=%.3f)\n",
        local, s.start_sec, global_id.c_str(), pending.confirmations, required,
        competing_score, own_score);
    return required > 0 && pending.confirmations >= required;
  }
  pending.valid = true;
  pending.start_sec = backfill_start;
  pending.clean_start_sec = s.start_sec;
  pending.end_sec = s.end_sec;
  pending.quality = quality;
  pending.global_id = global_id;
  pending.own_score = own_score;
  pending.competing_score = competing_score;
  pending.confirmations = 1;
  pending.emb = emb;
  LOG_INFO(
      "[speaker-id] local %d pending competing drift at %.3f -> %s "
      "(score=%.3f, own=%.3f, backfill=%.3f)\n",
      local, s.start_sec, global_id.c_str(), competing_score, own_score,
      backfill_start);
  return config_.local_drift_competing_candidate_min_confirmations == 1;
}

void SpeakerIdentityStage::AddReference(LocalEpoch* epoch, double quality,
                                        const std::vector<float>& emb) {
  if (epoch == nullptr) return;
  auto& refs = epoch->refs;
  refs.emplace_back(quality, emb);
  // Keep the best `max_ref_segs` spans by quality (confidence x duration).
  std::sort(refs.begin(), refs.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });
  if (static_cast<int>(refs.size()) > config_.max_ref_segs)
    refs.resize(config_.max_ref_segs);
  // Centroid = L2-normalized mean of the kept reference embeddings.
  std::vector<float> c(config_.embedding_dim, 0.0f);
  for (const auto& r : refs)
    for (int i = 0; i < config_.embedding_dim; ++i) c[i] += r.second[i];
  double nrm = 0.0;
  for (float v : c) nrm += static_cast<double>(v) * v;
  nrm = std::sqrt(nrm) + 1e-12;
  for (float& v : c) v = static_cast<float>(v / nrm);
  epoch->centroid = std::move(c);
}

std::string SpeakerIdentityStage::NewGlobalId() {
  while (true) {
    std::string id = "spk_" + std::to_string(next_global_id_++);
    if (!db_->Contains(id)) return id;
  }
}

void SpeakerIdentityStage::ResolveGlobal(int local, LocalEpoch* epoch) {
  if (epoch == nullptr) return;
  // An epoch that already has a global id keeps it; its centroid is strengthened
  // later in RefreshGlobalCentroids. A later drift starts a new epoch instead of
  // replacing this mapping.
  if (!epoch->global_id.empty()) return;
  if (epoch->centroid.empty()) return;

  // Trust the diarizer's within-session separation: two local slots of the SAME
  // session are distinct speakers, so a slot must not resolve to a global id
  // already taken by another slot of its session. Matching IS allowed across
  // sessions (cross-session voiceprint stitching after a reset). A drifted epoch
  // is the exception: it represents evidence that the local slot changed speaker
  // over time, so it may match a global id already owned by another local slot.
  const int spk = config_.speakers_per_session > 0
                      ? config_.speakers_per_session
                      : 4;
  const int session = local / spk;
  const int ref_count = static_cast<int>(epoch->refs.size());
  if (session > 0 && ref_count < config_.cross_session_match_min_refs) {
    LOG_INFO("[speaker-id] local %d deferred (%d/%d refs for cross-session)\n",
             local, ref_count, config_.cross_session_match_min_refs);
    return;
  }
  std::vector<std::string> exclude;
  if (!epoch->allow_same_session_match ||
      !config_.local_drift_allow_same_session_match) {
    for (const auto& kv : local_epochs_) {
      if (kv.first == local || kv.first / spk != session) continue;
      for (const auto& other_epoch : kv.second) {
        if (!other_epoch.global_id.empty())
          exclude.push_back(other_epoch.global_id);
      }
    }
  }
  float score = 0.0f;
  const int idx = db_->MatchExcluding(epoch->centroid.data(),
                                      config_.match_threshold, exclude, &score);
  std::string resolved;
  bool enrolled = false;
  if (idx >= 0) {
    resolved = db_->SpeakerIdAt(idx);  // returning speaker -> its registry id
  } else {
    // No registry speaker matches: enrol a genuinely new identity. E2: require
    // enough best spans first so a single noisy span cannot spawn a spurious
    // speaker. The registry is never capped -- it grows to recognise everyone.
    if (ref_count < config_.enroll_min_refs)
      return;
    if (session > 0 && config_.defer_unmatched_cross_session) {
      LOG_INFO(
          "[speaker-id] local %d stays local-only (no cross-session match, "
          "%d refs)\n",
          local, ref_count);
      return;
    }
    resolved = NewGlobalId();
    db_->Enroll(resolved, epoch->centroid.data());
    enrolled = true;
  }
  epoch->global_id = resolved;
  LOG_INFO("[speaker-id] local %d epoch %.3f -> %s (%s cosine=%.3f, %zu refs)\n",
           local, epoch->start_sec, resolved.c_str(),
           enrolled ? "enrolled" : "match", score, epoch->refs.size());
}

void SpeakerIdentityStage::RefreshGlobalCentroids() {
  // Cross-session accumulation: each global's registry centroid is the
  // L2-normalized mean of the best references from ALL local slots mapped to it
  // (across sessions). The more clean evidence a speaker accumulates, the more
  // robust its voiceprint, so the speaker reliably re-matches next session
  // instead of fragmenting into a duplicate id.
  std::map<std::string, std::vector<std::pair<double, std::vector<float>>>> g;
  for (const auto& kv : local_epochs_) {
    for (const auto& epoch : kv.second) {
      if (epoch.global_id.empty()) continue;
      auto& refs = g[epoch.global_id];
      refs.insert(refs.end(), epoch.refs.begin(), epoch.refs.end());
    }
  }
  global_centroid_.clear();
  for (auto& kv : g) {
    auto& refs = kv.second;
    std::sort(refs.begin(), refs.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });
    if (static_cast<int>(refs.size()) > config_.max_ref_segs)
      refs.resize(config_.max_ref_segs);
    std::vector<float> c(config_.embedding_dim, 0.0f);
    for (const auto& r : refs)
      for (int i = 0; i < config_.embedding_dim; ++i) c[i] += r.second[i];
    double nrm = 0.0;
    for (float v : c) nrm += static_cast<double>(v) * v;
    nrm = std::sqrt(nrm) + 1e-12;
    for (float& v : c) v = static_cast<float>(v / nrm);
    db_->Update(kv.first, c.data());
    global_centroid_[kv.first] = std::move(c);
  }
}

void SpeakerIdentityStage::MergeReconcile() {
  // De-duplicate the registry: merge any two globals whose centroids are
  // confidently the same person. This repairs the early-session duplicate (a
  // returning speaker enrolled before its centroid was strong enough to match)
  // by REMOVING the duplicate entry from the registry, so it holds exactly one
  // id per real speaker -- without ever capping the total speaker count.
  auto id_num = [](const std::string& s) {
    const auto p = s.rfind('_');
    return p == std::string::npos ? 0 : std::atoi(s.c_str() + p + 1);
  };
  const int spk = config_.speakers_per_session > 0
                      ? config_.speakers_per_session
                      : 4;
  bool merged = true;
  while (merged) {
    merged = false;
    // Which globals ever co-occurred in one session: the diarizer separated them
    // into distinct slots, so they are distinct people and must not merge unless
    // the voiceprint is overwhelmingly identical (a diarizer over-split).
    std::map<int, std::set<std::string>> session_globals;
    for (const auto& kv : local_epochs_) {
      for (const auto& epoch : kv.second) {
        if (!epoch.global_id.empty())
          session_globals[kv.first / spk].insert(epoch.global_id);
      }
    }
    auto co_session = [&](const std::string& x, const std::string& y) {
      for (const auto& sg : session_globals)
        if (sg.second.count(x) && sg.second.count(y)) return true;
      return false;
    };
    std::vector<std::string> ids;
    ids.reserve(global_centroid_.size());
    for (const auto& kv : global_centroid_) ids.push_back(kv.first);
    for (std::size_t i = 0; i < ids.size() && !merged; ++i) {
      for (std::size_t j = i + 1; j < ids.size() && !merged; ++j) {
        const auto& a = global_centroid_[ids[i]];
        const auto& b = global_centroid_[ids[j]];
        double cos = 0.0;
        for (int k = 0; k < config_.embedding_dim; ++k) cos += a[k] * b[k];
        const float thr = co_session(ids[i], ids[j])
                              ? config_.cosession_merge_threshold
                              : config_.merge_threshold;
        if (cos <= thr) continue;
        // Keep the earlier-enrolled id (smaller number = original); re-point the
        // duplicate's slots to it and delete the duplicate from the registry.
        std::string keep = ids[i], drop = ids[j];
        if (id_num(drop) < id_num(keep)) std::swap(keep, drop);
        for (auto& kv : local_epochs_)
          for (auto& epoch : kv.second)
            if (epoch.global_id == drop) epoch.global_id = keep;
        db_->Remove(drop);
        global_centroid_.erase(drop);
        LOG_INFO("[speaker-id] merged %s -> %s (cosine=%.3f, same speaker)\n",
                 drop.c_str(), keep.c_str(), cos);
        merged = true;
      }
    }
    if (merged) RefreshGlobalCentroids();  // recompute after re-pointing slots
  }
}

std::vector<float> SpeakerIdentityStage::EmbedSpan(double start_sec,
                                                   double end_sec) {
  // E1: embed the centre of the span (skip an edge margin) to avoid the
  // onset/offset crosstalk that contaminates turn boundaries.
  // Cap the embedded window: a voiceprint needs only a few seconds, and a long
  // single-speaker turn would otherwise feed huge audio to TitaNet (mel/encoder
  // buffers grow with length) and exhaust GPU memory over a long session.
  return EmbedSpan(start_sec, end_sec, config_.edge_margin_sec,
                   config_.max_embed_window_sec);
}

std::vector<float> SpeakerIdentityStage::EmbedSpan(double start_sec,
                                                   double end_sec,
                                                   double edge_margin_sec,
                                                   double max_window_sec) {
  double a = start_sec;
  double b = end_sec;
  if (b - a > 2.0 * edge_margin_sec + 0.5) {
    a += edge_margin_sec;
    b -= edge_margin_sec;
  }
  if (max_window_sec > 0.0 && b - a > max_window_sec) {
    const double middle = 0.5 * (a + b);
    a = middle - 0.5 * max_window_sec;
    b = middle + 0.5 * max_window_sec;
  }
  if (b <= a) return {};
  const long start_sample = tb_.SampleAt(a);
  const long end_sample = tb_.SampleAt(b);
  if (end_sample <= start_sample) return {};

  std::lock_guard<std::mutex> lock(embedding_mutex_);
  const auto key = std::make_pair(start_sample, end_sample);
  const auto cached = embedding_cache_.find(key);
  if (cached != embedding_cache_.end()) return cached->second;

  std::vector<float> pcm = audio_.ReadSpan(start_sample, end_sample);
  if (pcm.empty()) return {};
  core::AudioChunk chunk;
  chunk.samples = pcm.data();
  chunk.num_samples = static_cast<int>(pcm.size());
  chunk.sample_rate = static_cast<int>(tb_.sample_rate());
  chunk.t_start_sec = a;
  std::vector<float> embedding = embedder_->Embed(chunk);
  if (static_cast<int>(embedding.size()) != config_.embedding_dim) return {};
  embedding_cache_.emplace(key, embedding);
  return embedding;
}

bool SpeakerIdentityStage::PrecomputeSpan(double start_sec, double end_sec,
                                          double min_duration_sec,
                                          double edge_margin_sec,
                                          double max_window_sec) {
  if (embedder_ == nullptr || end_sec - start_sec + 1e-9 < min_duration_sec) {
    return false;
  }
  return !EmbedSpan(start_sec, end_sec, edge_margin_sec, max_window_sec)
              .empty();
}

std::size_t SpeakerIdentityStage::cached_embedding_count() const {
  std::lock_guard<std::mutex> lock(embedding_mutex_);
  return embedding_cache_.size();
}

SpeakerIdentityStage::SpanEvidence SpeakerIdentityStage::EvaluateSpan(
    double start_sec, double end_sec,
    const std::vector<std::string>& active_ids, double min_duration_sec,
    double edge_margin_sec, double max_window_sec) {
  SpanEvidence evidence;
  if (embedder_ == nullptr || db_ == nullptr || active_ids.empty() ||
      end_sec - start_sec + 1e-9 < min_duration_sec) {
    return evidence;
  }
  const std::vector<float> embedding = EmbedSpan(
      start_sec, end_sec, edge_margin_sec, max_window_sec);
  if (embedding.empty()) return evidence;
  evidence.embedding_available = true;

  std::map<std::string, std::vector<float>> robust_by_identity;
  for (const auto& [local_speaker, epochs] : local_epochs_) {
    (void)local_speaker;
    for (const auto& epoch : epochs) {
      if (epoch.global_id.empty()) continue;
      auto& scores = robust_by_identity[epoch.global_id];
      for (const auto& reference : epoch.refs) {
        scores.push_back(Cosine(embedding, reference.second));
      }
    }
  }

  evidence.session_scores.reserve(active_ids.size());
  evidence.robust_scores.reserve(active_ids.size());
  evidence.session_gallery_complete = true;
  evidence.robust_gallery_complete = true;
  for (const auto& speaker_id : active_ids) {
    const auto session = global_centroid_.find(speaker_id);
    if (session == global_centroid_.end()) {
      evidence.session_gallery_complete = false;
    } else {
      evidence.session_scores.push_back(
          {speaker_id, Cosine(embedding, session->second)});
    }

    auto robust = robust_by_identity.find(speaker_id);
    if (robust == robust_by_identity.end() || robust->second.empty()) {
      evidence.robust_gallery_complete = false;
      continue;
    }
    std::sort(robust->second.begin(), robust->second.end(),
              [](float left, float right) { return left > right; });
    const std::size_t retained = (robust->second.size() + 1) / 2;
    double total = 0.0;
    for (std::size_t index = 0; index < retained; ++index) {
      total += robust->second[index];
    }
    evidence.robust_scores.push_back(
        {speaker_id, static_cast<float>(total / retained)});
  }
  const auto by_id = [](const VoiceprintScore& left,
                        const VoiceprintScore& right) {
    return left.speaker_id < right.speaker_id;
  };
  std::sort(evidence.session_scores.begin(), evidence.session_scores.end(),
            by_id);
  std::sort(evidence.robust_scores.begin(), evidence.robust_scores.end(),
            by_id);
  return evidence;
}

std::string SpeakerIdentityStage::IdentityAt(int local_speaker,
                                             double at_sec) const {
  const LocalEpoch* epoch = ActiveEpoch(local_speaker, at_sec);
  return epoch == nullptr ? std::string() : epoch->global_id;
}

void SpeakerIdentityStage::Process(std::vector<core::DiarSegment>& segs) {
  if (segs.empty() || embedder_ == nullptr || db_ == nullptr) return;

  // ── Enrolment (Sortformer "提选优质段落" -> TitaNet voiceprint) ──────────
  // Per local speaker, pick the highest-quality fresh CLEAN span (single
  // speaker, confidence x duration) still inside the retain window; TitaNet
  // builds/refines that speaker's centroid voiceprint and enrols it.
  const long audio_base = audio_.base_sample();
  std::map<int, const core::DiarSegment*> candidate;
  std::map<int, double> best_q;
  for (const auto& s : segs) {
    if (s.local_speaker < 0) continue;
    if (tb_.SampleAt(s.start_sec) < audio_base) continue;  // audio aged out
    if (!IsClean(s, segs)) continue;
    LocalEpoch& epoch = EnsureEpoch(s.local_speaker, s.start_sec);
    if (s.end_sec <= epoch.last_embedded_end) {
      continue;  // already covered
    }
    const double q =
        static_cast<double>(s.confidence) * (s.end_sec - s.start_sec);
    auto c = candidate.find(s.local_speaker);
    if (c == candidate.end() || q > best_q[s.local_speaker]) {
      candidate[s.local_speaker] = &s;
      best_q[s.local_speaker] = q;
    }
  }
  for (const auto& kv : candidate) {
    const int local = kv.first;
    const core::DiarSegment& s = *kv.second;
    std::vector<float> emb = EmbedSpan(s.start_sec, s.end_sec);
    if (emb.empty()) continue;
    LocalEpoch* epoch = ActiveEpoch(local, s.start_sec);
    if (epoch == nullptr) epoch = &EnsureEpoch(local, s.start_sec);
    float own_score = 0.0f;
    float competing_score = 0.0f;
    std::string competing_id;
    if (s.end_sec - s.start_sec >=
            config_.local_drift_competing_min_span_sec &&
        s.start_sec - epoch->start_sec >= config_.local_drift_min_epoch_sec) {
      const auto blocked = OverlappingGlobalIds(s, segs, local);
      competing_id = BestCompetingGlobal(
          *epoch, emb, blocked, config_.local_drift_competing_threshold,
          config_.local_drift_competing_margin, &own_score,
          &competing_score);
      if (competing_id.empty() &&
          config_.local_drift_competing_candidate_threshold > 0.0f &&
          config_.local_drift_competing_backfill_sec > 0.0) {
        float candidate_own_score = 0.0f;
        float candidate_score = 0.0f;
        std::string candidate_id = BestCompetingGlobal(
            *epoch, emb, blocked,
            config_.local_drift_competing_candidate_threshold,
            config_.local_drift_competing_candidate_margin,
            &candidate_own_score, &candidate_score);
        if (!candidate_id.empty()) {
          const double backfill_start = std::max(
              epoch->last_embedded_end,
              BackfillStartForLocal(s, segs, local));
          const bool confirmed = RecordPendingCompeting(
              local, s, backfill_start, best_q[local], emb, candidate_id,
              candidate_own_score, candidate_score);
          if (!confirmed) {
            epoch->last_embedded_end = s.end_sec;
            continue;
          }
          competing_id = candidate_id;
          own_score = candidate_own_score;
          competing_score = candidate_score;
        }
      }
    }
    if (competing_id.empty()) {
      auto pending_it = pending_competing_.find(local);
      if (pending_it != pending_competing_.end()) {
        const bool expired =
            s.start_sec - pending_it->second.clean_start_sec >
            config_.local_drift_competing_backfill_sec;
        const bool active_epoch_confirmed =
            config_.local_drift_competing_threshold > 0.0f &&
            Cosine(epoch->centroid, emb) >=
                config_.local_drift_competing_threshold;
        if (expired || active_epoch_confirmed) {
          pending_competing_.erase(pending_it);
        }
      }
    }
    if (ShouldSplitEpoch(*epoch, s.start_sec, s.end_sec, emb)) {
      const float cos = Cosine(epoch->centroid, emb);
      LOG_INFO(
          "[speaker-id] local %d drift at %.3f (cosine=%.3f < %.3f); "
          "starting new epoch\n",
          local, s.start_sec, cos, config_.local_drift_threshold);
      epoch = &StartEpoch(local, s.start_sec,
                          /*allow_same_session_match=*/true);
      pending_competing_.erase(local);
    } else if (!competing_id.empty()) {
      // A return split may include a short lead-in, but it must never cross
      // clean audio already committed to the active epoch. A saved pending
      // candidate can still extend this boundary to its own earlier evidence.
      double epoch_start = std::max(
          epoch->last_embedded_end, BackfillStartForLocal(s, segs, local));
      auto pending_it = pending_competing_.find(local);
      const bool use_pending =
          pending_it != pending_competing_.end() && pending_it->second.valid &&
          pending_it->second.global_id == competing_id &&
          s.start_sec - pending_it->second.clean_start_sec <=
              config_.local_drift_competing_backfill_sec;
      if (use_pending)
        epoch_start = std::min(epoch_start, pending_it->second.start_sec);
      LOG_INFO(
          "[speaker-id] local %d competing drift at %.3f (%s %.3f > "
          "%s %.3f + %.3f); starting new epoch at %.3f\n",
          local, s.start_sec, competing_id.c_str(), competing_score,
          epoch->global_id.c_str(), own_score,
          config_.local_drift_competing_margin, epoch_start);
      epoch = &StartEpoch(local, epoch_start,
                          /*allow_same_session_match=*/true);
      epoch->global_id = competing_id;
      if (use_pending) {
        AddReference(epoch, pending_it->second.quality, pending_it->second.emb);
        epoch->last_embedded_end = pending_it->second.end_sec;
      }
      pending_competing_.erase(local);
    }
    AddReference(epoch, best_q[local], emb);
    epoch->last_embedded_end = std::max(epoch->last_embedded_end, s.end_sec);
    ResolveGlobal(local, epoch);
  }
  // Strengthen every global speaker's centroid with all accumulated references
  // (cross-session), so returning speakers re-match reliably without capping the
  // registry.
  RefreshGlobalCentroids();
  // Merge confident duplicate ids (same person enrolled twice early on).
  MergeReconcile();

  // ── Identity assignment ─────────────────────────────────────────────────
  // TitaNet resolves the global identity of EACH local speaker (its voiceprint
  // centroid -> match/enrol against the registry, with cross-session stitching);
  // every segment then carries its local speaker's resolved global id. Matching
  // is done at the speaker level, not per-segment: a single short segment's
  // voiceprint is too noisy to re-decide identity for these similar voices. A
  // long-session local slot can still split into multiple time-ordered epochs
  // when later clean evidence is inconsistent with its current voiceprint.
  for (auto& s : segs) {
    const LocalEpoch* epoch = ActiveEpoch(s.local_speaker, s.start_sec);
    if (epoch != nullptr && !epoch->global_id.empty()) {
      s.speaker_id = epoch->global_id;
    }
  }
}

void SpeakerIdentityStage::Reset() {
  audio_.Reset();
  {
    std::lock_guard<std::mutex> lock(embedding_mutex_);
    embedding_cache_.clear();
  }
  local_epochs_.clear();
  pending_competing_.clear();
  global_centroid_.clear();
  next_global_id_ = db_ ? db_->Size() : 0;
}

}  // namespace pipeline
}  // namespace orator
