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
  const std::vector<float>& emb = local_centroid_[local];
  // If we already enrolled this local speaker, keep refining that entry.
  auto mapped = local_to_global_.find(local);
  if (mapped != local_to_global_.end() &&
      session_enrolled_.count(mapped->second)) {
    db_->Update(mapped->second, emb.data());
    return;
  }
  float score = 0.0f;
  const int idx = db_->Match(emb.data(), config_.match_threshold, &score);
  std::string resolved;
  bool enrolled = false;
  if (idx >= 0) {
    resolved = db_->SpeakerIdAt(idx);
  } else {
    // E2: only enrol a NEW identity once enough best spans agree, so a single
    // noisy span cannot spawn a spurious speaker (false split).
    if (static_cast<int>(local_refs_[local].size()) < config_.enroll_min_refs)
      return;
    resolved = NewGlobalId();
    db_->Enroll(resolved, emb.data());
    session_enrolled_.insert(resolved);
    enrolled = true;
  }
  // Log only when the local speaker's resolved identity is new or changes, so
  // the trace stays at one line per identity decision (not per delivery).
  const bool changed = mapped == local_to_global_.end() ||
                       mapped->second != resolved;
  local_to_global_[local] = resolved;
  if (changed) {
    LOG_INFO("[speaker-id] local %d -> %s (%s cosine=%.3f, %zu refs)\n", local,
             resolved.c_str(), enrolled ? "enrolled" : "match", score,
             local_refs_[local].size());
  }
}

void SpeakerIdentityStage::Process(std::vector<core::DiarSegment>& segs) {
  if (segs.empty() || embedder_ == nullptr || db_ == nullptr) return;

  // Sortformer "提选优质段落": per local speaker, pick the highest-quality fresh
  // clean span (quality = confidence x duration), only if its audio is still in
  // the retained window (else it would be re-picked every delivery and never
  // read). TitaNet then refines that speaker's centroid voiceprint.
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
    // E1: embed the centre of the span (skip an edge margin) to avoid the
    // onset/offset crosstalk that contaminates turn boundaries.
    double a = s.start_sec, b = s.end_sec;
    if ((b - a) > 2 * config_.edge_margin_sec + 0.5) {
      a += config_.edge_margin_sec;
      b -= config_.edge_margin_sec;
    }
    std::vector<float> pcm =
        audio_.ReadSpan(tb_.SampleAt(a), tb_.SampleAt(b));
    if (pcm.empty()) continue;  // span aged out of the retain window
    core::AudioChunk chunk;
    chunk.samples = pcm.data();
    chunk.num_samples = static_cast<int>(pcm.size());
    chunk.sample_rate = static_cast<int>(tb_.sample_rate());
    chunk.t_start_sec = a;
    std::vector<float> emb = embedder_->Embed(chunk);
    if (static_cast<int>(emb.size()) != config_.embedding_dim) continue;
    AddReference(local, best_q[local], emb);
    local_last_embedded_end_[local] = s.end_sec;
    ResolveGlobal(local);
  }

  // Assign the resolved global identity to every segment (revisable: the map
  // may have changed since the last delivery).
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
  session_enrolled_.clear();
  next_global_id_ = db_ ? db_->Size() : 0;
}

}  // namespace pipeline
}  // namespace orator
