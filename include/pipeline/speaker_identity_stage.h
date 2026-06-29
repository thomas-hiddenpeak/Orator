#pragma once

// SpeakerIdentityStage (Spec 010): the post-diarization speaker-identity stage
// that runs INSIDE the diarization pipeline (Constitution Art. III: it does not
// form a fourth pipeline and never reads ASR/VAD state directly other than
// through the protocol-published VAD speech segments it is fed).
//
// On each diarization delivery it receives the freshly derived speaker-segment
// view and resolves a persistent GLOBAL voiceprint identity for each
// diarizer-local speaker: it embeds CLEAN single-speaker audio with the TitaNet
// embedder, matches the embedding against the SpeakerDatabase (cosine >= tau =>
// known identity, else auto-enroll a new one), and fills DiarSegment::speaker_id
// with the resolved global id. The local->global map is revisable: it refines
// as more clean audio accumulates and the next delivery carries any change.

#include <map>
#include <mutex>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "core/stages.h"
#include "core/time_base.h"
#include "core/types.h"
#include "model/speaker_database.h"
#include "pipeline/retained_audio_buffer.h"

namespace orator {
namespace pipeline {

struct SpeakerIdConfig {
  int embedding_dim = 192;
  double min_embed_sec = 3.0;    // shortest clean span worth embedding (longer
                                // = cleaner = better separation; EER ~halves
                                // from 1.5s to 4s on the meeting ground truth)
  float match_threshold = 0.55f;  // cosine tau ~ measured EER for ~3-4s spans
  float min_confidence = 0.5f;    // diar mean-activity gate
  double overlap_eps_sec = 0.1;   // tolerance for "overlaps another speaker"
  int max_ref_segs = 6;           // best clean spans averaged per voiceprint
  double retain_sec = 180.0;      // audio retention window for span reads
};

class SpeakerIdentityStage {
 public:
  SpeakerIdentityStage(core::ISpeakerEmbedder* embedder,
                       model::SpeakerDatabase* db, core::TimeBase tb,
                       SpeakerIdConfig config);

  // Feed mono-16k audio in stream order (called on the diarization thread).
  void AppendAudio(const float* samples, int n);

  // Resolve global identities and fill DiarSegment::speaker_id in place
  // (called on the diarization thread, before the segments are delivered).
  void Process(std::vector<core::DiarSegment>& segs);

  void Reset();

  int enrolled_count() const { return db_->Size(); }

 private:
  // True when [s.start, s.end] is a clean single-speaker span (Sortformer
  // confidence + no other-speaker overlap; no VAD dependency — diar's
  // end-to-end separation already marks single-speaker speech).
  bool IsClean(const core::DiarSegment& s,
               const std::vector<core::DiarSegment>& all) const;
  // Add a high-quality span's embedding to a local speaker's reference set
  // (keeps the best `max_ref_segs` by quality) and recompute the centroid.
  void AddReference(int local, double quality, const std::vector<float>& emb);
  // Match the local centroid voiceprint against the registry; enroll if unseen.
  void ResolveGlobal(int local);
  std::string NewGlobalId();

  core::ISpeakerEmbedder* embedder_;
  model::SpeakerDatabase* db_;
  core::TimeBase tb_;
  SpeakerIdConfig config_;
  RetainedAudioBuffer audio_;

  // Diarization-thread-only state (no lock needed). Per local speaker: the best
  // reference embeddings (quality = confidence x duration) + their centroid.
  std::map<int, std::vector<std::pair<double, std::vector<float>>>> local_refs_;
  std::map<int, std::vector<float>> local_centroid_;
  std::map<int, double> local_last_embedded_end_;   // local -> last span end
  std::map<int, std::string> local_to_global_;      // local -> global id
  std::set<std::string> session_enrolled_;          // ids this session created
  int next_global_id_ = 0;
};

}  // namespace pipeline
}  // namespace orator

