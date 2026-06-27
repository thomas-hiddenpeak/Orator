// SpeakerIdentityStage implementation (Spec 010). See header for the contract.

#include "pipeline/speaker_identity_stage.h"

#include <algorithm>
#include <cmath>

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

void SpeakerIdentityStage::AddVadSegment(double start_sec, double end_sec) {
  if (end_sec <= start_sec) return;
  std::lock_guard<std::mutex> lk(vad_mutex_);
  vad_segments_.emplace_back(start_sec, end_sec);
}

double SpeakerIdentityStage::VadCoverage(
    double start, double end,
    const std::vector<std::pair<double, double>>& vad) const {
  const double dur = end - start;
  if (dur <= 0.0) return 0.0;
  double covered = 0.0;
  for (const auto& v : vad) {
    const double a = std::max(start, v.first);
    const double b = std::min(end, v.second);
    if (b > a) covered += b - a;
  }
  return covered / dur;
}

bool SpeakerIdentityStage::IsClean(
    const core::DiarSegment& s, const std::vector<core::DiarSegment>& all,
    const std::vector<std::pair<double, double>>& vad) const {
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
  // VAD-confirmed (skipped when VAD is unavailable so the feature still works).
  if (!vad.empty() &&
      VadCoverage(s.start_sec, s.end_sec, vad) < config_.vad_min_coverage) {
    return false;
  }
  return true;
}

void SpeakerIdentityStage::UpdateLocalEmbedding(int local,
                                                const std::vector<float>& emb) {
  auto it = local_emb_.find(local);
  if (it == local_emb_.end()) {
    local_emb_[local] = emb;
    return;
  }
  std::vector<float>& cur = it->second;
  const float a = config_.ema_alpha;
  double nrm = 0.0;
  for (size_t i = 0; i < cur.size() && i < emb.size(); ++i) {
    cur[i] = a * cur[i] + (1.0f - a) * emb[i];
    nrm += static_cast<double>(cur[i]) * cur[i];
  }
  nrm = std::sqrt(nrm) + 1e-12;
  for (float& v : cur) v = static_cast<float>(v / nrm);
}

std::string SpeakerIdentityStage::NewGlobalId() {
  while (true) {
    std::string id = "spk_" + std::to_string(next_global_id_++);
    if (!db_->Contains(id)) return id;
  }
}

void SpeakerIdentityStage::ResolveGlobal(int local) {
  const std::vector<float>& emb = local_emb_[local];
  // If we already enrolled this local speaker, keep refining that entry.
  auto mapped = local_to_global_.find(local);
  if (mapped != local_to_global_.end() &&
      session_enrolled_.count(mapped->second)) {
    db_->Update(mapped->second, emb.data());
    return;
  }
  float score = 0.0f;
  const int idx = db_->Match(emb.data(), config_.match_threshold, &score);
  if (idx >= 0) {
    local_to_global_[local] = db_->SpeakerIdAt(idx);
  } else {
    const std::string id = NewGlobalId();
    db_->Enroll(id, emb.data());
    session_enrolled_.insert(id);
    local_to_global_[local] = id;
  }
}

void SpeakerIdentityStage::Process(std::vector<core::DiarSegment>& segs) {
  if (segs.empty() || embedder_ == nullptr || db_ == nullptr) return;

  std::vector<std::pair<double, double>> vad;
  {
    std::lock_guard<std::mutex> lk(vad_mutex_);
    vad = vad_segments_;
  }

  // For each local speaker, pick the longest fresh clean span (one not already
  // embedded) as this delivery's embedding candidate.
  std::map<int, const core::DiarSegment*> candidate;
  for (const auto& s : segs) {
    if (s.local_speaker < 0) continue;
    if (!IsClean(s, segs, vad)) continue;
    auto last = local_last_embedded_end_.find(s.local_speaker);
    if (last != local_last_embedded_end_.end() && s.end_sec <= last->second) {
      continue;  // already covered
    }
    auto c = candidate.find(s.local_speaker);
    if (c == candidate.end() ||
        (s.end_sec - s.start_sec) >
            (c->second->end_sec - c->second->start_sec)) {
      candidate[s.local_speaker] = &s;
    }
  }

  // Embed each candidate, refine the per-local voiceprint, resolve identity.
  for (const auto& kv : candidate) {
    const int local = kv.first;
    const core::DiarSegment& s = *kv.second;
    const long s0 = tb_.SampleAt(s.start_sec);
    const long s1 = tb_.SampleAt(s.end_sec);
    std::vector<float> pcm = audio_.ReadSpan(s0, s1);
    if (pcm.empty()) continue;  // span aged out of the retain window
    core::AudioChunk chunk;
    chunk.samples = pcm.data();
    chunk.num_samples = static_cast<int>(pcm.size());
    chunk.sample_rate = static_cast<int>(tb_.sample_rate());
    chunk.t_start_sec = s.start_sec;
    std::vector<float> emb = embedder_->Embed(chunk);
    if (static_cast<int>(emb.size()) != config_.embedding_dim) continue;
    UpdateLocalEmbedding(local, emb);
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
  {
    std::lock_guard<std::mutex> lk(vad_mutex_);
    vad_segments_.clear();
  }
  local_emb_.clear();
  local_last_embedded_end_.clear();
  local_to_global_.clear();
  session_enrolled_.clear();
  next_global_id_ = db_ ? db_->Size() : 0;
}

}  // namespace pipeline
}  // namespace orator
