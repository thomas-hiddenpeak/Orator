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
  double edge_margin_sec = 0.3;   // trim each span edge (avoid boundary crosstalk)
  double max_embed_window_sec = 10.0;  // cap embedded audio (>10s is plenty for
                                       // a voiceprint and bounds GPU memory)
  int enroll_min_refs = 1;        // clean spans required before enrolling a NEW
                                  // id; 1 is safe because the diarizer already
                                  // separates within-session speakers (no false
                                  // split) and same-session ids never merge
  int cross_session_match_min_refs = 1;  // refs required before a reset-session
                                  // slot may match an existing global id; >1
                                  // reduces one-span false re-identification
  bool defer_unmatched_cross_session = false;  // later-session slots that do not
                                  // match a known global stay local-only instead
                                  // of immediately enrolling a new global id
  double retain_sec = 180.0;      // audio retention window for span reads
  int speakers_per_session = 4;   // diarizer slots per session (Sortformer = 4);
                                  // two slots of one session are distinct
                                  // speakers and never merge to one global id
  float merge_threshold = 0.70f;  // cosine above which two global centroids are
                                  // the SAME person and are merged (well above
                                  // match_threshold and the max distinct-speaker
                                  // similarity, so only confident duplicates fuse)
  float cosession_merge_threshold = 0.85f;  // stricter threshold for two globals
                                  // that ever co-occurred in one session: the
                                  // diarizer judged them distinct, so only a
                                  // very high cosine (a diarizer over-split of one
                                  // person) may merge them
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
  // Rebuild every global speaker's registry centroid from the best references
  // of all local slots currently mapped to it (cross-session accumulation): a
  // returning speaker's voiceprint strengthens over sessions, so it reliably
  // re-matches its existing id instead of fragmenting into a new one. The
  // registry is never capped -- genuinely new speakers still enroll.
  void RefreshGlobalCentroids();
  // Reconcile the registry: merge any two global ids whose centroids are
  // confidently the same person (cosine > merge_threshold). This repairs the
  // unavoidable early-session duplicate (a returning speaker enrolled before its
  // centroid was strong enough to match) WITHOUT capping the speaker count.
  void MergeReconcile();
  std::string NewGlobalId();
  // Embed the centre of a span (edge-trimmed, window-capped) with TitaNet;
  // returns empty if the audio has aged out of the retain window.
  std::vector<float> EmbedSpan(double start_sec, double end_sec);

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
  std::map<int, std::string> local_to_global_;      // local -> canonical global id
  std::map<std::string, std::vector<float>> global_centroid_;  // id -> centroid
  int next_global_id_ = 0;
};

}  // namespace pipeline
}  // namespace orator
