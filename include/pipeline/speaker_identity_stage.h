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
  float local_drift_threshold = 0.0f;  // disabled when <= 0. If a later clean
                                  // span from the same diarizer-local slot has
                                  // cosine below this value against the current
                                  // local epoch centroid, start a new identity
                                  // epoch for that slot instead of rewriting
                                  // the previous epoch.
  double local_drift_min_span_sec = 5.0;  // shortest clean span allowed to
                                  // trigger an epoch split; shorter spans still
                                  // refine the current epoch if accepted by the
                                  // regular clean-span gate.
  double local_drift_min_epoch_sec = 60.0;  // minimum age before a local epoch
                                  // may split; prevents early noisy spans from
                                  // fragmenting a speaker.
  bool local_drift_allow_same_session_match = true;  // a drifted epoch may
                                  // match a global id owned by another local
                                  // slot from the same diarizer session.
  float local_drift_competing_threshold = 0.0f;  // disabled when <= 0. If a
                                  // later clean span is closer to another
                                  // existing global id than the current epoch
                                  // by local_drift_competing_margin, start a
                                  // new epoch bound to that competing id.
  float local_drift_competing_margin = 0.05f;
  double local_drift_competing_min_span_sec = 5.0;
  float local_drift_competing_candidate_threshold = 0.0f;  // disabled when <=0.
                                  // Weak competing evidence below the strong
                                  // threshold is held pending and only applied
                                  // if a later strong competing span confirms
                                  // the same global id.
  float local_drift_competing_candidate_margin = 0.05f;
  int local_drift_competing_candidate_min_confirmations = 0;  // disabled at 0;
                                  // repeated non-overlapping candidate spans
                                  // naming one competing id confirm a split
  double local_drift_competing_backfill_sec = 0.0;  // max pending age to
                                  // backfill from a later confirmed split.
  double local_drift_competing_backfill_gap_sec = 3.0;  // same-local gap used
                                  // to extend the pending start to the short
                                  // local run preceding the clean span.
};

class SpeakerIdentityStage {
 public:
  struct VoiceprintScore {
    std::string speaker_id;
    float score = 0.0f;
  };

  struct SpanEvidence {
    bool embedding_available = false;
    bool session_gallery_complete = false;
    bool robust_gallery_complete = false;
    std::vector<VoiceprintScore> session_scores;
    std::vector<VoiceprintScore> robust_scores;
  };

  struct RetainedReferenceEvidence {
    std::string evidence_id;
    int local_speaker = -1;
    double epoch_start_sec = 0.0;
    std::string speaker_id;
    double source_start_sec = 0.0;
    double source_end_sec = 0.0;
    long embedding_start_sample = 0;
    long embedding_end_sample = 0;
    double embedding_start_sec = 0.0;
    double embedding_end_sec = 0.0;
    double quality = 0.0;
  };

  struct OverlapExcludedSpanEvidence {
    SpanEvidence evidence;
    long query_embedding_start_sample = 0;
    long query_embedding_end_sample = 0;
    std::vector<std::string> intersecting_reference_ids;
  };

  SpeakerIdentityStage(core::ISpeakerEmbedder* embedder,
                       model::SpeakerDatabase* db, core::TimeBase tb,
                       SpeakerIdConfig config);

  // Feed mono-16k audio in stream order (called on the diarization thread).
  void AppendAudio(const float* samples, int n);

  // Resolve global identities and fill DiarSegment::speaker_id in place
  // (called on the diarization thread, before the segments are delivered).
  void Process(std::vector<core::DiarSegment>& segs);

  // Query the same retained audio and TitaNet model against two independent
  // session galleries. This returns model evidence only; it never chooses an
  // identity or mutates the diarization track.
  SpanEvidence EvaluateSpan(double start_sec, double end_sec,
                            const std::vector<std::string>& active_ids,
                            double min_duration_sec,
                            double edge_margin_sec,
                            double max_window_sec);

  // Diagnostic-only view of the same query after removing retained gallery
  // references that contain any of the query's embedded samples. It returns
  // raw model evidence and never changes identity or runtime state.
  OverlapExcludedSpanEvidence EvaluateSpanWithoutOverlappingReferences(
      double start_sec, double end_sec,
      const std::vector<std::string>& active_ids, double min_duration_sec,
      double edge_margin_sec, double max_window_sec);

  // Final retained gallery provenance for evidence replay. Reference contents
  // remain private; this exposes only immutable source and embedding bounds.
  std::vector<RetainedReferenceEvidence> RetainedReferences() const;

  // Cache acoustic-only evidence without consulting either speaker gallery.
  // Final EvaluateSpan calls still score against the then-current galleries.
  bool PrecomputeSpan(double start_sec, double end_sec, double min_duration_sec,
                      double edge_margin_sec, double max_window_sec);
  std::size_t cached_embedding_count() const;

  std::string IdentityAt(int local_speaker, double at_sec) const;

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
  struct LocalReference {
    double quality = 0.0;
    double source_start_sec = 0.0;
    double source_end_sec = 0.0;
    long embedding_start_sample = 0;
    long embedding_end_sample = 0;
    std::vector<float> embedding;
  };
  struct LocalEpoch {
    double start_sec = 0.0;
    std::string global_id;
    std::vector<LocalReference> refs;
    std::vector<float> centroid;
    double last_embedded_end = 0.0;
    bool allow_same_session_match = false;
  };
  struct PendingCompetingEpoch {
    bool valid = false;
    double start_sec = 0.0;
    double clean_start_sec = 0.0;
    double end_sec = 0.0;
    double reference_start_sec = 0.0;
    double reference_end_sec = 0.0;
    double quality = 0.0;
    std::string global_id;
    float own_score = 0.0f;
    float competing_score = 0.0f;
    int confirmations = 0;
    std::vector<float> emb;
  };

  LocalEpoch& EnsureEpoch(int local, double start_sec);
  LocalEpoch& StartEpoch(int local, double start_sec,
                         bool allow_same_session_match);
  LocalEpoch* ActiveEpoch(int local, double at_sec);
  const LocalEpoch* ActiveEpoch(int local, double at_sec) const;
  void AddReference(LocalEpoch* epoch, double quality, double source_start_sec,
                    double source_end_sec,
                    const std::vector<float>& emb);
  float Cosine(const std::vector<float>& a, const std::vector<float>& b) const;
  bool ShouldSplitEpoch(const LocalEpoch& epoch, double start_sec,
                        double end_sec,
                        const std::vector<float>& emb) const;
  double BackfillStartForLocal(const core::DiarSegment& s,
                               const std::vector<core::DiarSegment>& all,
                               int local) const;
  std::pair<double, double> EmbeddingWindow(double start_sec,
                                            double end_sec) const;
  std::pair<double, double> EmbeddingWindow(double start_sec, double end_sec,
                                            double edge_margin_sec,
                                            double max_window_sec) const;
  std::set<std::string> OverlappingGlobalIds(
      const core::DiarSegment& s, const std::vector<core::DiarSegment>& all,
      int local) const;
  std::string BestCompetingGlobal(const LocalEpoch& epoch,
                                  const std::vector<float>& emb,
                                  const std::set<std::string>& blocked_ids,
                                  float threshold, float margin,
                                  float* own_score,
                                  float* best_score) const;
  bool RecordPendingCompeting(int local, const core::DiarSegment& s,
                              double backfill_start, double quality,
                              const std::vector<float>& emb,
                              const std::string& global_id, float own_score,
                              float competing_score);
  // Match the local centroid voiceprint against the registry; enroll if unseen.
  void ResolveGlobal(int local, LocalEpoch* epoch);
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
  std::vector<float> EmbedSpan(double start_sec, double end_sec,
                               double edge_margin_sec,
                               double max_window_sec);

  core::ISpeakerEmbedder* embedder_;
  model::SpeakerDatabase* db_;
  core::TimeBase tb_;
  SpeakerIdConfig config_;
  RetainedAudioBuffer audio_;

  // TitaNet owns reusable CUDA scratch and is not re-entrant. This mutex also
  // protects the immutable-audio embedding cache shared by the diar worker and
  // the speaker-evidence precompute worker.
  mutable std::mutex embedding_mutex_;
  std::map<std::pair<long, long>, std::vector<float>> embedding_cache_;

  // Diarization-thread-only state (no lock needed). Per local speaker: the best
  // reference embeddings (quality = confidence x duration) + their centroid.
  std::map<int, std::vector<LocalEpoch>> local_epochs_;
  std::map<int, PendingCompetingEpoch> pending_competing_;
  std::map<std::string, std::vector<float>> global_centroid_;  // id -> centroid
  int next_global_id_ = 0;
};

}  // namespace pipeline
}  // namespace orator
