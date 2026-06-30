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
      audio_(static_cast<int>(tb.sample_rate()), config.retain_sec) {
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
  // Single speaker: no other-speaker segment overlaps this span.
  for (const auto& o : all) {
    if (&o == &s) continue;
    if (o.local_speaker == s.local_speaker) continue;
    const double a = std::max(s.start_sec, o.start_sec);
    const double b = std::min(s.end_sec, o.end_sec);
    if (b - a > config_.overlap_eps_sec) return false;
  }
  return true;
}

void SpeakerIdentityStage::AddReference(int local, double quality,
                                        const std::vector<float>& emb) {
  auto& refs = local_refs_[local];
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
  local_centroid_[local] = std::move(c);
}

std::string SpeakerIdentityStage::NewGlobalId() {
  while (true) {
    std::string id = "spk_" + std::to_string(next_global_id_++);
    if (!db_->Contains(id)) return id;
  }
}

void SpeakerIdentityStage::ResolveGlobal(int local) {
  // A slot that already has a global id keeps it (stable identity); its centroid
  // is strengthened later in RefreshGlobalCentroids.
  auto mapped = local_to_global_.find(local);
  if (mapped != local_to_global_.end()) return;

  // Trust the diarizer's within-session separation: two local slots of the SAME
  // session are distinct speakers, so a slot must not resolve to a global id
  // already taken by another slot of its session. Matching IS allowed across
  // sessions (cross-session voiceprint stitching after a reset).
  const int spk = config_.speakers_per_session > 0
                      ? config_.speakers_per_session
                      : 4;
  const int session = local / spk;
  std::vector<std::string> exclude;
  for (const auto& kv : local_to_global_) {
    if (kv.first != local && kv.first / spk == session)
      exclude.push_back(kv.second);
  }
  float score = 0.0f;
  const int idx = db_->MatchExcluding(local_centroid_[local].data(),
                                      config_.match_threshold, exclude, &score);
  std::string resolved;
  bool enrolled = false;
  if (idx >= 0) {
    resolved = db_->SpeakerIdAt(idx);  // returning speaker -> its registry id
  } else {
    // No registry speaker matches: enrol a genuinely new identity. E2: require
    // enough best spans first so a single noisy span cannot spawn a spurious
    // speaker. The registry is never capped -- it grows to recognise everyone.
    if (static_cast<int>(local_refs_[local].size()) < config_.enroll_min_refs)
      return;
    resolved = NewGlobalId();
    db_->Enroll(resolved, local_centroid_[local].data());
    enrolled = true;
  }
  local_to_global_[local] = resolved;
  LOG_INFO("[speaker-id] local %d -> %s (%s cosine=%.3f, %zu refs)\n", local,
           resolved.c_str(), enrolled ? "enrolled" : "match", score,
           local_refs_[local].size());
}

void SpeakerIdentityStage::RefreshGlobalCentroids() {
  // Cross-session accumulation: each global's registry centroid is the
  // L2-normalized mean of the best references from ALL local slots mapped to it
  // (across sessions). The more clean evidence a speaker accumulates, the more
  // robust its voiceprint, so the speaker reliably re-matches next session
  // instead of fragmenting into a duplicate id.
  std::map<std::string, std::vector<std::pair<double, std::vector<float>>>> g;
  for (const auto& kv : local_to_global_) {
    auto it = local_refs_.find(kv.first);
    if (it == local_refs_.end()) continue;
    auto& refs = g[kv.second];
    refs.insert(refs.end(), it->second.begin(), it->second.end());
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
  bool merged = true;
  while (merged) {
    merged = false;
    std::vector<std::string> ids;
    ids.reserve(global_centroid_.size());
    for (const auto& kv : global_centroid_) ids.push_back(kv.first);
    for (std::size_t i = 0; i < ids.size() && !merged; ++i) {
      for (std::size_t j = i + 1; j < ids.size() && !merged; ++j) {
        const auto& a = global_centroid_[ids[i]];
        const auto& b = global_centroid_[ids[j]];
        double cos = 0.0;
        for (int k = 0; k < config_.embedding_dim; ++k) cos += a[k] * b[k];
        if (cos <= config_.merge_threshold) continue;
        // Keep the earlier-enrolled id (smaller number = original); re-point the
        // duplicate's slots to it and delete the duplicate from the registry.
        std::string keep = ids[i], drop = ids[j];
        if (id_num(drop) < id_num(keep)) std::swap(keep, drop);
        for (auto& kv : local_to_global_)
          if (kv.second == drop) kv.second = keep;
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
  double a = start_sec, b = end_sec;
  if ((b - a) > 2 * config_.edge_margin_sec + 0.5) {
    a += config_.edge_margin_sec;
    b -= config_.edge_margin_sec;
  }
  // Cap the embedded window: a voiceprint needs only a few seconds, and a long
  // single-speaker turn would otherwise feed huge audio to TitaNet (mel/encoder
  // buffers grow with length) and exhaust GPU memory over a long session.
  if (b - a > config_.max_embed_window_sec) {
    const double mid = 0.5 * (a + b);
    a = mid - 0.5 * config_.max_embed_window_sec;
    b = mid + 0.5 * config_.max_embed_window_sec;
  }
  std::vector<float> pcm = audio_.ReadSpan(tb_.SampleAt(a), tb_.SampleAt(b));
  if (pcm.empty()) return {};  // span aged out of the retain window
  core::AudioChunk chunk;
  chunk.samples = pcm.data();
  chunk.num_samples = static_cast<int>(pcm.size());
  chunk.sample_rate = static_cast<int>(tb_.sample_rate());
  chunk.t_start_sec = a;
  std::vector<float> emb = embedder_->Embed(chunk);
  if (static_cast<int>(emb.size()) != config_.embedding_dim) return {};
  return emb;
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
    auto last = local_last_embedded_end_.find(s.local_speaker);
    if (last != local_last_embedded_end_.end() && s.end_sec <= last->second) {
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
    AddReference(local, best_q[local], emb);
    local_last_embedded_end_[local] = s.end_sec;
    ResolveGlobal(local);
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
  // voiceprint is too noisy to re-decide identity for these similar voices, and
  // the diarizer has already grouped the segment under its speaker.
  for (auto& s : segs) {
    auto it = local_to_global_.find(s.local_speaker);
    if (it != local_to_global_.end()) s.speaker_id = it->second;
  }
}

void SpeakerIdentityStage::Reset() {
  audio_.Reset();
  local_refs_.clear();
  local_centroid_.clear();
  local_last_embedded_end_.clear();
  local_to_global_.clear();
  global_centroid_.clear();
  next_global_id_ = db_ ? db_->Size() : 0;
}

}  // namespace pipeline
}  // namespace orator
